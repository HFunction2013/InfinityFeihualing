#pragma once
#include <string>
#include "game.hpp"

class SaveSystem {
public:
    // Save game to file with HMAC anti-tamper. Returns true on success.
    static bool save(const Game& game, const std::string& filepath);

    // Load game from file. Verifies HMAC. Returns true on success.
    // If tampering detected, returns false and sets error message.
    static bool load(const std::string& filepath, PoetryDB* pdb, IdiomDB* idb,
                     Game& out_game, std::string& error);

    // List save files in directory
    static std::vector<std::string> list_saves(const std::string& dir);

    // The secret salt used for HMAC (compiled in, not user-visible)
    static const char* SECRET_SALT;
};
