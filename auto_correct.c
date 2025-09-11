#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <ctype.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define AUTOCORRECT_MAX_LENGTH 127
#define AUTOCORRECT_MIN_LENGTH 5

// Autocorrect data structure
static struct {
    uint8_t buffer[AUTOCORRECT_MAX_LENGTH];
    uint8_t length;
    bool suppressed;
} autocorrect_state;

// Simple autocorrect dictionary - based on QMK's approach
// Format: [typo_length][typo][correction_length][correction]
static const uint8_t autocorrect_data[] = {
    // "teh" -> "the"
    3, 't', 'e', 'h', 3, 't', 'h', 'e',
    // "adn" -> "and"  
    3, 'a', 'd', 'n', 3, 'a', 'n', 'd',
    // "taht" -> "that"
    4, 't', 'a', 'h', 't', 4, 't', 'h', 'a', 't',
    // "recieve" -> "receive"
    7, 'r', 'e', 'c', 'i', 'e', 'v', 'e', 7, 'r', 'e', 'c', 'e', 'i', 'v', 'e',
    // "seperate" -> "separate" 
    8, 's', 'e', 'p', 'e', 'r', 'a', 't', 'e', 8, 's', 'e', 'p', 'a', 'r', 'a', 't', 'e',
    // End marker
    0
};

// Check if character should trigger autocorrect check
static bool is_autocorrect_trigger(uint16_t keycode) {
    return (keycode == SPACE || keycode == DOT || keycode == COMMA ||
            keycode == QMARK || keycode == EXCL || keycode == RET);
}

// Send a sequence of keycodes
static void send_autocorrect_keycode(uint16_t keycode) {
    zmk_hid_keyboard_press(keycode);
    zmk_endpoints_send_report(HID_USAGE_DESKTOP_KEYBOARD);
    k_msleep(1);
    zmk_hid_keyboard_release(keycode);
    zmk_endpoints_send_report(HID_USAGE_DESKTOP_KEYBOARD);
    k_msleep(1);
}

// Perform the autocorrection
static void apply_autocorrect(uint8_t backspaces, const uint8_t *correction, uint8_t correction_length) {
    autocorrect_state.suppressed = true;
    
    // Send backspaces
    for (uint8_t i = 0; i < backspaces; i++) {
        send_autocorrect_keycode(BSPC);
    }
    
    // Send correction
    for (uint8_t i = 0; i < correction_length; i++) {
        char c = correction[i];
        if (c >= 'a' && c <= 'z') {
            send_autocorrect_keycode(A + (c - 'a'));
        } else if (c >= 'A' && c <= 'Z') {
            // Handle uppercase (would need shift logic)
            send_autocorrect_keycode(A + (c - 'A'));
        }
    }
    
    autocorrect_state.suppressed = false;
    LOG_INF("Autocorrected with %d backspaces, %d chars", backspaces, correction_length);
}

// Check buffer against autocorrect dictionary
static bool check_autocorrect(void) {
    if (autocorrect_state.length < AUTOCORRECT_MIN_LENGTH) {
        return false;
    }
    
    const uint8_t *data = autocorrect_data;
    
    while (*data) {
        uint8_t typo_length = *data++;
        const uint8_t *typo = data;
        data += typo_length;
        uint8_t correction_length = *data++;
        const uint8_t *correction = data;
        data += correction_length;
        
        // Check if we have a match at the end of our buffer
        if (typo_length <= autocorrect_state.length) {
            uint8_t start_pos = autocorrect_state.length - typo_length;
            if (memcmp(&autocorrect_state.buffer[start_pos], typo, typo_length) == 0) {
                apply_autocorrect(typo_length, correction, correction_length);
                return true;
            }
        }
    }
    
    return false;
}

// Handle keycode events
static int autocorrect_keycode_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state || autocorrect_state.suppressed) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = ev->keycode;
    
    // Handle letters
    if (keycode >= A && keycode <= Z) {
        char c = 'a' + (keycode - A);
        
        // Add to buffer
        if (autocorrect_state.length < AUTOCORRECT_MAX_LENGTH - 1) {
            autocorrect_state.buffer[autocorrect_state.length++] = c;
        } else {
            // Shift buffer left
            memmove(autocorrect_state.buffer, autocorrect_state.buffer + 1, AUTOCORRECT_MAX_LENGTH - 1);
            autocorrect_state.buffer[AUTOCORRECT_MAX_LENGTH - 1] = c;
        }
    }
    // Handle autocorrect triggers
    else if (is_autocorrect_trigger(keycode)) {
        check_autocorrect();
    }
    // Handle backspace
    else if (keycode == BSPC) {
        if (autocorrect_state.length > 0) {
            autocorrect_state.length--;
        }
    }
    // Other keys reset the buffer
    else {
        autocorrect_state.length = 0;
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

// Initialize autocorrect
static int autocorrect_init(const struct device *dev) {
    memset(&autocorrect_state, 0, sizeof(autocorrect_state));
    LOG_INF("Autocorrect initialized");
    return 0;
}

ZMK_LISTENER(autocorrect, autocorrect_keycode_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
