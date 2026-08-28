#include "poetry_db.hpp"
#include "utils.hpp"
#include "t2s.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string PoetryDB::normalize(const std::string& s) {
    std::string r = t2s::convert(s);
    r = utils::strip_punct(r);
    r = utils::trim(r);
    return r;
}

std::vector<std::string> PoetryDB::split_half_lines(const std::string& paragraph) {
    std::vector<std::string> result;
    std::string simp = t2s::convert(paragraph);
    std::string cur;
    for (size_t i = 0; i < simp.size(); ) {
        size_t start = i;
        uint32_t cp = utils::utf8_decode(simp, i);
        if (cp == 0 && start == i) { cur += simp[start]; i = start + 1; continue; }
        if (utils::is_punct(cp)) {
            std::string h = utils::strip_punct(cur);
            h = utils::trim(h);
            if (!h.empty()) result.push_back(h);
            cur.clear();
        } else {
            cur += simp.substr(start, i - start);
        }
    }
    std::string h = utils::strip_punct(cur);
    h = utils::trim(h);
    if (!h.empty()) result.push_back(h);
    return result;
}

bool PoetryDB::load_json_file(const std::string& path, const std::string& dynasty) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    content = utils::sanitize_utf8(content);
    try {
        json j = json::parse(content, nullptr, false);
        if (j.is_discarded() || !j.is_array()) return false;
        for (const auto& entry : j) {
            Poem p;
            p.dynasty = dynasty;
            p.author = entry.value("author", "");
            if (entry.contains("title")) p.title = entry["title"].get<std::string>();
            else if (entry.contains("rhythmic")) p.title = entry["rhythmic"].get<std::string>();
            p.id = entry.value("id", "");
            if (p.id.empty()) p.id = p.author + "_" + p.title + "_" + std::to_string(poems_.size());

            if (!entry.contains("paragraphs") || !entry["paragraphs"].is_array()) continue;
            for (const auto& para : entry["paragraphs"]) {
                std::string text = para.get<std::string>();
                p.lines.push_back(text);
                p.norm_lines.push_back(normalize(text));
                auto halves = split_half_lines(text);
                for (auto& h : halves) p.half_lines.push_back(h);
            }
            if (p.lines.empty()) continue;
            poems_.push_back(std::move(p));
        }
        return true;
    } catch (...) {
        return false;
    }
}

void PoetryDB::index_poem(size_t idx) {
    const Poem& p = poems_[idx];
    for (const auto& line : p.norm_lines) {
        if (!line.empty()) line_index_[line] = idx;
    }
    for (const auto& hl : p.half_lines) {
        if (hl.empty()) continue;
        half_line_index_[hl] = idx;
        size_t global_idx = all_half_lines_.size();
        all_half_lines_.push_back(hl);
        half_line_poem_.push_back(idx);
        // index by each CJK char
        auto chars = utils::utf8_split(hl);
        std::unordered_set<std::string> seen;
        for (const auto& ch : chars) {
            if (utils::is_cjk_char(ch) && seen.insert(ch).second) {
                char_index_[ch].push_back(global_idx);
            }
        }
    }
}

bool PoetryDB::load(const std::string& root_dir) {
    poems_.clear();
    line_index_.clear();
    half_line_index_.clear();
    char_index_.clear();
    all_half_lines_.clear();
    half_line_poem_.clear();

    struct DirConf { std::string sub; std::string prefix; std::string dynasty; };
    std::vector<DirConf> dirs = {
        {"全唐诗", "poet.tang.", "tang"},
        {"全唐诗", "poet.song.", "song"},
        {"宋词", "ci.song.", "song-ci"},
    };

    int loaded = 0;
    for (const auto& dc : dirs) {
        fs::path dir = fs::path(root_dir) / dc.sub;
        if (!fs::exists(dir)) continue;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string name = entry.path().filename().string();
            if (name.rfind(dc.prefix, 0) != 0) continue;
            if (name.substr(name.size() - 5) != ".json") continue;
            if (load_json_file(entry.path().string(), dc.dynasty)) loaded++;
        }
    }

    // Build indexes
    for (size_t i = 0; i < poems_.size(); i++) index_poem(i);

    return !poems_.empty();
}

int PoetryDB::find_line(const std::string& text) const {
    std::string norm = normalize(text);
    auto it = line_index_.find(norm);
    return it != line_index_.end() ? (int)it->second : -1;
}

int PoetryDB::find_half_line(const std::string& text) const {
    std::string norm = normalize(text);
    auto it = half_line_index_.find(norm);
    return it != half_line_index_.end() ? (int)it->second : -1;
}

int PoetryDB::find_any(const std::string& text) const {
    int r = find_half_line(text);
    if (r >= 0) return r;
    return find_line(text);
}

std::vector<std::string> PoetryDB::half_lines_with_char(const std::string& ch) const {
    std::vector<std::string> result;
    auto it = char_index_.find(ch);
    if (it == char_index_.end()) return result;
    result.reserve(it->second.size());
    for (size_t idx : it->second) {
        result.push_back(all_half_lines_[idx]);
    }
    return result;
}

std::string PoetryDB::random_half_line() const {
    if (all_half_lines_.empty()) return "";
    return all_half_lines_[utils::rand_int((int)all_half_lines_.size())];
}

int PoetryDB::poem_of_half_line(const std::string& hl) const {
    std::string norm = normalize(hl);
    auto it = half_line_index_.find(norm);
    return it != half_line_index_.end() ? (int)it->second : -1;
}
