#include "pages/PageAbout.hpp"
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
 * P3 — build_page_about: 关于 (6 卡片, 静态内容)
 * ============================================================================ */

GtkWidget* build_page_about()
{
    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    /* ── Card 1: 作者信息 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

        struct Author {
            const char* avatar_label;
            const char* name;
            const char* role;
            const char* btn_label;
            const char* url;
        };
        const Author authors[] = {
            {"🖼", "LTCat",       "原作者",      "赞助原作者", "https://afdian.com/a/LTCat"},
            {"🖼", "PCL-Community","社区维护",    "GitHub 主页","https://github.com/PCL-Community"},
            {"🖼", "X-LeeHe",     "PCL-CGRE 作者","GitHub 主页","https://github.com/X-LeeHe"},
            {"🖼", "PCL-CGRE",    "v0.1.0-dev",  "源代码",     "https://github.com/X-LeeHe/PCL-CGRE"},
        };

        for (auto& a : authors) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_bottom(row, 8);

            /* avatar placeholder */
            GtkWidget* av = gtk_label_new(a.avatar_label);
            gtk_widget_add_css_class(av, "ack-avatar");
            gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), av);

            /* name + role */
            GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            GtkWidget* name_lbl = gtk_label_new(a.name);
            gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "<span weight='bold'>%s</span>", a.name);
                gtk_label_set_markup(GTK_LABEL(name_lbl), buf);
            }
            gtk_box_append(GTK_BOX(info), name_lbl);

            GtkWidget* role_lbl = gtk_label_new(a.role);
            gtk_widget_set_halign(role_lbl, GTK_ALIGN_START);
            gtk_widget_set_opacity(role_lbl, 0.55);
            gtk_box_append(GTK_BOX(info), role_lbl);

            gtk_box_append(GTK_BOX(row), info);
            gtk_widget_set_hexpand(info, TRUE);

            /* link button */
            GtkWidget* link_btn = gtk_button_new_with_label(a.btn_label);
            gtk_widget_set_valign(link_btn, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), link_btn);

            gtk_box_append(GTK_BOX(inner), row);
        }

        gtk_box_append(GTK_BOX(content), build_card("关于 PCL-CGRE", inner));
    }

    /* ── Card 2: 特别感谢 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

        struct Thanks {
            const char* name;
            const char* role;
            const char* btn_label;
        };
        const Thanks thanks[] = {
            {"bangbang93", "BMCLAPI 维护者", "赞助"},
            {"MCMOD",       "MC 百科",        "打开"},
        };

        for (auto& t : thanks) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_bottom(row, 8);

            GtkWidget* av = gtk_label_new("🖼");
            gtk_widget_add_css_class(av, "ack-avatar");
            gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), av);

            GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            GtkWidget* name_lbl = gtk_label_new(t.name);
            {
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                    "<span weight='bold'>%s</span>", t.name);
                gtk_label_set_markup(GTK_LABEL(name_lbl), buf);
            }
            gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(info), name_lbl);

            GtkWidget* role_lbl = gtk_label_new(t.role);
            gtk_widget_set_opacity(role_lbl, 0.55);
            gtk_widget_set_halign(role_lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(info), role_lbl);

            gtk_box_append(GTK_BOX(row), info);
            gtk_widget_set_hexpand(info, TRUE);

            GtkWidget* btn = gtk_button_new_with_label(t.btn_label);
            gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), btn);

            gtk_box_append(GTK_BOX(inner), row);
        }

        gtk_box_append(GTK_BOX(content), build_card("特别感谢", inner));
    }

    /* ── Card 3: 贡献者 ────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

        /* 占位头像网格 */
        GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_bottom(grid, 8);
        for (int i = 0; i < 8; i++) {
            GtkWidget* av = gtk_label_new("🖼");
            gtk_widget_add_css_class(av, "ack-avatar");
            gtk_widget_set_halign(av, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(grid), av);
        }
        gtk_box_append(GTK_BOX(inner), grid);

        GtkWidget* more_btn = gtk_button_new_with_label("查看所有贡献者 →");
        gtk_widget_set_halign(more_btn, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(inner), more_btn);

        gtk_box_append(GTK_BOX(content), build_card("贡献者", inner));
    }

    /* ── Card 4: 法律信息 (折叠) ──────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        GtkWidget* privacy = gtk_label_new(
            "PCL-CGRE 为开源项目，不收集任何个人数据。\n"
            "设置文件仅存储在您的本地设备。");
        gtk_widget_set_halign(privacy, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(privacy, 8);
        gtk_box_append(GTK_BOX(inner), privacy);

        GtkWidget* copyright_lbl = gtk_label_new(
            "Copyright © 2024-2026 X-LeeHe & PCL-Community\n"
            "基于 PCL-CE (Community Edition) 构建");
        gtk_widget_set_halign(copyright_lbl, GTK_ALIGN_START);
        gtk_widget_set_opacity(copyright_lbl, 0.55);
        gtk_widget_set_margin_bottom(copyright_lbl, 8);
        gtk_box_append(GTK_BOX(inner), copyright_lbl);

        GtkWidget* src_btn = gtk_button_new_with_label("社区源代码");
        gtk_widget_set_halign(src_btn, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(inner), src_btn);

        GtkWidget* expander = gtk_expander_new("法律信息");
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);
        gtk_box_append(GTK_BOX(content), build_card("法律信息", expander));
    }

    /* ── Card 5: 上游法律信息 (折叠) ──────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        GtkWidget* pcl2_lbl = gtk_label_new(
            "PCL2 (Plain Craft Launcher 2) 由 LTCat 开发。\n"
            "PCL-CGRE 是基于 PCL-CE 的重写版本，使用 GTK4/libadwaita。");
        gtk_widget_set_halign(pcl2_lbl, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(pcl2_lbl, 8);
        gtk_box_append(GTK_BOX(inner), pcl2_lbl);

        GtkWidget* btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(btn_row), gtk_button_new_with_label("条款与免责"));
        gtk_box_append(GTK_BOX(btn_row), gtk_button_new_with_label("上游源码 (PCL2)"));
        gtk_box_append(GTK_BOX(btn_row), gtk_button_new_with_label("上游源码 (PCL-CE)"));
        gtk_box_append(GTK_BOX(inner), btn_row);

        GtkWidget* expander = gtk_expander_new("上游法律信息");
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);
        gtk_box_append(GTK_BOX(content), build_card("上游法律信息", expander));
    }

    /* ── Card 6: 许可证列表 (折叠) ────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        struct License {
            const char* lib;
            const char* license;
            const char* url;
        };
        const License libs[] = {
            {"GTK 4",       "LGPL 2.1+", "https://www.gtk.org"},
            {"libadwaita",   "LGPL 2.1+", "https://gnome.pages.gitlab.gnome.org/libadwaita"},
            {"nlohmann/json","MIT",       "https://github.com/nlohmann/json"},
            {"Pango",        "LGPL 2+",   "https://pango.gnome.org"},
            {"Cairo",        "LGPL 2.1 / MPL 1.1", "https://www.cairographics.org"},
            {"OpenSSL",      "Apache 2.0", "https://www.openssl.org"},
        };

        for (auto& l : libs) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_bottom(row, 4);

            GtkWidget* name_lbl = gtk_label_new(l.lib);
            gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
            gtk_widget_set_size_request(name_lbl, 120, -1);
            gtk_box_append(GTK_BOX(row), name_lbl);

            GtkWidget* lic_lbl = gtk_label_new(l.license);
            gtk_widget_set_halign(lic_lbl, GTK_ALIGN_START);
            gtk_widget_set_opacity(lic_lbl, 0.55);
            gtk_widget_set_size_request(lic_lbl, 110, -1);
            gtk_box_append(GTK_BOX(row), lic_lbl);

            GtkWidget* site_btn = gtk_button_new_with_label("网站");
            gtk_widget_set_valign(site_btn, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), site_btn);

            GtkWidget* lic_btn = gtk_button_new_with_label("许可");
            gtk_widget_set_valign(lic_btn, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), lic_btn);

            gtk_box_append(GTK_BOX(inner), row);
        }

        GtkWidget* expander = gtk_expander_new("第三方许可证");
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);
        gtk_box_append(GTK_BOX(content), build_card("第三方许可证", expander));
    }

    return sw;
}

}  // namespace pcl
