#include "core/PluginRegistry.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <cstring>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

namespace pcl {

/* ═══════════════════════════════════════════════════════════════════════
 * 插件入口点 ABI 约定
 *
 * 每个 .so 必须导出以下 C 符号 (extern "C"):
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
    const char* home = g_get_home_dir();
    return std::string(home) + "/.config/pcl-cgre/plugins";
}

std::vector<std::string> PluginRegistry::discover_files() const
{
    std::vector<std::string> files;
    std::string dir = plugins_dir();

    DIR* dp = opendir(dir.c_str());
    if (!dp) {
        LOG_INFO("PluginRegistry: plugins dir not found (%s) — skipping scan",
                 dir.c_str());
        return files;
    }

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        const char* name = entry->d_name;

        /* 只处理 .so 文件 */
        size_t len = strlen(name);
        if (len < 4) continue;
        if (strcmp(name + len - 3, ".so") != 0) continue;

        /* 跳过隐藏文件 */
        if (name[0] == '.') continue;

        files.push_back(dir + "/" + name);
    }
    closedir(dp);

    std::sort(files.begin(), files.end());
    return files;
}

PluginDescriptor PluginRegistry::try_load(const std::string& so_path)
{
    PluginDescriptor desc;
    desc.so_path = so_path;

    /* dlopen */
    void* handle = dlopen(so_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        LOG_WARN("PluginRegistry: dlopen(%s) failed — %s",
                 so_path.c_str(), dlerror());
        return desc;  // name.empty() → caller discards
    }

    /* dlsym: pcl_plugin_name (required) */
    auto pfn_name = reinterpret_cast<PfnName>(
        dlsym(handle, "pcl_plugin_name"));
    if (!pfn_name) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_name' missing in %s",
                 so_path.c_str());
        dlclose(handle);
        return desc;
    }

    /* dlsym: pcl_plugin_description (required) */
    auto pfn_desc = reinterpret_cast<PfnDescription>(
        dlsym(handle, "pcl_plugin_description"));
    if (!pfn_desc) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_description' missing in %s",
                 so_path.c_str());
        dlclose(handle);
        return desc;
    }

    /* dlsym: pcl_plugin_create_page (required) */
    auto pfn_page = reinterpret_cast<PfnCreatePage>(
        dlsym(handle, "pcl_plugin_create_page"));
    if (!pfn_page) {
        LOG_WARN("PluginRegistry: symbol 'pcl_plugin_create_page' missing in %s",
                 so_path.c_str());
        dlclose(handle);
        return desc;
    }

    /* 填充描述符 — 捕获函数指针 (handle 保持 open 供后续调用) */
    const char* nm   = pfn_name();
    const char* dscr = pfn_desc();

    desc.name        = nm   ? nm   : "(unnamed)";
    desc.description = dscr ? dscr : "";
    desc.create_page = [handle, pfn_page]() -> GtkWidget* {
        return pfn_page();
    };

    LOG_INFO("PluginRegistry: loaded plugin '%s' from %s",
             desc.name.c_str(), so_path.c_str());

    /* 不 dlclose — handle 泄漏用于保持符号有效。
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
