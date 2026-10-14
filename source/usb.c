// SPDX-License-Identifier: GPL-3.0-only
// Reads an XInput (Xbox 360 protocol) USB controller through libSceUsbd and
// publishes its state translated to DS4 buttons/sticks. Tested device:
// Flydigi Direwolf 2 (045E:028E, interface FF/5D/01, IN 0x81, OUT 0x02).

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>

#include <Syscall.h>

#include "flydigi.h"
#include "pad.h"

#define START_DELAY_US (3u * 1000u * 1000u)
#define RESCAN_DELAY_US (2u * 1000u * 1000u)
#define READ_TIMEOUT_MS 50
#define FAST_ERRORS_BEFORE_RECONNECT 20
#define POWER_TICK_INTERVAL_US (5u * 1000u * 1000u)
#define MAX_LOGGED_INPUT_CHANGES 200

int32_t sceSystemServicePowerTick(void);

// ---------------------------------------------------------------------------
// libSceUsbd (libusb-1.0 clone), resolved at runtime.
// ---------------------------------------------------------------------------

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} UsbDeviceDescriptor;

typedef struct {
    int32_t (*Init)(void);
    int32_t (*Exit)(void);
    int32_t (*GetDeviceList)(void ***list);
    int32_t (*FreeDeviceList)(void **list, int32_t unref);
    int32_t (*GetDeviceDescriptor)(void *dev, UsbDeviceDescriptor *desc);
    int32_t (*Open)(void *dev, void **handle);
    void (*Close)(void *handle);
    int32_t (*KernelDriverActive)(void *handle, int32_t iface);
    int32_t (*DetachKernelDriver)(void *handle, int32_t iface);
    int32_t (*AttachKernelDriver)(void *handle, int32_t iface);
    int32_t (*ClaimInterface)(void *handle, int32_t iface);
    int32_t (*ReleaseInterface)(void *handle, int32_t iface);
    int32_t (*ControlTransfer)(void *handle, uint8_t type, uint8_t request, uint16_t value,
                               uint16_t index, uint8_t *data, uint16_t length, uint32_t timeout);
    int32_t (*InterruptTransfer)(void *handle, uint8_t endpoint, uint8_t *data, int32_t length,
                                 int32_t *transferred, uint32_t timeout);
} UsbdApi;

static const struct {
    const char *name;
    size_t offset;
    bool required;
} k_usbd_symbols[] = {
    {"sceUsbdInit", offsetof(UsbdApi, Init), true},
    {"sceUsbdExit", offsetof(UsbdApi, Exit), true},
    {"sceUsbdGetDeviceList", offsetof(UsbdApi, GetDeviceList), true},
    {"sceUsbdFreeDeviceList", offsetof(UsbdApi, FreeDeviceList), true},
    {"sceUsbdGetDeviceDescriptor", offsetof(UsbdApi, GetDeviceDescriptor), true},
    {"sceUsbdOpen", offsetof(UsbdApi, Open), true},
    {"sceUsbdClose", offsetof(UsbdApi, Close), true},
    {"sceUsbdKernelDriverActive", offsetof(UsbdApi, KernelDriverActive), false},
    {"sceUsbdDetachKernelDriver", offsetof(UsbdApi, DetachKernelDriver), false},
    {"sceUsbdAttachKernelDriver", offsetof(UsbdApi, AttachKernelDriver), false},
    {"sceUsbdClaimInterface", offsetof(UsbdApi, ClaimInterface), true},
    {"sceUsbdReleaseInterface", offsetof(UsbdApi, ReleaseInterface), true},
    {"sceUsbdControlTransfer", offsetof(UsbdApi, ControlTransfer), true},
    {"sceUsbdInterruptTransfer", offsetof(UsbdApi, InterruptTransfer), true},
};

static UsbdApi g_usbd;

typedef struct {
    void *handle;
    int iface;
    uint8_t ep_in;
    uint8_t ep_out;
    bool detached;
} Device;

// ---------------------------------------------------------------------------
// State shared with the game threads (double buffered, lock free).
// ---------------------------------------------------------------------------

