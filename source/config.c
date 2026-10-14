// SPDX-License-Identifier: GPL-3.0-only
// flydigi_ds4.ini parser. No PS4 dependencies, so it is unit tested on a PC
// (see tests/tests.c).
//
// [default] (and lines before any section) applies to every game; a section
// named after a title ID, e.g. [CUSA17776], overrides it for that game only.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "flydigi.h"
#include "pad.h"

FlydigiConfig g_config;

static const struct {
    const char *name;
    uint32_t bits;
} k_ds4_names[] = {
    {"CROSS", SCE_PAD_BUTTON_CROSS},       {"XIS", SCE_PAD_BUTTON_CROSS},
    {"CIRCLE", SCE_PAD_BUTTON_CIRCLE},     {"BOLA", SCE_PAD_BUTTON_CIRCLE},
    {"CIRCULO", SCE_PAD_BUTTON_CIRCLE},    {"SQUARE", SCE_PAD_BUTTON_SQUARE},
    {"QUADRADO", SCE_PAD_BUTTON_SQUARE},   {"TRIANGLE", SCE_PAD_BUTTON_TRIANGLE},
    {"TRIANGULO", SCE_PAD_BUTTON_TRIANGLE}, {"L1", SCE_PAD_BUTTON_L1},
    {"R1", SCE_PAD_BUTTON_R1},             {"L2", SCE_PAD_BUTTON_L2},
    {"R2", SCE_PAD_BUTTON_R2},             {"L3", SCE_PAD_BUTTON_L3},
    {"R3", SCE_PAD_BUTTON_R3},             {"OPTIONS", SCE_PAD_BUTTON_OPTIONS},
    {"TOUCHPAD", SCE_PAD_BUTTON_TOUCH_PAD}, {"TOUCH_PAD", SCE_PAD_BUTTON_TOUCH_PAD},
    {"UP", SCE_PAD_BUTTON_UP},             {"CIMA", SCE_PAD_BUTTON_UP},
    {"DOWN", SCE_PAD_BUTTON_DOWN},         {"BAIXO", SCE_PAD_BUTTON_DOWN},
    {"LEFT", SCE_PAD_BUTTON_LEFT},         {"ESQUERDA", SCE_PAD_BUTTON_LEFT},
    {"RIGHT", SCE_PAD_BUTTON_RIGHT},       {"DIREITA", SCE_PAD_BUTTON_RIGHT},
    {"NONE", 0},                           {"NENHUM", 0},
};

static const struct {
    const char *name;
    int input;
} k_input_names[] = {
    {"A", IN_A},         {"B", IN_B},        {"X", IN_X},         {"Y", IN_Y},          {"LB", IN_LB},
    {"RB", IN_RB},       {"LT", IN_LT},      {"RT", IN_RT},       {"START", IN_START},  {"MENU", IN_START},
    {"BACK", IN_BACK},   {"VIEW", IN_BACK},  {"GUIDE", IN_GUIDE}, {"HOME", IN_GUIDE},   {"L3", IN_L3},
    {"LS", IN_L3},       {"R3", IN_R3},      {"RS", IN_R3},       {"UP", IN_UP},        {"DOWN", IN_DOWN},
    {"LEFT", IN_LEFT},   {"RIGHT", IN_RIGHT},
};

