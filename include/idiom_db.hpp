#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct Idiom {
    std::string word;        // simplified idiom (4 chars typically)
    std::string pinyin;
    std::string explanation;
};

class IdiomDB {
public:
    bool load(const std::string& json_path);
    // Load character pinyin data from chinese-xinhua/data/word.json
    bool load_pinyin(const std::string& word_json_path);
    bool pinyin_loaded() const { return !char_pinyin_.empty(); }

    size_t count() const { return idioms_.size(); }
    bool has(const std::string& word) const;
    const Idiom* get(const std::string& word) const;
    std::vector<std::string> starting_with(const std::string& ch) const;
    // Find idioms whose first char pinyin matches (toned or toneless)
    std::vector<std::string> starting_with_pinyin(const std::string& pinyin, bool strict_tone) const;
    const std::vector<std::string>& all_words() const { return all_words_; }
    std::string random_idiom() const;
    static std::string first_char(const std::string& idiom);
    static std::string last_char(const std::string& idiom);
    // Get pinyin of a single CJK char (with tone). Returns "" if unknown.
    std::string get_pinyin(const std::string& ch) const;
    // Remove tone marks (hua -> hua)
    static std::string strip_tone(const std::string& pinyin);
    // Compare pinyin; if strict_tone, tone marks must match
    static bool pinyin_equal(const std::string& a, const std::string& b, bool strict_tone);

private:
    std::vector<Idiom> idioms_;
    std::unordered_map<std::string, size_t> word_index_;
    std::unordered_map<std::string, std::vector<size_t>> first_char_index_;
    std::vector<std::string> all_words_;
    std::unordered_map<std::string, std::string> char_pinyin_;
    std::unordered_map<std::string, std::vector<size_t>> pinyin_tone_index_;
    std::unordered_map<std::string, std::vector<size_t>> pinyin_notone_index_;
    void build_pinyin_index();
};
