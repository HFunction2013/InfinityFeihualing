#include "idiom_db.hpp"
#include "utils.hpp"
#include "t2s.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

bool IdiomDB::load(const std::string& json_path) {
    idioms_.clear();
    word_index_.clear();
    first_char_index_.clear();
    all_words_.clear();

    std::ifstream f(json_path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    content = utils::sanitize_utf8(content);
    try {
        json j = json::parse(content, nullptr, false);
        if (j.is_discarded() || !j.is_array()) return false;
        for (const auto& entry : j) {
            Idiom id;
            id.word = t2s::convert(entry.value("word", ""));
            id.pinyin = entry.value("pinyin", "");
            id.explanation = entry.value("explanation", "");
            if (id.word.empty()) continue;
            // Must be all CJK chars
            bool valid = true;
            auto chars = utils::utf8_split(id.word);
            for (const auto& ch : chars) {
                if (!utils::is_cjk_char(ch)) { valid = false; break; }
            }
            if (!valid) continue;

            size_t idx = idioms_.size();
            idioms_.push_back(std::move(id));
            const Idiom& stored = idioms_.back();
            word_index_[stored.word] = idx;
            all_words_.push_back(stored.word);
            std::string fc = first_char(stored.word);
            if (!fc.empty()) first_char_index_[fc].push_back(idx);
        }
        return !idioms_.empty();
    } catch (...) {
        return false;
    }
}

bool IdiomDB::has(const std::string& word) const {
    std::string w = t2s::convert(utils::trim(word));
    return word_index_.count(w) > 0;
}

const Idiom* IdiomDB::get(const std::string& word) const {
    std::string w = t2s::convert(utils::trim(word));
    auto it = word_index_.find(w);
    if (it == word_index_.end()) return nullptr;
    return &idioms_[it->second];
}

std::vector<std::string> IdiomDB::starting_with(const std::string& ch) const {
    std::vector<std::string> result;
    auto it = first_char_index_.find(ch);
    if (it == first_char_index_.end()) return result;
    result.reserve(it->second.size());
    for (size_t idx : it->second) {
        result.push_back(idioms_[idx].word);
    }
    return result;
}

std::string IdiomDB::random_idiom() const {
    if (all_words_.empty()) return "";
    return all_words_[utils::rand_int((int)all_words_.size())];
}

std::string IdiomDB::first_char(const std::string& idiom) {
    return utils::utf8_first_char(idiom);
}

std::string IdiomDB::last_char(const std::string& idiom) {
    return utils::utf8_last_char(idiom);
}
