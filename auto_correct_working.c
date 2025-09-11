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

#include <zephyr/logging/log.h>

#include "trie_dict.h"

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

// Trie search function
static bool search_trie(struct TrieNode *root, const char *word) {
    struct TrieNode *temp = root;
    
    for (int i = 0; word[i] != '\0'; i++) {
        if (!isalpha(word[i])) return false;
        
        int position = tolower(word[i]) - 'a';
        if (position < 0 || position >= 26 || temp->children[position] == NULL) {
            return false;
        }
        temp = temp->children[position];
    }
    return temp != NULL && temp->is_leaf == 1;
}

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

// Function to actually send key events
static void send_keycode_event(uint16_t keycode, bool pressed) {
    struct zmk_keycode_state_changed_event {
        struct zmk_event_header header;
        uint16_t keycode;
        uint8_t implicit_modifiers;
        uint8_t explicit_modifiers;
        bool state;
        int64_t timestamp;
    } ev = {
        .keycode = keycode,
        .implicit_modifiers = 0,
        .explicit_modifiers = 0,
        .state = pressed,
        .timestamp = k_uptime_get()
    };
    
    ZMK_EVENT_RAISE(zmk_keycode_state_changed, ev);
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
        send_keycode_event(BSPC, true);   // Press backspace
        k_msleep(10);
        send_keycode_event(BSPC, false);  // Release backspace  
        k_msleep(10);
    }
    
    // Type the correct word
    for (int i = 0; i < correct_len; i++) {
        char c = correct_word[i];
        uint16_t keycode = 0;
        
        // Convert character to keycode
        if (c >= 'a' && c <= 'z') {
            keycode = A + (c - 'a');
        } else if (c >= 'A' && c <= 'Z') {
            // Handle uppercase with shift
            send_keycode_event(LSHFT, true);
            keycode = A + (c - 'A');
        }
        
        if (keycode > 0) {
            send_keycode_event(keycode, true);   // Press key
            k_msleep(10);
            send_keycode_event(keycode, false);  // Release key
            k_msleep(10);
            
            // Release shift if it was pressed
            if (c >= 'A' && c <= 'Z') {
                send_keycode_event(LSHFT, false);
                k_msleep(10);
            }
        }
    }
    
    // Store the corrected word
    strncpy(auto_correct_data.last_corrected_word, correct_word, MAX_WORD_LENGTH - 1);
    
    // Clear correction flag
    auto_correct_data.correction_in_progress = false;
}

// inits main struct
static int auto_correct_init(const struct device *dev) {
    LOG_INF("Auto-correct module initialized");
    return 0;
}

// Function to handle position state changes
static int auto_correct_position_changed(const zmk_event_t *eh) {
    struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || auto_correct_data.correction_in_progress) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Only process key press events
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Get the keycode from the keymap
    // TODO: zmk_keymap_keycode_from_position doesn't exist in current ZMK API
    // For now, skip position-based keycode detection
    uint16_t keycode = 0; // Placeholder - this function needs keycode events instead
    
    // Handle letter keys
    if (keycode >= A && keycode <= Z) {
        char letter = 'a' + (keycode - A);
        
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
    else if (keycode == SPACE || 
             keycode == DOT ||
             keycode == COMMA ||
             keycode == RET ||
             keycode == TAB) {
        
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
    // Handle backspace - reset word buffer
    else if (keycode == BSPC) {
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

// manages new position presses
ZMK_LISTENER(behavior_auto_correct, auto_correct_position_changed);
ZMK_SUBSCRIPTION(behavior_auto_correct, zmk_position_state_changed);

// Initialize the auto-correct module
SYS_INIT(auto_correct_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