static FlydigiState g_states[2];
static int g_state_index;
static bool g_connected;
static uint32_t g_rumble; // large << 8 | small
static bool g_running;

static void publish_state(const FlydigiState *state) {
    int next = 1 - __atomic_load_n(&g_state_index, __ATOMIC_RELAXED);
    g_states[next] = *state;
    __atomic_store_n(&g_state_index, next, __ATOMIC_RELEASE);
}

bool flydigi_get_state(FlydigiState *out) {
    if (!__atomic_load_n(&g_connected, __ATOMIC_ACQUIRE)) {
        return false;
    }
    *out = g_states[__atomic_load_n(&g_state_index, __ATOMIC_ACQUIRE)];
    return true;
}

void flydigi_set_rumble(uint8_t large, uint8_t small) {
    __atomic_store_n(&g_rumble, (uint32_t)(large << 8 | small), __ATOMIC_RELEASE);
}

// ---------------------------------------------------------------------------
// Device discovery
// ---------------------------------------------------------------------------

static int load_usbd(void) {
    int32_t ret = sceSysmoduleLoadModule(ORBIS_SYSMODULE_USBD);
    log_line("sceSysmoduleLoadModule(USBD) = 0x%08X\n", ret);

    char path[128];
    int handle = -1;
    snprintf(path, sizeof(path), "/%s/common/lib/libSceUsbd.sprx", sceKernelGetFsSandboxRandomWord());
    ret = sys_dynlib_load_prx(path, &handle);
    if (ret != 0) {
        handle = -1;
        ret = sys_dynlib_load_prx("/system/common/lib/libSceUsbd.sprx", &handle);
    }
    log_line("libSceUsbd: 0x%08X, handle %d\n", ret, handle);
    if (ret != 0) {
        return -1;
    }

    for (size_t i = 0; i < sizeof(k_usbd_symbols) / sizeof(k_usbd_symbols[0]); i++) {
        void **slot = (void **)((uint8_t *)&g_usbd + k_usbd_symbols[i].offset);
        if (sys_dynlib_dlsym(handle, k_usbd_symbols[i].name, slot) != 0) {
            *slot = NULL;
        }
        if (*slot == NULL && k_usbd_symbols[i].required) {
            log_line("missing required symbol: %s\n", k_usbd_symbols[i].name);
            return -1;
        }
    }
    return 0;
}

static bool is_never_touch(const UsbDeviceDescriptor *desc) {
    return desc->idVendor == 0x054C     // Sony: DS4, camera, internal devices
           || desc->bDeviceClass == 0x09  // hub
           || desc->bDeviceClass == 0x08  // mass storage
           || desc->bDeviceClass == 0xE0; // wireless / Bluetooth
}

// Finds the Xbox 360 gamepad interface (FF/5D/01, alternate setting 0) in a
// raw configuration descriptor.
static bool find_xinput_interface(const uint8_t *cfg, int len, Device *out) {
    bool in_pad_iface = false;
    bool found = false;

    for (int pos = 0; pos + 2 <= len;) {
        uint8_t dlen = cfg[pos];
        uint8_t dtype = cfg[pos + 1];
        if (dlen < 2 || pos + dlen > len) {
            break;
        }
        if (dtype == 0x04 && dlen >= 9) {
            if (found) {
                break; // past the pad interface's endpoints
            }
            in_pad_iface = cfg[pos + 3] == 0 && cfg[pos + 5] == 0xFF && cfg[pos + 6] == 0x5D && cfg[pos + 7] == 0x01;
            if (in_pad_iface) {
                out->iface = cfg[pos + 2];
                out->ep_in = 0;
                out->ep_out = 0;
                found = true;
            }
        } else if (dtype == 0x05 && dlen >= 7 && in_pad_iface) {
            uint8_t addr = cfg[pos + 2];
            if ((cfg[pos + 3] & 0x03) == 0x03) {
                if ((addr & 0x80) && out->ep_in == 0) {
                    out->ep_in = addr;
                } else if (!(addr & 0x80) && out->ep_out == 0) {
                    out->ep_out = addr;
                }
            }
        }
        pos += dlen;
    }
    return found && out->ep_in != 0;
}

