// SPDX-License-Identifier: GPL-3.0-only
// PC-side unit tests for xinput.c, translate.c and config.c.
// Run with tests\run_tests.ps1.

#include <stdio.h>
#include <string.h>

#include "flydigi.h"
#include "pad.h"

static int g_failures;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            g_failures++;                                            \
        }                                                            \
    } while (0)

#define MS 1000u

// Xbox 360 button word bits.
#define XB_UP 0x0001
#define XB_BACK 0x0020
#define XB_R3 0x0080
#define XB_LB 0x0100
#define XB_GUIDE 0x0400
#define XB_A 0x1000

static FlydigiConfig defaults(void) {
    FlydigiConfig cfg;
    config_set_defaults(&cfg);
    return cfg;
}

static FlydigiConfig parse(const char *text) {
    FlydigiConfig cfg = defaults();
    ConfigErrors errors;
    config_parse(text, NULL, &cfg, &errors);
    CHECK(errors.count == 0);
    return cfg;
}

static void make_report(uint8_t *report, uint16_t buttons, uint8_t lt, uint8_t rt, int16_t lx, int16_t ly,
                        int16_t rx, int16_t ry) {
    memset(report, 0, 32);
    report[1] = 0x14;
    report[2] = (uint8_t)buttons;
    report[3] = (uint8_t)(buttons >> 8);
    report[4] = lt;
    report[5] = rt;
    report[6] = (uint8_t)lx;
    report[7] = (uint8_t)((uint16_t)lx >> 8);
    report[8] = (uint8_t)ly;
    report[9] = (uint8_t)((uint16_t)ly >> 8);
    report[10] = (uint8_t)rx;
    report[11] = (uint8_t)((uint16_t)rx >> 8);
    report[12] = (uint8_t)ry;
    report[13] = (uint8_t)((uint16_t)ry >> 8);
}

// Parses and translates one report with its own touch tracker and time.
typedef struct {
    FlydigiConfig cfg;
    TouchTracker touch;
} Session;

static Session session(FlydigiConfig cfg) {
    Session s;
    s.cfg = cfg;
    memset(&s.touch, 0, sizeof(s.touch));
    return s;
}

static FlydigiState feed(Session *s, uint64_t now, uint16_t buttons, uint8_t lt, uint8_t rt, int16_t lx, int16_t ly,
                         int16_t rx, int16_t ry) {
    uint8_t report[32];
    make_report(report, buttons, lt, rt, lx, ly, rx, ry);
    RawPad pad;
    FlydigiState state;
    memset(&state, 0, sizeof(state));
    CHECK(xinput_parse_report(report, 20, &pad));
    pad_translate(&pad, &s->cfg, &s->touch, now, &state);
    return state;
}

