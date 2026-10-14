// SPDX-License-Identifier: GPL-3.0-only
// RawPad -> DS4 state, applying the user's configuration. No PS4
// dependencies, so it is unit tested on a PC (see tests/tests.c).

#include <stdint.h>

#include "flydigi.h"
#include "pad.h"

#define TOUCH_WIDTH 1920
#define TOUCH_HEIGHT 942
#define TAP_CLICK_US (100u * 1000u)  // click sent after a quick tap
#define HOLD_CLICK_US (500u * 1000u) // holding still this long clicks and holds

const FlydigiState k_neutral_state = {.lx = 0x80, .ly = 0x80, .rx = 0x80, .ry = 0x80};

static uint8_t axis_to_ds4(int32_t value) {
    int32_t scaled = (value + 32768) >> 8;
    return (uint8_t)(scaled > 255 ? 255 : scaled);
}

// Xbox Y grows upwards, DS4 Y grows downwards.
static uint8_t axis_to_ds4_inverted(int32_t value) {
    int32_t scaled = (32768 - value) >> 8;
    return (uint8_t)(scaled > 255 ? 255 : scaled);
}

static bool stick_active(int32_t x, int32_t y, int32_t deadzone) {
    return x > deadzone || x < -deadzone || y > deadzone || y < -deadzone;
}

static uint8_t max_u8(uint8_t a, uint8_t b) {
    return a > b ? a : b;
}

// Stick deflection -> position on the touchpad, centred when the stick is.
// Full deflection reaches the edges (0 and size - 1 after clamping).
static uint16_t stick_to_touch(int32_t value, int32_t size, bool inverted) {
    int32_t half = size / 2;
    int32_t offset = (int32_t)((int64_t)value * half / 32768);
    int32_t pos = inverted ? half - offset : half + offset;
    if (pos < 0) {
        pos = 0;
    }
    if (pos > size - 1) {
        pos = size - 1;
    }
    return (uint16_t)pos;
}

static void new_touch(TouchTracker *touch) {
    touch->touch_id = (uint8_t)((touch->touch_id + 1) & 0x7F); // DS4 ids are 7 bit
}

// Swipe mode: while the touchpad key is held the right stick is a finger on
// the pad. A quick tap clicks on release, holding still clicks and holds, and
// R3 clicks wherever the finger is.
static void update_swipe(const RawPad *pad, const FlydigiConfig *cfg, TouchTracker *touch, uint64_t now,
                         bool key, FlydigiState *out) {
    if (key) {
        if (!touch->holding) {
            touch->holding = true;
            touch->moved = false;
            touch->held_click = false;
            touch->pressed_at = now;
            new_touch(touch);
        }
        if (stick_active(pad->rx, pad->ry, cfg->deadzone_right)) {
            touch->moved = true;
        }
        if (touch->moved) {
            out->touch_active = true;
            out->touch_x = stick_to_touch(pad->rx, TOUCH_WIDTH, false);
            out->touch_y = stick_to_touch(pad->ry, TOUCH_HEIGHT, true);
            out->touch_id = touch->touch_id;
            // The stick is the finger now, not the camera.
            out->rx = out->ry = 0x80;
            out->right_active = false;
            if (pad->inputs & (1u << IN_R3)) {
                out->buttons &= ~cfg->map[IN_R3];
                out->buttons |= SCE_PAD_BUTTON_TOUCH_PAD;
            }
        } else if (now - touch->pressed_at >= HOLD_CLICK_US) {
            touch->held_click = true;
        }
        if (touch->held_click) {
            out->buttons |= SCE_PAD_BUTTON_TOUCH_PAD;
        }
    } else if (touch->holding) {
        if (!touch->moved && !touch->held_click) {
            touch->click_until = now + TAP_CLICK_US;
        }
        touch->holding = false;
    }

    if (touch->click_until != 0) {
        if (now < touch->click_until) {
            out->buttons |= SCE_PAD_BUTTON_TOUCH_PAD;
        } else {
            touch->click_until = 0;
        }
    }
}

void pad_translate(const RawPad *pad, const FlydigiConfig *cfg, TouchTracker *touch, uint64_t now_us,
                   FlydigiState *out) {
    uint32_t digital = 0;
    for (int i = 0; i < IN_COUNT; i++) {
        if (pad->inputs & (1u << i)) {
            digital |= cfg->map[i];
        }
    }
    uint32_t buttons = digital;
    if (pad->lt > cfg->trigger_threshold) {
        buttons |= cfg->map[IN_LT];
    }
    if (pad->rt > cfg->trigger_threshold) {
        buttons |= cfg->map[IN_RT];
    }

    // Analog L2/R2: a trigger mapped there passes its travel, while a digital
    // button mapped there counts as fully pressed.
    uint8_t l2 = (digital & SCE_PAD_BUTTON_L2) ? 255 : 0;
    uint8_t r2 = (digital & SCE_PAD_BUTTON_R2) ? 255 : 0;
    if (cfg->map[IN_LT] & SCE_PAD_BUTTON_L2) {
        l2 = max_u8(l2, pad->lt);
    }
    if (cfg->map[IN_RT] & SCE_PAD_BUTTON_L2) {
        l2 = max_u8(l2, pad->rt);
    }
    if (cfg->map[IN_LT] & SCE_PAD_BUTTON_R2) {
        r2 = max_u8(r2, pad->lt);
    }
    if (cfg->map[IN_RT] & SCE_PAD_BUTTON_R2) {
        r2 = max_u8(r2, pad->rt);
    }

    out->buttons = buttons;
    out->l2 = l2;
    out->r2 = r2;
    out->lx = axis_to_ds4(pad->lx);
    out->ly = axis_to_ds4_inverted(pad->ly);
    out->rx = axis_to_ds4(pad->rx);
    out->ry = axis_to_ds4_inverted(pad->ry);
    out->left_active = stick_active(pad->lx, pad->ly, cfg->deadzone_left);
    out->right_active = stick_active(pad->rx, pad->ry, cfg->deadzone_right);
    out->touch_active = false;
    out->touch_x = 0;
    out->touch_y = 0;
    out->touch_id = 0;
    out->gyro_x = 0.0f; // filled by gyro_apply
    out->gyro_y = 0.0f;

    bool key = (buttons & SCE_PAD_BUTTON_TOUCH_PAD) != 0;
    if (cfg->touchpad_mode == TOUCHPAD_SWIPE) {
        out->buttons &= ~SCE_PAD_BUTTON_TOUCH_PAD; // decided by update_swipe
        update_swipe(pad, cfg, touch, now_us, key, out);
    } else {
        if (key && !touch->holding) {
            new_touch(touch);
        }
        touch->holding = key;
    }

    // A real DS4 click always has a finger on the pad; put it in the centre.
    if ((out->buttons & SCE_PAD_BUTTON_TOUCH_PAD) && !out->touch_active) {
        out->touch_active = true;
        out->touch_x = TOUCH_WIDTH / 2;
        out->touch_y = TOUCH_HEIGHT / 2;
        out->touch_id = touch->touch_id;
    }
}
