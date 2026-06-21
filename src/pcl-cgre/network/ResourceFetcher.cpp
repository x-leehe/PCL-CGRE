#include "network/ResourceFetcher.hpp"
#include "network/HttpUtil.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "pclcore/network/Dispatcher.hpp"

using json = nlohmann::json;

namespace {

/** Percent-encode a string using libcurl.
 *  Reuses the shared thread-local transfer handle (pcl::util::tl_handle) so
 *  there is one CURL handle per worker thread for both transfers and
 *  escaping, and curl_global_init() is guaranteed to have run first.
 *  curl_easy_escape() sets no transfer options, so sharing is safe. */
inline std::string url_encode(const std::string& s)
{
    if (s.empty()) return s;
    CURL* curl = pcl::util::tl_handle();
    if (!curl) return s;
    char* enc = curl_easy_escape(curl, s.c_str(), static_cast<int>(s.length()));
    std::string result(enc ? enc : s);
    if (enc) curl_free(enc);
    return result;
}

}  // anonymous namespace

namespace pcl::resource {

namespace {

/* ── Constants ────────────────────────────────────────────────────────────── */
constexpr const char* MODRINTH_SEARCH  = "https://api.modrinth.com/v2/search";
constexpr const char* CURSEFORGE_BASE  = "https://api.curseforge.com/v1";
constexpr int         DEFAULT_LIMIT    = 20;

/* ── Helper: return string value if present and is a string, else "" ─────── */
inline std::string jstr(const json& val) {
    return val.is_string() ? val.get<std::string>() : "";
}

/* ── Helper: return string member by key, or "" if missing/not a string ──── */
inline std::string jstr(const json& obj, const char* key) {
    auto it = obj.find(key);
    return (it != obj.end()) ? jstr(*it) : "";
}

/* ── CurseForge API key from environment ──────────────────────────────────── */
const char* get_curseforge_api_key()
{
    static const char* key = []() -> const char* {
        const char* k = std::getenv("CURSEFORGE_API_KEY");
        if (k && *k) LOG_INFO("ResourceFetcher: CurseForge API key found");
        return k;
    }();
    return key;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Modrinth search URL builder
 * ══════════════════════════════════════════════════════════════════════════ */
std::string build_modrinth_url(const std::string& query,
                               ProjectType        type,
                               const std::string& version_filter,
                               int                offset,
                               int                limit,
                               SortType           sort,
                               CompLoaderType     loader_filter)
{
    std::string url = MODRINTH_SEARCH;

    /* query */
    url += "?query=";
    if (!query.empty()) {
        url += url_encode(query);
    }

    /* facets: type [+ version] [+ loader] */
    const char* facet_type = project_type_to_modrinth_facet(type);
    std::string facets;
    if (facet_type && *facet_type) {
        facets = "[[\"project_type:";
        facets += facet_type;

        /* append version filter */
        if (!version_filter.empty() && strcmp(version_filter.c_str(), "全部版本") != 0) {
            facets += "\",\"versions:'";
            facets += version_filter;
            facets += "'";
        }

        /* append loader filter */
        if (loader_filter != CompLoaderType::Any) {
            const char* ldr_str = nullptr;
            switch (loader_filter) {
                case CompLoaderType::Forge:      ldr_str = "forge";      break;
                case CompLoaderType::Fabric:     ldr_str = "fabric";     break;
                case CompLoaderType::Quilt:      ldr_str = "quilt";      break;
                case CompLoaderType::NeoForge:   ldr_str = "neoforge";   break;
                case CompLoaderType::LiteLoader: ldr_str = "liteloader"; break;
                default: break;
            }
            if (ldr_str) {
                facets += "\",\"categories:'";
                facets += ldr_str;
                facets += "'";
            }
        }

        facets += "\"]]";
    } else if (!version_filter.empty()
               && strcmp(version_filter.c_str(), "全部版本") != 0) {
        facets = "[[\"versions:'";
        facets += version_filter;
        facets += "'\"]]";
    }

    if (!facets.empty()) {
        url += "&facets=";
        url += url_encode(facets);
    }

    url += "&offset=" + std::to_string(offset);
    url += "&limit="  + std::to_string(limit > 0 ? limit : DEFAULT_LIMIT);

    /* ── Sort ─────────────────────────────────────────────────────── */
    switch (sort) {
        case SortType::Relevance: url += "&index=relevance";          break;
        case SortType::Downloads: url += "&index=downloads";          break;
        case SortType::Follows:   url += "&index=follows";            break;
        case SortType::Newest:    url += "&index=newest";             break;
        case SortType::Updated:   url += "&index=updated";            break;
        default: break;  // Modrinth default = relevance
    }

    return url;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CurseForge search URL builder
 * ══════════════════════════════════════════════════════════════════════════ */
std::string build_curseforge_url(const std::string& query,
                                 ProjectType        type,
                                 const std::string& version_filter,
                                 int                offset,
                                 int                limit,
                                 SortType           sort,
                                 CompLoaderType     loader_filter)
{
    std::string url = CURSEFORGE_BASE;
    url += "/mods/search?gameId=432";

    if (!query.empty()) {
        url += "&searchFilter=";
        url += url_encode(query);
    }

    int class_id = project_type_to_curseforge_class(type);
    if (class_id > 0)
        url += "&classId=" + std::to_string(class_id);

    /* ── Sort ─────────────────────────────────────────────────────── */
    // CurseForge sortField values:
    //   1=Featured, 2=Popularity, 3=LastUpdated, 4=Name,
    //   5=Author, 6=TotalDownloads, 7=Category, 8=GameVersion,
    //   11=DateCreated
    switch (sort) {
        case SortType::Downloads: url += "&sortField=6";  break;
        case SortType::Follows:   url += "&sortField=2";  break;  // Popularity ~= Follows
        case SortType::Newest:    url += "&sortField=11"; break;  // DateCreated
        case SortType::Updated:   url += "&sortField=3";  break;  // LastUpdated
        case SortType::Relevance: url += "&sortField=4";  break;  // Name (closest to relevance)
        default: url += "&sortField=2"; break;  // Default: Popularity
    }
    url += "&sortOrder=desc";

    url += "&index="   + std::to_string(offset);
    url += "&pageSize=" + std::to_string(limit > 0 ? limit : DEFAULT_LIMIT);

    if (!version_filter.empty() && strcmp(version_filter.c_str(), "全部版本") != 0) {
        url += "&gameVersion=";
        url += url_encode(version_filter);
    }

    /* ── Loader filter ────────────────────────────────────────────── */
    if (loader_filter != CompLoaderType::Any) {
        int cf_loader = 0;
        switch (loader_filter) {
            case CompLoaderType::Forge:    cf_loader = 1; break;
            case CompLoaderType::Fabric:   cf_loader = 4; break;
            case CompLoaderType::Quilt:    cf_loader = 5; break;
            case CompLoaderType::NeoForge: cf_loader = 6; break;
            default: break;
        }
        if (cf_loader > 0)
            url += "&modLoaderType=" + std::to_string(cf_loader);
    }

    return url;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  JSON parsers
 * ══════════════════════════════════════════════════════════════════════════ */

SearchResult parse_modrinth_response(const char* body)
{
    SearchResult result;

    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error& e) {
        result.error = "JSON 解析失败";
        LOG_WARN("ResourceFetcher: Modrinth JSON parse error: %s", e.what());
        return result;
    }

    if (!j.is_object()) {
        result.error = "响应格式不正确";
        return result;
    }

    /* total_hits */
    auto total_it = j.find("total_hits");
    if (total_it != j.end() && total_it->is_number())
        result.total_hits = total_it->get<int>();
    result.modrinth_total = result.total_hits;

    /* hits[] */
    auto hits_it = j.find("hits");
    if (hits_it != j.end() && hits_it->is_array()) {
        for (auto& hit : *hits_it) {
            if (!hit.is_object()) continue;

            ResourceEntry e;
            e.title         = jstr(hit, "title");
            e.description   = jstr(hit, "description");
            e.icon_url      = jstr(hit, "icon_url");
            e.slug          = jstr(hit, "slug");
            e.project_id    = jstr(hit, "project_id");
            e.download_count = hit.value("downloads", 0);
            e.followers      = hit.value("followers", 0);
            e.date_modified = jstr(hit, "date_modified");
            e.source        = Source::Modrinth;
            /* project_url: https://modrinth.com/<type>/<slug> */
            {
                std::string pt = jstr(hit, "project_type");
                e.project_url = std::string("https://modrinth.com/") +
                                (pt.empty() ? "mod" : pt) + "/" + e.slug;
            }

            /* author */
            e.author = jstr(hit, "author");

            /* color */
            auto color_it = hit.find("color");
            if (color_it != hit.end() && color_it->is_number())
                e.color = color_it->get<int>();

            /* client_side / server_side */
            e.client_side = jstr(hit, "client_side");
            e.server_side = jstr(hit, "server_side");

            /* license */
            auto lic_it = hit.find("license");
            if (lic_it != hit.end() && lic_it->is_object())
                e.license_name = jstr(*lic_it, "name");

            /* categories */
            auto cats_it = hit.find("categories");
            if (cats_it != hit.end() && cats_it->is_array()) {
                for (auto& cat : *cats_it)
                    e.categories.push_back(jstr(cat));
            }

            /* game_versions (latest from versions[]) */
            auto vers_it = hit.find("versions");
            if (vers_it != hit.end() && vers_it->is_array()) {
                for (auto& v : *vers_it)
                    e.game_versions.push_back(jstr(v));
                if (!e.game_versions.empty())
                    e.version_range = e.game_versions.back();
            }

            /* loaders */
            if (cats_it != hit.end() && cats_it->is_array()) {
                for (auto& cat : *cats_it) {
                    std::string cat_str = jstr(cat);
                    if (cat_str == "forge")      e.loaders.push_back(CompLoaderType::Forge);
                    else if (cat_str == "fabric") e.loaders.push_back(CompLoaderType::Fabric);
                    else if (cat_str == "quilt")  e.loaders.push_back(CompLoaderType::Quilt);
                    else if (cat_str == "neoforge") e.loaders.push_back(CompLoaderType::NeoForge);
                }
            }

            /* wiki / source / discord URLs */
            e.wiki_url    = jstr(hit, "wiki_url");
            e.source_url  = jstr(hit, "source_url");
            e.discord_url = jstr(hit, "discord_url");

            /* gallery (screenshots) */
            auto gal_it = hit.find("gallery");
            if (gal_it != hit.end() && gal_it->is_array()) {
                for (auto& img : *gal_it) {
                    if (img.is_object())
                        e.screenshot_urls.push_back(jstr(img, "url"));
                }
            }

            result.hits.push_back(std::move(e));
        }
    }

    result.success = true;
    result.modrinth_offset = result.hits.size();
    return result;
}

SearchResult parse_curseforge_response(const char* body)
{
    SearchResult result;

    json j;
    try {
        j = json::parse(body);
    } catch (const json::parse_error& e) {
        result.error = "JSON 解析失败";
        LOG_WARN("ResourceFetcher: CurseForge JSON parse error: %s", e.what());
        return result;
    }

    if (!j.is_object()) {
        result.error = "响应格式不正确";
        return result;
    }

    /* pagination.totalCount */
    auto pag_it = j.find("pagination");
    if (pag_it != j.end() && pag_it->is_object()) {
        auto tc_it = pag_it->find("totalCount");
        if (tc_it != pag_it->end() && tc_it->is_number())
            result.total_hits = tc_it->get<int>();
    }

    /* data[] */
    auto data_it = j.find("data");
    if (data_it != j.end() && data_it->is_array()) {
        for (auto& item : *data_it) {
            if (!item.is_object()) continue;

            ResourceEntry e;
            e.title         = jstr(item, "name");
            e.description   = jstr(item, "summary");
            e.project_id    = std::to_string(item.value("id", 0));
            e.slug          = jstr(item, "slug");
            e.download_count = item.value("downloadCount", 0);
            e.date_modified = jstr(item, "dateModified");
            e.project_url   = std::string("https://www.curseforge.com/minecraft/mc-mods/") + e.slug;
            e.source        = Source::CurseForge;

            /* logo.thumbnailUrl */
            auto logo_it = item.find("logo");
            if (logo_it != item.end() && logo_it->is_object())
                e.icon_url = jstr(*logo_it, "thumbnailUrl");

            /* first file's gameVersions[0] as version_range */
            auto lf_it = item.find("latestFiles");
            if (lf_it != item.end() && lf_it->is_array() && !lf_it->empty()) {
                auto& file = (*lf_it)[0];
                if (file.is_object()) {
                    auto gv_it = file.find("gameVersions");
                    if (gv_it != file.end() && gv_it->is_array() && !gv_it->empty())
                        e.version_range = jstr((*gv_it)[0]);
                }
            }

            /* icon will be loaded lazily on the main thread */
            result.hits.push_back(std::move(e));
        }
    }

    result.success = true;
    result.curseforge_offset = static_cast<int>(result.hits.size());
    return result;
}

struct SearchThreadData {
    SearchCallback callback;
    std::string    query;
    ProjectType    project_type;
    Source         source;
    std::string    version_filter;
    int            offset;
    int            limit;
    SortType       sort;
    CompLoaderType loader_filter;
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Worker thread entry point
 * ══════════════════════════════════════════════════════════════════════════ */
void search_thread(SearchThreadData* td)
{

    LOG_INFO("ResourceFetcher: worker thread started "
             "(query='%s', type=%d, source=%d, ver='%s')",
             td->query.c_str(), (int)td->project_type, (int)td->source,
             td->version_filter.c_str());

    SearchResult combined;
    bool        any_success = false;

    /* ── Modrinth ──────────────────────────────────────────────────────── */
    bool try_modrinth = (td->source == Source::All || td->source == Source::Modrinth)
                     && project_type_to_modrinth_facet(td->project_type) != nullptr;

    if (try_modrinth) {
        std::string url = build_modrinth_url(
            td->query, td->project_type, td->version_filter,
            td->offset, td->limit, td->sort, td->loader_filter);
        LOG_INFO("ResourceFetcher: Modrinth URL → %s", url.c_str());

        std::string body = pcl::util::http_get_sync(url.c_str());
        if (!body.empty()) {
            SearchResult mr = parse_modrinth_response(body.c_str());
            if (mr.success) {
                combined.hits.insert(combined.hits.end(),
                    std::make_move_iterator(mr.hits.begin()),
                    std::make_move_iterator(mr.hits.end()));
                combined.total_hits += mr.total_hits;
                any_success = true;
                LOG_INFO("ResourceFetcher: Modrinth OK — %lu hits",
                         (unsigned long)mr.hits.size());
            } else {
                LOG_WARN("ResourceFetcher: Modrinth parse failed: %s",
                         mr.error.c_str());
            }
        } else {
            LOG_WARN("ResourceFetcher: Modrinth returned empty body");
        }
    }

    /* ── CurseForge ────────────────────────────────────────────────────── */
    bool try_cf = (td->source == Source::All || td->source == Source::CurseForge)
               && (get_curseforge_api_key() != nullptr);

    if (try_cf) {
        std::string url = build_curseforge_url(
            td->query, td->project_type, td->version_filter,
            td->offset, td->limit, td->sort, td->loader_filter);
        LOG_INFO("ResourceFetcher: CurseForge URL → %s", url.c_str());

        std::string body = pcl::util::http_get_sync(url.c_str(), get_curseforge_api_key());
        if (!body.empty()) {
            SearchResult cf = parse_curseforge_response(body.c_str());
            if (cf.success) {
                combined.hits.insert(combined.hits.end(),
                    std::make_move_iterator(cf.hits.begin()),
                    std::make_move_iterator(cf.hits.end()));
                combined.total_hits += cf.total_hits;
                any_success = true;
                LOG_INFO("ResourceFetcher: CurseForge OK — %lu hits",
                         (unsigned long)cf.hits.size());
            } else {
                LOG_WARN("ResourceFetcher: CurseForge parse failed: %s",
                         cf.error.c_str());
            }
        } else {
            LOG_WARN("ResourceFetcher: CurseForge returned empty body");
        }
    } else if (td->source == Source::All || td->source == Source::CurseForge) {
        LOG_INFO("ResourceFetcher: CURSEFORGE_API_KEY not set, skipping CurseForge");
    }

    /* ── Build final result ────────────────────────────────────────────── */
    if (!any_success) {
        combined.success = false;
        combined.error   = "所有下载源均请求失败";
    } else {
        combined.success = true;
        /* sort combined hits by download_count descending */
        std::sort(combined.hits.begin(), combined.hits.end(),
                  [](const ResourceEntry& a, const ResourceEntry& b) {
                      return a.download_count > b.download_count;
                  });
    }

    /* ── Dispatch to main thread ───────────────────────────────────────── */
    pclcore::network::get_dispatcher().dispatch(
        [cb = std::move(td->callback), result = std::move(combined)]() mutable {
            LOG_INFO("ResourceFetcher: invoking callback on main thread "
                     "(success=%d, hits=%lu, total=%lu)",
                     result.success,
                     (unsigned long)result.hits.size(),
                     (unsigned long)result.total_hits);
            cb(std::move(result));
        });

    delete td;
}

}  // anonymous namespace

/* ═══════════════════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════════════════ */

void search_resources(const std::string&  query,
                      ProjectType         type,
                      Source              source,
                      const std::string&  version_filter,
                      int                 offset,
                      int                 limit,
                      SortType            sort,
                      CompLoaderType      loader_filter,
                      SearchCallback      cb)
{
    LOG_INFO("ResourceFetcher: search_resources called — spawning worker thread "
             "(sort=%d, loader=%d)", (int)sort, (int)loader_filter);

    auto* td = new SearchThreadData{
        std::move(cb),
        query,
        type,
        source,
        version_filter,
        offset,
        limit,
        sort,
        loader_filter,
    };

    try {
        std::thread(search_thread, td).detach();
    } catch (const std::system_error& e) {
        LOG_ERR("ResourceFetcher: std::thread FAILED: %s", e.what());
        SearchResult err;
        err.success = false;
        err.error   = "无法创建后台线程";
        td->callback(std::move(err));
        delete td;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Parallel icon loader with disk cache
 *
 *  - Cache dir: /tmp/PCL-CGRE/Icons/<hash>.png
 *  - N worker threads fetch URLs from a shared atomic index.
 *  - 60 s overall deadline; 10 s per-request timeout.
 * ══════════════════════════════════════════════════════════════════════════ */

namespace {

constexpr int         ICON_PARALLEL     = 6;   // worker threads
constexpr int         ICON_MAX_BYTES    = 1'000'000;
constexpr int         REQUEST_TIMEOUT_S = 10;
constexpr auto        ICON_DEADLINE     = std::chrono::seconds(60);

/* ── Per-user icon cache directory ──────────────────────────────────────
 *  Prefer $XDG_CACHE_HOME, then ~/.cache; fall back to /tmp only when the
 *  environment exposes neither. A fixed /tmp path collides between users on
 *  a shared host (the first user owns it; others silently fail to write and
 *  re-download every icon). Resolved once and cached. */
const std::string& cache_dir()
{
    static const std::string dir = []() -> std::string {
        const char* xdg = std::getenv("XDG_CACHE_HOME");
        if (xdg && *xdg) return std::string(xdg) + "/PCL-CGRE/Icons";
        const char* home = std::getenv("HOME");
        if (home && *home) return std::string(home) + "/.cache/PCL-CGRE/Icons";
        return "/tmp/PCL-CGRE/Icons";
    }();
    return dir;
}

/* ── djb2 hash, 64-bit (deterministic across runs) ──────────────────────
 *  64 bits keeps cache-filename collisions (→ wrong icon served) negligible
 *  over the lifetime of the on-disk cache. */
uint64_t hash_url(const std::string& url)
{
    uint64_t h = 5381;
    for (unsigned char c : url) h = ((h << 5) + h) + c;
    return h;
}

/* ── libcurl write callback for binary data (std::vector<uint8_t>) ─────
 *  Must be a named function, NOT a lambda — curl_easy_setopt uses C
 *  variadic (...), and lambdas do not decay to function pointers there. */
size_t icon_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* v = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total = size * nmemb;
    v->insert(v->end(), reinterpret_cast<uint8_t*>(ptr),
              reinterpret_cast<uint8_t*>(ptr) + total);
    return total;
}

std::string cache_path_for(const std::string& url)
{
    char hex[20];
    snprintf(hex, sizeof(hex), "%016llx",
             static_cast<unsigned long long>(hash_url(url)));
    return cache_dir() + "/" + hex + ".png";
}

/* ── Ensure cache directory exists (idempotent, thread-safe via call_once) ── */
void ensure_cache_dir()
{
    static std::once_flag dir_flag;
    std::call_once(dir_flag, []() {
        std::error_code ec;
        std::filesystem::create_directories(cache_dir(), ec);
        if (!ec)
            LOG_DBG("ResourceFetcher: cache dir %s ready", cache_dir().c_str());
    });
}

/* ── Shared state for worker threads ────────────────────────────────── */

struct IconWork {
    const std::vector<std::string>*           urls;
    IconCallback                              on_icon;
    const char*                               api_key;
    std::atomic<int>                          next_index{0};
    std::chrono::steady_clock::time_point     deadline;
    std::atomic<int>                          loaded{0};
    std::atomic<int>                          failed{0};
};

/* ── Download one icon (with cache), dispatch to main thread.
 *    `curl` is a per-worker handle reused across icons for HTTP keep-alive
 *    (its static options are configured once in icon_worker). ────────── */
void fetch_and_dispatch(IconWork* w, int i, CURL* curl)
{
    const std::string& url = (*w->urls)[i];
    if (url.empty()) return;

    std::vector<uint8_t> data;
    std::string cache_path = cache_path_for(url);

    /* 1. Try cache — paths are unique per URL hash, so concurrent reads of
     *    distinct files need no shared lock; the atomic-rename writes below
     *    guarantee a reader never observes a torn file. */
    {
        std::ifstream ifs(cache_path, std::ios::binary);
        if (ifs) {
            data.assign(std::istreambuf_iterator<char>(ifs),
                        std::istreambuf_iterator<char>());
            if (data.size() > ICON_MAX_BYTES)
                data.clear();
        }
    }

    /* 2. Download if not cached */
    if (data.empty() && curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

        struct curl_slist* headers = nullptr;
        if (w->api_key &&
            url.find("curseforge.com") != std::string::npos) {
            std::string hdr = std::string("x-api-key: ") + w->api_key;
            headers = curl_slist_append(headers, hdr.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        std::vector<uint8_t> raw;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &raw);

        CURLcode res = curl_easy_perform(curl);

        /* Detach + free the per-request header list before the next reuse. */
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
        if (headers)
            curl_slist_free_all(headers);

        if (res == CURLE_OK && !raw.empty() && raw.size() < ICON_MAX_BYTES) {
            data = std::move(raw);
            w->loaded.fetch_add(1);
            LOG_DBG("ResourceFetcher: icon[%d] OK — %zu bytes", i, data.size());

            /* Save to cache: write a per-(batch,index) temp file then
             * atomically rename, so parallel workers never produce a torn
             * .png and no global write lock is needed. */
            ensure_cache_dir();
            std::string tmp = cache_path + ".tmp." +
                std::to_string(reinterpret_cast<uintptr_t>(w)) + "." +
                std::to_string(i);
            std::ofstream ofs(tmp, std::ios::binary);
            if (ofs) {
                ofs.write(reinterpret_cast<const char*>(data.data()),
                          static_cast<std::streamsize>(data.size()));
                ofs.close();
                std::error_code ec;
                std::filesystem::rename(tmp, cache_path, ec);
                if (ec) std::filesystem::remove(tmp, ec);
            }
        } else if (res != CURLE_OK) {
            LOG_INFO("ResourceFetcher: icon[%d] HTTP error: %s",
                     i, curl_easy_strerror(res));
            w->failed.fetch_add(1);
        } else {
            w->failed.fetch_add(1);
        }
    } else if (!data.empty()) {
        w->loaded.fetch_add(1);
        LOG_DBG("ResourceFetcher: icon[%d] cache hit — %zu bytes",
                i, data.size());
    } else {
        /* no cache hit and no usable handle */
        w->failed.fetch_add(1);
    }

    /* 3. Dispatch to main thread via dispatcher */
    if (!data.empty()) {
        auto cb_copy = w->on_icon;
        pclcore::network::get_dispatcher().dispatch(
            [cb_copy, data = std::move(data), i]() mutable {
                cb_copy(i, std::move(data));
            });
    }
}

/* ── Worker thread: one reused CURL handle, grab indices, fetch, repeat ─ */
void icon_worker(IconWork* w)
{
    int n = static_cast<int>(w->urls->size());

    /* One handle per worker, reused across every icon this worker pulls so
     * keep-alive holds to the (few) icon CDN hosts. Released at the end. */
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)REQUEST_TIMEOUT_S);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "PCL-CGRE/0.1");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
        /* Write callback for binary data (named function, not lambda —
         * curl_easy_setopt uses C variadic and lambdas don't decay to
         * function pointers there). */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, icon_write_cb);
    }

    for (;;) {
        if (std::chrono::steady_clock::now() > w->deadline) break;
        int i = w->next_index.fetch_add(1);
        if (i >= n) break;
        fetch_and_dispatch(w, i, curl);
    }

    if (curl) curl_easy_cleanup(curl);
}

}  // anonymous namespace

/* ── Coordinator thread: spawns workers, joins them, cleans up ──────── */
void icon_coordinator(IconWork* w)
{
    auto* urls_heap = const_cast<std::vector<std::string>*>(w->urls);

    std::vector<std::thread> workers;
    workers.reserve(ICON_PARALLEL);
    for (int t = 0; t < ICON_PARALLEL; t++) {
        try {
            workers.emplace_back(icon_worker, w);
        } catch (const std::system_error& e) {
            LOG_ERR("ResourceFetcher: failed to spawn icon worker %d: %s",
                    t, e.what());
        }
    }

    for (auto& wt : workers) {
        if (wt.joinable())
            wt.join();
    }

    LOG_INFO("ResourceFetcher: icon load done — %d loaded, %d failed, "
             "%zu total",
             w->loaded.load(),
             w->failed.load(),
             urls_heap->size());

    delete urls_heap;
    delete w;
}

void load_icons_async(std::vector<std::string> urls, IconCallback on_icon)
{
    size_t non_empty = 0;
    for (auto& u : urls) if (!u.empty()) non_empty++;
    LOG_INFO("ResourceFetcher: load_icons_async — %zu urls (%zu non-empty), "
             "%d workers", urls.size(), non_empty, ICON_PARALLEL);

    if (urls.empty()) return;

    /* Initialise libcurl once, on this (main) thread, before any icon worker
     * thread calls curl_easy_init() — keeps the implicit global init off the
     * worker threads where it would race. */
    pcl::util::ensure_curl_global();

    ensure_cache_dir();

    auto* urls_heap = new std::vector<std::string>(std::move(urls));

    auto* w = new IconWork{
        urls_heap,
        std::move(on_icon),
        get_curseforge_api_key(),
        std::atomic<int>{0},
        std::chrono::steady_clock::now() + ICON_DEADLINE,
        std::atomic<int>{0},
        std::atomic<int>{0},
    };

    // Fire-and-forget: coordinator spawns workers, joins, cleans up.
    try {
        std::thread(icon_coordinator, w).detach();
    } catch (const std::system_error& e) {
        LOG_ERR("ResourceFetcher: std::thread for coordinator failed: %s",
                e.what());
        delete urls_heap;
        delete w;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Formatting helpers
 * ══════════════════════════════════════════════════════════════════════════ */

std::string format_download_count(uint64_t count)
{
    char buf[32];
    if (count >= 1'000'000'000) {
        snprintf(buf, sizeof(buf), "%.1fB", count / 1'000'000'000.0);
    } else if (count >= 1'000'000) {
        snprintf(buf, sizeof(buf), "%.1fM", count / 1'000'000.0);
    } else if (count >= 1'000) {
        snprintf(buf, sizeof(buf), "%.1fK", count / 1'000.0);
    } else {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)count);
    }
    return buf;
}

std::string format_date(const char* iso8601)
{
    if (!iso8601 || !*iso8601) return "—";
    std::string s(iso8601);
    if (s.size() >= 10) return s.substr(0, 10);
    return s;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  CompFavorites — persistent favourites system
 * ══════════════════════════════════════════════════════════════════════════ */

std::vector<FavData> CompFavorites::s_favorites;
std::mutex           CompFavorites::s_mutex;
std::string          CompFavorites::s_file_path;
bool                 CompFavorites::s_loaded = false;

static std::string get_favorites_file_path()
{
    std::string dir;
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        dir = xdg;
    else {
        const char* home = std::getenv("HOME");
        dir = home ? std::string(home) + "/.config" : "/tmp";
    }
    dir += "/PCL-CGRE";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir + "/favorites.json";
}

void CompFavorites::load()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_loaded) return;
    s_loaded = true;

    s_file_path = get_favorites_file_path();

    /* Read JSON file via std::ifstream */
    std::ifstream ifs(s_file_path);
    if (!ifs.is_open()) {
        LOG_INFO("CompFavorites: no favorites file at %s, starting fresh",
                 s_file_path.c_str());
        /* Ensure at least one default collection */
        FavData def;
        def.name = "默认收藏夹";
        def.id   = "default";
        s_favorites.push_back(std::move(def));
        save();
        return;
    }

    std::string raw((std::istreambuf_iterator<char>(ifs)),
                     std::istreambuf_iterator<char>());

    json j;
    try {
        j = json::parse(raw);
    } catch (const json::parse_error& e) {
        LOG_WARN("CompFavorites: JSON parse error: %s", e.what());
        return;
    }

    if (j.is_array()) {
        for (auto& obj : j) {
            if (!obj.is_object()) continue;

            FavData fd;
            fd.name = jstr(obj, "name");
            fd.id   = jstr(obj, "id");

            auto favs_it = obj.find("favs");
            if (favs_it != obj.end() && favs_it->is_array()) {
                for (auto& f : *favs_it)
                    fd.favs.push_back(jstr(f));
            }

            auto notes_it = obj.find("notes");
            if (notes_it != obj.end() && notes_it->is_object()) {
                for (auto& [key, val] : notes_it->items())
                    fd.notes[key] = jstr(val);
            }

            s_favorites.push_back(std::move(fd));
        }
    }

    if (s_favorites.empty()) {
        FavData def;
        def.name = "默认收藏夹";
        def.id   = "default";
        s_favorites.push_back(std::move(def));
    }

    LOG_INFO("CompFavorites: loaded %zu collections", s_favorites.size());
}

void CompFavorites::save()
{
    if (s_file_path.empty()) return;

    json j = json::array();

    for (auto& fd : s_favorites) {
        json obj;
        obj["name"] = fd.name;
        obj["id"]   = fd.id;

        json favs = json::array();
        for (auto& f : fd.favs)
            favs.push_back(f);
        obj["favs"] = std::move(favs);

        if (!fd.notes.empty()) {
            json notes;
            for (auto& [k, v] : fd.notes)
                notes[k] = v;
            obj["notes"] = std::move(notes);
        }

        j.push_back(std::move(obj));
    }

    std::string out = j.dump(2);  // pretty-print with 2-space indent
    std::ofstream ofs(s_file_path);
    if (ofs.is_open()) {
        ofs << out;
    }
}

bool CompFavorites::is_favorited(const std::string& project_id)
{
    if (!s_loaded) load();
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& fd : s_favorites)
        for (auto& f : fd.favs)
            if (f == project_id) return true;
    return false;
}

bool CompFavorites::toggle(const std::string& project_id)
{
    if (!s_loaded) load();
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_favorites.empty()) {
        FavData def;
        def.name = "默认收藏夹";
        def.id   = "default";
        s_favorites.push_back(std::move(def));
    }

