#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// Typo correction table
struct typo_correction {
    const char* typo;
    const char* correction;
};

static const struct typo_correction corrections[] = {
    {"teh", "the"},
    {"adn", "and"}, 
    {"yuo", "you"},
    {"hte", "the"},
    {"taht", "that"},
    {"thsi", "this"},
    {"seperate", "separate"},
    {"recieve", "receive"},
    {"definately", "definitely"},
    {"occured", "occurred"}
};

#define NUM_CORRECTIONS (sizeof(corrections) / sizeof(corrections[0]))

// Buffer to store recent keystrokes for autocorrect processing
#define BUFFER_SIZE 32
static char input_buffer[BUFFER_SIZE];
static int buffer_pos = 0;

// Autocorrect action: Show exactly what would be corrected
static void perform_autocorrect_action(const char* typo, const char* correction) {
    int typo_len = strlen(typo);
    int correction_len = strlen(correction);
    
    LOG_INF("🎯 AUTOCORRECT TRIGGERED!");
    LOG_INF("   📝 Detected typo: '%s'", typo);
    LOG_INF("   🔧 Would send: %d backspaces", typo_len);
    LOG_INF("   ✍️  Would type: '%s'", correction);
    LOG_INF("   💡 Net effect: '%s' -> '%s'", typo, correction);
    
    // Update buffer to simulate the correction
    int correction_diff = correction_len - typo_len;
    buffer_pos = buffer_pos - typo_len + correction_len;
    if (buffer_pos < BUFFER_SIZE) {
        strncpy(&input_buffer[buffer_pos - correction_len], correction, correction_len);
    }
    
    LOG_INF("✅ Autocorrect simulation complete!");
}

// Typo detection and correction trigger
static void process_autocorrect(void) {
    for (int i = 0; i < NUM_CORRECTIONS; i++) {
        const char* typo = corrections[i].typo;
        const char* correction = corrections[i].correction;
        
        int typo_len = strlen(typo);
        if (buffer_pos >= typo_len) {
            // Check if buffer ends with this typo
            if (strncmp(&input_buffer[buffer_pos - typo_len], typo, typo_len) == 0) {
                perform_autocorrect_action(typo, correction);
                break;
            }
        }
    }
}

// Simulate adding a character to the buffer (would be called by key handler)
static void add_char_to_buffer(char c) {
    if (buffer_pos < BUFFER_SIZE - 1) {
        input_buffer[buffer_pos++] = c;
        input_buffer[buffer_pos] = '\0';
        
        // Process autocorrect on word boundaries (space, enter, etc.)
        if (c == ' ' || c == '\n' || c == '\t') {
            process_autocorrect();
        }
    } else {
        // Buffer full, shift left and add new character
        memmove(input_buffer, input_buffer + 1, BUFFER_SIZE - 2);
        input_buffer[BUFFER_SIZE - 2] = c;
        input_buffer[BUFFER_SIZE - 1] = '\0';
        buffer_pos = BUFFER_SIZE - 1;
    }
}

// Demo function showing autocorrect detection and correction logic
K_WORK_DELAYABLE_DEFINE(autocorrect_demo_work, NULL);

static void autocorrect_demo_handler(struct k_work *work) {
    LOG_INF("🚀 Autocorrect Demo: Simulating typing common typos...");
    
    // Demo different typos
    const char* demo_typos[] = {"teh ", "adn ", "yuo ", "taht "};
    int num_demos = sizeof(demo_typos) / sizeof(demo_typos[0]);
    static int demo_index = 0;
    
    const char* current_demo = demo_typos[demo_index % num_demos];
    LOG_INF("📝 Demo typing: '%s'", current_demo);
    
    // Clear buffer for clean demo
    memset(input_buffer, 0, BUFFER_SIZE);
    buffer_pos = 0;
    
    // Simulate typing the demo text
    for (int i = 0; i < strlen(current_demo); i++) {
        add_char_to_buffer(current_demo[i]);
    }
    
    demo_index++;
    
    // Schedule next demo in 30 seconds
    k_work_reschedule(&autocorrect_demo_work, K_SECONDS(30));
}

static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("🎯 ZMK Autocorrect Detection Engine Loaded!");
    LOG_INF("� DETECTS typos and shows correction actions");
    LOG_INF("✨ Initialized with %d correction patterns", NUM_CORRECTIONS);
    LOG_INF("💡 Framework ready - shows exactly what would be corrected");
    LOG_INF("🚧 Note: External modules have limited ZMK API access");
    LOG_INF("📋 This demonstrates the detection + correction logic");
    
    // Clear input buffer
    memset(input_buffer, 0, BUFFER_SIZE);
    buffer_pos = 0;
    
    // Initialize the demo work handler
    k_work_init_delayable(&autocorrect_demo_work, autocorrect_demo_handler);
    
    // Start demo in 15 seconds
    k_work_schedule(&autocorrect_demo_work, K_SECONDS(15));
    
    LOG_INF("🚀 Autocorrect demo ready - will trigger in 15 seconds!");
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
