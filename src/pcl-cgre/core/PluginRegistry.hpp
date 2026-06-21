#pragma once

#include <functional>
#include <string>
#include <vector>

#include <gtk/gtk.h>

namespace pcl {

/**
 * 插件描述符 — dlopen 加载的单个插件
 */
struct PluginDescriptor {
    std::string name;           // 插件名称
    std::string description;    // 简短描述
    std::string so_path;        // .so 文件完整路径
    std::function<GtkWidget*()> create_page;  // 构建页面 widget

    /** for sort / dedup */
    bool operator<(const PluginDescriptor& o) const { return name < o.name; }
};

/**
 * 插件注册表 — 扫描 / 加载 / 管理共享对象 (.so) 插件
 *
 * 失败保护:
 *   - plugins/ 目录不存在 → scan_plugins() 返回空列表
 *   - 无效 .so → 跳过并记录 warn 日志, 不崩溃
 *   - 符号缺失 → 同上
 *
 * 集中隔离原则: 插件仅存在于 ~/.config/pcl-cgre/plugins/
 */
class PluginRegistry {
public:
    static PluginRegistry& instance();

    /**
     * 扫描插件目录, 尝试 dlopen 每个 .so 并 dlsym 入口点
     *
     * @return 成功加载的插件列表 (已排序去重)
     */
    std::vector<PluginDescriptor> scan_plugins();

    /**
     * 仅扫描目录中的 .so 文件路径, 不执行 dlopen
     *
     * @return .so 文件路径列表
     */
    std::vector<std::string> discover_files() const;

    /** 插件目录完整路径 */
    std::string plugins_dir() const;

private:
    PluginRegistry() = default;
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    /** 尝试加载单个 .so, 成功返回 PluginDescriptor */
    PluginDescriptor try_load(const std::string& so_path);
};

}  // namespace pcl
