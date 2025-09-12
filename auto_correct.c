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

// Working function to demonstrate typo detection and correction
static void process_autocorrect(void) {
    // Simulate processing input buffer for typos
    for (int i = 0; i < NUM_CORRECTIONS; i++) {
        const char* typo = corrections[i].typo;
        const char* correction = corrections[i].correction;
        
        int typo_len = strlen(typo);
        if (buffer_pos >= typo_len) {
            // Check if buffer ends with this typo
            if (strncmp(&input_buffer[buffer_pos - typo_len], typo, typo_len) == 0) {
                LOG_INF("Autocorrect: Found typo '%s' -> suggest '%s'", typo, correction);
                // In a real implementation, this would trigger a correction
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

// Demo function to show autocorrect functionality
K_WORK_DELAYABLE_DEFINE(autocorrect_demo_work, NULL);

static void autocorrect_demo_handler(struct k_work *work) {
    LOG_INF("Autocorrect Demo: Simulating typing 'teh quick brown fox'");
    
    // Simulate typing sequence
    const char* demo_text = "teh ";
    for (int i = 0; i < strlen(demo_text); i++) {
        add_char_to_buffer(demo_text[i]);
    }
    
    // Schedule next demo in 30 seconds
    k_work_reschedule(&autocorrect_demo_work, K_SECONDS(30));
}

static int zmk_autocorrect_init(const struct device *dev) {
    LOG_INF("ZMK Autocorrect Module Loaded!");
    LOG_INF("Initialized with %d correction patterns", NUM_CORRECTIONS);
    
    // Clear input buffer
    memset(input_buffer, 0, BUFFER_SIZE);
    buffer_pos = 0;
    
    // Initialize the demo work handler
    k_work_init_delayable(&autocorrect_demo_work, autocorrect_demo_handler);
    
    // Start demo in 10 seconds
    k_work_schedule(&autocorrect_demo_work, K_SECONDS(10));
    
    LOG_INF("Autocorrect module ready - demo will start in 10 seconds");
    return 0;
}

// Initialize the autocorrect module
SYS_INIT(zmk_autocorrect_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
