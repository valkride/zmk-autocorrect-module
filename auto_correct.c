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

static void check_typos(void) {
    if (state.length < 3) return;
    
    // Check "teh" -> log it for now
    if (state.length >= 3 && 
        strncmp(&state.buffer[state.length - 3], "teh", 3) == 0) {
        LOG_INF("Detected typo: teh -> should be 'the'");
        return;
    }
    
    // Check "adn" -> log it
    if (state.length >= 3 && 
        strncmp(&state.buffer[state.length - 3], "adn", 3) == 0) {
        LOG_INF("Detected typo: adn -> should be 'and'");
        return;
    }
}

static int autocorrect_listener(const zmk_event_t *eh) {
    // Use the working pattern from zmk source
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = ev->keycode;
    
    // Track typing
    if (is_letter(keycode)) {
        char c = keycode_to_char(keycode);
        if (c && state.length < BUFFER_SIZE - 1) {
            state.buffer[state.length++] = c;
            state.buffer[state.length] = '\0';
            LOG_DBG("Buffer: %s", state.buffer);
        }
    } else if (is_trigger(keycode)) {
        check_typos();
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

ZMK_LISTENER(autocorrect, autocorrect_listener);
ZMK_SUBSCRIPTION(autocorrect, zmk_keycode_state_changed);

static int autocorrect_init(const struct device *dev) {
    memset(&state, 0, sizeof(state));
    LOG_INF("ZMK Autocorrect module loaded - typo detection active");
    return 0;
}

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