static FlydigiState feed_buttons(Session *s, uint64_t now, uint16_t buttons) {
    return feed(s, now, buttons, 0, 0, 0, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Default mapping
// ---------------------------------------------------------------------------

static void test_real_reports(void) {
    // Captured from the Direwolf 2 in flydigi_diag.txt.
    const uint8_t idle[32] = {0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 0xFF, 0x09, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x7A,
                              0x82, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t settling[32] = {0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x23, 0x00, 0x09, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81,
                                  0x7D, 0x7F, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    FlydigiConfig cfg = defaults();
    TouchTracker touch;
    memset(&touch, 0, sizeof(touch));
    RawPad pad;
    FlydigiState s;

    CHECK(xinput_parse_report(idle, 32, &pad));
    pad_translate(&pad, &cfg, &touch, 0, &s);
    CHECK(s.buttons == 0 && s.l2 == 0 && s.r2 == 0);
    CHECK(s.lx == 0x80 && s.ly == 0x80 && s.rx == 0x80 && s.ry == 0x80);
    CHECK(!s.left_active && !s.right_active && !s.touch_active);

    CHECK(xinput_parse_report(settling, 32, &pad));
    pad_translate(&pad, &cfg, &touch, 0, &s);
    CHECK(!s.left_active);
}

static void test_non_input_reports_are_ignored(void) {
    const uint8_t led_status[3] = {0x01, 0x03, 0x06};
    RawPad pad;
    CHECK(!xinput_parse_report(led_status, 3, &pad));
    uint8_t short_report[32];
    make_report(short_report, 0, 0, 0, 0, 0, 0, 0);
    CHECK(!xinput_parse_report(short_report, 13, &pad));
}

static void test_each_default_button(void) {
    const struct {
        uint16_t xinput;
        uint32_t ds4;
        const char *name;
    } cases[] = {
        {0x0001, SCE_PAD_BUTTON_UP, "dpad up"},     {0x0002, SCE_PAD_BUTTON_DOWN, "dpad down"},
        {0x0004, SCE_PAD_BUTTON_LEFT, "dpad left"}, {0x0008, SCE_PAD_BUTTON_RIGHT, "dpad right"},
        {0x0010, SCE_PAD_BUTTON_OPTIONS, "start"},  {0x0020, SCE_PAD_BUTTON_TOUCH_PAD, "back"},
        {0x0040, SCE_PAD_BUTTON_L3, "L3"},          {0x0080, SCE_PAD_BUTTON_R3, "R3"},
        {0x0100, SCE_PAD_BUTTON_L1, "LB"},          {0x0200, SCE_PAD_BUTTON_R1, "RB"},
        {0x1000, SCE_PAD_BUTTON_CROSS, "A"},        {0x2000, SCE_PAD_BUTTON_CIRCLE, "B"},
        {0x4000, SCE_PAD_BUTTON_SQUARE, "X"},       {0x8000, SCE_PAD_BUTTON_TRIANGLE, "Y"},
        {0x0400, 0, "guide"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        Session s = session(defaults());
        FlydigiState st = feed_buttons(&s, 0, cases[i].xinput);
        if (st.buttons != cases[i].ds4) {
            printf("FALHOU botao %s: esperado %08X, veio %08X\n", cases[i].name, cases[i].ds4, st.buttons);
            g_failures++;
        }
    }
}

static void test_triggers(void) {
    Session s = session(defaults());
    FlydigiState st = feed(&s, 0, 0, 255, 10, 0, 0, 0, 0);
    CHECK(st.l2 == 255 && (st.buttons & SCE_PAD_BUTTON_L2));
    CHECK(st.r2 == 10 && !(st.buttons & SCE_PAD_BUTTON_R2));
}

static void test_sticks(void) {
    Session s = session(defaults());
    FlydigiState st = feed(&s, 0, 0, 0, 0, 32767, 32767, 0, 0); // left stick up-right
    CHECK(st.lx == 255 && st.ly == 0 && st.left_active && !st.right_active);

    st = feed(&s, 0, 0, 0, 0, -32768, -32768, 0, 0); // left stick down-left
    CHECK(st.lx == 0 && st.ly == 255 && st.left_active);

    st = feed(&s, 0, 0, 0, 0, 0, 0, 0, 32767); // right stick up
    CHECK(st.rx == 0x80 && st.ry == 0 && st.right_active && !st.left_active);

    st = feed(&s, 0, 0, 0, 0, 2000, -2000, 0, 0); // inside the default 9% deadzone
    CHECK(!st.left_active);
}

static void test_remapped_triggers_and_buttons(void) {
    Session s = session(parse("LT = CROSS\n"   // trigger used as a plain button
                              "RT = L2\n"      // right trigger drives the analog L2
                              "LB = R2\n"      // digital button drives R2 fully
                              "A = L1+R1\n"));
    FlydigiState st = feed(&s, 0, XB_LB | XB_A, 200, 90, 0, 0, 0, 0);
    CHECK(st.buttons == (SCE_PAD_BUTTON_CROSS | SCE_PAD_BUTTON_L2 | SCE_PAD_BUTTON_R2 | SCE_PAD_BUTTON_L1 |
                         SCE_PAD_BUTTON_R1));
    CHECK(st.l2 == 90);
    CHECK(st.r2 == 255);

    st = feed(&s, 0, 0, 10, 0, 0, 0, 0, 0); // LT below the threshold
    CHECK(st.buttons == 0 && st.l2 == 0 && st.r2 == 0);
}

// ---------------------------------------------------------------------------
// Touchpad
// ---------------------------------------------------------------------------

static void test_click_mode_puts_a_finger_in_the_centre(void) {
    Session s = session(defaults());
    FlydigiState st = feed_buttons(&s, 0, XB_BACK);
    CHECK(st.buttons == SCE_PAD_BUTTON_TOUCH_PAD);
    CHECK(st.touch_active && st.touch_x == 960 && st.touch_y == 471);
    uint8_t first_id = st.touch_id;

    st = feed_buttons(&s, 10 * MS, XB_BACK); // still the same press
    CHECK(st.touch_id == first_id);

    st = feed_buttons(&s, 20 * MS, 0);
    CHECK(st.buttons == 0 && !st.touch_active);

    st = feed_buttons(&s, 30 * MS, XB_BACK); // new press, new finger
    CHECK(st.touch_active && st.touch_id != first_id);
}

static FlydigiConfig swipe_config(void) {
    FlydigiConfig cfg = defaults();
    cfg.touchpad_mode = TOUCHPAD_SWIPE;
    return cfg;
}

static void test_swipe_mode_tap_clicks_on_release(void) {
    Session s = session(swipe_config());
    FlydigiState st = feed_buttons(&s, 0, XB_BACK);
    CHECK(!(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD)); // not yet: could become a swipe
    st = feed_buttons(&s, 80 * MS, XB_BACK);
    CHECK(!(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD));
    st = feed_buttons(&s, 120 * MS, 0); // released quickly
    CHECK(st.buttons == SCE_PAD_BUTTON_TOUCH_PAD && st.touch_active);
    st = feed_buttons(&s, 200 * MS, 0);
    CHECK(st.buttons == SCE_PAD_BUTTON_TOUCH_PAD); // pulse still on
    st = feed_buttons(&s, 230 * MS, 0);
    CHECK(st.buttons == 0 && !st.touch_active); // pulse over
    CHECK(s.touch.click_until == 0);
}

static void test_swipe_mode_hold_still_clicks_and_holds(void) {
    Session s = session(swipe_config());
    feed_buttons(&s, 0, XB_BACK);
    FlydigiState st = feed_buttons(&s, 499 * MS, XB_BACK);
    CHECK(!(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD));
    st = feed_buttons(&s, 500 * MS, XB_BACK);
    CHECK(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD);
    st = feed_buttons(&s, 2000 * MS, XB_BACK);
    CHECK(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD);
    st = feed_buttons(&s, 2010 * MS, 0); // release: no extra tap pulse
    CHECK(st.buttons == 0);
}

static void test_swipe_mode_stick_moves_the_finger(void) {
    Session s = session(swipe_config());
    feed_buttons(&s, 0, XB_BACK);

    FlydigiState st = feed(&s, 20 * MS, XB_BACK, 0, 0, 0, 0, 32767, 0); // right stick fully right
    CHECK(st.touch_active && st.touch_x == 1919 && st.touch_y == 471);
    CHECK(!(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD)); // touching, not clicking
    CHECK(!st.right_active && st.rx == 0x80 && st.ry == 0x80); // camera does not move
    uint8_t id = st.touch_id;

    st = feed(&s, 40 * MS, XB_BACK, 0, 0, 0, 0, 0, 32767); // stick up: finger at the top
    CHECK(st.touch_x == 960 && st.touch_y == 1 && st.touch_id == id);

    st = feed(&s, 60 * MS, XB_BACK, 0, 0, 0, 0, -32768, -32768); // bottom-left corner
    CHECK(st.touch_x == 0 && st.touch_y == 941);

    st = feed(&s, 80 * MS, XB_BACK | XB_R3, 0, 0, 0, 0, -32768, -32768); // R3 clicks there
    CHECK(st.buttons == SCE_PAD_BUTTON_TOUCH_PAD && st.touch_x == 0);

    st = feed(&s, 900 * MS, XB_BACK, 0, 0, 0, 0, 0, 0); // stick back to centre, still touching
    CHECK(st.touch_active && !(st.buttons & SCE_PAD_BUTTON_TOUCH_PAD));

    st = feed(&s, 920 * MS, 0, 0, 0, 0, 0, 0, 0); // release after a swipe: no click
    CHECK(st.buttons == 0 && !st.touch_active);

    st = feed(&s, 940 * MS, 0, 0, 0, 0, 0, 32767, 0); // stick is the camera again
    CHECK(st.right_active && st.rx == 255 && !st.touch_active);
}

static void test_swipe_mode_follows_remapped_touch_key(void) {
    FlydigiConfig cfg = parse("BACK = NONE\nGUIDE = TOUCHPAD\ntouchpad_mode = deslizar\n");
    CHECK(cfg.touchpad_mode == TOUCHPAD_SWIPE);
    Session s = session(cfg);
    FlydigiState st = feed(&s, 0, XB_BACK, 0, 0, 0, 0, 32767, 0);
    CHECK(!st.touch_active && st.right_active); // BACK no longer touches
    feed_buttons(&s, 10 * MS, 0);
    st = feed(&s, 20 * MS, XB_GUIDE, 0, 0, 0, 0, 32767, 0);
    CHECK(st.touch_active && st.touch_x == 1919);
}

static void test_other_buttons_work_while_touching(void) {
    Session s = session(swipe_config());
    FlydigiState st = feed(&s, 0, XB_BACK | XB_A | XB_UP, 0, 0, 0, 0, 32767, 0);
    CHECK(st.buttons == (SCE_PAD_BUTTON_CROSS | SCE_PAD_BUTTON_UP));
    CHECK(st.touch_active);
}

// ---------------------------------------------------------------------------
// Gyro (gyro.c + motion decoding in xinput.c)
// ---------------------------------------------------------------------------

static RawPad motion_report(uint8_t b14, uint8_t b15, uint8_t b16) {
    uint8_t report[32];
    make_report(report, 0, 0, 0, 0, 0, 0, 0);
    report[14] = b14;
    report[15] = b15;
    report[16] = b16;
    RawPad pad;
    CHECK(xinput_parse_report(report, 32, &pad));
    return pad;
}

static void test_motion_decoding_from_real_bytes(void) {
    RawPad pad = motion_report(0x03, 0xEB, 0x0F); // tilting: from flydigi_ds4.txt
    CHECK(pad.motion_h == 3 && pad.motion_v == -21);
    pad = motion_report(0xF2, 0xFC, 0xFF);
    CHECK(pad.motion_h == -14 && pad.motion_v == -4);
    pad = motion_report(0xFF, 0x09, 0xF0);
    CHECK(pad.motion_h == -1 && pad.motion_v == 9);
    pad = motion_report(0x00, 0x00, 0x00);
    CHECK(pad.motion_h == 0 && pad.motion_v == 0);

    // A standard 20-byte report without the motion bytes.
    uint8_t report[32];
    make_report(report, 0, 0, 0, 0, 0, 0, 0);
    report[14] = 0x16;
    CHECK(xinput_parse_report(report, 14, &pad));
    CHECK(pad.motion_h == 0 && pad.motion_v == 0);
}

static FlydigiConfig gyro_config(void) {
    FlydigiConfig cfg = defaults();
    cfg.gyro = true;
    return cfg;
}

// Feeds captured motion bytes (one row every 12 ms, as logged) and returns
// how much each DS4 axis moved in total.
static void run_motion(const uint8_t rows[][3], int count, const FlydigiConfig *cfg, float *sum_x, float *sum_y) {
    GyroTracker gyro;
    memset(&gyro, 0, sizeof(gyro));
    *sum_x = 0.0f;
    *sum_y = 0.0f;
    for (int i = 0; i < count; i++) {
        RawPad pad = motion_report(rows[i][0], rows[i][1], rows[i][2]);
        gyro_feed(&gyro, pad.motion_h, pad.motion_v, (uint64_t)i * 12 * MS);
        FlydigiState st;
        memset(&st, 0, sizeof(st));
        gyro_apply(&gyro, cfg, &st);
        *sum_x += st.gyro_x < 0 ? -st.gyro_x : st.gyro_x;
        *sum_y += st.gyro_y < 0 ? -st.gyro_y : st.gyro_y;
    }
}

// 25 rows from phase 2 of the motion test: tilting forward and back.
static const uint8_t k_real_pitch[][3] = {
    {0x00, 0xEB, 0x0F}, {0x00, 0xEA, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEA, 0x0F},
    {0x00, 0xEB, 0x0F}, {0x00, 0xEA, 0x0F}, {0x00, 0xEA, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEA, 0x0F},
    {0x02, 0xEB, 0x0F}, {0x02, 0xEA, 0x0F}, {0x02, 0xEB, 0x0F}, {0x01, 0xEA, 0x0F}, {0x01, 0xEA, 0x0F},
    {0x01, 0xEB, 0x0F}, {0x02, 0xEB, 0x0F}, {0x02, 0xEB, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEB, 0x0F},
    {0x00, 0xEA, 0x0F}, {0x00, 0xEA, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEB, 0x0F}, {0x00, 0xEA, 0x0F},
};

// 25 rows from phase 4: lying on the table, turning left and right.
static const uint8_t k_real_yaw[][3] = {
    {0xEB, 0x01, 0xF0}, {0xEB, 0x01, 0xF0}, {0xEB, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEB, 0x00, 0xF0},
    {0xEB, 0x00, 0xF0}, {0xEB, 0x00, 0xF0}, {0xEB, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEB, 0x00, 0xF0},
    {0xEB, 0xFF, 0xFF}, {0xEB, 0xFF, 0xFF}, {0xEB, 0x00, 0xF0}, {0xEA, 0x01, 0xF0}, {0xEB, 0x00, 0xF0},
    {0xEA, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEB, 0x00, 0xF0}, {0xEB, 0x00, 0xF0},
    {0xEB, 0x01, 0xF0}, {0xEB, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEA, 0x00, 0xF0}, {0xEA, 0x00, 0xF0},
};

static void test_gyro_axes_with_real_motion(void) {
    FlydigiConfig cfg = gyro_config();
    float x, y;

    run_motion(k_real_pitch, 25, &cfg, &x, &y);
    CHECK(x > 0.0f && x > 10.0f * y); // tilting drives pitch only

    run_motion(k_real_yaw, 25, &cfg, &x, &y);
    CHECK(y > 0.0f && y > 10.0f * x); // turning drives yaw only
}

static void test_gyro_scale_sign_and_options(void) {
    // Saturated turn to the right: +22 every 4 ms, packets every 1 ms.
    FlydigiConfig cfg = gyro_config();
    GyroTracker gyro;
    memset(&gyro, 0, sizeof(gyro));
    for (uint64_t t = 0; t <= 100; t++) {
        gyro_feed(&gyro, t % 4 == 0 ? 22 : 0, 0, t * MS);
    }
    FlydigiState st;
    memset(&st, 0, sizeof(st));
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_y < -3.7f && st.gyro_y > -4.3f); // about 4 rad/s, to the right = negative yaw
    CHECK(st.gyro_x == 0.0f);

    cfg.gyro_sensitivity = 50;
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_y < -1.85f && st.gyro_y > -2.15f);

    cfg.gyro_invert_horizontal = true;
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_y > 1.85f);

    cfg.gyro = false;
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_x == 0.0f && st.gyro_y == 0.0f);

    // Vertical: positive value = nose up = positive pitch; inverted flips it.
    cfg = gyro_config();
    memset(&gyro, 0, sizeof(gyro));
    for (uint64_t t = 0; t <= 100; t++) {
        gyro_feed(&gyro, 0, t % 4 == 0 ? 10 : 0, t * MS);
    }
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_x > 1.5f && st.gyro_y == 0.0f);
    cfg.gyro_invert_vertical = true;
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_x < -1.5f);
}

