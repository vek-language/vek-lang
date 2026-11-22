#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

using std::string;
using std::vector;
using std::getline;
using std::runtime_error;
using std::ifstream;

namespace fs = std::filesystem;

struct Metadata {
    string name;
    string author;

    string version;
    string description;

    vector<uint8_t> icon_data;
};

static string trim(std::string s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return start == string::npos ? "" : s.substr(start, end - start + 1);
}

void load_metadata(const fs::path& src_file, Metadata& metadata) {
    fs::path cfg = src_file.parent_path() / "vek.toml";
    if (!fs::exists(cfg)) return;

    ifstream file(cfg);
    string line;
    fs::path icon_rel;

    while (getline(file, line)) {
        auto hash = line.find('#');
        if (hash != string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty() || line.front() == '[') continue;

        auto eq = line.find('=');
        if (eq == string::npos) continue;

        string key = trim(line.substr(0, eq));
        string value = trim(line.substr(eq + 1));
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);

        if (key == "name") metadata.name = value;
        else if (key == "version") metadata.version = value;
        else if (key == "author") metadata.author = value;
        else if (key == "description") metadata.description = value;
        else if (key == "icon") icon_rel = value;
    }

    if (!icon_rel.empty()) {
        fs::path icon = cfg.parent_path() / icon_rel;
        if (fs::exists(icon)) {
            ifstream f(icon, std::ios::binary);
            metadata.icon_data.assign(std::istreambuf_iterator<char>(f), {});
        }
    }
}