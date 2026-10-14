// SPDX-License-Identifier: GPL-3.0-only
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <orbis/libkernel.h>

#include <GoldHEN.h>

#include "flydigi.h"

// O_WRONLY | O_CREAT | O_TRUNC (FreeBSD values)
#define LOG_OPEN_FLAGS (0x0001 | 0x0200 | 0x0400)

static int g_log_fd = -1;

void log_open(const char *path) {
    g_log_fd = sceKernelOpen(path, LOG_OPEN_FLAGS, 0777);
    if (g_log_fd >= 0) {
        return;
    }
    // The game sandbox may hide /data; GoldHEN can lift it temporarily.
    struct jailbreak_backup backup;
    if (sys_sdk_jailbreak(&backup) == 0) {
        g_log_fd = sceKernelOpen(path, LOG_OPEN_FLAGS, 0777);
        sys_sdk_unjailbreak(&backup);
    }
}

void log_close(void) {
    if (g_log_fd >= 0) {
        sceKernelFsync(g_log_fd);
        sceKernelClose(g_log_fd);
        g_log_fd = -1;
    }
}

void log_line(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }
    if (len >= (int)sizeof(buf)) {
        len = sizeof(buf) - 1;
    }
    printf("[flydigi_ds4] %s", buf);
    if (g_log_fd >= 0) {
        sceKernelWrite(g_log_fd, buf, len);
    }
}

// Long buffers are split into lines of 32 bytes.
void log_hex(const char *label, const void *data, int len) {
    const uint8_t *bytes = data;
    for (int off = 0; off < len; off += 32) {
        char line[128];
        int pos = 0;
        for (int i = off; i < len && i < off + 32; i++) {
            pos += snprintf(line + pos, sizeof(line) - pos, "%02X ", bytes[i]);
        }
        log_line("%s%s\n", label, line);
    }
}

void notify(const char *fmt, ...) {
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message), fmt, args);
    va_end(args);
    req.type = NotificationRequest;
    req.useIconImageUri = 1;
    req.targetId = -1;
    strcpy(req.iconUri, "cxml://psnotification/tex_icon_system");
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
    log_line("[notification] %s\n", req.message);
}
