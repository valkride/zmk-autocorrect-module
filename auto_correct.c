#define DT_DRV_COMPAT zmk_behavior_autocorrect

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <string.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define AUTOCORRECT_BUFFER_SIZE 32

static struct {
    char buffer[AUTOCORRECT_BUFFER_SIZE];
    uint8_t length;
    bool enabled;
    bool processing;
} autocorrect_state = {
    .enabled = false,
    .processing = false,
    .length = 0
};

// Autocorrect dictionary - simple approach
static struct {
    const char *typo;
    const char *correction;
    uint8_t typo_len;
} corrections[] = {
    {"teh", "the", 3},
    {"adn", "and", 3},
    {"taht", "that", 4},
    {"recieve", "receive", 7},
    {"seperate", "separate", 8},
    {NULL, NULL, 0} // End marker
};

static bool is_letter(uint16_t keycode) {
    return keycode >= A && keycode <= Z;
}

static bool is_trigger_key(uint16_t keycode) {
    return keycode == SPACE || keycode == DOT || keycode == COMMA || 
           keycode == RET || keycode == QMARK || keycode == EXCL ||
           keycode == SEMI || keycode == COLON;
}

static char keycode_to_char(uint16_t keycode) {
    if (keycode >= A && keycode <= Z) {
        return 'a' + (keycode - A);
    }
    return 0;
}

// Send a keycode using ZMK's position system
static void send_keycode(uint16_t keycode) {
    // Create a temporary position for autocorrect
    uint32_t position = 0; // Use position 0 for autocorrect injections
    
    // Press
    zmk_keymap_position_state_changed(ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL, 
                                     position, true, k_uptime_get());
    k_msleep(1);
    
    // Release  
    zmk_keymap_position_state_changed(ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                                     position, false, k_uptime_get());
    k_msleep(1);
}

// Apply autocorrection
static void apply_correction(const char *typo, const char *correction, uint8_t typo_len) {
    if (autocorrect_state.processing) return;
    
    autocorrect_state.processing = true;
    
    LOG_INF("Autocorrecting '%s' -> '%s'", typo, correction);
    
    // Send backspaces to delete the typo
    for (uint8_t i = 0; i < typo_len; i++) {
        send_keycode(BSPC);
    }
    
    // Type the correction
    for (int i = 0; correction[i] != '\0'; i++) {
        char c = correction[i];
        if (c >= 'a' && c <= 'z') {
            send_keycode(A + (c - 'a'));
        }
    }
    
    // Update buffer to reflect correction
    autocorrect_state.length -= typo_len;
    int correction_len = strlen(correction);
    if (autocorrect_state.length + correction_len < AUTOCORRECT_BUFFER_SIZE - 1) {
        for (int i = 0; i < correction_len; i++) {
            autocorrect_state.buffer[autocorrect_state.length + i] = correction[i];
        }
        autocorrect_state.length += correction_len;
        autocorrect_state.buffer[autocorrect_state.length] = '\0';
    }
    
    autocorrect_state.processing = false;
}

// Check for typos and apply corrections
static void check_and_correct_typos(void) {
    if (!autocorrect_state.enabled || autocorrect_state.processing) {
        return;
    }
    
    // Check each correction pattern
    for (int i = 0; corrections[i].typo != NULL; i++) {
        uint8_t typo_len = corrections[i].typo_len;
        
        if (autocorrect_state.length >= typo_len) {
            // Check if buffer ends with this typo
            if (strncmp(&autocorrect_state.buffer[autocorrect_state.length - typo_len], 
                       corrections[i].typo, typo_len) == 0) {
                apply_correction(corrections[i].typo, corrections[i].correction, typo_len);
                return;
            }
        }
    }
}

// Event listener for keystrokes
static int autocorrect_keycode_listener(const zmk_event_t *eh) {
    if (!autocorrect_state.enabled || autocorrect_state.processing) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Simple event handling - cast directly
    const struct zmk_keycode_state_changed *ev = (const struct zmk_keycode_state_changed *)eh;
    
    // Only process key press events
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    uint16_t keycode = ev->keycode;
    
    // Handle letters - add to buffer
    if (is_letter(keycode)) {
        char c = keycode_to_char(keycode);
        if (c && autocorrect_state.length < AUTOCORRECT_BUFFER_SIZE - 1) {
            autocorrect_state.buffer[autocorrect_state.length] = c;
            autocorrect_state.length++;
            autocorrect_state.buffer[autocorrect_state.length] = '\0';
        }
    }
    // Handle trigger keys - check for corrections
    else if (is_trigger_key(keycode)) {
        check_and_correct_typos();
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

static int behavior_autocorrect_init(const struct device *dev) {
    memset(&autocorrect_state.buffer, 0, sizeof(autocorrect_state.buffer));
    autocorrect_state.enabled = false;
    autocorrect_state.processing = false;
    autocorrect_state.length = 0;
    
    LOG_INF("Autocorrect behavior initialized (disabled by default)");
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    // Toggle autocorrect on/off
    autocorrect_state.enabled = !autocorrect_state.enabled;
    
    LOG_INF("Autocorrect %s", autocorrect_state.enabled ? "ENABLED" : "DISABLED");
    
    // Clear buffer when toggling
    autocorrect_state.length = 0;
    autocorrect_state.buffer[0] = '\0';
    
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_autocorrect_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

// Event listener setup - using a different approach to avoid linker issues
static int autocorrect_listener_wrapper(const zmk_event_t *eh);

ZMK_LISTENER(behavior_autocorrect, autocorrect_listener_wrapper);
ZMK_SUBSCRIPTION(behavior_autocorrect, zmk_keycode_state_changed);

static int autocorrect_listener_wrapper(const zmk_event_t *eh) {
    // Only listen to keycode events when autocorrect behavior is present
    #if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
    return autocorrect_keycode_listener(eh);
    #else
    return ZMK_EV_EVENT_BUBBLE;
    #endif
}

#define AC_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_autocorrect_init, NULL, NULL, NULL, POST_KERNEL,         \
                             CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                  \
                             &behavior_autocorrect_driver_api);

DT_INST_FOREACH_STATUS_OKAY(AC_INST)

#endif
