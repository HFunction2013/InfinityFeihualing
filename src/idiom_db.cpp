#include "idiom_db.hpp"
#include "utils.hpp"
#include "t2s.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>

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

// ---------------------------------------------------------------------------
// Pinyin support
// ---------------------------------------------------------------------------

bool IdiomDB::load_pinyin(const std::string& word_json_path) {
    char_pinyin_.clear();
    std::ifstream f(word_json_path);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    content = utils::sanitize_utf8(content);
    try {
        json j = json::parse(content, nullptr, false);
        if (j.is_discarded() || !j.is_array()) return false;
        for (const auto& entry : j) {
            std::string word = t2s::convert(entry.value("word", ""));
            std::string py = entry.value("pinyin", "");
            if (word.empty() || py.empty()) continue;
            // word.json may have multiple pinyin separated by space; take first
            size_t sp = py.find(' ');
            if (sp != std::string::npos) py = py.substr(0, sp);
            char_pinyin_[word] = py;
        }
        build_pinyin_index();
        return !char_pinyin_.empty();
    } catch (...) {
        return false;
    }
}

void IdiomDB::build_pinyin_index() {
    pinyin_tone_index_.clear();
    pinyin_notone_index_.clear();
    for (size_t i = 0; i < idioms_.size(); i++) {
        std::string fc = first_char(idioms_[i].word);
        auto it = char_pinyin_.find(fc);
        if (it == char_pinyin_.end()) continue;
        std::string toned = it->second;
        std::string toneless = strip_tone(toned);
        pinyin_tone_index_[toned].push_back(i);
        pinyin_notone_index_[toneless].push_back(i);
    }
}

std::string IdiomDB::get_pinyin(const std::string& ch) const {
    auto it = char_pinyin_.find(ch);
    if (it == char_pinyin_.end()) return "";
    return it->second;
}

std::string IdiomDB::strip_tone(const std::string& pinyin) {
    std::string result;
    result.reserve(pinyin.size());
    // Map toned UTF-8 sequences to plain letters
    static const std::pair<const char*, const char*> table[] = {
        {"ā", "a"}, {"á", "a"}, {"ǎ", "a"}, {"à", "a"},
        {"ē", "e"}, {"é", "e"}, {"ě", "e"}, {"è", "e"},
        {"ī", "i"}, {"í", "i"}, {"ǐ", "i"}, {"ì", "i"},
        {"ō", "o"}, {"ó", "o"}, {"ǒ", "o"}, {"ò", "o"},
        {"ū", "u"}, {"ú", "u"}, {"ǔ", "u"}, {"ù", "u"},
        {"ǖ", "v"}, {"ǘ", "v"}, {"ǚ", "v"}, {"ǜ", "v"}, {"ü", "v"},
    };
    // Simple approach: iterate UTF-8 chars, replace if in table
    size_t i = 0;
    while (i < pinyin.size()) {
        bool matched = false;
        for (const auto& kv : table) {
            size_t klen = strlen(kv.first);
            if (i + klen <= pinyin.size() &&
                pinyin.compare(i, klen, kv.first) == 0) {
                result += kv.second;
                i += klen;
                matched = true;
                break;
            }
        }
        if (!matched) {
            result += pinyin[i++];
        }
    }
    return result;
}

bool IdiomDB::pinyin_equal(const std::string& a, const std::string& b, bool strict_tone) {
    if (strict_tone) return a == b;
    return strip_tone(a) == strip_tone(b);
}

std::vector<std::string> IdiomDB::starting_with_pinyin(const std::string& pinyin, bool strict_tone) const {
    std::vector<std::string> result;
    const auto& idx = strict_tone ? pinyin_tone_index_ : pinyin_notone_index_;
    std::string key = strict_tone ? pinyin : strip_tone(pinyin);
    auto it = idx.find(key);
    if (it == idx.end()) return result;
    result.reserve(it->second.size());
    for (size_t idx2 : it->second) {
        result.push_back(idioms_[idx2].word);
    }
    return result;
}
