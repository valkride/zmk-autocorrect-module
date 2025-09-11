#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/usb.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>
#include <dt-bindings/zmk/hid_usage_pages.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_WORD_LENGTH 32
#define AUTOCORRECT_MIN_WORD_LENGTH 3

// Structure to hold autocorrect data
struct behavior_auto_correct_data {
    char current_word[MAX_WORD_LENGTH];
    int word_pos;
    bool in_word;
    char last_corrected_word[MAX_WORD_LENGTH];
    bool correction_in_progress;
};

static struct behavior_auto_correct_data auto_correct_data = {
    .current_word = {0},
    .word_pos = 0,
    .in_word = false,
    .last_corrected_word = {0},
    .correction_in_progress = false
};

// Simple correction dictionary - add more as needed
static const char* get_correction(const char* word) {
    // Common typos and their corrections
    if (strcmp(word, "teh") == 0) return "the";
    if (strcmp(word, "adn") == 0) return "and";
    if (strcmp(word, "taht") == 0) return "that";
    if (strcmp(word, "recieve") == 0) return "receive";
    if (strcmp(word, "seperate") == 0) return "separate";
    if (strcmp(word, "occured") == 0) return "occurred";
    if (strcmp(word, "acheive") == 0) return "achieve";
    if (strcmp(word, "beleive") == 0) return "believe";
    if (strcmp(word, "definately") == 0) return "definitely";
    if (strcmp(word, "accomodate") == 0) return "accommodate";
    
    return NULL; // No correction found
}

// Function to send HID key events using ZMK's HID system
static void send_key_sequence(uint16_t keycode, bool pressed) {
    if (pressed) {
        zmk_hid_keyboard_press(keycode);
    } else {
        zmk_hid_keyboard_release(keycode);
    }
    zmk_endpoints_send_report(HID_USAGE_KEY);
    k_msleep(10); // Small delay between key events
}

// Function to perform actual correction with real key replacement
static void perform_correction(const char *wrong_word, const char *correct_word) {
    int wrong_len = strlen(wrong_word);
    int correct_len = strlen(correct_word);
    
    // Set flag to prevent infinite loops during correction
    auto_correct_data.correction_in_progress = true;
    
    LOG_INF("AUTO-CORRECTING: '%s' -> '%s'", wrong_word, correct_word);
    
    // Send backspaces to remove wrong word
    for (int i = 0; i < wrong_len; i++) {
        send_key_sequence(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE, true);
        send_key_sequence(HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE, false);
        k_msleep(20);
    }
    
    // Type the correct word
    for (int i = 0; i < correct_len; i++) {
        char c = correct_word[i];
        uint16_t keycode = 0;
        
        // Convert character to HID keycode
        if (c >= 'a' && c <= 'z') {
            keycode = HID_USAGE_KEY_KEYBOARD_A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            // Handle uppercase with shift
            send_key_sequence(HID_USAGE_KEY_KEYBOARD_LEFTSHIFT, true);
            keycode = HID_USAGE_KEY_KEYBOARD_A + (c - 'A');
        }
        
        if (keycode > 0) {
            send_key_sequence(keycode, true);
            send_key_sequence(keycode, false);
            k_msleep(20);
            
            // Release shift if it was pressed
            if (c >= 'A' && c <= 'Z') {
                send_key_sequence(HID_USAGE_KEY_KEYBOARD_LEFTSHIFT, false);
            }
        }
    }
    
    // Store the corrected word
    strncpy(auto_correct_data.last_corrected_word, correct_word, MAX_WORD_LENGTH - 1);
    
    // Clear correction flag
    auto_correct_data.correction_in_progress = false;
}

// Event listener for keycode state changed events
static int auto_correct_keycode_pressed(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || auto_correct_data.correction_in_progress) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    // Only process key press events (not releases)
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint16_t keycode = ev->keycode;

    // Handle letter keys (A-Z)
    if (keycode >= HID_USAGE_KEY_KEYBOARD_A && keycode <= HID_USAGE_KEY_KEYBOARD_Z) {
        char letter = 'a' + (keycode - HID_USAGE_KEY_KEYBOARD_A);
        
        // Add letter to current word if we have space
        if (auto_correct_data.word_pos < MAX_WORD_LENGTH - 1) {
            auto_correct_data.current_word[auto_correct_data.word_pos] = letter;
            auto_correct_data.word_pos++;
            auto_correct_data.current_word[auto_correct_data.word_pos] = '\0';
            auto_correct_data.in_word = true;
        }
        
        LOG_DBG("Building word: '%s'", auto_correct_data.current_word);
    }
    // Handle word boundary characters (space, punctuation, etc.)
    else if (keycode == HID_USAGE_KEY_KEYBOARD_SPACEBAR || 
             keycode == HID_USAGE_KEY_KEYBOARD_PERIOD_AND_GREATER_THAN ||
             keycode == HID_USAGE_KEY_KEYBOARD_COMMA_AND_LESS_THAN ||
             keycode == HID_USAGE_KEY_KEYBOARD_RETURN_ENTER ||
             keycode == HID_USAGE_KEY_KEYBOARD_TAB) {
        
        // If we were building a word, check for corrections
        if (auto_correct_data.in_word && auto_correct_data.word_pos >= AUTOCORRECT_MIN_WORD_LENGTH) {
            LOG_DBG("Word complete: '%s', checking for corrections", auto_correct_data.current_word);
            
            const char *correction = get_correction(auto_correct_data.current_word);
            if (correction) {
                perform_correction(auto_correct_data.current_word, correction);
            }
        }
        
        // Reset word buffer
        auto_correct_data.word_pos = 0;
        auto_correct_data.current_word[0] = '\0';
        auto_correct_data.in_word = false;
    }
    // Handle backspace - adjust word buffer
    else if (keycode == HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE) {
        if (auto_correct_data.word_pos > 0) {
            auto_correct_data.word_pos--;
            auto_correct_data.current_word[auto_correct_data.word_pos] = '\0';
        }
        if (auto_correct_data.word_pos == 0) {
            auto_correct_data.in_word = false;
        }
    }
    // Any other key resets word buffer
    else {
        auto_correct_data.word_pos = 0;
        auto_correct_data.current_word[0] = '\0';
        auto_correct_data.in_word = false;
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

// Initialize the auto-correct module
static int auto_correct_init(const struct device *dev) {
    LOG_INF("Auto-correct module initialized");
    return 0;
}

// Event listener setup
ZMK_LISTENER(behavior_auto_correct, auto_correct_keycode_pressed);
ZMK_SUBSCRIPTION(behavior_auto_correct, zmk_keycode_state_changed);

// Device initialization
SYS_INIT(auto_correct_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
