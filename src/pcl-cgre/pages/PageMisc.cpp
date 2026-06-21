#include "pages/PageMisc.hpp"
#include "pages/LaunchPage.hpp"
#include "core/ConfigManager.hpp"
#include "util/IconHelper.hpp"
#include "widgets/FormWidgets.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <gtk/gtk.h>

namespace pcl {

/* ============================================================================
 * P2 — build_page_misc: 杂项 (3 卡片, 11 项)
 * ============================================================================ */

GtkWidget* build_page_misc()
{
    using CM = ConfigManager;
    auto& cfg = CM::instance();

    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    /* ── Card 1: 系统 ──────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 公告显示 */
        {
            const char* labels[] = {"全部公告", "仅重要", "关闭"};
            const char* vals[]   = {"all", "important", "off"};
            std::string cur = cfg.get_or<std::string>("launcher.system.announcements", "all");
            int sel = (cur == "important") ? 1 : (cur == "off") ? 2 : 0;
            gtk_box_append(GTK_BOX(inner),
                labeled_row("公告显示", make_dropdown_str(labels, vals, 3, sel, "launcher.system.announcements")));
        }

        /* 最大帧率 */
        {
            int v = cfg.get_or<int>("launcher.system.max_fps", 60);
            GtkWidget* sr = make_slider_entry("launcher.system.max_fps", 1, 240, 1, v, "%.0f");
            gtk_box_append(GTK_BOX(inner), labeled_row("最大帧率", sr));
        }

        /* 最大日志行数 */
        {
            int v = cfg.get_or<int>("launcher.system.max_log_lines", 10000);
            GtkWidget* sr = make_slider_entry("launcher.system.max_log_lines", 1000, 100000, 1000, v, "%.0f");
            gtk_box_append(GTK_BOX(inner), labeled_row("最大日志行数", sr));
        }

        gtk_box_append(GTK_BOX(inner),
            make_check("禁用硬件加速", "launcher.system.disable_hw_accel",
                       cfg.get_or<bool>("launcher.system.disable_hw_accel", false)));

        /* [导出设置] [导入设置] */
        gtk_box_append(GTK_BOX(inner), action_buttons_row({"导出设置", "导入设置"}));

        gtk_box_append(GTK_BOX(content), build_card("系统", inner));
    }

    /* ── Card 2: 网络 ──────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        gtk_box_append(GTK_BOX(inner),
            make_check("DNS over HTTPS", "network.doh_enabled",
                       cfg.get_or<bool>("network.doh_enabled", false)));

        /* HTTP 代理 */
        {
            const char* items[] = {"无代理", "系统代理", "自定义"};
            const char* vals[]  = {"none", "system", "custom"};
            std::string cur = cfg.get_or<std::string>("network.http_proxy.type", "none");
            int sel = (cur == "system") ? 1 : (cur == "custom") ? 2 : 0;

            GtkWidget* toggle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            gtk_widget_add_css_class(toggle, "linked");
            gtk_widget_set_margin_bottom(toggle, 6);

            GtkWidget* btns[3];
            for (int i = 0; i < 3; i++) {
                btns[i] = gtk_toggle_button_new_with_label(items[i]);
                gtk_widget_set_size_request(btns[i], 80, -1);
                gtk_box_append(GTK_BOX(toggle), btns[i]);
            }
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btns[sel]), TRUE);

            auto toggle_cb = +[](GtkToggleButton* self, gpointer data) {
                if (!gtk_toggle_button_get_active(self)) return;
                GtkWidget* group = GTK_WIDGET(data);
                for (GtkWidget* sib = gtk_widget_get_first_child(group); sib;
                     sib = gtk_widget_get_next_sibling(sib)) {
                    if (sib != GTK_WIDGET(self) && GTK_IS_TOGGLE_BUTTON(sib))
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sib), FALSE);
                }
            };
            for (int i = 0; i < 3; i++)
                g_signal_connect(btns[i], "toggled", G_CALLBACK(toggle_cb), toggle);

            gtk_box_append(GTK_BOX(inner), labeled_row("HTTP 代理", toggle));

            /* 自定义代理 URL */
            {
                std::string url = cfg.get_or<std::string>("network.http_proxy.url", "");
                GtkWidget* e = gtk_entry_new();
                gtk_entry_set_placeholder_text(GTK_ENTRY(e), "http://host:port");
                gtk_editable_set_text(GTK_EDITABLE(e), url.c_str());
                gtk_widget_set_hexpand(e, TRUE);
                GtkWidget* lr = labeled_row("代理 URL", e);
                if (cur != "custom") gtk_widget_set_visible(lr, FALSE);
                gtk_box_append(GTK_BOX(inner), lr);

                auto* sp = new std::string("network.http_proxy.url");
                g_signal_connect(e, "notify::text",
                    G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                        auto* p = static_cast<std::string*>(d);
                        ConfigManager::instance().set(*p,
                            std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                    }), sp);

                g_object_set_data(G_OBJECT(toggle), "proxy-url-row", lr);
            }

            /* 类型切换 → config + 条件显示 */
            for (int i = 0; i < 3; i++) {
                g_object_set_data(G_OBJECT(btns[i]), "proxy-type", (void*)vals[i]);
                g_signal_connect(btns[i], "toggled",
                    G_CALLBACK(+[](GtkToggleButton* btn, gpointer) {
                        if (!gtk_toggle_button_get_active(btn)) return;
                        const char* t = static_cast<const char*>(
                            g_object_get_data(G_OBJECT(btn), "proxy-type"));
                        ConfigManager::instance().set("network.http_proxy.type", std::string(t));
                        GtkWidget* group = gtk_widget_get_parent(GTK_WIDGET(btn));
                        GtkWidget* url_row = GTK_WIDGET(g_object_get_data(G_OBJECT(group), "proxy-url-row"));
                        if (url_row) gtk_widget_set_visible(url_row, std::string(t) == "custom");
                    }), nullptr);
            }
        }

        gtk_box_append(GTK_BOX(content), build_card("网络", inner));
    }

    /* ── Card 3: 调试 (折叠) ────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 动画速度 */
        {
            double v = cfg.get_or<double>("launcher.debug.anim_speed", 1.0);
            GtkWidget* sr = make_slider_entry("launcher.debug.anim_speed", 0.1, 5.0, 0.1, v, "%.1f");
            gtk_box_append(GTK_BOX(inner), labeled_row("动画速度", sr));
        }

        gtk_box_append(GTK_BOX(inner),
            make_check("跳过文件拷贝", "launcher.debug.skip_copy",
                       cfg.get_or<bool>("launcher.debug.skip_copy", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("调试模式", "launcher.debug.mode",
                       cfg.get_or<bool>("launcher.debug.mode", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("调试延迟", "launcher.debug.delay",
                       cfg.get_or<bool>("launcher.debug.delay", false)));

        GtkWidget* expander = gtk_expander_new("调试选项");
        gtk_expander_set_expanded(GTK_EXPANDER(expander), FALSE);
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);
        gtk_box_append(GTK_BOX(content), build_card("调试选项", expander));
    }

    return sw;
}

}  // namespace pcl
