#pragma once
#include <filesystem>

namespace fs = std::filesystem;

fs::path get_executable_dir();
fs::path get_vek_dir();
