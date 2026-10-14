// SPDX-License-Identifier: GPL-3.0-only
// flydigi_ds4 - GoldHEN plugin that lets a Flydigi Direwolf 2 (USB, XInput
// mode) play PS4 games. The controller is read in usb.c and merged into what
// the game gets from libScePad, so the DS4 keeps working alongside it.
//
// The pad hooks follow GoldHEN's gamepad_helper plugin (jocover): scePadRead /
// scePadReadState are detoured, and the hooks fetch the real data through the
// *Ext variants, patched to behave like the plain functions.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>

#include <Common.h>
#include <Patcher.h>

#include "flydigi.h"
#include "pad.h"

#define attr_module_hidden __attribute__((weak)) __attribute__((visibility("hidden")))
#define attr_public __attribute__((visibility("default")))

attr_public const char *g_pluginName = "flydigi_ds4";
attr_public const char *g_pluginDesc = "Flydigi Direwolf 2 (USB) as a PS4 controller";
attr_public const char *g_pluginAuth = "flydigi_ds4";
attr_public uint32_t g_pluginVersion = 0x00000130; // 1.30

#define LOG_PATH "/data/GoldHEN/flydigi_ds4.txt"
#define CONFIG_PATH "/data/GoldHEN/flydigi_ds4.ini"
#define CONFIG_MAX_SIZE (16 * 1024)

HOOK_INIT(scePadRead);
HOOK_INIT(scePadReadState);
HOOK_INIT(scePadSetVibration);

static Patcher g_read_ext_patcher;
static Patcher g_read_state_ext_patcher;

// Merges the Flydigi into one sample the game is about to receive.
static void merge_flydigi(ScePadData *data) {
    FlydigiState state;
    if (!flydigi_get_state(&state)) {
        return;
    }
    // System overlay open (PS button menu etc.): the game must not see input.
    if (data->buttons & SCE_PAD_BUTTON_INTERCEPTED) {
        return;
    }
    if (!data->connected) {
        // DS4 off: present a neutral, connected pad driven only by the Flydigi.
        data->buttons = 0;
        data->leftStick.x = data->leftStick.y = 0x80;
        data->rightStick.x = data->rightStick.y = 0x80;
        data->analogButtons.l2 = data->analogButtons.r2 = 0;
        memset(&data->orientation, 0, sizeof(data->orientation));
        data->orientation.w = 1.0f;
        memset(&data->acceleration, 0, sizeof(data->acceleration));
        memset(&data->angularVelocity, 0, sizeof(data->angularVelocity));
        memset(&data->touchData, 0, sizeof(data->touchData));
        data->timestamp = sceKernelGetProcessTime();
        data->connected = true;
        if (data->connectedCount == 0) {
            data->connectedCount = 1;
        }
    }

    data->buttons |= state.buttons;
    if (state.left_active) {
        data->leftStick.x = state.lx;
        data->leftStick.y = state.ly;
    }
    if (state.right_active) {
        data->rightStick.x = state.rx;
        data->rightStick.y = state.ry;
    }
    if (state.l2 > data->analogButtons.l2) {
        data->analogButtons.l2 = state.l2;
    }
    if (state.r2 > data->analogButtons.r2) {
        data->analogButtons.r2 = state.r2;
    }
    if (state.touch_active) {
        data->touchData.touchNum = 1;
        data->touchData.touch[0].x = state.touch_x;
        data->touchData.touch[0].y = state.touch_y;
        data->touchData.touch[0].id = state.touch_id;
    }
    // Added, so a DS4 lying still contributes nothing and a moving one still works.
    data->angularVelocity.x += state.gyro_x;
    data->angularVelocity.y += state.gyro_y;
}

int32_t scePadRead_hook(int32_t handle, ScePadData *data, int32_t count) {
    int32_t ret = scePadReadExt(handle, data, count);
    if (ret <= 0) {
        return ret;
    }
    for (int32_t i = 0; i < ret; i++) {
        merge_flydigi(&data[i]);
    }
    return ret;
}

int32_t scePadReadState_hook(int32_t handle, ScePadData *data) {
    int32_t ret = scePadReadStateExt(handle, data);
    if (ret == 0) {
        merge_flydigi(data);
    }
    return ret;
}

int32_t scePadSetVibration_hook(int32_t handle, const ScePadVibrationParam *param) {
    if (param) {
        flydigi_set_rumble((uint8_t)(param->largeMotor * g_config.vibration / 100),
                           (uint8_t)(param->smallMotor * g_config.vibration / 100));
    }
    return HOOK_CONTINUE(scePadSetVibration, int32_t (*)(int32_t, const ScePadVibrationParam *), handle, param);
}

