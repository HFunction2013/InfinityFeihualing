#include "game.hpp"
#include "utils.hpp"
#include "t2s.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------

void Game::setup(GameMode m, UsedGranularity g) {
    mode = m;
    granularity = g;
    players.clear();
    current_player = 0;
    current_char.clear();
    game_over = false;
    winner.clear();
    history.clear();
    used_half_lines.clear();
    used_full_lines.clear();
    used_poem_ids.clear();
}

void Game::add_player(const Player& p) {
    players.push_back(p);
}

void Game::start() {
    // Build bot memories
    if (mode == GameMode::Feihualing && poetry_db) {
        for (auto& p : players) {
            if (p.is_bot()) p.build_memory(poetry_db->all_half_lines());
        }
    } else if (mode == GameMode::Chengyu && idiom_db) {
        for (auto& p : players) {
            if (p.is_bot()) p.build_memory(idiom_db->all_words());
        }
    }

    // Pick starting character
    if (mode == GameMode::Feihualing && poetry_db) {
        // Pick a random half-line and use its last char
        std::string hl = poetry_db->random_half_line();
        current_char = last_cjk_char(hl);
        // Record an implicit opening move by "system"
        MoveRecord rec;
        rec.player_name = "(开局)";
        rec.text = hl;
        rec.next_char = current_char;
        history.push_back(rec);
    } else if (mode == GameMode::Chengyu && idiom_db) {
        std::string id = idiom_db->random_idiom();
        current_char = IdiomDB::last_char(id);
        used_half_lines.insert(id);  // idioms use used_half_lines set
        MoveRecord rec;
        rec.player_name = "(开局)";
        rec.text = id;
        rec.next_char = current_char;
        history.push_back(rec);
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string Game::last_cjk_char(const std::string& text) {
    std::string simp = t2s::convert(text);
    auto chars = utils::utf8_split(simp);
    for (int i = (int)chars.size() - 1; i >= 0; i--) {
        if (utils::is_cjk_char(chars[i])) return chars[i];
    }
    return "";
}

std::string Game::find_full_line(int poem_idx, const std::string& half_line) const {
    if (!poetry_db || poem_idx < 0) return "";
    const Poem* p = poetry_db->get_poem(poem_idx);
    if (!p) return "";
    std::string norm_hl = PoetryDB::normalize(half_line);
    for (const auto& line : p->norm_lines) {
        if (line.find(norm_hl) != std::string::npos) return line;
    }
    return "";
}

bool Game::is_used_feihualing(const std::string& half_line, int poem_idx, const std::string& full_line) const {
    switch (granularity) {
        case UsedGranularity::HalfLine:
            return used_half_lines.count(half_line) > 0;
        case UsedGranularity::FullLine:
            return used_full_lines.count(full_line) > 0;
        case UsedGranularity::WholePoem:
            return used_poem_ids.count(poem_idx) > 0;
    }
    return false;
}

void Game::mark_used_feihualing(const std::string& half_line, int poem_idx, const std::string& full_line) {
    switch (granularity) {
        case UsedGranularity::HalfLine:
            used_half_lines.insert(half_line);
            break;
        case UsedGranularity::FullLine:
            if (!full_line.empty()) used_full_lines.insert(full_line);
            else used_half_lines.insert(half_line); // fallback
            break;
        case UsedGranularity::WholePoem:
            if (poem_idx >= 0) used_poem_ids.insert(poem_idx);
            break;
    }
}

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

ValidateResult Game::validate_feihualing(const std::string& text) const {
    ValidateResult r;
    std::string norm = PoetryDB::normalize(text);
    if (norm.empty()) { r.reason = "输入为空"; return r; }

    // Must contain the required character
    if (!current_char.empty() && norm.find(current_char) == std::string::npos) {
        r.reason = "诗句中必须包含字「" + current_char + "」";
        return r;
    }

    // Must exist in DB (try half-line first, then full line)
    int poem_idx = poetry_db ? poetry_db->find_half_line(norm) : -1;
    std::string matched_hl = norm;
    std::string matched_fl;

    if (poem_idx < 0) {
        // Try as full line
        poem_idx = poetry_db ? poetry_db->find_line(norm) : -1;
        if (poem_idx >= 0) {
            // It's a full line; split to half-lines and use the last one for the char chain
            matched_fl = norm;
            // Find which half-line contains the current_char, or use the last half-line
            auto halves = PoetryDB::split_half_lines(text);
            if (!halves.empty()) {
                // pick the half-line that contains current_char, or the last one
                matched_hl = halves.back();
                for (const auto& h : halves) {
                    if (h.find(current_char) != std::string::npos) {
                        matched_hl = h;
                        break;
                    }
                }
            }
        }
    } else {
        matched_fl = find_full_line(poem_idx, matched_hl);
    }

    if (poem_idx < 0) {
        r.reason = "该诗句不在诗库中，请输入库内的诗句";
        return r;
    }

    // Check used
    if (is_used_feihualing(matched_hl, poem_idx, matched_fl)) {
        switch (granularity) {
            case UsedGranularity::HalfLine: r.reason = "该半句已被使用过"; break;
            case UsedGranularity::FullLine: r.reason = "该整句已被使用过"; break;
            case UsedGranularity::WholePoem: r.reason = "该首诗已被使用过"; break;
        }
        return r;
    }

    r.valid = true;
    r.poem_idx = poem_idx;
    r.half_line = matched_hl;
    r.full_line = matched_fl;
    return r;
}

ValidateResult Game::validate_chengyu(const std::string& text) const {
    ValidateResult r;
    std::string norm = t2s::convert(utils::trim(text));
    if (norm.empty()) { r.reason = "输入为空"; return r; }

    // Must start with required character
    if (!current_char.empty()) {
        std::string fc = IdiomDB::first_char(norm);
        if (fc != current_char) {
            r.reason = "成语必须以「" + current_char + "」开头";
            return r;
        }
    }

    // Must exist in DB
    if (!idiom_db || !idiom_db->has(norm)) {
        r.reason = "该成语不在词库中，请输入库内的成语";
        return r;
    }

    // Check used
    if (used_half_lines.count(norm) > 0) {
        r.reason = "该成语已被使用过";
        return r;
    }

    r.valid = true;
    r.half_line = norm;
    return r;
}

ValidateResult Game::validate(const std::string& text) const {
    if (mode == GameMode::Feihualing) return validate_feihualing(text);
    return validate_chengyu(text);
}

// ---------------------------------------------------------------------------
// Move application
// ---------------------------------------------------------------------------

bool Game::apply_move(const std::string& text) {
    if (game_over) return false;
    ValidateResult r = validate(text);
    if (!r.valid) return false;

    std::string played = r.half_line;
    std::string next_char;

    if (mode == GameMode::Feihualing) {
        mark_used_feihualing(r.half_line, r.poem_idx, r.full_line);
        next_char = last_cjk_char(r.half_line);
    } else {
        used_half_lines.insert(r.half_line);
        next_char = IdiomDB::last_char(r.half_line);
    }

    current_char = next_char;
    current().score++;
    current().consecutive_skips = 0;

    MoveRecord rec;
    rec.player_name = current().name;
    rec.text = played;
    rec.next_char = next_char;
    rec.is_bot = current().is_bot();
    history.push_back(rec);

    next_player();
    return true;
}

void Game::next_player() {
    if (players.empty()) return;
    current_player = (current_player + 1) % (int)players.size();
}

// ---------------------------------------------------------------------------
// Bot
// ---------------------------------------------------------------------------

std::vector<std::string> Game::get_candidates() const {
    std::vector<std::string> result;
    if (current_char.empty()) return result;

    if (mode == GameMode::Feihualing && poetry_db) {
        auto hls = poetry_db->half_lines_with_char(current_char);
        for (const auto& hl : hls) {
            int pid = poetry_db->poem_of_half_line(hl);
            std::string fl = find_full_line(pid, hl);
            if (!is_used_feihualing(hl, pid, fl)) {
                result.push_back(hl);
            }
        }
    } else if (mode == GameMode::Chengyu && idiom_db) {
        auto ids = idiom_db->starting_with(current_char);
        for (const auto& id : ids) {
            if (used_half_lines.count(id) == 0) {
                result.push_back(id);
            }
        }
    }
    return result;
}

bool Game::bot_can_move() const {
    if (!current().is_bot()) return false;
    auto cands = get_candidates();
    for (const auto& c : cands) {
        if (current().knows(c)) return true;
    }
    return false;
}

std::string Game::bot_move() {
    if (!current().is_bot() || game_over) return "";
    auto cands = get_candidates();
    std::string move = current().pick_move(cands);
    if (!move.empty()) {
        apply_move(move);
    } else {
        // Bot stuck
        current().consecutive_skips++;
        MoveRecord rec;
        rec.player_name = current().name;
        rec.text = "(无法接招)";
        rec.next_char = current_char;
        rec.is_bot = true;
        history.push_back(rec);
        next_player();
    }
    return move;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

std::string Game::to_json() const {
    json j;
    j["version"] = 1;
    j["mode"] = (mode == GameMode::Feihualing) ? "feihualing" : "chengyu";
    j["granularity"] = (int)granularity;
    j["current_char"] = current_char;
    j["current_player"] = current_player;
    j["game_over"] = game_over;
    j["winner"] = winner;

    json players_arr = json::array();
    for (const auto& p : players) {
        json pj;
        pj["name"] = p.name;
        pj["score"] = p.score;
        pj["type"] = p.is_bot() ? "bot" : "human";
        pj["consecutive_skips"] = p.consecutive_skips;
        if (p.is_bot()) {
            json bc;
            bc["mode"] = (int)p.bot_config.mode;
            bc["count"] = p.bot_config.count;
            bc["percent"] = p.bot_config.percent;
            bc["seed"] = p.bot_config.seed;
            pj["bot_config"] = bc;
            // Save bot memory as sorted array
            json mem = json::array();
            for (const auto& e : p.memory) mem.push_back(e);
            pj["memory"] = mem;
        }
        players_arr.push_back(pj);
    }
    j["players"] = players_arr;

    json used_hl = json::array();
    for (const auto& s : used_half_lines) used_hl.push_back(s);
    j["used_half_lines"] = used_hl;

    json used_fl = json::array();
    for (const auto& s : used_full_lines) used_fl.push_back(s);
    j["used_full_lines"] = used_fl;

    json used_poems = json::array();
    for (int id : used_poem_ids) used_poems.push_back(id);
    j["used_poem_ids"] = used_poems;

    json hist = json::array();
    for (const auto& h : history) {
        json hj;
        hj["player"] = h.player_name;
        hj["text"] = h.text;
        hj["next_char"] = h.next_char;
        hj["is_bot"] = h.is_bot;
        hist.push_back(hj);
    }
    j["history"] = hist;

    return j.dump(2);
}

Game Game::from_json(const std::string& json_str, PoetryDB* pdb, IdiomDB* idb) {
    Game g;
    g.poetry_db = pdb;
    g.idiom_db = idb;
    json j = json::parse(json_str);

    std::string m = j.value("mode", "feihualing");
    g.mode = (m == "chengyu") ? GameMode::Chengyu : GameMode::Feihualing;
    g.granularity = (UsedGranularity)j.value("granularity", (int)UsedGranularity::HalfLine);
    g.current_char = j.value("current_char", "");
    g.current_player = j.value("current_player", 0);
    g.game_over = j.value("game_over", false);
    g.winner = j.value("winner", "");

    for (const auto& pj : j["players"]) {
        Player p;
        p.name = pj.value("name", "");
        p.score = pj.value("score", 0);
        p.consecutive_skips = pj.value("consecutive_skips", 0);
        std::string pt = pj.value("type", "human");
        p.type = (pt == "bot") ? PlayerType::Bot : PlayerType::Human;
        if (p.is_bot() && pj.contains("bot_config")) {
            const auto& bc = pj["bot_config"];
            p.bot_config.mode = (BotMemoryMode)bc.value("mode", (int)BotMemoryMode::Percent);
            p.bot_config.count = bc.value("count", 500);
            p.bot_config.percent = bc.value("percent", 10.0);
            p.bot_config.seed = bc.value("seed", (uint32_t)0);
        }
        if (p.is_bot() && pj.contains("memory")) {
            for (const auto& e : pj["memory"]) p.memory.insert(e.get<std::string>());
        }
        g.players.push_back(p);
    }

    if (j.contains("used_half_lines"))
        for (const auto& s : j["used_half_lines"]) g.used_half_lines.insert(s.get<std::string>());
    if (j.contains("used_full_lines"))
        for (const auto& s : j["used_full_lines"]) g.used_full_lines.insert(s.get<std::string>());
    if (j.contains("used_poem_ids"))
        for (const auto& id : j["used_poem_ids"]) g.used_poem_ids.insert(id.get<int>());

    if (j.contains("history")) {
        for (const auto& hj : j["history"]) {
            MoveRecord h;
            h.player_name = hj.value("player", "");
            h.text = hj.value("text", "");
            h.next_char = hj.value("next_char", "");
            h.is_bot = hj.value("is_bot", false);
            g.history.push_back(h);
        }
    }
    return g;
}