static int read_config_descriptor(void *handle, uint8_t *buf, int size) {
    if (g_usbd.ControlTransfer(handle, 0x80, 0x06, 0x0200, 0, buf, 9, 500) < 9) {
        return -1;
    }
    int total = buf[2] | (buf[3] << 8);
    if (total > size) {
        total = size;
    }
    return g_usbd.ControlTransfer(handle, 0x80, 0x06, 0x0200, 0, buf, (uint16_t)total, 500);
}

static void send_out(const Device *dev, uint8_t *data, int len) {
    if (dev->ep_out == 0) {
        return;
    }
    int32_t sent = 0;
    g_usbd.InterruptTransfer(dev->handle, dev->ep_out, data, len, &sent, 100);
}

static void send_rumble(const Device *dev, uint8_t large, uint8_t small) {
    uint8_t packet[8] = {0x00, 0x08, 0x00, large, small, 0x00, 0x00, 0x00};
    send_out(dev, packet, sizeof(packet));
}

static void log_string_descriptor(void *handle, const char *label, uint8_t index) {
    if (index == 0) {
        return;
    }
    uint8_t raw[255];
    int32_t ret = g_usbd.ControlTransfer(handle, 0x80, 0x06, (uint16_t)(0x0300 | index), 0x0409, raw, sizeof(raw), 500);
    char text[64];
    int n = 0;
    for (int i = 2; i + 1 < ret && n < (int)sizeof(text) - 1; i += 2) {
        text[n++] = (raw[i + 1] == 0 && raw[i] >= 0x20 && raw[i] < 0x7F) ? (char)raw[i] : '?';
    }
    text[n] = '\0';
    log_line("  %s: \"%s\"\n", label, text);
}

// verbose: describe the device in the log, so that a log from someone with an
// unsupported controller is enough to add support for it.
static bool try_open_device(void *usb_dev, const UsbDeviceDescriptor *desc, bool verbose, Device *out) {
    void *handle = NULL;
    if (g_usbd.Open(usb_dev, &handle) != 0 || handle == NULL) {
        if (verbose) {
            log_line("  could not open\n");
        }
        return false;
    }
    uint8_t cfg[512];
    int cfg_len = read_config_descriptor(handle, cfg, sizeof(cfg));
    if (verbose) {
        log_string_descriptor(handle, "manufacturer", desc->iManufacturer);
        log_string_descriptor(handle, "product", desc->iProduct);
    }
    if (cfg_len <= 0 || !find_xinput_interface(cfg, cfg_len, out)) {
        if (verbose) {
            log_line("  no XInput interface (not supported yet). Descriptor:\n");
            if (cfg_len > 0) {
                log_hex("  cfg ", cfg, cfg_len);
            }
        }
        g_usbd.Close(handle);
        return false;
    }
    out->handle = handle;
    out->detached = false;
    if (g_usbd.KernelDriverActive && g_usbd.DetachKernelDriver && g_usbd.KernelDriverActive(handle, out->iface) == 1) {
        out->detached = g_usbd.DetachKernelDriver(handle, out->iface) == 0;
    }
    int32_t ret = g_usbd.ClaimInterface(handle, out->iface);
    if (ret != 0) {
        log_line("%04X:%04X: could not claim interface %d (0x%08X)\n", desc->idVendor, desc->idProduct, out->iface, ret);
        if (out->detached && g_usbd.AttachKernelDriver) {
            g_usbd.AttachKernelDriver(handle, out->iface);
        }
        g_usbd.Close(handle);
        return false;
    }
    log_line("controller %04X:%04X opened: interface %d, IN 0x%02X, OUT 0x%02X\n", desc->idVendor, desc->idProduct,
             out->iface, out->ep_in, out->ep_out);
    uint8_t led[3] = {0x01, 0x03, 0x06}; // player 1 LED
    send_out(out, led, sizeof(led));
    return true;
}

static int g_last_device_count = -1;