// Returns the file size, or -1 when it cannot be read.
static int read_text_file(const char *path, char *buf, size_t size) {
    int fd = sceKernelOpen(path, 0 /* O_RDONLY */, 0);
    if (fd < 0) {
        return -1;
    }
    size_t total = 0;
    while (total < size - 1) {
        int64_t n = (int64_t)sceKernelRead(fd, buf + total, size - 1 - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    sceKernelClose(fd);
    buf[total] = '\0';
    return (int)total;
}

// Fills g_config from flydigi_ds4.ini. Returns a notice for the player when
// the file has mistakes, otherwise NULL.
static const char *load_config(const char *titleid, bool *found) {
    static char text[CONFIG_MAX_SIZE];
    static char notice[160];
    config_set_defaults(&g_config);
    *found = read_text_file(CONFIG_PATH, text, sizeof(text)) >= 0;
    if (!*found) {
        return NULL;
    }
    ConfigErrors errors;
    config_parse(text, titleid, &g_config, &errors);
    if (errors.count == 0) {
        return NULL;
    }
    snprintf(notice, sizeof(notice), "flydigi_ds4.ini: %d error(s). Line %d: %s", errors.count, errors.first_line,
             errors.first_message);
    return notice;
}

int32_t attr_public plugin_load(int32_t argc, const char *argv[]) {
    struct proc_info info;
    memset(&info, 0, sizeof(info));
    bool have_info = sys_sdk_proc_info(&info) == 0;

    bool config_found;
    const char *notice = load_config(have_info ? info.titleid : NULL, &config_found);
    if (g_config.log) {
        log_open(LOG_PATH);
    }
    log_line("flydigi_ds4 v1.3 | GoldHEN SDK 0x%08X\n", sys_sdk_version());
    if (have_info) {
        log_line("game: %s (%s) version %s\n", info.titleid, info.name, info.version);
    }
    log_line("config: %s%s\n", config_found ? CONFIG_PATH : "defaults (file not found)",
             notice ? " - with errors" : "");
    if (notice) {
        log_line("%s\n", notice);
    }

    char module[256];
    snprintf(module, sizeof(module), "/%s/common/lib/libScePad.sprx", sceKernelGetFsSandboxRandomWord());
    int pad_handle = 0;
    sys_dynlib_load_prx(module, &pad_handle);

    // Original bytes, in case a firmware lays these functions out differently.
    log_hex("scePadRead         ", (const void *)scePadRead, 16);
    log_hex("scePadReadState    ", (const void *)scePadReadState, 16);
    log_hex("scePadReadExt      ", (const void *)scePadReadExt, 16);
    log_hex("scePadReadStateExt ", (const void *)scePadReadStateExt, 16);
    log_hex("scePadSetVibration ", (const void *)scePadSetVibration, 16);

    Patcher_Construct(&g_read_ext_patcher);
    Patcher_Construct(&g_read_state_ext_patcher);
    uint8_t xor_ecx_ecx[5] = {0x31, 0xC9, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(&g_read_ext_patcher, (uint64_t)scePadReadExt, xor_ecx_ecx, sizeof(xor_ecx_ecx));
    uint8_t xor_edx_edx[5] = {0x31, 0xD2, 0x90, 0x90, 0x90};
    Patcher_Install_Patch(&g_read_state_ext_patcher, (uint64_t)scePadReadStateExt, xor_edx_edx, sizeof(xor_edx_edx));

    HOOK32(scePadRead);
    HOOK32(scePadReadState);
    HOOK32(scePadSetVibration);
    log_line("hooks installed: read %p, readState %p, vibration %p\n", Detour_scePadRead.StubPtr,
             Detour_scePadReadState.StubPtr, Detour_scePadSetVibration.StubPtr);

    flydigi_start(notice);
    return 0;
}

int32_t attr_public plugin_unload(int32_t argc, const char *argv[]) {
    flydigi_stop();
    UNHOOK(scePadRead);
    UNHOOK(scePadReadState);
    UNHOOK(scePadSetVibration);
    Patcher_Destroy(&g_read_ext_patcher);
    Patcher_Destroy(&g_read_state_ext_patcher);
    log_line("plugin unloaded\n");
    log_close();
    return 0;
}

int32_t attr_module_hidden module_start(int64_t argc, const void *args) {
    return 0;
}

int32_t attr_module_hidden module_stop(int64_t argc, const void *args) {
    return 0;
}
