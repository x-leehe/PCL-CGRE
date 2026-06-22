#include "network/McVersionFetcher.hpp"
#include "network/HttpUtil.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#if defined(__linux__) && !defined(__ANDROID__)
#include <limits.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <thread>

#include "pclcore/network/Dispatcher.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace pcl::mc {

namespace {

/* ── URL sources (BMCLAPI first, Mojang fallback) ─────────────────────── */
constexpr const char* SOURCE_URLS[] = {
    "https://bmclapi2.bangbang93.com/mc/game/version_manifest.json",
    "https://launchermeta.mojang.com/mc/game/version_manifest.json",
};
constexpr const char* SOURCE_NAMES[] = { "BMCLAPI", "Mojang" };
constexpr int SOURCE_COUNT = 2;

/* ── Helper: return string value if present and is a string, else "" ──── */
inline std::string jstr(const json& val) {
    return val.is_string() ? val.get<std::string>() : "";
}

/* ── Helper: return string member by key, or "" if missing/not a string ─ */
inline std::string jstr(const json& obj, const char* key) {
    auto it = obj.find(key);
    return (it != obj.end()) ? jstr(*it) : "";
}

/* ── Parse an ISO 8601 date string into a time_t. ─────────────────────── */
time_t parse_iso8601(const char* str)
{
    if (!str || !*str) return 0;

    struct tm tm = {};
    int tz_h = 0, tz_m = 0;
    char tz_sign = '+';

    int n = sscanf(str, "%d-%d-%dT%d:%d:%d%c%d:%d",
                   &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                   &tm.tm_hour, &tm.tm_min, &tm.tm_sec,
                   &tz_sign, &tz_h, &tz_m);
    if (n < 6) return 0;

    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = -1;

    time_t t =
#ifdef _WIN32
        _mkgmtime(&tm);
#else
        timegm(&tm);
#endif
    if (n >= 7) {
        int offset = tz_h * 3600 + tz_m * 60;
        if (tz_sign == '-') offset = -offset;
        t -= offset;
    }
    return t;
}

/* ── Parse the version manifest JSON. ────────────────────────────────── */
VersionManifest parse_manifest(const char* body, const char* source)
{
    VersionManifest m;
    m.source_name = source;
    m.loaded = true;

    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error& e) {
        g_warning("McVersionFetcher: JSON parse error from %s: %s",
                  source, e.what());
        m.loaded = false;
        m.error = "JSON 解析失败";
        return m;
    }

    if (!j.is_object()) {
        m.loaded = false;
        m.error = "响应格式不正确";
        return m;
    }

    /* latest */
    auto latest_it = j.find("latest");
    if (latest_it != j.end() && latest_it->is_object()) {
        m.latest_release  = jstr(*latest_it, "release");
        m.latest_snapshot = jstr(*latest_it, "snapshot");
    }

    /* versions array */
    std::vector<VersionEntry> all;
    auto versions_it = j.find("versions");
    if (versions_it != j.end() && versions_it->is_array()) {
        for (auto& ver : *versions_it) {
            if (!ver.is_object()) continue;

            VersionEntry e;
            e.id   = jstr(ver, "id");
            e.type = jstr(ver, "type");
            e.url  = jstr(ver, "url");
            std::string rt = jstr(ver, "releaseTime");
            if (rt.empty()) rt = jstr(ver, "time");
            e.release_time = rt;
            e.timestamp    = parse_iso8601(rt.c_str());
            all.push_back(std::move(e));
        }
    }

    /* Validate: manifest should have >= 200 versions */
    if (all.size() < 200) {
        m.loaded = false;
        m.error = "版本列表过短，数据可能不完整";
        return m;
    }

    /* Categorise into groups (matches PCL-CE order).
     * Move version entries out of `all` for zero-copy transfer. */
    struct GroupDef { const char* label; const char* icon; const char* type; };
    const GroupDef defs[] = {
        {"正式版",     "Minecraft",           "release"},
        {"预览版",     "Command",             "snapshot"},
        {"远古版本",   "Minecraft-Alpha",      "old_alpha"},
        {"愚人节版本", "Minecraft-AprilFool",  "april_fool"},
    };

