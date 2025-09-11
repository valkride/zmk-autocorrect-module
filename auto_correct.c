#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Simple initialization function that just logs
static int autocorrect_init(const struct device *dev) {
    LOG_INF("ZMK Autocorrect module loaded successfully");
    return 0;
}

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
