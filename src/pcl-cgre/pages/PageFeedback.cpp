#include "pages/PageFeedback.hpp"
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
 * P3 — build_page_feedback: 反馈 (GitHub Issues)
 * ============================================================================ */

GtkWidget* build_page_feedback()
{
    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        GtkWidget* desc = gtk_label_new(
            "遇到问题或有建议？欢迎提交 GitHub Issue。\n"
            "请尽量提供详细的复现步骤和日志信息。");
        gtk_widget_set_halign(desc, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(desc, 12);
        gtk_box_append(GTK_BOX(inner), desc);

        GtkWidget* btn = gtk_button_new_with_label("提交新反馈");
        gtk_widget_add_css_class(btn, "suggested-action");
        gtk_widget_set_halign(btn, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(inner), btn);

        gtk_box_append(GTK_BOX(content), build_card("提交反馈", inner));
    }

    return sw;
}

}  // namespace pcl