static void test_gyro_stops_when_motion_stops(void) {
    FlydigiConfig cfg = gyro_config();
    GyroTracker gyro;
    memset(&gyro, 0, sizeof(gyro));
    for (uint64_t t = 0; t <= 40; t++) {
        gyro_feed(&gyro, t % 4 == 0 ? -22 : 0, t % 4 == 0 ? 15 : 0, t * MS);
    }
    CHECK(gyro_moving(&gyro));
    // The controller falls silent: the read loop feeds zeros every 50 ms.
    gyro_feed(&gyro, 0, 0, 90 * MS);
    gyro_feed(&gyro, 0, 0, 140 * MS);
    CHECK(!gyro_moving(&gyro));
    FlydigiState st;
    memset(&st, 0, sizeof(st));
    gyro_apply(&gyro, &cfg, &st);
    CHECK(st.gyro_x == 0.0f && st.gyro_y == 0.0f);
}

// ---------------------------------------------------------------------------
// config.c
// ---------------------------------------------------------------------------

static void test_config_defaults_match_v1_behaviour(void) {
    FlydigiConfig cfg = defaults();
    CHECK(cfg.trigger_threshold == 20);
    CHECK(cfg.deadzone_left == 2949 && cfg.deadzone_right == 2949);
    CHECK(cfg.vibration == 100 && !cfg.log && cfg.notifications);
    CHECK(cfg.touchpad_mode == TOUCHPAD_CLICK);
    CHECK(!cfg.gyro && cfg.gyro_sensitivity == 100 && !cfg.gyro_invert_horizontal && !cfg.gyro_invert_vertical);
}

