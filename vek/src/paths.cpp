#include "paths.hpp"
#include <cstdlib>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#elif __APPLE__
    #include <mach-o/dyld.h>
    #include <vector>
#else
    #include <unistd.h>
#endif

fs::path get_executable_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, buf, MAX_PATH);
    if (len > 0) return fs::path(buf).parent_path();
#elif __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buf(size);
    if (_NSGetExecutablePath(buf.data(), &size) == 0) {
        return fs::weakly_canonical(buf.data()).parent_path();
    }
#else
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return exe.parent_path();
#endif
    return fs::current_path();
}

fs::path get_vek_dir() {
#ifdef _WIN32
    if (const wchar_t* env = _wgetenv(L"VEK_HOME"); env && *env) {
        return fs::weakly_canonical(env);
    }
#else
    if (const char* env = std::getenv("VEK_HOME"); env && *env) {
        return fs::weakly_canonical(env);
    }
#endif
    return get_executable_dir();
}
