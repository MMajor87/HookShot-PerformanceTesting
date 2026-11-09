// (local, HookShot-PerformanceTesting) The renderer's own update: at start, the latest release of the testing repository is
// asked for. When it is a newer build of this DLL for the same game version, the player is asked; yes downloads the release's
// hookshot_vk.dll and its .sha256, checks the hash and puts the new DLL in place for the next start (the running one is renamed
// to hookshot_vk.dll.old, deleted at a later start). Only this DLL is updated: the game itself is built on each machine by the
// official Hookshot Builder. RX_VK_UPDATE_CHECK=0 (hookshot_env.txt) turns it off.
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace mkwvk {
void logf(const char* fmt, ...);

namespace {

// The release this DLL is: <game version>-perf.<n>. Bump kPerfBuild for each release (scripts: Hookshot-backups\release.sh)
#ifndef PERF_BUILD_NUMBER
#define PERF_BUILD_NUMBER 1
#endif
constexpr const char* kGameVersion = "v2.0.10";
constexpr int kPerfBuild = PERF_BUILD_NUMBER;   // (a test build overrides it: -DPERF_BUILD_NUMBER=0)
constexpr const wchar_t* kApiHost = L"api.github.com";
constexpr const wchar_t* kLatestPath = L"/repos/MMajor87/HookShot-PerformanceTesting/releases/latest";
constexpr const wchar_t* kAgent = L"HookShot-PerformanceTesting-update";

std::string this_tag() {
    return std::string(kGameVersion) + "-perf." + std::to_string(kPerfBuild);
}

/// GET https://host/path (redirects followed): the body, empty on any failure. timeout_ms for each step
std::string http_get(const std::wstring& host, const std::wstring& path, bool api, int timeout_ms, size_t max_bytes) {
    std::string body;
    HINTERNET s = WinHttpOpen(kAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s) return body;
    WinHttpSetTimeouts(s, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    HINTERNET c = WinHttpConnect(s, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE)
                    : nullptr;
    const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\n" : L"Accept: application/octet-stream\r\n";
    if (r && WinHttpSendRequest(r, headers, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
        DWORD status = 0, size = sizeof status;
        WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                            WINHTTP_NO_HEADER_INDEX);
        if (status == 200) {
            std::vector<char> buf(64 << 10);
            DWORD got = 0;
            while (body.size() < max_bytes && WinHttpReadData(r, buf.data(), (DWORD)buf.size(), &got) && got) body.append(buf.data(), got);
            if (body.size() >= max_bytes) body.clear();
        }
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    WinHttpCloseHandle(s);
    return body;
}

/// GET of a full https URL (a release asset's browser_download_url)
std::string http_get_url(const std::string& url, int timeout_ms, size_t max_bytes) {
    const std::wstring w(url.begin(), url.end());
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof u;
    wchar_t host[256], path[2048];
    u.lpszHostName = host, u.dwHostNameLength = 256, u.lpszUrlPath = path, u.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(w.c_str(), 0, 0, &u) || u.nScheme != INTERNET_SCHEME_HTTPS) return {};
    return http_get(std::wstring(host, u.dwHostNameLength), std::wstring(path, u.dwUrlPathLength), false, timeout_ms, max_bytes);
}

/// "name":"value" of a JSON text, the first one after `from`
std::string json_string(const std::string& j, const char* name, size_t from = 0, size_t* at = nullptr) {
    const std::string key = std::string("\"") + name + "\"";
    size_t k = j.find(key, from);
    if (k == std::string::npos) return {};
    k = j.find('"', j.find(':', k + key.size()) + 1);
    if (k == std::string::npos) return {};
    const size_t e = j.find('"', k + 1);
    if (e == std::string::npos) return {};
    if (at) *at = e;
    return j.substr(k + 1, e - k - 1);
}

/// the release asset's download URL whose name is `name`
std::string asset_url(const std::string& j, const std::string& name) {
    size_t at = 0;
    for (;;) {
        const std::string u = json_string(j, "browser_download_url", at, &at);
        if (u.empty()) return {};
        if (u.size() > name.size() && u.compare(u.size() - name.size() - 1, std::string::npos, "/" + name) == 0) return u;
    }
}

/// tag "<game version>-perf.<n>": its game version and n (n -1: not one of ours)
int perf_build_of(const std::string& tag, std::string* game) {
    const size_t p = tag.rfind("-perf.");
    if (p == std::string::npos) return -1;
    if (game) *game = tag.substr(0, p);
    const char* n = tag.c_str() + p + 6;
    char* end = nullptr;
    const long v = strtol(n, &end, 10);
    return end != n && !*end ? (int)v : -1;
}

std::string sha256_hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    unsigned char out[32];
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0 &&
            BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0) == 0 && BCryptFinishHash(h, out, sizeof out, 0) == 0) {
            static const char* d = "0123456789abcdef";
            for (unsigned char b : out) hex += d[b >> 4], hex += d[b & 15];
        }
        if (h) BCryptDestroyHash(h);
        BCryptCloseAlgorithmProvider(alg, 0);
    }
    return hex;
}