    for (auto& d : defs) {
        VersionGroup g;
        g.label       = d.label;
        g.block_icon  = d.icon;
        g.type_filter = d.type;
        g.versions.reserve(all.size() / 4);  // rough estimate
        for (auto& v : all) {
            if (v.type == d.type)
                g.versions.push_back(std::move(v));
        }
        std::sort(g.versions.begin(), g.versions.end(),
                  [](const VersionEntry& a, const VersionEntry& b) {
                      return a.timestamp > b.timestamp;
                  });
        m.groups.push_back(std::move(g));
    }

    return m;
}

/* ── Worker thread: synchronous HTTP fetch, then dispatch to main. ────── */
void fetch_thread(ManifestCallback* cb)
{

    LOG_INFO("Fetcher: worker thread started");

    /* Try each source in order (matches PCL-CE DlSourceLoader pattern) */
    VersionManifest result;
    bool success = false;

    for (int i = 0; i < SOURCE_COUNT; i++) {
        LOG_INFO("Fetcher: trying %s (%s)",
                 SOURCE_NAMES[i], SOURCE_URLS[i]);

        std::string body = pcl::util::http_get_sync(SOURCE_URLS[i]);
        if (body.empty()) {
            LOG_WARN("Fetcher: %s returned empty response",
                     SOURCE_NAMES[i]);
            continue;
        }

        LOG_DBG("Fetcher: %s returned %lu bytes",
                SOURCE_NAMES[i], (unsigned long)body.size());

        result = parse_manifest(body.c_str(), SOURCE_NAMES[i]);
        if (result.loaded) {
            success = true;
            size_t total = 0;
            for (auto& g : result.groups) total += g.versions.size();
            LOG_INFO("Fetcher: %s parse OK — %lu versions total, "
                     "latest_release=%s, latest_snapshot=%s",
                     SOURCE_NAMES[i], (unsigned long)total,
                     result.latest_release.c_str(),
                     result.latest_snapshot.c_str());
            break;
        }

        LOG_WARN("Fetcher: %s parse failed: %s",
                 SOURCE_NAMES[i], result.error.c_str());
    }

    if (!success) {
        result.loaded = false;
        result.error = "所有下载源均请求失败";
        result.source_name = "All";
        LOG_ERR("Fetcher: all sources exhausted, reporting failure");
    }

    /* Dispatch result via the configured dispatcher.
     *   GUI → GtkDispatcher (g_idle_add → GTK main thread)
     *   CLI → SynchronousDispatcher (inline, then promise/future unblocks) */
    pclcore::network::get_dispatcher().dispatch(
        [cb = std::move(*cb), result = std::move(result)]() mutable {
            LOG_INFO("Fetcher: invoking callback (loaded=%d, groups=%lu)",
                     result.loaded, (unsigned long)result.groups.size());
            cb(std::move(result));
        });

    delete cb;
}

}  // anonymous namespace

/* ── Public API ────────────────────────────────────────────────────────── */
void fetch_version_manifest(ManifestCallback callback)
{
    LOG_INFO("Fetcher: fetch_version_manifest called, spawning worker thread");

    /* Copy callback to heap, pass to worker thread.
     * Worker does synchronous HTTP fetch, then dispatches result
     * to the GTK main thread via g_idle_add. */
    auto* cb = new ManifestCallback(std::move(callback));

    try {
        std::thread(fetch_thread, cb).detach();
    } catch (const std::system_error& e) {
        LOG_ERR("Fetcher: std::thread FAILED — cannot start worker thread: %s", e.what());
        /* Fallback: report error directly */
        VersionManifest m;
        m.loaded = false;
        m.error = "无法创建后台线程";
        (*cb)(std::move(m));
        delete cb;
    }
}

/* ── Local April Fool version list loader ─────────────────────────────── */