static bool open_controller(Device *out) {
    void **list = NULL;
    int32_t count = g_usbd.GetDeviceList(&list);
    if (count < 0 || list == NULL) {
        return false;
    }
    // Describe the devices once, and again whenever something is plugged in.
    bool verbose = g_config.log && count != g_last_device_count;
    g_last_device_count = count;

    bool opened = false;
    for (int i = 0; i < count && !opened; i++) {
        UsbDeviceDescriptor desc;
        memset(&desc, 0, sizeof(desc));
        if (g_usbd.GetDeviceDescriptor(list[i], &desc) != 0) {
            continue;
        }
        bool skip = is_never_touch(&desc);
        if (verbose) {
            log_line("usb: %04X:%04X class %02X/%02X/%02X%s\n", desc.idVendor, desc.idProduct, desc.bDeviceClass,
                     desc.bDeviceSubClass, desc.bDeviceProtocol, skip ? " (skipped)" : "");
        }
        if (!skip) {
            opened = try_open_device(list[i], &desc, verbose, out);
        }
    }
    g_usbd.FreeDeviceList(list, 1);
    return opened;
}

static void close_controller(Device *dev) {
    send_rumble(dev, 0, 0);
    g_usbd.ReleaseInterface(dev->handle, dev->iface);
    if (dev->detached && g_usbd.AttachKernelDriver) {
        g_usbd.AttachKernelDriver(dev->handle, dev->iface);
    }
    g_usbd.Close(dev->handle);
    dev->handle = NULL;
}

// ---------------------------------------------------------------------------
// Read loop
// ---------------------------------------------------------------------------

static void log_input_change(const RawPad *pad, int *logged) {
    if (*logged >= MAX_LOGGED_INPUT_CHANGES) {
        return;
    }
    (*logged)++;
    log_line("input: buttons %05X  LT %3u  RT %3u  LX %6d  LY %6d  RX %6d  RY %6d\n", pad->inputs, pad->lt, pad->rt,
             pad->lx, pad->ly, pad->rx, pad->ry);
}

// Runs until the controller stops answering or the plugin is unloaded.
static void run_controller(const Device *dev) {
    uint8_t report[64];
    uint32_t sent_rumble = 0;
    int fast_errors = 0;
    int logged = 0;
    uint32_t last_inputs = 0xFFFFFFFF;
    FlydigiState last_state = k_neutral_state;
    uint64_t last_power_tick = 0;
    TouchTracker touch;
    memset(&touch, 0, sizeof(touch));
    GyroTracker gyro;
    memset(&gyro, 0, sizeof(gyro));
    RawPad pad;
    memset(&pad, 0, sizeof(pad));
    bool have_pad = false;

    while (__atomic_load_n(&g_running, __ATOMIC_ACQUIRE)) {
        int32_t got = 0;
        uint64_t started = sceKernelGetProcessTime();
        int32_t ret = g_usbd.InterruptTransfer(dev->handle, dev->ep_in, report, sizeof(report), &got,
                                               READ_TIMEOUT_MS);
        uint64_t now = sceKernelGetProcessTime();

        bool fresh = got > 0 && xinput_parse_report(report, got, &pad);
        if (fresh) {
            fast_errors = 0;
            have_pad = true;
            // Log button changes only; the Direwolf 2 also streams motion data.
            uint32_t inputs = pad.inputs | (pad.lt > g_config.trigger_threshold ? 1u << IN_LT : 0) |
                              (pad.rt > g_config.trigger_threshold ? 1u << IN_RT : 0);
            if (inputs != last_inputs) {
                log_input_change(&pad, &logged);
                last_inputs = inputs;
            }
        } else if (got == 0 && ret != 0 && now - started < (READ_TIMEOUT_MS * 1000u) / 2) {
            // Failed long before the timeout: the device is likely gone.
            have_pad = false;
            publish_state(&k_neutral_state);
            if (++fast_errors >= FAST_ERRORS_BEFORE_RECONNECT) {
                log_line("read failed: 0x%08X\n", ret);
                return;
            }
            sceKernelUsleep(5000);
        }

        if (g_config.gyro) {
            if (fresh) {
                gyro_feed(&gyro, pad.motion_h, pad.motion_v, now);
            } else if (have_pad && gyro_moving(&gyro)) {
                gyro_feed(&gyro, 0, 0, now); // silent controller: let the rate fall to zero
            }
        }

        // Touch timers (tap click, hold click) and the gyro also advance while
        // the controller is silent, so re-translate the last report then.
        if (fresh || (have_pad && (touch.holding || touch.click_until != 0 || gyro_moving(&gyro)))) {
            FlydigiState state;
            memset(&state, 0, sizeof(state));
            pad_translate(&pad, &g_config, &touch, now, &state);
            gyro_apply(&gyro, &g_config, &state);
            publish_state(&state);

            // Player activity keeps the console from dimming or sleeping.
            // Motion is left out: the sensor is never perfectly quiet, even
            // with the controller lying on a table.
            FlydigiState activity = state;
            activity.gyro_x = 0.0f;
            activity.gyro_y = 0.0f;
            if (memcmp(&activity, &last_state, sizeof(activity)) != 0) {
                last_state = activity;
                if (now - last_power_tick > POWER_TICK_INTERVAL_US) {
                    sceSystemServicePowerTick();
                    last_power_tick = now;
                }
            }
        }

        uint32_t rumble = __atomic_load_n(&g_rumble, __ATOMIC_ACQUIRE);
        if (rumble != sent_rumble) {
            send_rumble(dev, (uint8_t)(rumble >> 8), (uint8_t)rumble);
            sent_rumble = rumble;
        }
    }
}

