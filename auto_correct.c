#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/keys.h>
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
static bool correcting = false;

// Convert character to ZMK keycode
static uint32_t char_to_keycode(char c) {
    if (c >= 'a' && c <= 'z') {
        return ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_A + (c - 'a'));
    } else if (c >= 'A' && c <= 'Z') {
        return ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_A + (c - 'A'));
    }
    return 0;
}

// Send REAL keystroke correction 
static void send_correction(const char* typo, const char* correction) {
    if (correcting) return; // Prevent recursion
    correcting = true;
    
    int typo_len = strlen(typo);
    int correction_len = strlen(correction);
    int64_t timestamp = k_uptime_get();
    
    LOG_INF("� CORRECTING: '%s' -> '%s'", typo, correction);
    
    // Send backspaces to delete the typo
    for (int i = 0; i < typo_len; i++) {
        raise_zmk_keycode_state_changed_from_encoded(
            ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE), 
            true, timestamp);
        k_msleep(5);
        raise_zmk_keycode_state_changed_from_encoded(
            ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE), 
            false, timestamp + 1);
        k_msleep(5);
    }
    
    // Type the correction
    for (int i = 0; i < correction_len; i++) {
        uint32_t keycode = char_to_keycode(correction[i]);
        if (keycode) {
            // Handle uppercase
            bool shift = (correction[i] >= 'A' && correction[i] <= 'Z');
            
            if (shift) {
                raise_zmk_keycode_state_changed_from_encoded(
                    ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_LEFT_SHIFT), 
                    true, timestamp);
                k_msleep(5);
            }
            
            raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
            k_msleep(5);
            raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp + 1);
            
            if (shift) {
                k_msleep(5);
                raise_zmk_keycode_state_changed_from_encoded(
                    ZMK_HID_USAGE(HID_USAGE_KEY, HID_USAGE_KEY_KEYBOARD_LEFT_SHIFT), 
                    false, timestamp + 2);
            }
            k_msleep(10);
        }
    }
    
    LOG_INF("✅ Correction complete!");
    correcting = false;
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
    if (!ev || !ev->state || correcting) return ZMK_EV_EVENT_BUBBLE; // Only on key press, skip when correcting
    
    uint16_t keycode = ev->keycode;
    
    // Check for word separators (space, punctuation, enter)
    if (keycode == HID_USAGE_KEY_KEYBOARD_SPACEBAR ||
        keycode == HID_USAGE_KEY_KEYBOARD_RETURN_ENTER ||
        keycode == HID_USAGE_KEY_KEYBOARD_PERIOD ||
        keycode == HID_USAGE_KEY_KEYBOARD_COMMA ||
        keycode == HID_USAGE_KEY_KEYBOARD_SEMICOLON ||
        keycode == HID_USAGE_KEY_KEYBOARD_APOSTROPHE_AND_QUOTE ||
        keycode == HID_USAGE_KEY_KEYBOARD_GRAVE_ACCENT_AND_TILDE) {
        check_and_correct();
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Add letters to buffer
    if (keycode >= HID_USAGE_KEY_KEYBOARD_A && keycode <= HID_USAGE_KEY_KEYBOARD_Z) {
        char c = 'a' + (keycode - HID_USAGE_KEY_KEYBOARD_A);
        add_to_buffer(c);
    }
    
    // Clear buffer on backspace or non-letter keys
    if (keycode == HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE) {
        if (buffer_pos > 0) buffer_pos--;
        word_buffer[buffer_pos] = '\0';
    } else if (keycode < HID_USAGE_KEY_KEYBOARD_A || keycode > HID_USAGE_KEY_KEYBOARD_Z) {
        // Non-letter key pressed, reset buffer (except for separators which are handled above)
        buffer_pos = 0;
        memset(word_buffer, 0, WORD_BUFFER_SIZE);
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(autocorrect, autocorrect_keycode_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("🎯 ZMK Autocorrect Module ACTIVE!");
    LOG_INF("⚡ REAL keystroke correction enabled");
    LOG_INF("📝 Correcting %d typos: teh->the, adn->and, yuo->you, etc.", MAX_TYPOS);
    LOG_INF("🚀 Type normally - corrections happen automatically!");
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
