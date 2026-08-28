#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdint>

enum class PlayerType { Human, Bot };

enum class BotMemoryMode { All, Count, Percent };

struct BotConfig {
    BotMemoryMode mode = BotMemoryMode::Percent;
    int count = 500;        // for Count mode: number of entries to memorize
    double percent = 10.0;  // for Percent mode: 0.0 - 100.0
    uint32_t seed = 0;      // RNG seed for memory sampling; 0 = random
    std::string describe() const;
};

class Player {
public:
    std::string name;
    int score = 0;
    int consecutive_skips = 0; // times failed to move
    PlayerType type = PlayerType::Human;
    BotConfig bot_config;

    // Bot memory: set of normalized entries (half-lines or idioms) the bot knows
    std::unordered_set<std::string> memory;

    bool is_bot() const { return type == PlayerType::Bot; }

    // Sample memory from all available entries based on bot_config
    void build_memory(const std::vector<std::string>& all_entries);

    // From candidates (valid, unused, in-memory), pick one.
    // Returns empty if no candidates.
    std::string pick_move(const std::vector<std::string>& candidates) const;

    // Check if bot knows an entry
    bool knows(const std::string& entry) const { return memory.count(entry) > 0; }
};
