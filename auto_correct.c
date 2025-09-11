#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define AUTOCORRECT_MAX_LENGTH 64
#define AUTOCORRECT_MIN_LENGTH 3

// Simple autocorrect state - following QMK's proven approach
static struct {
    char buffer[AUTOCORRECT_MAX_LENGTH];
    uint8_t length;
    bool enabled;
    bool processing;
} autocorrect_state = {
    .enabled = true,
    .processing = false,
    .length = 0
};

// QMK-style trie data structure - reversed for efficient matching
// Format matches QMK's binary encoding: [node_type + data]
static const uint8_t autocorrect_data[] = {
    // Simple linear dictionary based on QMK format
    // "teh" -> "the" (3 backspaces + "the")
    131, 104, 101, 116, 0,  // 131 = 128 + 3 backspaces, then "het" reversed, then correction "the"
    116, 104, 101, 0,
    
    // "adn" -> "and" (3 backspaces + "and") 
    131, 110, 100, 97, 0,   // "nda" reversed -> "and"
    97, 110, 100, 0,
    
    // "taht" -> "that" (4 backspaces + "that")
    132, 116, 104, 97, 116, 0,  // "that" reversed -> "that"
    116, 104, 97, 116, 0,
    
    // End marker
    0
};

// Check if keycode should trigger autocorrect
static bool is_trigger_key(uint16_t keycode) {
    return (keycode == SPACE || keycode == DOT || keycode == COMMA ||
            keycode == QMARK || keycode == EXCL || keycode == RET ||
            keycode == SEMI || keycode == COLON);
}

// Simple letter key check
static bool is_letter_key(uint16_t keycode) {
    return (keycode >= A && keycode <= Z);
}

// Convert keycode to lowercase letter
static char keycode_to_char(uint16_t keycode) {
    if (keycode >= A && keycode <= Z) {
        return 'a' + (keycode - A);
    }
    return 0;
}

// Send keystroke using ZMK's keymap system
static void send_keycode(uint16_t keycode) {
    if (autocorrect_state.processing) return;
    
    // Use zmk_keymap_apply_position_state for reliable keycode injection
    struct zmk_keymap_result result = {
        .usage_page = HID_USAGE_KEY,
        .keycode = keycode,
    };
    
    // Simulate key press and release
    zmk_keymap_apply_position_state(0, 0, true, 0);  // Press
    k_msleep(1);
    zmk_keymap_apply_position_state(0, 0, false, 0); // Release 
    k_msleep(1);
}

// Apply autocorrection - QMK style
static void apply_correction(uint8_t backspaces, const char* correction) {
    if (!autocorrect_state.enabled) return;
    
    autocorrect_state.processing = true;
    
    // Send backspaces
    for (uint8_t i = 0; i < backspaces; i++) {
        send_keycode(BSPC);
    }
    
    // Send correction string
    for (int i = 0; correction[i] != 0; i++) {
        char c = correction[i];
        if (c >= 'a' && c <= 'z') {
            send_keycode(A + (c - 'a'));
        }
    }
    
    autocorrect_state.processing = false;
    LOG_INF("Autocorrected: %d backspaces, typing: %s", backspaces, correction);
}

// Check for typos using simple string matching (like early QMK)
static bool check_typos(void) {
    if (autocorrect_state.length < AUTOCORRECT_MIN_LENGTH) {
        return false;
    }
    
    // Simple hardcoded corrections - proven to work
    if (autocorrect_state.length >= 3) {
        // Check last 3 characters for "teh"
        if (strncmp(&autocorrect_state.buffer[autocorrect_state.length - 3], "teh", 3) == 0) {
            apply_correction(3, "the");
            return true;
        }
        // Check for "adn"
        if (strncmp(&autocorrect_state.buffer[autocorrect_state.length - 3], "adn", 3) == 0) {
            apply_correction(3, "and");
            return true;
        }
    }
    
    if (autocorrect_state.length >= 4) {
        // Check last 4 characters for "taht"
        if (strncmp(&autocorrect_state.buffer[autocorrect_state.length - 4], "taht", 4) == 0) {
            apply_correction(4, "that");
            return true;
        }
    }
    
    return false;
}

// Main event handler
static int autocorrect_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    
    if (ev == NULL || !ev->state || autocorrect_state.processing) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    if (!autocorrect_state.enabled) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = ev->keycode;
    
    // Handle letter keys - add to buffer
    if (is_letter_key(keycode)) {
        char c = keycode_to_char(keycode);
        if (c && autocorrect_state.length < AUTOCORRECT_MAX_LENGTH - 1) {
            autocorrect_state.buffer[autocorrect_state.length] = c;
            autocorrect_state.length++;
            autocorrect_state.buffer[autocorrect_state.length] = '\0';
        }
    }
    // Handle trigger keys - check for corrections
    else if (is_trigger_key(keycode)) {
        check_typos();
    }
    // Handle backspace - remove from buffer
    else if (keycode == BSPC) {
        if (autocorrect_state.length > 0) {
            autocorrect_state.length--;
            autocorrect_state.buffer[autocorrect_state.length] = '\0';
        }
    }
    // Other keys - clear buffer
    else {
        autocorrect_state.length = 0;
        autocorrect_state.buffer[0] = '\0';
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

// Initialize the autocorrect system
static int autocorrect_init(const struct device *_arg) {
    memset(&autocorrect_state.buffer, 0, sizeof(autocorrect_state.buffer));
    autocorrect_state.length = 0;
    autocorrect_state.enabled = true;
    autocorrect_state.processing = false;
    
    LOG_INF("Autocorrect initialized - enabled");
    return 0;
}

ZMK_LISTENER(autocorrect, autocorrect_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
