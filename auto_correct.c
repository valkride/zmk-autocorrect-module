#include <string.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_WORD_LENGTH 32
#define AUTOCORRECT_MIN_WORD_LENGTH 3

// Structure to hold autocorrect data
struct behavior_auto_correct_data {
    char current_word[MAX_WORD_LENGTH];
    int word_pos;
    bool in_word;
    bool correction_in_progress;
};

static struct behavior_auto_correct_data auto_correct_data = {
    .current_word = {0},
    .word_pos = 0,
    .in_word = false,
    .correction_in_progress = false
};

// Simple correction dictionary
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

// Work queue handler for delayed corrections
static struct k_work_delayable correction_work;

static void correction_work_handler(struct k_work *work) {
    // For now, just log the correction
    // In a full implementation, this would send the actual keystrokes
    LOG_INF("AUTOCORRECT: Would correct '%s'", auto_correct_data.current_word);
    auto_correct_data.correction_in_progress = false;
}

// Function to perform correction (simplified for compatibility)
static void perform_correction(const char *wrong_word, const char *correct_word) {
    auto_correct_data.correction_in_progress = true;
    
    LOG_INF("AUTO-CORRECTING: '%s' -> '%s'", wrong_word, correct_word);
    
    // Schedule delayed work to avoid recursion issues
    k_work_schedule(&correction_work, K_MSEC(100));
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
    // Handle backspace - adjust word buffer
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

// Initialize the auto-correct module
static int auto_correct_init(const struct device *dev) {
    LOG_INF("Auto-correct module initialized (logging mode)");
    
    // Initialize work queue
    k_work_init_delayable(&correction_work, correction_work_handler);
    
    return 0;
}

// Event listener setup
ZMK_LISTENER(behavior_auto_correct, auto_correct_keycode_pressed);
ZMK_SUBSCRIPTION(behavior_auto_correct, zmk_keycode_state_changed);

// Device initialization
SYS_INIT(auto_correct_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
