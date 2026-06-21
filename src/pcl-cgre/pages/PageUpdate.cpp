#include "pages/PageUpdate.hpp"
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
 * P3 — build_page_update: 更新 (版本信息 + 检查)
 * ============================================================================ */

GtkWidget* build_page_update()
{
    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        GtkWidget* ver_lbl = gtk_label_new("当前版本: PCL-CGRE v0.1.0-dev (Linux)");
        gtk_widget_set_halign(ver_lbl, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(ver_lbl, 12);
        gtk_box_append(GTK_BOX(inner), ver_lbl);

        GtkWidget* status_lbl = gtk_label_new("更新检查尚未实现");
        gtk_widget_set_halign(status_lbl, GTK_ALIGN_START);
        gtk_widget_set_opacity(status_lbl, 0.55);
        gtk_widget_set_margin_bottom(status_lbl, 12);
        gtk_box_append(GTK_BOX(inner), status_lbl);

        GtkWidget* check_btn = gtk_button_new_with_label("检查更新");
        gtk_widget_add_css_class(check_btn, "suggested-action");
        gtk_widget_set_halign(check_btn, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(inner), check_btn);

        gtk_box_append(GTK_BOX(content), build_card("更新", inner));
    }

    return sw;
}

}  // namespace pcl
