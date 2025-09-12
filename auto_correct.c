#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define WORD_BUFFER_SIZE 32
#define MAX_TYPOS 10

// Typo correction table
struct typo_correction {
    const char* typo;
    const char* correction;
};

static const struct typo_correction typos[MAX_TYPOS] = {
    {"teh", "the"},
    {"adn", "and"}, 
    {"yuo", "you"},
    {"wiht", "with"},
    {"recieve", "receive"},
    {"seperate", "separate"},
    {"definately", "definitely"},
    {"occured", "occurred"},
    {"neccessary", "necessary"},
    {"accomodate", "accommodate"}
};

static char word_buffer[WORD_BUFFER_SIZE] = {0};
static int buffer_pos = 0;

// Send backspaces and replacement text
static void send_correction(const char* typo, const char* correction) {
    int typo_len = strlen(typo);
    int correction_len = strlen(correction);
    
    LOG_INF("Correcting '%s' -> '%s'", typo, correction);
    
    // Send backspaces to delete the typo
    for (int i = 0; i < typo_len; i++) {
        zmk_hid_keyboard_press(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
        zmk_endpoints_send_report(HID_USAGE_CONSUMER);
        k_msleep(1);
        zmk_hid_keyboard_release(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE);
        zmk_endpoints_send_report(HID_USAGE_CONSUMER);
        k_msleep(1);
    }
    
    // Type the correction
    for (int i = 0; i < correction_len; i++) {
        uint8_t keycode = 0;
        uint8_t modifiers = 0;
        
        // Simple character to keycode conversion
        char c = correction[i];
        if (c >= 'a' && c <= 'z') {
            keycode = HID_USAGE_KEY_KEYBOARD_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            keycode = HID_USAGE_KEY_KEYBOARD_A + (c - 'A');
            modifiers = KEYBOARD_MODIFIER_LEFTSHIFT;
        }
        
        if (keycode) {
            if (modifiers) {
                zmk_hid_keyboard_press(HID_USAGE_KEY_KEYBOARD_LEFT_SHIFT);
            }
            zmk_hid_keyboard_press(keycode);
            zmk_endpoints_send_report(HID_USAGE_CONSUMER);
            k_msleep(1);
            zmk_hid_keyboard_release(keycode);
            if (modifiers) {
                zmk_hid_keyboard_release(HID_USAGE_KEY_KEYBOARD_LEFT_SHIFT);
            }
            zmk_endpoints_send_report(HID_USAGE_CONSUMER);
            k_msleep(1);
        }
    }
}

// Check if current word is a typo and correct it
static void check_and_correct() {
    if (buffer_pos == 0) return;
    
    word_buffer[buffer_pos] = '\0';
    
    for (int i = 0; i < MAX_TYPOS; i++) {
        if (strcmp(word_buffer, typos[i].typo) == 0) {
            send_correction(typos[i].typo, typos[i].correction);
            break;
        }
    }
    
    // Clear buffer for next word
    buffer_pos = 0;
    memset(word_buffer, 0, WORD_BUFFER_SIZE);
}

// Add character to word buffer
static void add_to_buffer(char c) {
    if (buffer_pos < WORD_BUFFER_SIZE - 1) {
        word_buffer[buffer_pos++] = c;
    }
}

// Handle keycode events
static int autocorrect_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev || !ev->state) return ZMK_EV_EVENT_BUBBLE; // Only on key press
    
    uint16_t keycode = ev->keycode;
    
    // Check for word separators (space, punctuation, enter)
    if (keycode == HID_USAGE_KEY_KEYBOARD_SPACEBAR ||
        keycode == HID_USAGE_KEY_KEYBOARD_RETURN_ENTER ||
        keycode == HID_USAGE_KEY_KEYBOARD_PERIOD ||
        keycode == HID_USAGE_KEY_KEYBOARD_COMMA ||
        keycode == HID_USAGE_KEY_KEYBOARD_SEMICOLON ||
        keycode == HID_USAGE_KEY_KEYBOARD_EXCLAMATION) {
        check_and_correct();
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Add letters to buffer
    if (keycode >= HID_USAGE_KEY_KEYBOARD_A && keycode <= HID_USAGE_KEY_KEYBOARD_Z) {
        char c = 'a' + (keycode - HID_USAGE_KEY_KEYBOARD_A);
        add_to_buffer(c);
    }
    
    // Clear buffer on backspace
    if (keycode == HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE) {
        if (buffer_pos > 0) buffer_pos--;
        word_buffer[buffer_pos] = '\0';
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(autocorrect, autocorrect_keycode_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("🎯 ZMK Autocorrect Module Active!");
    LOG_INF("📝 Monitoring for %d common typos", MAX_TYPOS);
    LOG_INF("✨ Real-time autocorrect ready!");
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
