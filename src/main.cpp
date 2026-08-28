#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "game.hpp"
#include "poetry_db.hpp"
#include "idiom_db.hpp"
#include "save_system.hpp"
#include "utils.hpp"
#include "t2s.hpp"

// ---------------------------------------------------------------------------
// Global databases
// ---------------------------------------------------------------------------
static PoetryDB g_poetry_db;
static IdiomDB g_idiom_db;
static std::string g_data_root = "third_party";

// ---------------------------------------------------------------------------
// Terminal helpers
// ---------------------------------------------------------------------------
static void clear_screen() {
#if defined(_WIN32)
    system("cls");
#else
    system("clear");
#endif
}

static std::string input_line(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return utils::trim(line);
}

static int input_int(const std::string& prompt, int min_val, int max_val, int default_val) {
    while (true) {
        std::string s = input_line(prompt + " [" + std::to_string(min_val) + "-" +
                                   std::to_string(max_val) + "] (默认" + std::to_string(default_val) + "): ");
        if (s.empty()) return default_val;
        try {
            int v = std::stoi(s);
            if (v >= min_val && v <= max_val) return v;
        } catch (...) {}
        std::cout << "  输入无效，请重新输入。\n";
    }
}

static void pause() {
    std::cout << "\n按回车键继续...";
    std::cin.get();
}

static void print_separator() {
    std::cout << "========================================\n";
}

// ---------------------------------------------------------------------------
// Data loading
// ---------------------------------------------------------------------------
static bool load_databases() {
    std::cout << "正在加载诗词库...\n";
    std::string poetry_root = g_data_root + "/chinese-poetry";
    if (!g_poetry_db.load(poetry_root)) {
        std::cout << "警告: 诗词库加载失败，路径: " << poetry_root << "\n";
        std::cout << "请确保已执行 git submodule update --init\n";
        return false;
    }
    std::cout << "  诗词: " << g_poetry_db.poem_count() << " 首, "
              << g_poetry_db.half_line_count() << " 半句\n";

    std::cout << "正在加载成语库...\n";
    std::string idiom_path = g_data_root + "/chinese-xinhua/data/idiom.json";
    if (!g_idiom_db.load(idiom_path)) {
        std::cout << "警告: 成语库加载失败，路径: " << idiom_path << "\n";
        return false;
    }
    std::cout << "  成语: " << g_idiom_db.count() << " 条\n";
    return true;
}

// ---------------------------------------------------------------------------
// Game setup
// ---------------------------------------------------------------------------
static GameMode select_mode() {
    clear_screen();
    print_separator();
    std::cout << "  选择游戏模式\n";
    print_separator();
    std::cout << "  1. 无限飞花令 (诗词接龙)\n";
    std::cout << "  2. 成语接龙\n";
    std::cout << "  0. 返回\n";
    print_separator();
    while (true) {
        int c = input_int("请选择", 0, 2, 1);
        if (c == 1) return GameMode::Feihualing;
        if (c == 2) return GameMode::Chengyu;
        if (c == 0) return (GameMode)-1;
    }
}

static UsedGranularity select_granularity() {
    clear_screen();
    print_separator();
    std::cout << "  选择「用过」的粒度\n";
    print_separator();
    std::cout << "  1. 半句不能用 (最宽松)\n";
    std::cout << "  2. 整一句不能用\n";
    std::cout << "  3. 整一首不能用 (最严格)\n";
    print_separator();
    int c = input_int("请选择", 1, 3, 1);
    switch (c) {
        case 1: return UsedGranularity::HalfLine;
        case 2: return UsedGranularity::FullLine;
        case 3: return UsedGranularity::WholePoem;
    }
    return UsedGranularity::HalfLine;
}

static BotConfig configure_bot(const std::string& name) {
    BotConfig cfg;
    clear_screen();
    print_separator();
    std::cout << "  配置机器人: " << name << "\n";
    print_separator();
    std::cout << "  记忆模式:\n";
    std::cout << "  1. 记住全部\n";
    std::cout << "  2. 随机记住 N 条\n";
    std::cout << "  3. 记住百分比 %%\n";
    int mode = input_int("请选择", 1, 3, 3);
    switch (mode) {
        case 1: cfg.mode = BotMemoryMode::All; break;
        case 2:
            cfg.mode = BotMemoryMode::Count;
            cfg.count = input_int("记住多少条", 1, 100000, 500);
            break;
        case 3:
            cfg.mode = BotMemoryMode::Percent;
            cfg.percent = input_int("记住百分比 (1-100)", 1, 100, 10);
            break;
    }
    cfg.seed = (uint32_t)time(nullptr) + rand();
    return cfg;
}

