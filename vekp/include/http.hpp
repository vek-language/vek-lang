#pragma once
#include <stdexcept>
#include <string>

#ifdef _WIN32
    #include <windows.h>
    #include <winhttp.h>
#else
    #include <curl/curl.h>
#endif

namespace http {

inline std::string download_github_zip(const std::string& path) {
#ifdef _WIN32
    std::wstring host = L"github.com";
    std::wstring wpath(path.begin(), path.end());

    HINTERNET session = WinHttpOpen(L"vek_packagemanager", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) throw std::runtime_error("WinHttpOpen failed.");

    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpConnect failed.");
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", wpath.c_str(), NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("WinHttpOpenRequest failed.");
    }

    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Network error connecting to GitHub.");
    }

    DWORD status = 0, status_size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    NULL, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Package not found (HTTP " + std::to_string(status) + ").");
    }

    std::string result;
    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (WinHttpReadData(request, chunk.data(), available, &read)) {
            result.append(chunk.data(), read);
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;

#else
    std::string url = "https://github.com" + path;
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize libcurl.");

    std::string result;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "vek_packagemanager");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, void* data) -> size_t {
        static_cast<std::string*>(data)->append(ptr, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) throw std::runtime_error(curl_easy_strerror(res));
    if (status != 200) throw std::runtime_error("Package not found (HTTP " + std::to_string(status) + ").");
    return result;
#endif
}

}