static void test_config_remap_and_options(void) {
    const char *text =
        "\xEF\xBB\xBF; comentario\r\n"
        "[default]\r\n"
        "A = bola\r\n"
        "B = CROSS   ; troca A e B\r\n"
        "LB = L1+R1\r\n"
        "back = none\r\n"
        "deadzone_left = 15%\r\n"
        "trigger_threshold = 50\r\n"
        "vibration = 40\r\n"
        "log = sim\r\n"
        "notifications = nao\r\n"
        "touchpad_mode = swipe\r\n"
        "gyro = sim\r\n"
        "gyro_sensitivity = 250%\r\n"
        "gyro_invert_vertical = sim\r\n";
    FlydigiConfig cfg = defaults();
    ConfigErrors errors;
    config_parse(text, "CUSA00001", &cfg, &errors);
    CHECK(errors.count == 0);
    CHECK(cfg.map[IN_A] == SCE_PAD_BUTTON_CIRCLE);
    CHECK(cfg.map[IN_B] == SCE_PAD_BUTTON_CROSS);
    CHECK(cfg.map[IN_LB] == (SCE_PAD_BUTTON_L1 | SCE_PAD_BUTTON_R1));
    CHECK(cfg.map[IN_BACK] == 0);
    CHECK(cfg.map[IN_Y] == SCE_PAD_BUTTON_TRIANGLE);
    CHECK(cfg.deadzone_left == 15 * 32767 / 100);
    CHECK(cfg.deadzone_right == 2949);
    CHECK(cfg.trigger_threshold == 127);
    CHECK(cfg.vibration == 40);
    CHECK(cfg.log && !cfg.notifications);
    CHECK(cfg.touchpad_mode == TOUCHPAD_SWIPE);
    CHECK(cfg.gyro && cfg.gyro_sensitivity == 250 && cfg.gyro_invert_vertical && !cfg.gyro_invert_horizontal);
}

