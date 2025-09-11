#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <dt-bindings/zmk/keys.h>

#include "trie_dict.h"

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

// Function to perform actual correction
static void perform_correction(const char *wrong_word, const char *correct_word) {
    auto_correct_data.correction_in_progress = true;
    
    // For now, store the correction - actual key injection to be implemented
    strncpy(auto_correct_data.last_corrected_word, correct_word, MAX_WORD_LENGTH - 1);
    
    auto_correct_data.correction_in_progress = false;
}

static int auto_correct_init(const struct device *dev) {
    return 0;
}

static int auto_correct_position_changed(const zmk_event_t *eh) {
    struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || auto_correct_data.correction_in_progress) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = zmk_keymap_keycode_from_position(ev->position);
    
    if (keycode >= A && keycode <= Z) {
        char letter = 'a' + (keycode - A);
        
        if (auto_correct_data.word_pos < MAX_WORD_LENGTH - 1) {
            auto_correct_data.current_word[auto_correct_data.word_pos] = letter;
            auto_correct_data.word_pos++;
            auto_correct_data.current_word[auto_correct_data.word_pos] = '\0';
            auto_correct_data.in_word = true;
        }
    }
    else if (keycode == SPACE || keycode == DOT || keycode == COMMA || keycode == RET || keycode == TAB) {
        if (auto_correct_data.in_word && auto_correct_data.word_pos >= AUTOCORRECT_MIN_WORD_LENGTH) {
            const char *correction = get_correction(auto_correct_data.current_word);
            if (correction) {
                perform_correction(auto_correct_data.current_word, correction);
            }
        }
        
        auto_correct_data.word_pos = 0;
        auto_correct_data.current_word[0] = '\0';
        auto_correct_data.in_word = false;
    }
    else if (keycode == BSPC) {
        if (auto_correct_data.word_pos > 0) {
            auto_correct_data.word_pos--;
            auto_correct_data.current_word[auto_correct_data.word_pos] = '\0';
        }
        if (auto_correct_data.word_pos == 0) {
            auto_correct_data.in_word = false;
        }
    }
    else {
        auto_correct_data.word_pos = 0;
        auto_correct_data.current_word[0] = '\0';
        auto_correct_data.in_word = false;
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_auto_correct, auto_correct_position_changed);
ZMK_SUBSCRIPTION(behavior_auto_correct, zmk_position_state_changed);

SYS_INIT(auto_correct_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