namespace {

/** Resolve the binary's own directory. */
std::string resolve_binary_dir()
{
#if defined(__linux__) && !defined(__ANDROID__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string exe(buf);
        auto slash = exe.rfind('/');
        if (slash != std::string::npos)
            return exe.substr(0, slash);
    }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string exe(buf, len);
        auto slash = exe.rfind('\\');
        if (slash != std::string::npos)
            return exe.substr(0, slash);
    }
#endif
    return {};
}

/** Check if a regular file exists at `path`. */
static bool file_exists(const std::string& path)
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

/** Find lirpa_loof.json using the same strategy as IconHelper. */
std::string find_lirpa_loof_json()
{
    // 1. Environment variable override
    const char* env = std::getenv("PCL_RES_DIR");
    if (env) {
        std::string path = std::string(env) + "/lirpa_loof.json";
        if (file_exists(path))
            return path;
    }

    // 2. Relative to binary
    std::string bin_dir = resolve_binary_dir();
    if (!bin_dir.empty()) {
        auto build = bin_dir + "/resources/lirpa_loof.json";
        if (file_exists(build))
            return build;
        auto dev = bin_dir + "/../resources/lirpa_loof.json";
        if (file_exists(dev))
            return dev;
    }

    return {};
}

/** Parse lirpa_loof.json into VersionEntry vector.
 *
 *  Expected format:
 *    { "April_Fool_List": { "version_id": { "year": "2026", "desc": "..." }, ... } }
 *
 *  Sort order: newest year first.
 */
std::vector<VersionEntry> parse_lirpa_loof_json(const std::string& body)
{
    std::vector<VersionEntry> result;

    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error& e) {
        LOG_WARN("McVersionFetcher: lirpa_loof.json parse error: %s",
                 e.what());
        return result;
    }

    if (!j.is_object()) return result;

    auto list_it = j.find("April_Fool_List");
    if (list_it == j.end() || !list_it->is_object()) return result;

    for (auto& [ver_id, ver_obj] : list_it->items()) {
        if (!ver_obj.is_object()) continue;

        VersionEntry e;
        e.id   = ver_id;
        e.type = "april_fool";
        e.url  = "";  // no per-version JSON URL for local entries

        std::string year_str = jstr(ver_obj, "year");
        std::string desc_str = jstr(ver_obj, "desc");

        // Synthesize a release_time from year + April 1
        int year = year_str.empty() ? 0 : std::atoi(year_str.c_str());
        if (year > 0) {
            char iso[32];
            snprintf(iso, sizeof(iso), "%04d-04-01T00:00:00+00:00", year);
            e.release_time = iso;
            e.timestamp    = parse_iso8601(iso);
        }

        // Store description in a field — encode it in `url` as a sentinel
        if (!desc_str.empty())
            e.url = std::string("april_fool_local:") + desc_str;

        result.push_back(std::move(e));
    }

    // Sort newest year first
    std::sort(result.begin(), result.end(),
              [](const VersionEntry& a, const VersionEntry& b) {
                  return a.timestamp > b.timestamp;
              });

    return result;
}

}  // anonymous namespace

std::vector<VersionEntry> load_lirpa_loof_versions()
{
    /* Find lirpa_loof.json on disk */
    std::string path = find_lirpa_loof_json();
    if (path.empty()) {
        LOG_WARN("McVersionFetcher: lirpa_loof.json not found");
        return {};
    }

    LOG_INFO("McVersionFetcher: loading lirpa_loof.json from %s", path.c_str());

    // Read file
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        LOG_WARN("McVersionFetcher: cannot read lirpa_loof.json: %s",
                 std::strerror(errno));
        return {};
    }

    std::string contents((std::istreambuf_iterator<char>(ifs)),
                          std::istreambuf_iterator<char>());

    auto result = parse_lirpa_loof_json(contents);

    LOG_INFO("McVersionFetcher: loaded %lu april fool versions",
             (unsigned long)result.size());
    return result;
}

}  // namespace pcl::mc
