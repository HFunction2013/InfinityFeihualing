#include <iostream>
#include <fstream>
#include <cassert>
#include "game.hpp"
#include "poetry_db.hpp"
#include "idiom_db.hpp"
#include "save_system.hpp"
#include "utils.hpp"
#include "t2s.hpp"

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { tests_passed++; std::cout << "  [PASS] " << msg << std::endl; } \
    else { tests_failed++; std::cout << "  [FAIL] " << msg << std::endl; } \
} while(0)

int main() {
    std::cout << "=== FeihuaLing Test Suite ===" << std::endl;

    // 1. Load databases
    std::cout << "[1] Loading databases..." << std::endl;
    PoetryDB pdb;
    IdiomDB idb;
    bool pok = pdb.load("third_party/chinese-poetry");
    bool iok = idb.load("third_party/chinese-xinhua/data/idiom.json");
    std::cout << "    pok=" << pok << " iok=" << iok << std::endl;
    CHECK(pok, "Poetry DB loaded");
    CHECK(iok, "Idiom DB loaded");
    std::cout << "    Poems: " << pdb.poem_count() << ", Half-lines: " << pdb.half_line_count() << "\n";
    std::cout << "    Idioms: " << idb.count() << "\n";

    // 2. UTF-8 utils
    std::cout << "\n[2] UTF-8 utilities...\n";
    CHECK(utils::utf8_first_char("床前明月光") == "床", "utf8_first_char");
    CHECK(utils::utf8_last_char("床前明月光") == "光", "utf8_last_char");
    CHECK(utils::utf8_len("床前明月光") == 5, "utf8_len");

    // 3. T2S conversion
    std::cout << "\n[3] Traditional to simplified...\n";
    std::string simp = t2s::convert("床前明月光，疑是地上霜。");
    CHECK(simp.find("床") != std::string::npos, "t2s convert basic");
    // 國 -> 国
    CHECK(t2s::convert("國") == "国", "t2s 國->国");

    // 4. Poetry validation
    std::cout << "\n[4] Poetry validation...\n";
    Game game;
    game.poetry_db = &pdb;
    game.idiom_db = &idb;
    game.setup(GameMode::Feihualing, UsedGranularity::HalfLine);
    game.start();

    // Invalid input (not in DB)
    ValidateResult vr = game.validate("这不是一句诗xyz");
    CHECK(!vr.valid, "Reject non-poetry input");
    CHECK(!vr.reason.empty(), "Validation gives a reason");

    // 5. Idiom validation
    std::cout << "\n[5] Idiom validation...\n";
    Game game2;
    game2.poetry_db = &pdb;
    game2.idiom_db = &idb;
    game2.setup(GameMode::Chengyu, UsedGranularity::HalfLine);
    game2.start();

    ValidateResult vr2 = game2.validate("这不是成语");
    CHECK(!vr2.valid, "Reject non-idiom input");
    CHECK(!vr2.reason.empty(), "Validation gives a reason");

    // Valid idiom check
    CHECK(idb.has("一心一意"), "Idiom '一心一意' exists in DB");

    // 6. Bot game (auto-play several rounds)
    std::cout << "\n[6] Bot auto-play test...\n";
    Game botgame;
    botgame.poetry_db = &pdb;
    botgame.idiom_db = &idb;
    botgame.setup(GameMode::Chengyu, UsedGranularity::HalfLine);
    Player b1, b2;
    b1.type = PlayerType::Bot; b1.name = "Bot1";
    b1.bot_config.mode = BotMemoryMode::Percent; b1.bot_config.percent = 50;
    b2.type = PlayerType::Bot; b2.name = "Bot2";
    b2.bot_config.mode = BotMemoryMode::All;
    botgame.add_player(b1);
    botgame.add_player(b2);
    botgame.start();

    int moves = 0;
    for (int i = 0; i < 20 && !botgame.game_over; i++) {
        std::string m = botgame.bot_move();
        if (!m.empty()) {
            moves++;
            std::cout << "    " << botgame.history.back().player_name << ": " << m
                      << " -> 「" << botgame.current_char << "」\n";
        }
    }
    CHECK(moves > 0, "Bots made at least one move");
    std::cout << "    Total bot moves: " << moves << "\n";

    // 7. Save / Load with HMAC
    std::cout << "\n[7] Save/Load with anti-tamper...\n";
    std::string savepath = "saves/test_save.save";
    bool saved = SaveSystem::save(botgame, savepath);
    CHECK(saved, "Save succeeded");

    Game loaded;
    std::string err;
    bool loaded_ok = SaveSystem::load(savepath, &pdb, &idb, loaded, err);
    CHECK(loaded_ok, "Load succeeded (HMAC valid)");
    if (!loaded_ok) std::cout << "    Error: " << err << "\n";
    CHECK(loaded.players.size() == 2, "Loaded game has 2 players");

    // 8. Tamper detection
    std::cout << "\n[8] Tamper detection...\n";
    // Read save file, corrupt the HMAC, write back
    {
        std::ifstream f(savepath);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        // Tamper: replace the mac value with all zeros
        size_t pos = content.find("\"mac\"");
        if (pos != std::string::npos) {
            // Find the value after "mac": and replace it
            size_t colon = content.find(":", pos);
            if (colon != std::string::npos) {
                size_t quote1 = content.find("\"", colon);
                if (quote1 != std::string::npos) {
                    size_t quote2 = content.find("\"", quote1 + 1);
                    if (quote2 != std::string::npos) {
                        content.replace(quote1 + 1, quote2 - quote1 - 1, "0000000000000000000000000000000000000000000000000000000000000000");
                    }
                }
            }
        }
        std::ofstream of(savepath);
        of << content;
    }
    Game tampered;
    std::string err2;
    bool tampered_load = SaveSystem::load(savepath, &pdb, &idb, tampered, err2);
    CHECK(!tampered_load, "Tampered save rejected");
    CHECK(!err2.empty(), "Error message provided");
    std::cout << "    Tamper error: " << err2 << "\n";

    // Cleanup
    std::remove(savepath.c_str());

    // Summary
    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    return tests_failed > 0 ? 1 : 0;
}
