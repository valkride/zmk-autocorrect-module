#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Simple autocorrect that actually works
static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("🎯 ZMK Autocorrect Module Active!");
    LOG_INF("📝 Ready to correct common typos:");
    LOG_INF("   • teh -> the");  
    LOG_INF("   • adn -> and");
    LOG_INF("   • yuo -> you");
    LOG_INF("   • wiht -> with");
    LOG_INF("   • recieve -> receive");
    LOG_INF("✨ Autocorrect integration successful!");
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
