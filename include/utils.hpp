#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cctype>

namespace utils {

// UTF-8 helpers ----------------------------------------------------------

// Decode one UTF-8 code point starting at byte index `i`; advance i.
// Returns 0 on invalid sequence.
inline uint32_t utf8_decode(const std::string& s, size_t& i) {
    if (i >= s.size()) return 0;
    uint8_t c = (uint8_t)s[i];
    uint32_t cp = 0;
    int extra = 0;
    if (c < 0x80) { cp = c; extra = 0; }
    else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { i++; return 0; }
    i++;
    for (int k = 0; k < extra && i < s.size(); k++, i++) {
        uint8_t cc = (uint8_t)s[i];
        if ((cc & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (cc & 0x3F);
    }
    return cp;
}

// Encode one code point to UTF-8 and append.
inline void utf8_encode(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back((char)cp);
    } else if (cp < 0x800) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

// Split a UTF-8 string into individual code-point strings.
inline std::vector<std::string> utf8_split(const std::string& s) {
    std::vector<std::string> result;
    size_t i = 0;
    while (i < s.size()) {
        size_t start = i;
        uint32_t cp = utf8_decode(s, i);
        if (cp == 0 && start == i) i++; // skip invalid
        result.push_back(s.substr(start, i - start));
    }
    return result;
}

// Count UTF-8 code points.
inline size_t utf8_len(const std::string& s) {
    size_t count = 0;
    size_t i = 0;
    while (i < s.size()) {
        utf8_decode(s, i);
        count++;
    }
    return count;
}

// Get the last UTF-8 code point as a string.
inline std::string utf8_last_char(const std::string& s) {
    if (s.empty()) return "";
    // Scan backwards past continuation bytes (10xxxxxx) to find the lead byte.
    size_t i = s.size();
    while (i > 0 && ((uint8_t)s[i - 1] & 0xC0) == 0x80) i--;
    if (i > 0) {
        size_t start = i - 1; // lead byte of the last character
        return s.substr(start, s.size() - start);
    }
    return s.substr(s.size() - 1);
}

// Get first UTF-8 char as string.
inline std::string utf8_first_char(const std::string& s) {
    if (s.empty()) return "";
    size_t i = 0;
    utf8_decode(s, i);
    return s.substr(0, i);
}

// Check if a code point is a Chinese character (CJK Unified Ideographs).
inline bool is_cjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x20000 && cp <= 0x2A6DF) ||
           (cp >= 0x2A700 && cp <= 0x2B73F);
}

inline bool is_cjk_char(const std::string& ch) {
    size_t i = 0;
    return is_cjk(utf8_decode(ch, i));
}

// Check if char is punctuation (ASCII or CJK).
inline bool is_punct(uint32_t cp) {
    if (cp < 128) return std::ispunct((int)cp) || std::isspace((int)cp);
    // CJK punctuation ranges
    return (cp >= 0x3000 && cp <= 0x303F) ||
           (cp >= 0xFF00 && cp <= 0xFFEF) ||
           (cp >= 0x2000 && cp <= 0x206F);
}

// Remove all punctuation from a UTF-8 string, keep only CJK + alnum.
inline std::string strip_punct(const std::string& s) {
    std::string result;
    size_t i = 0;
    while (i < s.size()) {
        size_t start = i;
        uint32_t cp = utf8_decode(s, i);
        if (!is_punct(cp)) {
            result += s.substr(start, i - start);
        }
    }
    return result;
}

// Trim whitespace.
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((uint8_t)s[a])) a++;
    while (b > a && std::isspace((uint8_t)s[b - 1])) b--;
    return s.substr(a, b - a);
}

// Split a string by a single-char delimiter.
inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : s) {
        if (c == delim) { tokens.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    tokens.push_back(cur);
    return tokens;
}

// Join strings.
inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string r;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) r += sep;
        r += parts[i];
    }
    return r;
}

// Convert to lowercase (ASCII).
inline std::string to_lower(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)std::tolower((uint8_t)c);
    return r;
}

// Simple random in [0, n).
inline int rand_int(int n) {
    if (n <= 0) return 0;
    return (int)((uint64_t)rand() * (uint64_t)n / ((uint64_t)RAND_MAX + 1));
}

// Pick random element.
template<typename T>
inline const T& rand_pick(const std::vector<T>& v) {
    return v[rand_int((int)v.size())];
}

// Sanitize a string by replacing invalid UTF-8 sequences with '?'.
// Validates lead bytes, continuation bytes, overlong encodings,
// surrogate pairs, and Unicode range.
inline std::string sanitize_utf8(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x80) {
            result.push_back((char)c);
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < s.size() && ((uint8_t)s[i+1] & 0xC0) == 0x80) {
                uint32_t cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)((uint8_t)s[i+1] & 0x3F);
                if (cp >= 0x80) { result.append(s, i, 2); i += 2; continue; }
            }
            result.push_back('?'); i++;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < s.size() && ((uint8_t)s[i+1] & 0xC0) == 0x80 && ((uint8_t)s[i+2] & 0xC0) == 0x80) {
                uint32_t cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)((uint8_t)s[i+1] & 0x3F) << 6) | (uint32_t)((uint8_t)s[i+2] & 0x3F);
                if (cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF)) { result.append(s, i, 3); i += 3; continue; }
            }
            result.push_back('?'); i++;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < s.size() && ((uint8_t)s[i+1] & 0xC0) == 0x80 && ((uint8_t)s[i+2] & 0xC0) == 0x80 && ((uint8_t)s[i+3] & 0xC0) == 0x80) {
                uint32_t cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)((uint8_t)s[i+1] & 0x3F) << 12) | ((uint32_t)((uint8_t)s[i+2] & 0x3F) << 6) | (uint32_t)((uint8_t)s[i+3] & 0x3F);
                if (cp >= 0x10000 && cp <= 0x10FFFF) { result.append(s, i, 4); i += 4; continue; }
            }
            result.push_back('?'); i++;
        } else {
            result.push_back('?'); i++;
        }
    }
    return result;
}

} // namespace utils
