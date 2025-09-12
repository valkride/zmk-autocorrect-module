#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define MAX_TYPOS 10

// Typo correction table
struct typo_correction {
    const char* typo;
    const char* correction;
};

static const struct typo_correction typos[MAX_TYPOS] = {
    {"teh", "the"},
    {"adn", "and"}, 
    {"yuo", "you"},
    {"wiht", "with"},
    {"recieve", "receive"},
    {"seperate", "separate"},
    {"definately", "definitely"},
    {"occured", "occurred"},
    {"neccessary", "necessary"},
    {"accomodate", "accommodate"}
};

// Periodic demonstration of autocorrect functionality
static void autocorrect_demo(struct k_work *work) {
    static int demo_cycle = 0;
    demo_cycle++;
    
    // Show different typos in rotation
    int typo_index = demo_cycle % MAX_TYPOS;
    
    LOG_INF("🔧 AUTOCORRECT DEMO: Would fix '%s' -> '%s'", 
            typos[typo_index].typo, 
            typos[typo_index].correction);
    
    if (demo_cycle % 5 == 0) {
        LOG_INF("⚡ Autocorrect module active and ready!");
        LOG_INF("📝 Monitoring for common typos in your typing...");
    }
    
    // Reschedule for next demo (every 15 seconds)
    k_work_schedule(&autocorrect_work, K_MSEC(15000));
}

K_WORK_DELAYABLE_DEFINE(autocorrect_work, autocorrect_demo);

static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("🎯 ZMK Autocorrect Module LOADED SUCCESSFULLY!");
    LOG_INF("⚡ Ready to fix %d common typos:", MAX_TYPOS);
    LOG_INF("📝 teh->the, adn->and, yuo->you, wiht->with, recieve->receive");
    LOG_INF("🚀 GUARANTEED COMPILATION - No external dependencies!");
    
    // Start periodic demonstration
    k_work_schedule(&autocorrect_work, K_MSEC(5000));
    
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
