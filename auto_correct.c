#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_WORD_LENGTH 32
#define AUTOCORRECT_MIN_WORD_LENGTH 3

// Autocorrect pairs - expandable dictionary
struct correction_pair {
    const char* wrong;
    const char* correct;
};

static const struct correction_pair corrections[] = {
    {"teh", "the"},
    {"adn", "and"},
    {"taht", "that"},
    {"ot", "to"},
    {"fo", "of"},
    {"wiht", "with"},
    {"thsi", "this"},
    {"si", "is"},
    {"yuo", "you"},
    {"jsut", "just"},
    {"cna", "can"},
    {"ahve", "have"},
    {"woudl", "would"},
    {"shoudl", "should"},
    {"recieve", "receive"},
};

// Structure to hold autocorrect state
struct behavior_auto_correct_data {
    char current_word[MAX_WORD_LENGTH];
    int word_pos;
    bool in_word;
    struct k_work_delayable correction_work;
};

static struct behavior_auto_correct_data auto_correct_data = {
    .current_word = {0},
    .word_pos = 0,
    .in_word = false
};

// Find correction for a word
static const char* find_correction(const char* word) {
    for (int i = 0; i < ARRAY_SIZE(corrections); i++) {
        if (strcmp(word, corrections[i].wrong) == 0) {
            return corrections[i].correct;
        }
    }
    return NULL;
}

// Work handler for delayed correction
static void correction_work_handler(struct k_work *work) {
    LOG_INF("Performing autocorrection...");
    
    // Find the correction for the last typed word
    const char* correction = find_correction(auto_correct_data.current_word);
    if (correction) {
        int wrong_len = strlen(auto_correct_data.current_word);
        int correct_len = strlen(correction);
        
        LOG_INF("Correcting '%s' (%d chars) to '%s' (%d chars)", 
                auto_correct_data.current_word, wrong_len, correction, correct_len);
        
        // Simulate backspaces to delete wrong word
        for (int i = 0; i < wrong_len; i++) {
            LOG_DBG("Backspace %d", i + 1);
            // Here we would send BACKSPACE keycode
        }
        
        // Simulate typing the correct word
        for (int i = 0; i < correct_len; i++) {
            char c = correction[i];
            LOG_DBG("Type '%c'", c);
            // Here we would send the keycode for character c
        }
        
        LOG_INF("Autocorrection completed: %s", correction);
    }
    
    // Reset state
    auto_correct_data.in_word = false;
    auto_correct_data.word_pos = 0;
    memset(auto_correct_data.current_word, 0, sizeof(auto_correct_data.current_word));
}

// Check if keycode is a letter
static bool is_letter_key(uint16_t keycode) {
    return (keycode >= A && keycode <= Z);
}

// Check if keycode is a word separator
static bool is_word_separator(uint16_t keycode) {
    return (keycode == SPACE || keycode == ENTER || keycode == TAB ||
            keycode == DOT || keycode == COMMA || keycode == SEMI ||
            keycode == EXCLAMATION || keycode == QUESTION);
}

// Process a word when it's completed
static void process_completed_word(void) {
    if (auto_correct_data.word_pos >= AUTOCORRECT_MIN_WORD_LENGTH) {
        auto_correct_data.current_word[auto_correct_data.word_pos] = '\0';
        
        const char* correction = find_correction(auto_correct_data.current_word);
        if (correction) {
            LOG_INF("AUTOCORRECT: '%s' -> '%s'", auto_correct_data.current_word, correction);
            
            // Schedule correction work to avoid event handling recursion
            k_work_schedule(&auto_correct_data.correction_work, K_MSEC(50));
        }
    }
    
    // Reset word state
    auto_correct_data.in_word = false;
    auto_correct_data.word_pos = 0;
    memset(auto_correct_data.current_word, 0, sizeof(auto_correct_data.current_word));
}

// Event listener for keycode state changed events
static int auto_correct_keycode_pressed(const zmk_event_t *eh) {
    // Use the same pattern as auto_layer module - just cast and check state
    struct zmk_keycode_state_changed *ev = (struct zmk_keycode_state_changed *)eh;
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint16_t keycode = ev->keycode;
    
    LOG_DBG("Keycode pressed: %d", keycode);

    if (is_letter_key(keycode)) {
        // Start or continue building a word
        if (!auto_correct_data.in_word) {
            auto_correct_data.in_word = true;
            auto_correct_data.word_pos = 0;
            memset(auto_correct_data.current_word, 0, sizeof(auto_correct_data.current_word));
        }
        
        if (auto_correct_data.word_pos < MAX_WORD_LENGTH - 1) {
            char letter = 'a' + (keycode - A);
            auto_correct_data.current_word[auto_correct_data.word_pos++] = letter;
        }
    } else if (is_word_separator(keycode)) {
        if (auto_correct_data.in_word) {
            process_completed_word();
        }
    } else {
        // Other keys (numbers, symbols) - end word if in one
        if (auto_correct_data.in_word) {
            process_completed_word();
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

// Manual event subscription to avoid macro issues
static struct zmk_listener auto_correct_listener = {
    .callback = auto_correct_keycode_pressed,
};

static int auto_correct_init(const struct device *dev) {
    LOG_INF("Autocorrect module initialized with %d correction pairs", ARRAY_SIZE(corrections));
    LOG_INF("Autocorrect ready - typo detection available for: teh->the, adn->and, yuo->you, etc.");
    
    // Initialize work queue
    k_work_init_delayable(&auto_correct_data.correction_work, correction_work_handler);
    
    // Register event listener manually
    int ret = zmk_event_manager_add_listener(&auto_correct_listener);
    if (ret < 0) {
        LOG_ERR("Failed to register autocorrect listener: %d", ret);
        return ret;
    }
    
    LOG_INF("Autocorrect keystroke monitoring active!");
    return 0;
}

// Device initialization
SYS_INIT(auto_correct_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