static void test_config_game_section_overrides_default(void) {
    const char *text =
        "[CUSA17776]\n"
        "vibration = 0\n"
        "Y = R3\n"
        "[CUSA99999]\n"
        "vibration = 77\n"
        "[default]\n"
        "vibration = 50\n"
        "Y = SQUARE\n";
    FlydigiConfig cfg = defaults();
    ConfigErrors errors;
    config_parse(text, "cusa17776", &cfg, &errors);
    CHECK(errors.count == 0);
    CHECK(cfg.vibration == 0);
    CHECK(cfg.map[IN_Y] == SCE_PAD_BUTTON_R3);

    cfg = defaults();
    config_parse(text, "CUSA12345", &cfg, &errors);
    CHECK(cfg.vibration == 50);
    CHECK(cfg.map[IN_Y] == SCE_PAD_BUTTON_SQUARE);
}

static void test_config_reports_errors(void) {
    const char *text =
        "[default]\n"
        "A = CROSS\n"
        "B = BOTAO_QUE_NAO_EXISTE\n"
        "vibration = 150\n"
        "sem sinal de igual\n"
        "opcao_inventada = 1\n"
        "touchpad_mode = talvez\n"
        "gyro_sensitivity = 0\n"
        "X = SQUARE\n";
    FlydigiConfig cfg = defaults();
    ConfigErrors errors;
    config_parse(text, NULL, &cfg, &errors);
    CHECK(errors.count == 6);
    CHECK(errors.first_line == 3);
    CHECK(strcmp(errors.first_message, "botao do PS4 desconhecido") == 0);
    CHECK(cfg.map[IN_B] == SCE_PAD_BUTTON_CIRCLE);
    CHECK(cfg.vibration == 100);
    CHECK(cfg.touchpad_mode == TOUCHPAD_CLICK);
}

int main(void) {
    test_real_reports();
    test_non_input_reports_are_ignored();
    test_each_default_button();
    test_triggers();
    test_sticks();
    test_remapped_triggers_and_buttons();
    test_click_mode_puts_a_finger_in_the_centre();
    test_swipe_mode_tap_clicks_on_release();
    test_swipe_mode_hold_still_clicks_and_holds();
    test_swipe_mode_stick_moves_the_finger();
    test_swipe_mode_follows_remapped_touch_key();
    test_other_buttons_work_while_touching();
    test_motion_decoding_from_real_bytes();
    test_gyro_axes_with_real_motion();
    test_gyro_scale_sign_and_options();
    test_gyro_stops_when_motion_stops();
    test_config_defaults_match_v1_behaviour();
    test_config_remap_and_options();
    test_config_game_section_overrides_default();
    test_config_reports_errors();
    if (g_failures == 0) {
        printf("OK: todos os testes passaram\n");
        return 0;
    }
    printf("%d falha(s)\n", g_failures);
    return 1;
}
