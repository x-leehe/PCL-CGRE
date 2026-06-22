#include "core/PluginRegistry.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace pcl {

/* ═══════════════════════════════════════════════════════════════════════
 * 插件入口点 ABI 约定
 *
 * 每个插件 (Linux: .so, Windows: .dll) 必须导出以下 C 符号 (extern "C"):
 *   const char* pcl_plugin_name();         — 插件名称
 *   const char* pcl_plugin_description();  — 简短描述
 *   GtkWidget* pcl_plugin_create_page();   — 创建页面 widget
 *
 * 所有三个均为必选; 缺失任一符号则跳过该插件。
 * ═══════════════════════════════════════════════════════════════════════ */

using PfnName        = const char* (*)();
using PfnDescription = const char* (*)();
using PfnCreatePage  = GtkWidget* (*)();

PluginRegistry& PluginRegistry::instance()
{
    static PluginRegistry reg;
    return reg;
}

std::string PluginRegistry::plugins_dir() const
{
    return std::string(g_get_user_config_dir()) + "/pcl-cgre/plugins";
}

std::vector<std::string> PluginRegistry::discover_files() const
{
    std::vector<std::string> files;
    std::string dir = plugins_dir();

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        LOG_INFO("PluginRegistry: plugins dir not found (%s) — skipping scan",
                 dir.c_str());
        return files;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
#ifdef _WIN32
        if (ext != ".dll") continue;
#else
        if (ext != ".so") continue;
#endif

        auto name = entry.path().filename().string();
        if (name[0] == '.') continue;

        files.push_back(entry.path().string());
    }

    std::sort(files.begin(), files.end());
    return files;
}

PluginDescriptor PluginRegistry::try_load(const std::string& plugin_path)
{
    PluginDescriptor desc;
    desc.plugin_path = plugin_path;

#ifdef _WIN32
    HMODULE handle = LoadLibraryA(plugin_path.c_str());
    if (!handle) {
        LOG_WARN("PluginRegistry: LoadLibraryA(%s) failed — error %lu",
                 plugin_path.c_str(), GetLastError());
        return desc;
    }

    auto pfn_name = reinterpret_cast<PfnName>(
        GetProcAddress(handle, "pcl_plugin_name"));
    if (!pfn_name) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_name' missing in %s",
                 plugin_path.c_str());
        FreeLibrary(handle);
        return desc;
    }

    auto pfn_desc = reinterpret_cast<PfnDescription>(
        GetProcAddress(handle, "pcl_plugin_description"));
    if (!pfn_desc) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_description' missing in %s",
                 plugin_path.c_str());
        FreeLibrary(handle);
        return desc;
    }

    auto pfn_page = reinterpret_cast<PfnCreatePage>(
        GetProcAddress(handle, "pcl_plugin_create_page"));
    if (!pfn_page) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_create_page' missing in %s",
                 plugin_path.c_str());
        FreeLibrary(handle);
        return desc;
    }
#else
    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        LOG_WARN("PluginRegistry: dlopen(%s) failed — %s",
                 plugin_path.c_str(), dlerror());
        return desc;
    }

    auto pfn_name = reinterpret_cast<PfnName>(
        dlsym(handle, "pcl_plugin_name"));
    if (!pfn_name) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_name' missing in %s",
                 plugin_path.c_str());
        dlclose(handle);
        return desc;
    }

    auto pfn_desc = reinterpret_cast<PfnDescription>(
        dlsym(handle, "pcl_plugin_description"));
    if (!pfn_desc) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_description' missing in %s",
                 plugin_path.c_str());
        dlclose(handle);
        return desc;
    }

    auto pfn_page = reinterpret_cast<PfnCreatePage>(
        dlsym(handle, "pcl_plugin_create_page"));
    if (!pfn_page) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_create_page' missing in %s",
                 plugin_path.c_str());
        dlclose(handle);
        return desc;
    }
#endif

    /* 填充描述符 — 捕获函数指针 (handle 保持 open 供后续调用) */
    const char* nm   = pfn_name();
    const char* dscr = pfn_desc();

    desc.name        = nm   ? nm   : "(unnamed)";
    desc.description = dscr ? dscr : "";
    desc.create_page = [handle, pfn_page]() -> GtkWidget* {
        return pfn_page();
    };

    LOG_INFO("PluginRegistry: loaded plugin '%s' from %s",
             desc.name.c_str(), plugin_path.c_str());

    /* 不 dlclose/FreeLibrary — handle 泄漏用于保持符号有效。
     * 这符合插件语义: 它们应该在整个进程生命周期内保持加载。 */
    return desc;
}

std::vector<PluginDescriptor> PluginRegistry::scan_plugins()
{
    std::vector<PluginDescriptor> plugins;
    auto files = discover_files();

    for (const auto& path : files) {
        PluginDescriptor desc = try_load(path);
        if (!desc.name.empty())
            plugins.push_back(std::move(desc));
    }

    std::sort(plugins.begin(), plugins.end());
    LOG_INFO("PluginRegistry: scan complete — %zu plugin(s) loaded",
             plugins.size());
    return plugins;
}

}  // namespace pcl