/// (a line "name=value" of a small text file: its value)
std::string read_value(const std::wstring& path, const char* name) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return {};
    char line[1024];
    std::string v;
    const size_t n = strlen(name);
    while (v.empty() && fgets(line, sizeof line, f)) {
        char* s = line;
        if ((unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s += 3;
        if (strncmp(s, name, n) || s[n] != '=') continue;
        v = s + n + 1;
        while (!v.empty() && (v.back() == '\n' || v.back() == '\r' || v.back() == ' ' || v.back() == '\t')) v.pop_back();
    }
    fclose(f);
    return v;
}

std::wstring widen(const std::string& s) {
    std::wstring w(s.size() + 1, 0);
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), (int)w.size());
    w.resize(n > 0 ? n - 1 : 0);
    return w;
}

void notify(const std::wstring& text, UINT icon) {
    if (getenv("RX_VK_UPDATE_ANSWER")) return;   // (a test: no message box)
    MessageBoxW(nullptr, text.c_str(), L"Hookshot performance build", MB_OK | icon | MB_TOPMOST | MB_SETFOREGROUND);
}

} // namespace

void perf_update_check() {
    static bool done;
    if (done) return;
    done = true;
    if (const char* e = getenv("RX_VK_UPDATE_CHECK"); e && !strcmp(e, "0")) return;
    // (this DLL's own path: <game>\bin\hookshot_vk.dll)
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&perf_update_check), &self))
        return;
    wchar_t buf[MAX_PATH];
    const DWORD len = GetModuleFileNameW(self, buf, MAX_PATH);
    if (!len || len >= MAX_PATH) return;
    const std::wstring dll(buf, len);
    DeleteFileW((dll + L".old").c_str());   // (the one an update replaced, no longer loaded)
    std::wstring game = dll.substr(0, dll.find_last_of(L"\\/"));
    if (!game.empty() && GetFileAttributesW((game + L"\\hookshot_version.txt").c_str()) == INVALID_FILE_ATTRIBUTES)
        game = game.substr(0, game.find_last_of(L"\\/"));   // (bin\ -> the game folder)
    // (a folder from the official builder says "v2.0.5", one from this repository's builder "v2.0.5-perf.<n>": the game version is
    // the part before "-perf.")
    std::string have_game = read_value(game + L"\\hookshot_version.txt", "version");
    if (const size_t p = have_game.find("-perf."); p != std::string::npos) have_game.resize(p);
    const std::string mine = this_tag();
    if (have_game != kGameVersion) {
        logf("vk: update: this renderer is %s (for Hookshot %s), the game folder is %s: no update check", mine.c_str(), kGameVersion,
             have_game.empty() ? "unknown" : have_game.c_str());
        return;
    }
    const std::string rel = http_get(kApiHost, kLatestPath, true, 2500, 1 << 20);
    const std::string tag = json_string(rel, "tag_name");
    if (tag.empty()) {
        logf("vk: update: the latest testing release could not be asked for (offline?); this renderer is %s", mine.c_str());
        return;
    }
    std::string rel_game;
    const int rel_build = perf_build_of(tag, &rel_game);
    logf("vk: update: this renderer is %s, the latest testing release is %s", mine.c_str(), tag.c_str());
    if (rel_build <= kPerfBuild || rel_game != kGameVersion) return;   // (up to date, or for another game version)
    const std::wstring skip_file = game + L"\\user\\hookshot_perf_update_skip.txt";
    if (read_value(skip_file, "skip") == tag) return;

    const std::wstring text = L"A new Hookshot performance build is out: " + widen(tag) + L" (this one is " + widen(mine) +
                              L").\n\nUpdate now? It downloads the new renderer (about 6 MB), checks it, and uses it from the "
                              L"next start. The game and your settings stay as they are.\n\n"
                              L"Yes: update now\nNo: not now (asked again next time)\nCancel: skip this version";
    // (RX_VK_UPDATE_ANSWER=yes|no|cancel: the answer without asking, for tests)
    const char* forced = getenv("RX_VK_UPDATE_ANSWER");
    const int answer = forced ? (!strcmp(forced, "yes") ? IDYES : !strcmp(forced, "cancel") ? IDCANCEL : IDNO)
                              : MessageBoxW(nullptr, text.c_str(), L"Hookshot performance build",
                                            MB_YESNOCANCEL | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    if (answer == IDCANCEL) {
        CreateDirectoryW((game + L"\\user").c_str(), nullptr);
        if (FILE* f = _wfopen(skip_file.c_str(), L"wb")) fprintf(f, "skip=%s\n", tag.c_str()), fclose(f);
        return;
    }
    if (answer != IDYES) return;

    const std::string dll_url = asset_url(rel, "hookshot_vk.dll"), sum_url = asset_url(rel, "hookshot_vk.dll.sha256");
    std::string why;
    std::string data, sum;
    if (dll_url.empty() || sum_url.empty()) why = "the release has no hookshot_vk.dll or hookshot_vk.dll.sha256";
    else if ((sum = http_get_url(sum_url, 15000, 4096)).size() < 64) why = "the checksum could not be downloaded";
    else if ((data = http_get_url(dll_url, 30000, 64u << 20)).size() < (1u << 20)) why = "the renderer could not be downloaded";
    else if (sha256_hex(data) != sum.substr(0, 64)) why = "the download does not match its checksum";
    if (why.empty()) {
        const std::wstring fresh = dll + L".new", old = dll + L".old";
        FILE* f = _wfopen(fresh.c_str(), L"wb");
        const bool wrote = f && fwrite(data.data(), 1, data.size(), f) == data.size();
        if (f) fclose(f);
        if (!wrote) why = "the new renderer could not be written next to the old one";
        else if (!MoveFileExW(dll.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) why = "the running renderer could not be renamed";
        else if (!MoveFileExW(fresh.c_str(), dll.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            MoveFileExW(old.c_str(), dll.c_str(), MOVEFILE_REPLACE_EXISTING);   // (put the old one back)
            why = "the new renderer could not be put in place";
        }
        if (!why.empty()) DeleteFileW(fresh.c_str());
    }
    if (!why.empty()) {
        logf("vk: update to %s failed: %s", tag.c_str(), why.c_str());
        notify(L"The update to " + widen(tag) + L" did not work: " + widen(why) + L".\n\nThis renderer stays as it is.",
               MB_ICONWARNING);
        return;
    }
    logf("vk: update: %s downloaded and checked (sha256 %s); in use from the next start", tag.c_str(), sum.substr(0, 64).c_str());
    notify(L"Updated to " + widen(tag) + L". Restart the game to use it.", MB_ICONINFORMATION);
}

} // namespace mkwvk
#else
namespace mkwvk {
void perf_update_check() {}
} // namespace mkwvk
#endif
