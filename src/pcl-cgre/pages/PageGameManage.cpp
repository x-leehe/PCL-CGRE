#include "pages/PageGameManage.hpp"
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
 * P1 — 游戏管理 (3 卡片, 12 项)
 * ============================================================================ */

GtkWidget* build_page_game_manage()
{
    using CM = ConfigManager;
    auto& cfg = CM::instance();

    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    /* ── Card 1: 下载源 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        {
            const char* labels[] = {"仅镜像源", "镜像源优先", "官方源优先", "仅官方源"};
            const char* vals[]   = {"mirror", "prefer_mirror", "prefer_official", "official"};
            std::string cur = cfg.get_or<std::string>("game.download.source", "prefer_official");
            int sel = 2; // default
            for (int i = 0; i < 4; i++) { if (cur == vals[i]) { sel = i; break; } }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("游戏文件源", make_dropdown_str(labels, vals, 4, sel, "game.download.source")));
        }
        {
            const char* labels[] = {"仅镜像源", "镜像源优先", "官方源优先", "仅官方源"};
            const char* vals[]   = {"mirror", "prefer_mirror", "prefer_official", "official"};
            std::string cur = cfg.get_or<std::string>("game.download.version_source", "prefer_official");
            int sel = 2;
            for (int i = 0; i < 4; i++) { if (cur == vals[i]) { sel = i; break; } }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("版本列表源", make_dropdown_str(labels, vals, 4, sel, "game.download.version_source")));
        }
        {
            int v = cfg.get_or<int>("game.download.threads", 64);
            GtkWidget* s = make_slider_entry("game.download.threads", 1, 256, 1, v, "%.0f");
            gtk_box_append(GTK_BOX(inner), labeled_row("下载线程数", s));
        }
        {
            int v = cfg.get_or<int>("game.download.speed_limit_kbps", 0);
            GtkWidget* s = make_slider_entry("game.download.speed_limit_kbps", 0, 100000, 100, v, "%.0f");
            gtk_box_append(GTK_BOX(inner), labeled_row("下载速度限制 (KB/s)", s));
        }
        gtk_box_append(GTK_BOX(inner),
            make_check("自动选择实例", "game.download.auto_select_instance",
                       cfg.get_or<bool>("game.download.auto_select_instance", true)));
        gtk_box_append(GTK_BOX(inner),
            make_check("下载后修复 Authlib", "game.download.fix_authlib",
                       cfg.get_or<bool>("game.download.fix_authlib", true)));

        gtk_box_append(GTK_BOX(content), build_card("下载源", inner));
    }

    /* ── Card 2: 社区源 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        {
            const char* labels[] = {"仅 CurseForge", "CurseForge 优先", "Modrinth 优先", "仅 Modrinth"};
            const char* vals[]   = {"curseforge", "prefer_curseforge", "prefer_modrinth", "modrinth"};
            std::string cur = cfg.get_or<std::string>("game.community.mod_source", "prefer_curseforge");
            int sel = 1;
            for (int i = 0; i < 4; i++) { if (cur == vals[i]) { sel = i; break; } }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("Mod 下载源", make_dropdown_str(labels, vals, 4, sel, "game.community.mod_source")));
        }
        {
            std::string fmt = cfg.get_or<std::string>("game.community.filename_format",
                                                      "${filename}");
            GtkWidget* e = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), "${filename}");
            gtk_editable_set_text(GTK_EDITABLE(e), fmt.c_str());
            gtk_widget_set_hexpand(e, TRUE);
            gtk_widget_set_valign(e, GTK_ALIGN_CENTER);

            auto* sp = new std::string("game.community.filename_format");
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);
            gtk_box_append(GTK_BOX(inner), labeled_row("文件名格式", e));

            /* 提示条：可用变量 */
            GtkWidget* hint = gtk_label_new(
                "可用变量:\n"
                "${filename} — 原名\n"
                "${zh_name} — 中文名\n"
                "${en_name} — 英文名");
            gtk_widget_set_margin_start(hint, 122);
            gtk_widget_set_margin_bottom(hint, 4);
            gtk_widget_set_opacity(hint, 0.55);
            gtk_widget_set_halign(hint, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(inner), hint);
        }
        {
            const char* labels[] = {"翻译优先", "文件名优先"};
            const char* vals[]   = {"translation_first", "filename_first"};
            std::string cur = cfg.get_or<std::string>("game.community.mod_list_style", "translation_first");
            int sel = (cur == "filename_first") ? 1 : 0;
            gtk_box_append(GTK_BOX(inner),
                labeled_row("Mod 列表样式", make_dropdown_str(labels, vals, 2, sel, "game.community.mod_list_style")));
        }
        gtk_box_append(GTK_BOX(inner),
            make_check("隐藏 Quilt", "game.community.hide_quilt",
                       cfg.get_or<bool>("game.community.hide_quilt", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("自动安装依赖", "game.community.auto_install_deps",
                       cfg.get_or<bool>("game.community.auto_install_deps", true)));

        gtk_box_append(GTK_BOX(content), build_card("社区资源", inner));
    }

    /* ── Card 3: 无障碍 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        gtk_box_append(GTK_BOX(inner),
            make_check("正式版更新通知", "game.accessibility.release_notice",
                       cfg.get_or<bool>("game.accessibility.release_notice", true)));
        gtk_box_append(GTK_BOX(inner),
            make_check("快照版更新通知", "game.accessibility.snapshot_notice",
                       cfg.get_or<bool>("game.accessibility.snapshot_notice", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("自动设置游戏语言", "game.accessibility.auto_lang",
                       cfg.get_or<bool>("game.accessibility.auto_lang", true)));
        gtk_box_append(GTK_BOX(inner),
            make_check("读取剪贴板下载链接", "game.accessibility.read_clipboard",
                       cfg.get_or<bool>("game.accessibility.read_clipboard", true)));

        gtk_box_append(GTK_BOX(content), build_card("无障碍", inner));
    }

    return sw;
}

}  // namespace pcl
