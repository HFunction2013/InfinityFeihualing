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
    CHECK(pok, "Poetry DB loaded");
    CHECK(iok, "Idiom DB loaded");
    bool pyok = idb.load_pinyin("third_party/chinese-xinhua/data/word.json");
    CHECK(pyok, "Pinyin data loaded");
    std::cout << "    Poems: " << pdb.poem_count() << ", Half-lines: " << pdb.half_line_count() << std::endl;
    std::cout << "    Idioms: " << idb.count() << std::endl;

    // 2. UTF-8 utils
    std::cout << "\n[2] UTF-8 utilities..." << std::endl;
    CHECK(utils::utf8_first_char("床前明月光") == "床", "utf8_first_char");
    CHECK(utils::utf8_last_char("床前明月光") == "光", "utf8_last_char");
    CHECK(utils::utf8_len("床前明月光") == 5, "utf8_len");

    // 3. T2S conversion
    std::cout << "\n[3] Traditional to simplified..." << std::endl;
    CHECK(t2s::convert("國") == "国", "t2s 國->国");

    // 4. 飞花令 (fixed keyword) validation
    std::cout << "\n[4] 飞花令 (fixed keyword) validation..." << std::endl;
    Game fhl;
    fhl.poetry_db = &pdb;
    fhl.idiom_db = &idb;
    fhl.setup(GameMode::Feihualing, UsedGranularity::HalfLine);
    fhl.keyword = "月";
    fhl.start();
    CHECK(fhl.keyword == "月", "Keyword set to 月");
    // Find a half-line containing 月
    auto mlines = pdb.half_lines_with_char("月");
    CHECK(!mlines.empty(), "DB has lines containing 月");
    if (!mlines.empty()) {
        ValidateResult vr = fhl.validate(mlines[0]);
        CHECK(vr.valid, "Accept poem containing keyword 月");
    }
    // Find a half-line NOT containing 月
    auto sunlines = pdb.half_lines_with_char("日");
    if (!sunlines.empty() && sunlines[0].find("月") == std::string::npos) {
        ValidateResult vr2 = fhl.validate(sunlines[0]);
        CHECK(!vr2.valid, "Reject poem NOT containing keyword 月");
        CHECK(vr2.reason.find("月") != std::string::npos, "Reason mentions keyword 月");
    }
    // Invalid input not in DB
    ValidateResult vr3 = fhl.validate("乱输入的句子xyz");
    CHECK(!vr3.valid, "Reject non-DB input in feihualing");

    // 5. 古诗接龙 (rolling char) validation
    std::cout << "\n[5] 古诗接龙 (rolling char) validation..." << std::endl;
    Game gsl;
    gsl.poetry_db = &pdb;
    gsl.idiom_db = &idb;
    gsl.setup(GameMode::GushiJielong, UsedGranularity::HalfLine);
    gsl.start();
    CHECK(!gsl.current_char.empty(), "古诗接龙 has starting char");
    std::cout << "    Starting char: 「" << gsl.current_char << "」" << std::endl;
    ValidateResult vr4 = gsl.validate("这不是诗");
    CHECK(!vr4.valid, "Reject non-poetry input in gushijielong");

    // 6. 成语接龙 validation
    std::cout << "\n[6] 成语接龙 validation..." << std::endl;
    Game cy;
    cy.poetry_db = &pdb;
    cy.idiom_db = &idb;
    cy.setup(GameMode::Chengyu, UsedGranularity::HalfLine);
    cy.start();
    ValidateResult vr5 = cy.validate("这不是成语");
    CHECK(!vr5.valid, "Reject non-idiom input");
    CHECK(idb.has("一心一意"), "Idiom '一心一意' exists in DB");

    // 6b. Pinyin utilities
    std::cout << "\n[6b] Pinyin utilities..." << std::endl;
    CHECK(idb.get_pinyin("花") == "huā", "get_pinyin(花)=huā");
    CHECK(idb.get_pinyin("化") == "huà", "get_pinyin(化)=huà");
    CHECK(IdiomDB::strip_tone("huā") == "hua", "strip_tone(huā)=hua");
    CHECK(IdiomDB::strip_tone("huà") == "hua", "strip_tone(huà)=hua");
    CHECK(IdiomDB::pinyin_equal("huā", "huā", true), "pinyin_equal strict huā=huā");
    CHECK(!IdiomDB::pinyin_equal("huā", "huà", true), "pinyin_equal strict huā≠huà");
    CHECK(IdiomDB::pinyin_equal("huā", "huà", false), "pinyin_equal nostrict huā=huà");

    // 6c. 成语接龙 - 拼音匹配(声调可不同)
    std::cout << "\n[6c] 成语接龙 pinyin (no tone)..." << std::endl;
    Game pyn;
    pyn.poetry_db = &pdb;
    pyn.idiom_db = &idb;
    pyn.setup(GameMode::Chengyu, UsedGranularity::HalfLine);
    pyn.pinyin_mode = PinyinMatch::PinyinNoTone;
    pyn.start();
    std::cout << "    Starting char: 「" << pyn.current_char << "」 ("
              << idb.get_pinyin(pyn.current_char) << ")" << std::endl;
    // Find an idiom starting with a char that has same pinyin (different tone) as current_char
    auto cands = pyn.get_candidates();
    CHECK(!cands.empty(), "PinyinNoTone has candidates");
    if (!cands.empty()) {
        std::string first = cands[0];
        std::string fc = IdiomDB::first_char(first);
        std::string fc_py = idb.get_pinyin(fc);
        std::string cur_py = idb.get_pinyin(pyn.current_char);
        CHECK(IdiomDB::pinyin_equal(fc_py, cur_py, false), "Candidate pinyin matches (no tone)");
        std::cout << "    Candidate: " << first << " (首字" << fc << " " << fc_py
                  << ", 目标" << cur_py << ")" << std::endl;
    }

    // 7. 飞花令 bot auto-play with fixed keyword
    std::cout << "\n[7] 飞花令 bot auto-play (keyword=花)..." << std::endl;
    Game botfhl;
    botfhl.poetry_db = &pdb;
    botfhl.idiom_db = &idb;
    botfhl.setup(GameMode::Feihualing, UsedGranularity::HalfLine);
    botfhl.keyword = "花";
    Player bf1, bf2;
    bf1.type = PlayerType::Bot; bf1.name = "BotA";
    bf1.bot_config.mode = BotMemoryMode::All;
    bf2.type = PlayerType::Bot; bf2.name = "BotB";
    bf2.bot_config.mode = BotMemoryMode::All;
    botfhl.add_player(bf1);
    botfhl.add_player(bf2);
    botfhl.start();
    int fhl_moves = 0;
    for (int i = 0; i < 15; i++) {
        std::string m = botfhl.bot_move();
        if (!m.empty()) {
            fhl_moves++;
            std::cout << "    " << botfhl.history.back().player_name << ": " << m << std::endl;
        }
    }
    CHECK(fhl_moves > 0, "飞花令 bots made moves with keyword 花");
    CHECK(botfhl.keyword == "花", "Keyword remains fixed after moves");
    std::cout << "    Total moves: " << fhl_moves << std::endl;

    // 8. 成语接龙 bot auto-play
    std::cout << "\n[8] 成语接龙 bot auto-play..." << std::endl;
    Game botcy;
    botcy.poetry_db = &pdb;
    botcy.idiom_db = &idb;
    botcy.setup(GameMode::Chengyu, UsedGranularity::HalfLine);
    Player bc1, bc2;
    bc1.type = PlayerType::Bot; bc1.name = "Bot1";
    bc1.bot_config.mode = BotMemoryMode::Percent; bc1.bot_config.percent = 50;
    bc2.type = PlayerType::Bot; bc2.name = "Bot2";
    bc2.bot_config.mode = BotMemoryMode::All;
    botcy.add_player(bc1);
    botcy.add_player(bc2);
    botcy.start();
    int cy_moves = 0;
    for (int i = 0; i < 15; i++) {
        std::string m = botcy.bot_move();
        if (!m.empty()) {
            cy_moves++;
            std::cout << "    " << botcy.history.back().player_name << ": " << m
                      << " -> 「" << botcy.current_char << "」" << std::endl;
        }
    }
    CHECK(cy_moves > 0, "成语接龙 bots made moves");
    std::cout << "    Total moves: " << cy_moves << std::endl;

    // 9. Save / Load with HMAC (飞花令 save includes keyword)
    std::cout << "\n[9] Save/Load with anti-tamper (keyword preserved)..." << std::endl;
    std::string savepath = "saves/test_save.save";
    bool saved = SaveSystem::save(botfhl, savepath);
    CHECK(saved, "Save succeeded");
    Game loaded;
    std::string err;
    bool loaded_ok = SaveSystem::load(savepath, &pdb, &idb, loaded, err);
    CHECK(loaded_ok, "Load succeeded (HMAC valid)");
    if (!loaded_ok) std::cout << "    Error: " << err << std::endl;
    CHECK(loaded.mode == GameMode::Feihualing, "Loaded game mode is 飞花令");
    CHECK(loaded.keyword == "花", "Loaded game keyword is 花");
    CHECK(loaded.players.size() == 2, "Loaded game has 2 players");

    // 10. Tamper detection
    std::cout << "\n[10] Tamper detection..." << std::endl;
    {
        std::ifstream f(savepath);
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t pos = content.find("\"mac\"");
        if (pos != std::string::npos) {
            size_t colon = content.find(":", pos);
            if (colon != std::string::npos) {
                size_t q1 = content.find("\"", colon);
                if (q1 != std::string::npos) {
                    size_t q2 = content.find("\"", q1 + 1);
                    if (q2 != std::string::npos) {
                        content.replace(q1 + 1, q2 - q1 - 1,
                            "0000000000000000000000000000000000000000000000000000000000000000");
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
    std::cout << "    Tamper error: " << err2 << std::endl;

    std::remove(savepath.c_str());

    // Summary
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Passed: " << tests_passed << std::endl;
    std::cout << "Failed: " << tests_failed << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
