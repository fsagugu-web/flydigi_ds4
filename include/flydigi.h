// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Physical inputs of the controller (Xbox names).
enum {
    IN_A,
    IN_B,
    IN_X,
    IN_Y,
    IN_LB,
    IN_RB,
    IN_LT,
    IN_RT,
    IN_START,
    IN_BACK,
    IN_GUIDE,
    IN_L3,
    IN_R3,
    IN_UP,
    IN_DOWN,
    IN_LEFT,
    IN_RIGHT,
    IN_COUNT
};

enum {
    TOUCHPAD_CLICK, // the touchpad key only clicks
    TOUCHPAD_SWIPE, // holding it turns the right stick into a finger
};

typedef struct {
    uint32_t map[IN_COUNT];    // DS4 button bits produced by each input
    int32_t deadzone_left;     // int16 stick units
    int32_t deadzone_right;
    uint8_t trigger_threshold; // 0-255, when LT/RT count as pressed
    uint8_t vibration;         // percent
    uint8_t touchpad_mode;     // TOUCHPAD_*
    bool log;
    bool notifications;
    bool gyro;                 // feed the controller's motion to the DS4 gyroscope
    uint16_t gyro_sensitivity; // percent
    bool gyro_invert_horizontal;
    bool gyro_invert_vertical;
} FlydigiConfig;

typedef struct {
    int count;
    int first_line;
    char first_message[64];
} ConfigErrors;

// Controller-independent snapshot in Xbox terms (Y axes grow upwards).
typedef struct {
    uint32_t inputs; // 1u << IN_* for each pressed digital input (LT/RT excluded)
    uint8_t lt, rt;
    int16_t lx, ly, rx, ry;
    int16_t motion_h; // Direwolf 2 motion sample: horizontal (yaw and roll)
    int16_t motion_v; // and vertical (pitch); 0 on controllers without it
} RawPad;

// State already translated to DS4 terms.
typedef struct {
    uint32_t buttons; // SCE_PAD_BUTTON_* bits
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    bool left_active;  // stick is deflected, so it overrides the DS4 stick
    bool right_active;
    bool touch_active; // a finger is on the virtual touchpad
    uint16_t touch_x;  // 0-1919
    uint16_t touch_y;  // 0-941
    uint8_t touch_id;
    float gyro_x; // rad/s, DS4 axes: x = pitch (nose up positive)
    float gyro_y; //                  y = yaw (turning left positive)
} FlydigiState;

// Turns the motion samples into an angular velocity.
typedef struct {
    bool started;
    uint64_t window_start;
    int32_t sum_h;
    int32_t sum_v;
    float rate_h; // rad/s, before sensitivity and axis mapping
    float rate_v;
} GyroTracker;

// What the touchpad key did across reports (tap, hold or swipe).
typedef struct {
    bool holding;
    bool moved;
    bool held_click;
    uint64_t pressed_at;
    uint64_t click_until;
    uint8_t touch_id;
} TouchTracker;

// config.c: filled by plugin_load before the USB thread starts, read-only after.
extern FlydigiConfig g_config;
void config_set_defaults(FlydigiConfig *cfg);
void config_parse(const char *text, const char *titleid, FlydigiConfig *cfg, ConfigErrors *errors);

// xinput.c
bool xinput_parse_report(const uint8_t *report, int len, RawPad *pad);

// translate.c
extern const FlydigiState k_neutral_state;
void pad_translate(const RawPad *pad, const FlydigiConfig *cfg, TouchTracker *touch, uint64_t now_us,
                   FlydigiState *out);

// gyro.c
void gyro_feed(GyroTracker *gyro, int16_t horizontal, int16_t vertical, uint64_t now_us);
bool gyro_moving(const GyroTracker *gyro);
void gyro_apply(const GyroTracker *gyro, const FlydigiConfig *cfg, FlydigiState *out);

// usb.c: background thread that owns the controller.
void flydigi_start(const char *startup_notice);
void flydigi_stop(void);
bool flydigi_get_state(FlydigiState *out); // false while no controller is connected
void flydigi_set_rumble(uint8_t large, uint8_t small);

// log.c
void log_open(const char *path);
void log_close(void);
void log_line(const char *fmt, ...);
void log_hex(const char *label, const void *data, int len);
void notify(const char *fmt, ...);
