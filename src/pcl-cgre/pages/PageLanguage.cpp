#include "pages/PageLanguage.hpp"
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
 * P2 — build_page_language: 语言 (1 卡片, 2 项)
 * ============================================================================ */

GtkWidget* build_page_language()
{
    using CM = ConfigManager;
    auto& cfg = CM::instance();

    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 界面语言 */
        {
            const char* labels[] = {"简体中文", "English (US)", "繁體中文"};
            const char* vals[]   = {"zh_CN", "en_US", "zh_TW"};
            std::string cur = cfg.get_or<std::string>("launcher.locale.ui_lang", "zh_CN");
            int sel = 0;
            for (int i = 0; i < 3; i++) if (cur == vals[i]) { sel = i; break; }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("界面语言", make_dropdown_str(labels, vals, 3, sel, "launcher.locale.ui_lang")));
        }

        /* 格式化文化 */
        {
            const char* labels[] = {"简体中文 (zh-CN)", "English (en-US)", "繁體中文 (zh-TW)"};
            const char* vals[]   = {"zh-CN", "en-US", "zh-TW"};
            std::string cur = cfg.get_or<std::string>("launcher.locale.format_culture", "zh-CN");
            int sel = 0;
            for (int i = 0; i < 3; i++) if (cur == vals[i]) { sel = i; break; }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("格式化文化", make_dropdown_str(labels, vals, 3, sel, "launcher.locale.format_culture")));
        }

        /* 提示条 */
        {
            GtkWidget* hint = gtk_label_new("语言更改将在重启后完全生效");
            gtk_widget_set_margin_start(hint, 122);
            gtk_widget_set_margin_top(hint, 12);
            gtk_widget_set_opacity(hint, 0.55);
            gtk_widget_set_halign(hint, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(inner), hint);
        }

        gtk_box_append(GTK_BOX(content), build_card("语言与区域", inner));
    }

    return sw;
}

}  // namespace pcl
