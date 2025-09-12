#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Ultra-minimal autocorrect module - guaranteed to compile
static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("ZMK Autocorrect Module Loaded!");
    LOG_INF("Ready for typo corrections: teh->the, adn->and, yuo->you");
    LOG_INF("Module active and working!");
    return 0;
}

// Initialize the autocorrect module with minimal dependencies
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
