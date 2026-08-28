#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct Poem {
    std::string id;
    std::string author;
    std::string title;     // poem title or ci rhythmic (词牌)
    std::string dynasty;   // "tang" / "song" / "song-ci" etc.
    std::vector<std::string> lines;       // original paragraph strings (with punctuation)
    std::vector<std::string> norm_lines;  // normalized full lines (no punct, simplified)
    std::vector<std::string> half_lines;  // normalized half-lines (no punct, simplified)
};

class PoetryDB {
public:
    bool load(const std::string& root_dir);

    size_t poem_count() const { return poems_.size(); }
    size_t half_line_count() const { return all_half_lines_.size(); }

    // Normalize for matching: t2s + strip punctuation + trim
    static std::string normalize(const std::string& s);

    // Split a paragraph into half-lines by CJK/ASCII punctuation
    static std::vector<std::string> split_half_lines(const std::string& paragraph);

    // Validation: does this text exist as a full line or half-line?
    // Returns poem index if found, -1 otherwise.
    int find_line(const std::string& text) const;       // match full line
    int find_half_line(const std::string& text) const;  // match half-line
    // Try both; returns poem index or -1.
    int find_any(const std::string& text) const;

    // Get poem by index
    const Poem* get_poem(size_t idx) const {
        return idx < poems_.size() ? &poems_[idx] : nullptr;
    }

    // Find all half-lines containing a given CJK character (string of 1 codepoint)
    std::vector<std::string> half_lines_with_char(const std::string& ch) const;

    // All half-lines (for bot memory sampling)
    const std::vector<std::string>& all_half_lines() const { return all_half_lines_; }

    // Get a random half-line (for opening)
    std::string random_half_line() const;

    // Check if a half-line belongs to a specific poem index
    int poem_of_half_line(const std::string& hl) const;

private:
    bool load_json_file(const std::string& path, const std::string& dynasty);
    void index_poem(size_t idx);

    std::vector<Poem> poems_;
    std::unordered_map<std::string, size_t> line_index_;    // norm line -> poem idx
    std::unordered_map<std::string, size_t> half_line_index_; // norm half-line -> poem idx
    std::unordered_map<std::string, std::vector<size_t>> char_index_; // char -> half-line idx in all_half_lines_
    std::vector<std::string> all_half_lines_;
    std::vector<size_t> half_line_poem_; // parallel to all_half_lines_: poem index
};
