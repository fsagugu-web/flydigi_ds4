// SPDX-License-Identifier: GPL-3.0-only
// Motion from the Direwolf 2. In XInput mode it does not expose a raw IMU:
// bytes 14-16 of its reports carry two processed 12-bit values, horizontal
// (yaw and roll) and vertical (pitch), about every 4 ms, saturating at +-22.
// Summing them over a short window gives an angular velocity for the DS4
// gyroscope. No PS4 dependencies, so it is unit tested on a PC.

#include <stdint.h>

#include "flydigi.h"

#define GYRO_WINDOW_US (16u * 1000u)

// Motion units -> radians. Assumes a saturated sample (22 per ~4 ms) is a
// fast turn of about 4 rad/s; gyro_sensitivity scales from there.
#define GYRO_RAD_PER_UNIT (4.0f * 0.004f / 22.0f)

void gyro_feed(GyroTracker *gyro, int16_t horizontal, int16_t vertical, uint64_t now_us) {
    if (!gyro->started) {
        gyro->started = true;
        gyro->window_start = now_us;
    }
    gyro->sum_h += horizontal;
    gyro->sum_v += vertical;
    uint64_t elapsed = now_us - gyro->window_start;
    if (elapsed >= GYRO_WINDOW_US) {
        float seconds = (float)elapsed / 1000000.0f;
        gyro->rate_h = (float)gyro->sum_h * GYRO_RAD_PER_UNIT / seconds;
        gyro->rate_v = (float)gyro->sum_v * GYRO_RAD_PER_UNIT / seconds;
        gyro->sum_h = 0;
        gyro->sum_v = 0;
        gyro->window_start = now_us;
    }
}

// True while there is motion left to report, so a silent controller still
// brings the rate back to zero.
bool gyro_moving(const GyroTracker *gyro) {
    return gyro->sum_h != 0 || gyro->sum_v != 0 || gyro->rate_h != 0.0f || gyro->rate_v != 0.0f;
}

// Turning right gives a positive horizontal value, and DS4 yaw is positive
// to the left. Tilting the nose up is assumed to give a positive vertical
// value (XInput convention); gyro_invert_* fixes either guess per game.
void gyro_apply(const GyroTracker *gyro, const FlydigiConfig *cfg, FlydigiState *out) {
    out->gyro_x = 0.0f;
    out->gyro_y = 0.0f;
    if (!cfg->gyro) {
        return;
    }
    float scale = (float)cfg->gyro_sensitivity / 100.0f;
    out->gyro_x = gyro->rate_v * scale * (cfg->gyro_invert_vertical ? -1.0f : 1.0f);
    out->gyro_y = -gyro->rate_h * scale * (cfg->gyro_invert_horizontal ? -1.0f : 1.0f);
}
