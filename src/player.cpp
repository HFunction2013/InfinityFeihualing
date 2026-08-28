#include "player.hpp"
#include "utils.hpp"
#include <algorithm>
#include <random>

std::string BotConfig::describe() const {
    switch (mode) {
        case BotMemoryMode::All: return "全部词库";
        case BotMemoryMode::Count: return "随机" + std::to_string(count) + "条";
        case BotMemoryMode::Percent: return std::to_string(percent) + "%";
    }
    return "未知";
}

void Player::build_memory(const std::vector<std::string>& all_entries) {
    memory.clear();
    if (type != PlayerType::Bot) return;

    size_t target = 0;
    switch (bot_config.mode) {
        case BotMemoryMode::All:
            target = all_entries.size();
            break;
        case BotMemoryMode::Count:
            target = std::min((size_t)bot_config.count, all_entries.size());
            break;
        case BotMemoryMode::Percent:
            target = (size_t)(all_entries.size() * bot_config.percent / 100.0);
            break;
    }
    if (target == 0) target = 1;
    if (target >= all_entries.size()) {
        for (const auto& e : all_entries) memory.insert(e);
        return;
    }

    // Fisher-Yates partial shuffle to pick target distinct entries
    std::vector<size_t> indices(all_entries.size());
    for (size_t i = 0; i < indices.size(); i++) indices[i] = i;

    std::mt19937 rng(bot_config.seed ? bot_config.seed : (uint32_t)rand());
    for (size_t i = 0; i < target; i++) {
        size_t j = i + (rng() % (indices.size() - i));
        std::swap(indices[i], indices[j]);
        memory.insert(all_entries[indices[i]]);
    }
}

std::string Player::pick_move(const std::vector<std::string>& candidates) const {
    if (candidates.empty()) return "";
    // Filter to memory
    std::vector<std::string> valid;
    for (const auto& c : candidates) {
        if (memory.count(c)) valid.push_back(c);
    }
    if (valid.empty()) return "";
    return valid[utils::rand_int((int)valid.size())];
}
