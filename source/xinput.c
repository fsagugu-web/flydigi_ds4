// SPDX-License-Identifier: GPL-3.0-only
// Xbox 360 (XInput) USB input report -> RawPad. No PS4 dependencies, so it is
// unit tested on a PC (see tests/tests.c).

#include <stdint.h>

#include "flydigi.h"

// Bit of each input in the report's button word; LT/RT are analog (bytes 4-5).
static const uint16_t k_input_bits[IN_COUNT] = {
    [IN_UP] = 0x0001,    [IN_DOWN] = 0x0002, [IN_LEFT] = 0x0004, [IN_RIGHT] = 0x0008,
    [IN_START] = 0x0010, [IN_BACK] = 0x0020, [IN_L3] = 0x0040,   [IN_R3] = 0x0080,
    [IN_LB] = 0x0100,    [IN_RB] = 0x0200,   [IN_GUIDE] = 0x0400, [IN_A] = 0x1000,
    [IN_B] = 0x2000,     [IN_X] = 0x4000,    [IN_Y] = 0x8000,
};

bool xinput_parse_report(const uint8_t *report, int len, RawPad *pad) {
    if (len < 14 || report[0] != 0x00 || report[1] < 0x14) {
        return false; // not an input report (LED status, rumble ack...)
    }
    uint16_t buttons = (uint16_t)(report[2] | (report[3] << 8));
    pad->inputs = 0;
    for (int i = 0; i < IN_COUNT; i++) {
        if (buttons & k_input_bits[i]) {
            pad->inputs |= 1u << i;
        }
    }
    pad->lt = report[4];
    pad->rt = report[5];
    pad->lx = (int16_t)(report[6] | (report[7] << 8));
    pad->ly = (int16_t)(report[8] | (report[9] << 8));
    pad->rx = (int16_t)(report[10] | (report[11] << 8));
    pad->ry = (int16_t)(report[12] | (report[13] << 8));

    // Direwolf 2 motion (bytes 14-16, reserved and zero on a standard pad):
    // two 12-bit signed values, low bytes first, high nibbles packed in byte 16.
    pad->motion_h = 0;
    pad->motion_v = 0;
    if (len >= 17) {
        int32_t h = report[14] | ((report[16] >> 4) << 8);
        int32_t v = report[15] | ((report[16] & 0x0F) << 8);
        pad->motion_h = (int16_t)(h >= 0x800 ? h - 0x1000 : h);
        pad->motion_v = (int16_t)(v >= 0x800 ? v - 0x1000 : v);
    }
    return true;
}
