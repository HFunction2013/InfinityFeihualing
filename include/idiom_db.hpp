#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Idiom {
    std::string word;        // simplified idiom (4 chars typically)
    std::string pinyin;
    std::string explanation;
};

class IdiomDB {
public:
    bool load(const std::string& json_path);

    size_t count() const { return idioms_.size(); }

    // Validation: does this idiom exist?
    bool has(const std::string& word) const;
    const Idiom* get(const std::string& word) const;

    // Find all idioms starting with a given CJK character
    std::vector<std::string> starting_with(const std::string& ch) const;

    // All idioms (for bot memory sampling)
    const std::vector<std::string>& all_words() const { return all_words_; }

    // Random idiom (for opening)
    std::string random_idiom() const;

    // First / last CJK char of an idiom
    static std::string first_char(const std::string& idiom);
    static std::string last_char(const std::string& idiom);

private:
    std::vector<Idiom> idioms_;
    std::unordered_map<std::string, size_t> word_index_;   // word -> index
    std::unordered_map<std::string, std::vector<size_t>> first_char_index_; // char -> idiom indices
    std::vector<std::string> all_words_;
};