void config_set_defaults(FlydigiConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->map[IN_A] = SCE_PAD_BUTTON_CROSS;
    cfg->map[IN_B] = SCE_PAD_BUTTON_CIRCLE;
    cfg->map[IN_X] = SCE_PAD_BUTTON_SQUARE;
    cfg->map[IN_Y] = SCE_PAD_BUTTON_TRIANGLE;
    cfg->map[IN_LB] = SCE_PAD_BUTTON_L1;
    cfg->map[IN_RB] = SCE_PAD_BUTTON_R1;
    cfg->map[IN_LT] = SCE_PAD_BUTTON_L2;
    cfg->map[IN_RT] = SCE_PAD_BUTTON_R2;
    cfg->map[IN_START] = SCE_PAD_BUTTON_OPTIONS;
    cfg->map[IN_BACK] = SCE_PAD_BUTTON_TOUCH_PAD;
    cfg->map[IN_GUIDE] = 0; // the PS button belongs to the system
    cfg->map[IN_L3] = SCE_PAD_BUTTON_L3;
    cfg->map[IN_R3] = SCE_PAD_BUTTON_R3;
    cfg->map[IN_UP] = SCE_PAD_BUTTON_UP;
    cfg->map[IN_DOWN] = SCE_PAD_BUTTON_DOWN;
    cfg->map[IN_LEFT] = SCE_PAD_BUTTON_LEFT;
    cfg->map[IN_RIGHT] = SCE_PAD_BUTTON_RIGHT;
    cfg->deadzone_left = 9 * 32767 / 100;
    cfg->deadzone_right = 9 * 32767 / 100;
    cfg->trigger_threshold = 8 * 255 / 100;
    cfg->vibration = 100;
    cfg->touchpad_mode = TOUCHPAD_CLICK;
    cfg->log = false;
    cfg->notifications = true;
    cfg->gyro = false;
    cfg->gyro_sensitivity = 100;
    cfg->gyro_invert_horizontal = false;
    cfg->gyro_invert_vertical = false;
}