static void setup_players(Game& game) {
    clear_screen();
    print_separator();
    std::cout << "  添加玩家\n";
    print_separator();
    std::cout << "  至少需要 1 名玩家。可以添加多名人类玩家和机器人。\n\n";

    int human_count = input_int("人类玩家数量", 1, 8, 1);
    for (int i = 0; i < human_count; i++) {
        Player p;
        p.type = PlayerType::Human;
        p.name = input_line("玩家 " + std::to_string(i + 1) + " 名字: ");
        if (p.name.empty()) p.name = "玩家" + std::to_string(i + 1);
        game.add_player(p);
    }

    int bot_count = input_int("机器人数量", 0, 8, 1);
    for (int i = 0; i < bot_count; i++) {
        Player p;
        p.type = PlayerType::Bot;
        p.name = "机器人" + std::to_string(i + 1);
        p.bot_config = configure_bot(p.name);
        game.add_player(p);
        std::cout << "  " << p.name << " 已添加 (记忆: " << p.bot_config.describe() << ")\n";
    }
}

// ---------------------------------------------------------------------------
// Game loop
// ---------------------------------------------------------------------------
static void print_game_state(const Game& game) {
    print_separator();
    std::string mode_name = (game.mode == GameMode::Feihualing) ? "无限飞花令" : "成语接龙";
    std::cout << "  " << mode_name;
    if (game.mode == GameMode::Feihualing) {
        std::string gname;
        switch (game.granularity) {
            case UsedGranularity::HalfLine: gname = "半句不可复用"; break;
            case UsedGranularity::FullLine: gname = "整句不可复用"; break;
            case UsedGranularity::WholePoem: gname = "整首不可复用"; break;
        }
        std::cout << " [" << gname << "]";
    }
    std::cout << "\n";
    print_separator();

    // Scores
    std::cout << "  分数: ";
    for (size_t i = 0; i < game.players.size(); i++) {
        if (i > 0) std::cout << " | ";
        std::string marker = (i == (size_t)game.current_player) ? ">>> " : "";
        std::cout << marker << game.players[i].name << ": " << game.players[i].score;
        if (game.players[i].is_bot()) std::cout << "(Bot)";
    }
    std::cout << "\n";

    // Current required char
    if (!game.current_char.empty()) {
        std::cout << "  当前需要的字: 「" << game.current_char << "」\n";
    }

    // Last few moves
    if (!game.history.empty()) {
        std::cout << "  最近接龙:\n";
        size_t start = (game.history.size() > 5) ? game.history.size() - 5 : 0;
        for (size_t i = start; i < game.history.size(); i++) {
            const auto& h = game.history[i];
            std::cout << "    " << h.player_name << ": " << h.text;
            if (!h.next_char.empty()) std::cout << "  → 「" << h.next_char << "」";
            std::cout << "\n";
        }
    }
    print_separator();
}

static void game_loop(Game& game) {
    while (!game.game_over) {
        clear_screen();
        print_game_state(game);

        if (game.current().is_bot()) {
            std::cout << "\n  " << game.current().name << " 正在思考...\n";
            std::string move = game.bot_move();
            if (move.empty()) {
                std::cout << "  " << game.current().name << " 接不上来！跳过回合。\n";
            } else {
                std::cout << "  " << game.current().name << " 出招: " << move << "\n";
            }
            pause();
            continue;
        }

        // Human turn
        std::cout << "\n  轮到 " << game.current().name << "\n";
        std::cout << "  输入你的" << (game.mode == GameMode::Feihualing ? "诗句" : "成语")
                  << " (输入 /save 存档, /hint 提示, /quit 退出):\n  > ";
        std::string input;
        std::getline(std::cin, input);
        input = utils::trim(input);

        if (input.empty()) continue;

        if (input == "/quit" || input == "/q") {
            std::string confirm = input_line("确定退出当前对局？(y/n): ");
            if (confirm == "y" || confirm == "Y") {
                game.game_over = true;
                game.winner = "(中途退出)";
                break;
            }
            continue;
        }

        if (input == "/save" || input == "/s") {
            std::string fname = input_line("存档文件名 (默认 game.save): ");
            if (fname.empty()) fname = "game.save";
            if (fname.find(".save") == std::string::npos) fname += ".save";
            std::string path = "saves/" + fname;
            if (SaveSystem::save(game, path)) {
                std::cout << "  已保存到 " << path << "\n";
            } else {
                std::cout << "  保存失败！\n";
            }
            pause();
            continue;
        }

        if (input == "/hint" || input == "/h") {
            auto cands = game.get_candidates();
            // Filter to a reasonable number
            size_t show = std::min(cands.size(), (size_t)10);
            if (cands.empty()) {
                std::cout << "  没有可用的接龙选项了...\n";
            } else {
                std::cout << "  可用选项 (共" << cands.size() << "条，显示前" << show << "条):\n";
                for (size_t i = 0; i < show; i++) {
                    std::cout << "    " << cands[i] << "\n";
                }
            }
            pause();
            continue;
        }

        // Validate and apply
        ValidateResult vr = game.validate(input);
        if (vr.valid) {
            game.apply_move(input);
            std::cout << "  有效！下一个字: 「" << game.current_char << "」\n";
            // Small pause for feedback
            pause();
        } else {
            std::cout << "  无效: " << vr.reason << "\n";
            pause();
        }
    }

    // Game over
    clear_screen();
    print_separator();
    std::cout << "  游戏结束\n";
    print_separator();
    std::cout << "  最终分数:\n";
    for (const auto& p : game.players) {
        std::cout << "    " << p.name << ": " << p.score;
        if (p.is_bot()) std::cout << " (Bot)";
        std::cout << "\n";
    }
    // Find winner
    int max_score = -1;
    std::string winner_name;
    for (const auto& p : game.players) {
        if (p.score > max_score) { max_score = p.score; winner_name = p.name; }
    }
    std::cout << "\n  冠军: " << winner_name << " (" << max_score << "分)\n";
    print_separator();
    pause();
}

