#include "app/PageAssembler.hpp"
#include "app/NavigationController.hpp"
#include "pages/LaunchPage.hpp"
#include "pages/SettingsPage.hpp"
#include "pages/MorePage.hpp"
#include "pages/DownloadPage.hpp"
#include "core/Log.hpp"
#include "core/PluginRegistry.hpp"

#include <cstring>
#include <adwaita.h>

namespace pcl {

void assemble_pages(AdwViewStack* stack)
{
    if (!stack) return;

    /* ── 启动页 — 两栏 PGTKDesign 布局 ── */
    GtkWidget* launch_page = build_launch_page();
    if (launch_page)
        adw_view_stack_add_titled(stack, launch_page, "launch", "启动");
    else
        LOG_WARN("PageAssembler: build_launch_page() returned null");

    /* ── 下载页 — 搜索框 + 卡片列表 ── */
    GtkWidget* download_page = build_download_page();
    if (download_page)
        adw_view_stack_add_titled(stack, download_page, "download", "下载");
    else
        LOG_WARN("PageAssembler: build_download_page() returned null");

    /* ── 用户切换到下载页时自动触发版本列表拉取 ── */
    g_signal_connect(stack, "notify::visible-child-name",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer) {
            AdwViewStack* stk = ADW_VIEW_STACK(obj);
            const char* name = adw_view_stack_get_visible_child_name(stk);
            if (!name || strcmp(name, "download") != 0) return;

            GtkWidget* page = adw_view_stack_get_child_by_name(stk, "download");
            if (!page) return;
            trigger_download_page_mc_fetch(page);
        }), nullptr);

    /* ── 设置页 — 完整设置界面 ── */
    GtkWidget* settings_page = build_settings_page();
    if (settings_page)
        adw_view_stack_add_titled(stack, settings_page, "settings", "设置");
    else
        LOG_WARN("PageAssembler: build_settings_page() returned null");

    /* ── 更多 — 帮助与工具箱 ── */
    GtkWidget* more_page = build_more_page();
    if (more_page)
        adw_view_stack_add_titled(stack, more_page, "more", "更多");
    else
        LOG_WARN("PageAssembler: build_more_page() returned null");

    /* ── 插件扫描 — 加载 plugins/ 目录下的 .so 文件 ── */
    {
        auto plugins = PluginRegistry::instance().scan_plugins();
        if (!plugins.empty()) {
            LOG_INFO("PageAssembler: registering %zu plugin(s) to More page",
                     plugins.size());

            /* 插件页面添加到 More 页面的右栏 stack */
            GtkWidget* more_right = NavigationController::instance().more_right();
            if (more_right && GTK_IS_STACK(more_right)) {
                for (size_t i = 0; i < plugins.size(); i++) {
                    auto& p = plugins[i];
                    GtkWidget* page = p.create_page();
                    if (!page) {
                        LOG_WARN("PageAssembler: plugin '%s' returned null page",
                                 p.name.c_str());
                        continue;
                    }

                    std::string page_name = "plugin-" + std::to_string(i);
                    gtk_stack_add_named(GTK_STACK(more_right), page,
                                       page_name.c_str());
                    LOG_INFO("PageAssembler: plugin '%s' page registered as '%s'",
                             p.name.c_str(), page_name.c_str());
                }
            } else {
                LOG_INFO("PageAssembler: more-right stack not available — "
                         "plugin pages skipped");
            }
        }
    }
}

}  // namespace pcl