static char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool equals_ignore_case(const char *a, const char *b) {
    while (*a && *b) {
        if (to_upper(*a) != to_upper(*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    return s;
}

// Accepts "15" or "15%".
static bool parse_percent(const char *s, int min, int max, int *out) {
    int value = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9' && digits < 4) {
        value = value * 10 + (*s - '0');
        s++;
        digits++;
    }
    if (*s == '%') {
        s++;
    }
    if (digits == 0 || *s != '\0' || value < min || value > max) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_bool(const char *s, bool *out) {
    static const char *const yes[] = {"true", "1", "sim", "yes", "on"};
    static const char *const no[] = {"false", "0", "nao", "n\xC3\xA3o", "no", "off"};
    for (size_t i = 0; i < sizeof(yes) / sizeof(yes[0]); i++) {
        if (equals_ignore_case(s, yes[i])) {
            *out = true;
            return true;
        }
    }
    for (size_t i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
        if (equals_ignore_case(s, no[i])) {
            *out = false;
            return true;
        }
    }
    return false;
}

// "CROSS" or a combination such as "L1+R1".
static bool parse_buttons(char *value, uint32_t *out) {
    uint32_t bits = 0;
    char *token = value;
    while (token) {
        char *plus = strchr(token, '+');
        if (plus) {
            *plus = '\0';
        }
        char *name = trim(token);
        bool found = false;
        for (size_t i = 0; i < sizeof(k_ds4_names) / sizeof(k_ds4_names[0]); i++) {
            if (equals_ignore_case(name, k_ds4_names[i].name)) {
                bits |= k_ds4_names[i].bits;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
        token = plus ? plus + 1 : NULL;
    }
    *out = bits;
    return true;
}

// Returns NULL on success or a short message for the on-screen error.
static const char *apply_option(FlydigiConfig *cfg, const char *key, char *value) {
    for (size_t i = 0; i < sizeof(k_input_names) / sizeof(k_input_names[0]); i++) {
        if (equals_ignore_case(key, k_input_names[i].name)) {
            return parse_buttons(value, &cfg->map[k_input_names[i].input]) ? NULL : "unknown PS4 button";
        }
    }
    int percent;
    if (equals_ignore_case(key, "deadzone_left")) {
        if (!parse_percent(value, 0, 50, &percent)) {
            return "use a number from 0 to 50";
        }
        cfg->deadzone_left = percent * 32767 / 100;
        return NULL;
    }
    if (equals_ignore_case(key, "deadzone_right")) {
        if (!parse_percent(value, 0, 50, &percent)) {
            return "use a number from 0 to 50";
        }
        cfg->deadzone_right = percent * 32767 / 100;
        return NULL;
    }
    if (equals_ignore_case(key, "trigger_threshold")) {
        if (!parse_percent(value, 1, 100, &percent)) {
            return "use a number from 1 to 100";
        }
        cfg->trigger_threshold = (uint8_t)(percent * 255 / 100);
        return NULL;
    }
    if (equals_ignore_case(key, "vibration")) {
        if (!parse_percent(value, 0, 100, &percent)) {
            return "use a number from 0 to 100";
        }
        cfg->vibration = (uint8_t)percent;
        return NULL;
    }
    if (equals_ignore_case(key, "log")) {
        return parse_bool(value, &cfg->log) ? NULL : "use yes or no";
    }
    if (equals_ignore_case(key, "notifications")) {
        return parse_bool(value, &cfg->notifications) ? NULL : "use yes or no";
    }
    if (equals_ignore_case(key, "gyro")) {
        return parse_bool(value, &cfg->gyro) ? NULL : "use yes or no";
    }
    if (equals_ignore_case(key, "gyro_sensitivity")) {
        if (!parse_percent(value, 1, 500, &percent)) {
            return "use a number from 1 to 500";
        }
        cfg->gyro_sensitivity = (uint16_t)percent;
        return NULL;
    }
    if (equals_ignore_case(key, "gyro_invert_horizontal")) {
        return parse_bool(value, &cfg->gyro_invert_horizontal) ? NULL : "use yes or no";
    }
    if (equals_ignore_case(key, "gyro_invert_vertical")) {
        return parse_bool(value, &cfg->gyro_invert_vertical) ? NULL : "use yes or no";
    }
    if (equals_ignore_case(key, "touchpad_mode")) {
        if (equals_ignore_case(value, "click") || equals_ignore_case(value, "clique")) {
            cfg->touchpad_mode = TOUCHPAD_CLICK;
        } else if (equals_ignore_case(value, "swipe") || equals_ignore_case(value, "deslizar")) {
            cfg->touchpad_mode = TOUCHPAD_SWIPE;
        } else {
            return "use click or swipe";
        }
        return NULL;
    }
    return "unknown option";
}

static void record_error(ConfigErrors *errors, int line, const char *message) {
    if (errors->count == 0) {
        errors->first_line = line;
        snprintf(errors->first_message, sizeof(errors->first_message), "%s", message);
    }
    errors->count++;
}

void config_parse(const char *text, const char *titleid, FlydigiConfig *cfg, ConfigErrors *errors) {
    memset(errors, 0, sizeof(*errors));
    if ((uint8_t)text[0] == 0xEF && (uint8_t)text[1] == 0xBB && (uint8_t)text[2] == 0xBF) {
        text += 3; // UTF-8 BOM some editors add
    }

    // Pass 0 applies [default], pass 1 the game's own section, so a game
    // section wins no matter where it appears in the file.
    for (int pass = 0; pass < 2; pass++) {
        bool active = pass == 0;
        int line_number = 0;
        const char *p = text;
        while (*p) {
            char line[256];
            size_t n = 0;
            while (*p && *p != '\n') {
                if (n < sizeof(line) - 1) {
                    line[n++] = *p;
                }
                p++;
            }
            if (*p == '\n') {
                p++;
            }
            line[n] = '\0';
            line_number++;

            char *comment = strpbrk(line, ";#");
            if (comment) {
                *comment = '\0';
            }
            char *content = trim(line);
            if (*content == '\0') {
                continue;
            }
            if (*content == '[') {
                char *close = strchr(content, ']');
                if (close) {
                    *close = '\0';
                }
                char *name = trim(content + 1);
                active = pass == 0 ? equals_ignore_case(name, "default")
                                   : (titleid != NULL && titleid[0] != '\0' && equals_ignore_case(name, titleid));
                continue;
            }
            if (!active) {
                continue;
            }
            char *equals = strchr(content, '=');
            if (equals == NULL) {
                record_error(errors, line_number, "missing =");
                continue;
            }
            *equals = '\0';
            const char *message = apply_option(cfg, trim(content), trim(equals + 1));
            if (message) {
                record_error(errors, line_number, message);
            }
        }
    }
}