// ---------------------------------------------------------------------------
// Load game menu
// ---------------------------------------------------------------------------
static void load_game_menu() {
    auto saves = SaveSystem::list_saves("saves");
    if (saves.empty()) {
        std::cout << "  没有找到存档文件。\n";
        pause();
        return;
    }
    clear_screen();
    print_separator();
    std::cout << "  选择存档\n";
    print_separator();
    for (size_t i = 0; i < saves.size(); i++) {
        std::cout << "  " << (i + 1) << ". " << saves[i] << "\n";
    }
    std::cout << "  0. 返回\n";
    print_separator();
    int idx = input_int("选择存档", 0, (int)saves.size(), 1) - 1;
    if (idx < 0) return;

    Game game;
    std::string error;
    if (SaveSystem::load(saves[idx], &g_poetry_db, &g_idiom_db, game, error)) {
        std::cout << "  存档加载成功！\n";
        pause();
        game_loop(game);
    } else {
        std::cout << "  加载失败: " << error << "\n";
        pause();
    }
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------
static void main_menu() {
    while (true) {
        clear_screen();
        print_separator();
        std::cout << "  飞花令 & 成语接龙\n";
        std::cout << "  终端版 v1.0\n";
        print_separator();
        std::cout << "  1. 新游戏\n";
        std::cout << "  2. 读取存档\n";
        std::cout << "  3. 关于\n";
        std::cout << "  0. 退出\n";
        print_separator();
        int choice = input_int("请选择", 0, 3, 1);

        if (choice == 0) {
            std::cout << "再见！\n";
            break;
        }

        if (choice == 3) {
            clear_screen();
            print_separator();
            std::cout << "  关于\n";
            print_separator();
            std::cout << "  基于 chinese-poetry 和 chinese-xinhua 数据库\n";
            std::cout << "  支持无限飞花令和成语接龙\n";
            std::cout << "  多玩家 / 多机器人 / 存档防篡改\n";
            std::cout << "  数据来源:\n";
            std::cout << "    - github.com/chinese-poetry/chinese-poetry\n";
            std::cout << "    - github.com/pwxcoo/chinese-xinhua\n";
            std::cout << "  繁简转换: OpenCC (Apache-2.0)\n";
            print_separator();
            pause();
            continue;
        }

        if (choice == 2) {
            load_game_menu();
            continue;
        }

        if (choice == 1) {
            GameMode mode = select_mode();
            if ((int)mode == -1) continue;

            UsedGranularity granularity = UsedGranularity::HalfLine;
            if (mode == GameMode::Feihualing) {
                granularity = select_granularity();
            }

            Game game;
            game.poetry_db = &g_poetry_db;
            game.idiom_db = &g_idiom_db;
            game.setup(mode, granularity);
            setup_players(game);

            if (game.players.empty()) {
                std::cout << "  没有玩家，取消游戏。\n";
                pause();
                continue;
            }

            game.start();
            game_loop(game);
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    srand((unsigned)time(nullptr));

    // Allow custom data root via command line
    if (argc >= 2) {
        g_data_root = argv[1];
    }

    std::cout << "========================================\n";
    std::cout << "  飞花令 & 成语接龙 终端版\n";
    std::cout << "========================================\n\n";

    if (!load_databases()) {
        std::cout << "\n数据库加载失败，程序退出。\n";
        std::cout << "请确保在项目根目录运行，且已初始化 submodule:\n";
        std::cout << "  git submodule update --init --recursive\n";
        return 1;
    }

    std::cout << "\n加载完成！\n";
    pause();

    main_menu();
    return 0;
}
