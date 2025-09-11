#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/init.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/battery.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>

#include <zephyr/logging/log.h>

#include "trie_dict.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Structure to hold autocorrect data
struct behavior_auto_correct_data {
    char correction_buffer[32];
    int buffer_pos;
};

static struct behavior_auto_correct_data auto_correct_data = {
    .correction_buffer = {0},
    .buffer_pos = 0
};

// inits main struct
static int auto_correct_init(const struct device *dev) {
    LOG_INF("Auto-correct module initialized");
    return 0;
}

// Function to handle keycode state changes
static int auto_correct_keycode_changed(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // Only process key press events
    if (!ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    
    // TODO: Add autocorrect logic here using the trie dictionary
    // For now, just log the keycode
    LOG_DBG("Auto-correct processing keycode: %d", ev->keycode);
    
    return ZMK_EV_EVENT_BUBBLE;
}

// manages new keycodes presses
ZMK_LISTENER(behavior_auto_correct, auto_correct_keycode_changed);
ZMK_SUBSCRIPTION(behavior_auto_correct, zmk_keycode_state_changed);

// Initialize the auto-correct module
SYS_INIT(auto_correct_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