    auto& favs = s_favorites[0].favs;
    auto it = std::find(favs.begin(), favs.end(), project_id);
    if (it != favs.end()) {
        favs.erase(it);
        save();
        return false;  // removed
    }

    favs.push_back(project_id);
    save();
    return true;  // added
}

const std::vector<FavData>& CompFavorites::collections()
{
    if (!s_loaded) load();
    return s_favorites;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  fetch_project_files — Modrinth version list API
 * ══════════════════════════════════════════════════════════════════════════ */
void fetch_project_files(const std::string& project_id,
                         bool               from_cf,
                         FilesCallback      cb)
{
    if (from_cf) {
        /* CurseForge file list requires API key */
        if (!get_curseforge_api_key()) {
            cb({});
            return;
        }

        std::string url = std::string(CURSEFORGE_BASE) +
                          "/mods/" + project_id + "/files?pageSize=50";
        /* Fetch synchronously in background thread */
        auto* data_pair = new auto(std::make_pair(std::move(url), std::move(cb)));
        try {
            std::thread([](std::pair<std::string, FilesCallback>* pair) {
                std::string body = pcl::util::http_get_sync(pair->first.c_str(),
                                                  get_curseforge_api_key());
                std::vector<CompFile> files;

                if (!body.empty()) {
                    try {
                        json j = json::parse(body);
                        auto data_it = j.find("data");
                        if (data_it != j.end() && data_it->is_array()) {
                            for (auto& fobj : *data_it) {
                                if (!fobj.is_object()) continue;
                                CompFile cf;
                                cf.file_name    = jstr(fobj, "displayName");
                                cf.download_url = jstr(fobj, "downloadUrl");
                                cf.file_size    = fobj.value("fileLength", 0);
                                cf.downloads    = fobj.value("downloadCount", 0);
                                cf.date         = jstr(fobj, "fileDate");
                                cf.file_id      = std::to_string(fobj.value("id", 0));

                                auto gv_it = fobj.find("gameVersions");
                                if (gv_it != fobj.end() && gv_it->is_array()) {
                                    for (auto& v : *gv_it)
                                        cf.game_versions.push_back(jstr(v));
                                    if (!cf.game_versions.empty())
                                        cf.game_version = cf.game_versions[0];
                                }
                                files.push_back(std::move(cf));
                            }
                        }
                    } catch (const json::parse_error& e) {
                        LOG_WARN("ResourceFetcher: CurseForge files JSON parse error: %s", e.what());
                    }
                }

                auto cb_copy = std::move(pair->second);
                auto files_copy = std::move(files);
                pclcore::network::get_dispatcher().dispatch(
                    [cb_copy, files_copy = std::move(files_copy)]() mutable {
                        cb_copy(std::move(files_copy));
                    });
                delete pair;
            }, data_pair).detach();
        } catch (const std::system_error& e) {
            LOG_ERR("ResourceFetcher: std::thread for cf-files failed: %s", e.what());
            cb({});
            delete data_pair;
        }
        return;
    }

    /* ── Modrinth ───────────────────────────────────────────────────── */
    std::string url = std::string("https://api.modrinth.com/v2/project/") +
                      project_id + "/version";
    auto* data_pair = new auto(std::make_pair(std::move(url), std::move(cb)));
    try {
        std::thread([](std::pair<std::string, FilesCallback>* pair) {
            std::string body = pcl::util::http_get_sync(pair->first.c_str());
            std::vector<CompFile> files;

            if (!body.empty()) {
                try {
                    json j = json::parse(body);
                    if (j.is_array()) {
                        for (auto& vobj : j) {
                            if (!vobj.is_object()) continue;
                            CompFile cf;
                            cf.file_name = jstr(vobj, "name");
                            cf.file_id   = jstr(vobj, "id");
                            cf.date      = jstr(vobj, "date_published");

                            auto fls_it = vobj.find("files");
                            if (fls_it != vobj.end() && fls_it->is_array() && !fls_it->empty()) {
                                auto& f0 = (*fls_it)[0];
                                if (f0.is_object()) {
                                    cf.download_url = jstr(f0, "url");
                                    cf.file_size    = f0.value("size", 0);
                                }
                            }

                            auto gv_it = vobj.find("game_versions");
                            if (gv_it != vobj.end() && gv_it->is_array()) {
                                for (auto& v : *gv_it)
                                    cf.game_versions.push_back(jstr(v));
                                if (!cf.game_versions.empty())
                                    cf.game_version = cf.game_versions[0];
                            }

                            auto lds_it = vobj.find("loaders");
                            if (lds_it != vobj.end() && lds_it->is_array()) {
                                for (auto& ld : *lds_it) {
                                    std::string ld_str = jstr(ld);
                                    if (ld_str == "forge")     cf.mod_loaders.push_back(CompLoaderType::Forge);
                                    else if (ld_str == "fabric") cf.mod_loaders.push_back(CompLoaderType::Fabric);
                                    else if (ld_str == "quilt")  cf.mod_loaders.push_back(CompLoaderType::Quilt);
                                    else if (ld_str == "neoforge") cf.mod_loaders.push_back(CompLoaderType::NeoForge);
                                }
                            }
                            files.push_back(std::move(cf));
                        }
                    }
                } catch (const json::parse_error& e) {
                    LOG_WARN("ResourceFetcher: Modrinth files JSON parse error: %s", e.what());
                }
            }

            auto cb_copy = std::move(pair->second);
            auto files_copy = std::move(files);
            pclcore::network::get_dispatcher().dispatch(
                [cb_copy, files_copy = std::move(files_copy)]() mutable {
                    cb_copy(std::move(files_copy));
                });
            delete pair;
        }, data_pair).detach();
    } catch (const std::system_error& e) {
        LOG_ERR("ResourceFetcher: std::thread for mr-files failed: %s", e.what());
        cb({});
        delete data_pair;
    }
}

}  // namespace pcl::resource
