#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static int auto_correct_init(const struct device *dev) {
    LOG_INF("Minimal Auto-correct module initialized successfully");
    return 0;
}

// Device initialization
SYS_INIT(auto_correct_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
