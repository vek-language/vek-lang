#pragma once

#include "http.hpp"
#include "miniz/miniz.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

using std::string;
using std::optional;

static const string GITHUB_HOST = "https://github.com/";

enum class ParseState {
    Owner,
    Repo, 
    Tag
};

struct PackageLink {
    string owner;
    string repo;
    string tag;

    string zip_path() const {
        return "/" + owner + "/" + repo + "/archive/refs/tags/" + tag + ".zip";
    }
};

inline optional<PackageLink> parse_package(const string& spec) {
    PackageLink link;
    ParseState state = ParseState::Owner;

    for (char c : spec) {
        if (state == ParseState::Owner && c == '/') state = ParseState::Repo;
        else if (state == ParseState::Repo && c == '@') state = ParseState::Tag;
        else {
            if (state == ParseState::Owner) link.owner += c;
            else if (state == ParseState::Repo) link.repo += c;
            else if (state == ParseState::Tag) link.tag += c;
        }
    }

    if (state != ParseState::Tag || link.owner.empty() || link.repo.empty() || link.tag.empty()) {
        return {};
    }
    return link;
}

inline void unzip_to_folder(const string& zip_bytes, const fs::path& dest_dir) {
    // Verify ZIP magic bytes (PK\x03\x04)
    if (zip_bytes.size() < 4 || zip_bytes[0] != 'P' || zip_bytes[1] != 'K' ||
        zip_bytes[2] != 0x03 || zip_bytes[3] != 0x04) {
        throw std::runtime_error("[Error] Downloaded file is not a valid .zip archive.");
    }

    // Open ZIP directly from memory
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zip_bytes.data(), zip_bytes.size(), 0)) {
        throw std::runtime_error("[Error] Failed to read .zip archive.");
    }

    int count = static_cast<int>(mz_zip_reader_get_num_files(&zip));
    fs::create_directories(dest_dir);

    for (int i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        string entry = stat.m_filename;

        // Strip GitHub's outer "repo-tag/" folder
        auto slash = entry.find('/');
        if (slash == string::npos || slash + 1 >= entry.size()) continue;

        string rel = entry.substr(slash + 1);
        if (rel.find("..") != string::npos) {
            mz_zip_reader_end(&zip);
            throw std::runtime_error("[Error] Unsafe path inside .zip archive.");
        }

        fs::path out_path = dest_dir / rel;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            fs::create_directories(out_path);
        } else {
            fs::create_directories(out_path.parent_path());
            if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0)) {
                mz_zip_reader_end(&zip);
                throw std::runtime_error("[Error] Failed to extract: " + rel);
            }
        }
    }

    mz_zip_reader_end(&zip);
}

inline void save_to_manifest(const string& pkg_name, const string& spec) {
    std::ifstream in("vek.toml");
    string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    if (content.find(pkg_name + " =") != string::npos) return;

    if (content.find("[dependencies]") == string::npos) content += "\n\n[dependencies]\n";
    if (!content.empty() && content.back() != '\n') content += '\n';
    content += pkg_name + " = \"" + spec + "\"\n";

    std::ofstream out("vek.toml");
    out << content;
}

inline void install_package(const string& spec) {
    if (!fs::exists("vek.toml")) {
        throw std::runtime_error("[Error] No vek.toml found in current directory.");
    }

    auto link = parse_package(spec);
    if (!link) {
        throw std::runtime_error("[Error] Invalid format. Expected: owner/repo@tag");
    }

    std::cout << "Downloading " << spec << "...\n";
    string zip_bytes = http::download_github_zip(link->zip_path());

    fs::path install_dir = fs::path(".vek") / "packages" / link->repo;
    if (fs::exists(install_dir)) fs::remove_all(install_dir);

    unzip_to_folder(zip_bytes, install_dir);

    // Verify it's a valid VEK package
    if (!fs::exists(install_dir / "vek.toml")) {
        fs::remove_all(install_dir);
        throw std::runtime_error("[Error] Repository is not a valid VEK package (missing vek.toml).");
    }

    save_to_manifest(link->repo, spec);
    std::cout << "Installed '" << link->repo << "' (" << link->tag << ")!\n";
}