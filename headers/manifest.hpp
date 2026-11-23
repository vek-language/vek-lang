#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using std::getline;
using std::ifstream;
using std::runtime_error;
using std::string;
using std::unordered_map;
using std::vector;

namespace fs = std::filesystem;

struct Metadata {
    string name;
    string author;
    string version;
    string description;
    vector<uint8_t> icon_data;
};

struct Manifest {
    Metadata metadata;
    string entry; // e.g. "src/main.vek"
    unordered_map<string, string> dependencies;
};

static string trim(string s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return start == string::npos ? "" : s.substr(start, end - start + 1);
}

inline void load_manifest(const fs::path& path, Manifest& manifest) {
    fs::path cfg = (path.filename() == "vek.toml")
        ? path
        : path.parent_path() / "vek.toml";

    if (!fs::exists(cfg)) return;

    ifstream file(cfg);
    string line, section = "app";
    fs::path icon_rel;

    while (getline(file, line)) {
        if (auto hash = line.find('#'); hash != string::npos) line.erase(hash);
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) continue;
        line = line.substr(start, line.find_last_not_of(" \t\r\n") - start + 1);

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        auto eq = line.find('=');
        if (eq == string::npos) continue;

        string key = line.substr(0, eq);
        string val = line.substr(eq + 1);
        // trim key & val
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);

        if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
            val = val.substr(1, val.size() - 2);
        }

        if (section == "app" || section == "package") {
            if (key == "name")             manifest.metadata.name = val;
            else if (key == "version")     manifest.metadata.version = val;
            else if (key == "author")      manifest.metadata.author = val;
            else if (key == "description") manifest.metadata.description = val;
            else if (key == "entry")       manifest.entry = val;
            else if (key == "icon")        icon_rel = val;
        } else if (section == "dependencies") {
            manifest.dependencies[key] = val;
        }
    }

    if (!icon_rel.empty()) {
        fs::path icon = cfg.parent_path() / icon_rel;
        if (fs::exists(icon)) {
            ifstream f(icon, std::ios::binary);
            manifest.metadata.icon_data.assign(std::istreambuf_iterator<char>(f), {});
        }
    }
}
