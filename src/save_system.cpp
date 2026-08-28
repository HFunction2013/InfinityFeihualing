#include "save_system.hpp"
#include "sha256.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

const char* SaveSystem::SECRET_SALT = "FeihuaLing_SaveSalt_v1_9f3a7c2e";

bool SaveSystem::save(const Game& game, const std::string& filepath) {
    std::string data = game.to_json();
    std::string mac = sha256::hmac(SECRET_SALT, data);

    json wrapper;
    wrapper["data"] = data;  // store as raw string to preserve exact bytes for HMAC
    wrapper["mac"] = mac;
    wrapper["app"] = "feihualing";
    wrapper["version"] = 1;

    fs::path p(filepath);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }
    std::ofstream f(filepath);
    if (!f.is_open()) return false;
    f << wrapper.dump(2);
    return true;
}

bool SaveSystem::load(const std::string& filepath, PoetryDB* pdb, IdiomDB* idb,
                      Game& out_game, std::string& error) {
    std::ifstream f(filepath);
    if (!f.is_open()) { error = "无法打开存档文件"; return false; }
    try {
        json wrapper;
        f >> wrapper;
        if (!wrapper.contains("data") || !wrapper.contains("mac")) {
            error = "存档格式损坏";
            return false;
        }
        std::string data = wrapper["data"].get<std::string>();
        std::string expected_mac = wrapper["mac"].get<std::string>();
        std::string actual_mac = sha256::hmac(SECRET_SALT, data);

        if (actual_mac != expected_mac) {
            error = "存档校验失败：文件可能已被篡改";
            return false;
        }
        out_game = Game::from_json(data, pdb, idb);
        return true;
    } catch (const std::exception& e) {
        error = std::string("存档解析失败: ") + e.what();
        return false;
    }
}

std::vector<std::string> SaveSystem::list_saves(const std::string& dir) {
    std::vector<std::string> result;
    if (!fs::exists(dir)) return result;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (name.size() > 5 && name.substr(name.size() - 5) == ".save") {
                result.push_back(entry.path().string());
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
