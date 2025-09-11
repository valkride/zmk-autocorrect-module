#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BUFFER_SIZE 32

static struct {
    char buffer[BUFFER_SIZE];
    uint8_t length;
    uint8_t correction_pending;
    char pending_correction[16];
    uint8_t backspaces_needed;
} state = {0};

static bool is_letter(uint16_t keycode) {
    return keycode >= A && keycode <= Z;
}

static bool is_trigger(uint16_t keycode) {
    return keycode == SPACE || keycode == DOT || keycode == COMMA || 
           keycode == RET || keycode == QMARK || keycode == EXCL;
}

static char keycode_to_char(uint16_t keycode) {
    if (keycode >= A && keycode <= Z) {
        return 'a' + (keycode - A);
    }
    return 0;
}

static void check_corrections(void) {
    if (state.length < 3) return;
    
    // Check "teh" -> "the"
    if (state.length >= 3 && 
        strncmp(&state.buffer[state.length - 3], "teh", 3) == 0) {
        state.correction_pending = 1;
        strcpy(state.pending_correction, "the");
        state.backspaces_needed = 3;
        return;
    }
    
    // Check "adn" -> "and"
    if (state.length >= 3 && 
        strncmp(&state.buffer[state.length - 3], "adn", 3) == 0) {
        state.correction_pending = 1;
        strcpy(state.pending_correction, "and");
        state.backspaces_needed = 3;
        return;
    }
    
    // Check "taht" -> "that"
    if (state.length >= 4 && 
        strncmp(&state.buffer[state.length - 4], "taht", 4) == 0) {
        state.correction_pending = 1;
        strcpy(state.pending_correction, "that");
        state.backspaces_needed = 4;
        return;
    }
}

static int autocorrect_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = ev->keycode;
    
    // Handle pending correction
    if (state.correction_pending) {
        if (is_trigger(keycode)) {
            // Apply correction by generating events
            for (int i = 0; i < state.backspaces_needed; i++) {
                struct zmk_keycode_state_changed backspace_ev = {
                    .keycode = BSPC,
                    .implicit_modifiers = 0,
                    .explicit_modifiers = 0,
                    .state = true
                };
                ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(backspace_ev));
                backspace_ev.state = false;
                ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(backspace_ev));
            }
            
            // Type correction
            for (int i = 0; state.pending_correction[i]; i++) {
                char c = state.pending_correction[i];
                if (c >= 'a' && c <= 'z') {
                    struct zmk_keycode_state_changed correct_ev = {
                        .keycode = A + (c - 'a'),
                        .implicit_modifiers = 0,
                        .explicit_modifiers = 0,
                        .state = true
                    };
                    ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(correct_ev));
                    correct_ev.state = false;
                    ZMK_EVENT_RAISE(new_zmk_keycode_state_changed(correct_ev));
                }
            }
            
            // Update buffer to reflect correction
            state.length -= state.backspaces_needed;
            for (int i = 0; state.pending_correction[i] && state.length < BUFFER_SIZE - 1; i++) {
                state.buffer[state.length++] = state.pending_correction[i];
            }
            state.buffer[state.length] = '\0';
            
            state.correction_pending = 0;
            LOG_INF("Applied autocorrection: %s", state.pending_correction);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Track typing
    if (is_letter(keycode)) {
        char c = keycode_to_char(keycode);
        if (c && state.length < BUFFER_SIZE - 1) {
            state.buffer[state.length++] = c;
            state.buffer[state.length] = '\0';
        }
    } else if (is_trigger(keycode)) {
        check_corrections();
    } else if (keycode == BSPC) {
        if (state.length > 0) {
            state.length--;
            state.buffer[state.length] = '\0';
        }
    } else {
        // Reset on other keys
        state.length = 0;
        state.buffer[0] = '\0';
    }
    
    return ZMK_EV_EVENT_BUBBLE;
}

static int autocorrect_init(const struct device *dev) {
    memset(&state, 0, sizeof(state));
    LOG_INF("Autocorrect module initialized");
    return 0;
}

ZMK_LISTENER(autocorrect, autocorrect_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
