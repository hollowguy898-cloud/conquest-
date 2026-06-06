// ============================================================================
// CONQUEST - Headless Training Entry Point
// Run self-play games for ML model training without SDL2/OpenGL.
//
// Usage:
//   conquest_train [options]
//
// Options:
//   --episodes N         Number of episodes to play (default: 100)
//   --depth N            AI search depth (default: 3)
//   --time-limit N       AI time limit per move in ms (default: 1000)
//   --temperature F      Policy temperature (default: 1.0)
//   --no-priors          Use uniform policy instead of AI priors
//   --max-turns N        Maximum turns per episode (default: 200)
//   --seed N             Starting map seed (default: 1)
//   --output-dir DIR     Output directory (default: ./training_data)
//   --single-file        Write all data to one file
//   --output-file FILE   Single output filename (default: training_data.bin)
//   --verbose            Print per-episode details
//   --no-dedup           Disable position deduplication
//   --random-draft       Randomize tech draft (default: true)
//   --sequential-draft   Pick techs sequentially instead of randomly
//   --help               Show this help message
// ============================================================================

#include "core.h"
#include "trainer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void print_usage() {
    std::printf("CONQUEST Training Mode - Headless Self-Play\n");
    std::printf("============================================\n\n");
    std::printf("Usage: conquest_train [options]\n\n");
    std::printf("Options:\n");
    std::printf("  --episodes N         Number of episodes to play (default: 100)\n");
    std::printf("  --depth N            AI search depth (default: 3)\n");
    std::printf("  --time-limit N       AI time limit per move in ms (default: 1000)\n");
    std::printf("  --temperature F      Policy temperature (default: 1.0)\n");
    std::printf("  --no-priors          Use uniform policy instead of AI priors\n");
    std::printf("  --max-turns N        Maximum turns per episode (default: 200)\n");
    std::printf("  --seed N             Starting map seed (default: 1)\n");
    std::printf("  --output-dir DIR     Output directory (default: ./training_data)\n");
    std::printf("  --single-file        Write all data to one file\n");
    std::printf("  --output-file FILE   Single output filename (default: training_data.bin)\n");
    std::printf("  --verbose            Print per-episode details\n");
    std::printf("  --no-dedup           Disable position deduplication\n");
    std::printf("  --random-draft       Randomize tech draft (default: true)\n");
    std::printf("  --sequential-draft   Pick techs sequentially instead of randomly\n");
    std::printf("  --help               Show this help message\n\n");
    std::printf("Output format:\n");
    std::printf("  Binary files with (state, policy, value) tuples.\n");
    std::printf("  Use tools/train_loader.py to load data in Python.\n\n");
    std::printf("Feature size: %d floats (28 channels × 127 hexes + 12 global)\n", FEATURE_SIZE);
    std::printf("Policy size:  %d floats\n", POLICY_SIZE);
}

int main(int argc, char* argv[]) {
    TrainingConfig config;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (std::strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) {
            config.num_episodes = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--depth") == 0 && i + 1 < argc) {
            config.ai_depth = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--time-limit") == 0 && i + 1 < argc) {
            config.ai_time_limit_ms = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--temperature") == 0 && i + 1 < argc) {
            config.temperature = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--no-priors") == 0) {
            config.use_priors = false;
        } else if (std::strcmp(argv[i], "--max-turns") == 0 && i + 1 < argc) {
            config.max_turns = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            config.seed_start = (uint32_t)std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            std::strncpy(config.output_dir, argv[++i], sizeof(config.output_dir) - 1);
        } else if (std::strcmp(argv[i], "--single-file") == 0) {
            config.single_file = true;
        } else if (std::strcmp(argv[i], "--output-file") == 0 && i + 1 < argc) {
            std::strncpy(config.output_file, argv[++i], sizeof(config.output_file) - 1);
        } else if (std::strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
        } else if (std::strcmp(argv[i], "--no-dedup") == 0) {
            config.deduplicate = false;
        } else if (std::strcmp(argv[i], "--random-draft") == 0) {
            config.random_draft = true;
        } else if (std::strcmp(argv[i], "--sequential-draft") == 0) {
            config.random_draft = false;
        } else {
            std::fprintf(stderr, "Unknown option: %s\nUse --help for usage.\n", argv[i]);
            return 1;
        }
    }

    // Validate
    if (config.num_episodes <= 0) {
        std::fprintf(stderr, "Error: --episodes must be > 0\n");
        return 1;
    }
    if (config.temperature <= 0.0f) {
        std::fprintf(stderr, "Error: --temperature must be > 0\n");
        return 1;
    }

    // Print config
    std::printf("=== CONQUEST Training Mode ===\n");
    std::printf("Episodes:      %d\n", config.num_episodes);
    std::printf("AI depth:      %d\n", config.ai_depth);
    std::printf("AI time limit: %d ms\n", config.ai_time_limit_ms);
    std::printf("Temperature:   %.2f\n", config.temperature);
    std::printf("Use priors:    %s\n", config.use_priors ? "yes" : "no");
    std::printf("Max turns:     %d\n", config.max_turns);
    std::printf("Start seed:    %u\n", config.seed_start);
    std::printf("Output dir:    %s\n", config.output_dir);
    std::printf("Single file:   %s\n", config.single_file ? "yes" : "no");
    std::printf("Deduplicate:   %s\n", config.deduplicate ? "yes" : "no");
    std::printf("Random draft:  %s\n", config.random_draft ? "yes" : "no");
    std::printf("Feature size:  %d\n", FEATURE_SIZE);
    std::printf("Policy size:   %d\n", POLICY_SIZE);
    std::printf("==============================\n\n");

    // Create output directory if needed
    // (simple system() call — safe in training context)
    char mkdir_cmd[512];
    std::snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", config.output_dir);
    std::system(mkdir_cmd);

    // Run training
    Trainer trainer;
    TrainingStats stats = trainer.run(config);

    // Print final stats
    stats.print();

    return 0;
}