static char g_startup_notice[160];

static void *usb_thread(void *arg) {
    (void)arg;
    sceKernelUsleep(START_DELAY_US);
    if (g_startup_notice[0] != '\0') {
        notify("%s", g_startup_notice);
    }
    if (load_usbd() != 0) {
        notify("Flydigi: USB library not available in this game");
        return NULL;
    }
    int32_t ret = g_usbd.Init();
    if (ret != 0) {
        log_line("sceUsbdInit = 0x%08X\n", ret);
        notify("Flydigi: USB init failed (0x%08X)", ret);
        return NULL;
    }

    bool announced_missing = false;
    while (__atomic_load_n(&g_running, __ATOMIC_ACQUIRE)) {
        Device dev;
        memset(&dev, 0, sizeof(dev));
        if (!open_controller(&dev)) {
            if (!announced_missing) {
                log_line("no XInput controller found, retrying every 2 s\n");
                announced_missing = true;
            }
            sceKernelUsleep(RESCAN_DELAY_US);
            continue;
        }
        announced_missing = false;
        publish_state(&k_neutral_state);
        __atomic_store_n(&g_connected, true, __ATOMIC_RELEASE);
        if (g_config.notifications) {
            notify("Flydigi connected");
        }

        run_controller(&dev);

        __atomic_store_n(&g_connected, false, __ATOMIC_RELEASE);
        publish_state(&k_neutral_state);
        close_controller(&dev);
        if (__atomic_load_n(&g_running, __ATOMIC_ACQUIRE)) {
            if (g_config.notifications) {
                notify("Flydigi disconnected");
            }
            sceKernelUsleep(RESCAN_DELAY_US);
        }
    }
    g_usbd.Exit();
    return NULL;
}

static OrbisPthread g_thread;
static bool g_thread_started;

// startup_notice (may be NULL) is shown once the game has had time to boot.
void flydigi_start(const char *startup_notice) {
    snprintf(g_startup_notice, sizeof(g_startup_notice), "%s", startup_notice ? startup_notice : "");
    publish_state(&k_neutral_state);
    __atomic_store_n(&g_running, true, __ATOMIC_RELEASE);
    g_thread_started = scePthreadCreate(&g_thread, NULL, usb_thread, NULL, "flydigi_usb") == 0;
}

// Waits for the thread so it never runs after the plugin code is unmapped.
void flydigi_stop(void) {
    __atomic_store_n(&g_running, false, __ATOMIC_RELEASE);
    if (g_thread_started) {
        scePthreadJoin(g_thread, NULL);
        g_thread_started = false;
    }
}
