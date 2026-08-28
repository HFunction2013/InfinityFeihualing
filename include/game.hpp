#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "player.hpp"
#include "poetry_db.hpp"
#include "idiom_db.hpp"

enum class GameMode { Feihualing, Chengyu };

enum class UsedGranularity { HalfLine, FullLine, WholePoem };  // only for Feihualing

struct MoveRecord {
    std::string player_name;
    std::string text;       // the half-line or idiom played
    std::string next_char;  // the char required after this move
    bool is_bot = false;
};

struct ValidateResult {
    bool valid = false;
    std::string reason;     // human-readable reason if invalid
    int poem_idx = -1;      // for poetry: matched poem index
    std::string half_line;  // normalized half-line / idiom
    std::string full_line;  // for poetry: normalized full line (if applicable)
};

class Game {
public:
    GameMode mode = GameMode::Feihualing;
    UsedGranularity granularity = UsedGranularity::HalfLine;
    std::vector<Player> players;
    int current_player = 0;
    std::string current_char;   // required char for next move
    bool game_over = false;
    std::string winner;
    std::vector<MoveRecord> history;

    // Used tracking
    std::unordered_set<std::string> used_half_lines;  // half-line granularity + chengyu
    std::unordered_set<std::string> used_full_lines;  // full-line granularity
    std::unordered_set<int> used_poem_ids;            // whole-poem granularity (using poem_idx as id)

    // External DB references (not owned)
    PoetryDB* poetry_db = nullptr;
    IdiomDB* idiom_db = nullptr;

    void setup(GameMode m, UsedGranularity g);
    void add_player(const Player& p);
    void start();  // pick starting char, build bot memories

    // Validate a player's input text. Returns result with details.
    ValidateResult validate(const std::string& text) const;

    // Apply a validated move. Returns false if invalid.
    bool apply_move(const std::string& text);

    // Advance to next player
    void next_player();

    // Get valid candidate moves for the current state (for bot / hint)
    std::vector<std::string> get_candidates() const;

    // Check if current player (bot) can move
    bool bot_can_move() const;

    // Bot makes a move automatically; returns the move text or empty if stuck
    std::string bot_move();

    // Current player reference
    Player& current() { return players[current_player]; }
    const Player& current() const { return players[current_player]; }

    // Serialization for save/load
    std::string to_json() const;
    static Game from_json(const std::string& json_str, PoetryDB* pdb, IdiomDB* idb);

    // Helper: extract last CJK char from text
    static std::string last_cjk_char(const std::string& text);

private:
    ValidateResult validate_feihualing(const std::string& text) const;
    ValidateResult validate_chengyu(const std::string& text) const;
    bool is_used_feihualing(const std::string& half_line, int poem_idx, const std::string& full_line) const;
    void mark_used_feihualing(const std::string& half_line, int poem_idx, const std::string& full_line);
    std::string find_full_line(int poem_idx, const std::string& half_line) const;
};
