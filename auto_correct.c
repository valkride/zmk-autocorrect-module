#include <zephyr/kernel.h>
#include <zephyr/init.h>

static int autocorrect_init(const struct device *dev) {
    return 0;
}

SYS_INIT(autocorrect_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
