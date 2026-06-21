#pragma once

#include <string>
#include <mutex>

#include <curl/curl.h>

#include "core/Log.hpp"

namespace pcl::util {

/** Timeout for all synchronous HTTP requests (seconds).
 *
 *  Aligns with REQUEST_TIMEOUT_S in ResourceFetcher.cpp icon loader.
 *  Without a timeout, a stalled TCP connection (e.g. after a period
 *  of inactivity when server-side keepalive has dropped) would block
 *  the worker thread indefinitely, and the UI would appear to freeze
 *  until the system TCP timeout fires (~30–120 s). */
constexpr int HTTP_SYNC_TIMEOUT_S = 10;

/** Run curl_global_init() exactly once for the whole process, before any
 *  curl_easy_init()/curl_easy_escape() on any thread.
 *
 *  The once_flag is a local static of this *named* inline function, so it
 *  has a single shared instance across every translation unit. An
 *  anonymous-namespace flag would instead be per-TU, letting two TUs race
 *  on libcurl's (non-thread-safe) implicit global init on first use. */
inline void ensure_curl_global()
{
    static std::once_flag flag;
    std::call_once(flag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

namespace {

/** libcurl write callback — appends received data to a std::string. */
inline size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* str = static_cast<std::string*>(userdata);
    str->append(ptr, size * nmemb);
    return size * nmemb;
}

/** Thread-local CURL handle — created lazily per worker thread and reused
 *  for all HTTP requests issued on that thread, so libcurl can keep TCP
 *  connections alive (HTTP/1.1 keep-alive) within a single fetch worker
 *  (e.g. McVersionFetcher tries two source URLs; ResourceFetcher queries
 *  Modrinth + CurseForge back to back).
 *
 *  The handle is owned by a thread_local object whose destructor runs at
 *  thread exit, so curl_easy_cleanup() is always called even for the
 *  short-lived, detached fetch threads — no handle, socket or
 *  connection-cache leak accumulates over the lifetime of the process.
 *
 *  curl_global_init() is guaranteed (via ensure_curl_global) to have run
 *  exactly once before the first handle is created. */
inline CURL* tl_handle()
{
    ensure_curl_global();

    /* RAII owner: ~Handle() fires on thread exit and releases the handle
     * (and its pooled connections / file descriptors). */
    struct Handle {
        CURL* h = nullptr;
        ~Handle() { if (h) curl_easy_cleanup(h); }
    };
    static thread_local Handle owner;

    if (!owner.h) {
        owner.h = curl_easy_init();
        if (owner.h) {
            curl_easy_setopt(owner.h, CURLOPT_TIMEOUT, (long)HTTP_SYNC_TIMEOUT_S);
            curl_easy_setopt(owner.h, CURLOPT_USERAGENT, "PCL-CGRE/0.1");
            curl_easy_setopt(owner.h, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(owner.h, CURLOPT_MAXREDIRS, 5L);
            curl_easy_setopt(owner.h, CURLOPT_WRITEFUNCTION, write_callback);
        }
    }
    return owner.h;
}

}  // anonymous namespace

/**
 * Synchronous HTTP GET. Returns the response body as a string,
 * or an empty string on failure. Logs errors via LOG_WARN.
 *
 * Called from background threads — never from the GTK main thread.
 * Uses a thread-local CURL handle for TCP connection reuse.
 */
inline std::string http_get_sync(const char* url)
{
    CURL* curl = tl_handle();
    if (!curl) {
        LOG_WARN("HttpUtil: curl_easy_init failed — no CURL handle");
        return {};
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);  // no extra headers

    std::string result;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG_WARN("HttpUtil: HTTP error from %s: %s",
                 url, curl_easy_strerror(res));
        return {};
    }

    return result;
}

/**
 * Synchronous HTTP GET with optional API key header (for CurseForge).
 */
inline std::string http_get_sync(const char* url, const char* api_key)
{
    CURL* curl = tl_handle();
    if (!curl) {
        LOG_WARN("HttpUtil: curl_easy_init failed — no CURL handle");
        return {};
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct curl_slist* headers = nullptr;
    if (api_key && *api_key) {
        std::string header = std::string("x-api-key: ") + api_key;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string result;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        LOG_WARN("HttpUtil: HTTP error from %s: %s",
                 url, curl_easy_strerror(res));
    }

    /* Detach the per-request header list from the reused handle before
     * freeing it, so the next request on this thread starts clean. */
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
    if (headers)
        curl_slist_free_all(headers);

    return (res == CURLE_OK) ? result : std::string{};
}

}  // namespace pcl::util
