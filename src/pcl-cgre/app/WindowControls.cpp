#include "app/WindowControls.hpp"
#include "util/IconHelper.hpp"

#include <adwaita.h>

namespace pcl {

void build_window_controls(GtkWidget* header_bar)
{
    if (!header_bar || !ADW_IS_HEADER_BAR(header_bar)) return;

    /* 隐藏默认窗口控件, 改用自定义 Lucide 图标按钮 */
    adw_header_bar_set_show_end_title_buttons(ADW_HEADER_BAR(header_bar), FALSE);

    /* 关闭按钮 — pack_end 先添加的在最右侧 */
    {
        GtkWidget* close_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);
        gtk_widget_add_css_class(close_btn, "window-ctrl-btn");
        gtk_widget_add_css_class(close_btn, "window-ctrl-close");
        gtk_button_set_child(GTK_BUTTON(close_btn),
                             icon::load("x-light", 18));
        gtk_widget_set_tooltip_text(close_btn, "关闭");
        g_signal_connect(close_btn, "clicked", G_CALLBACK(+[](GtkWidget* btn, gpointer) {
            gtk_window_close(GTK_WINDOW(gtk_widget_get_root(btn)));
        }), nullptr);
        adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), close_btn);
    }

    /* 最小化按钮 */
    {
        GtkWidget* min_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(min_btn), FALSE);
        gtk_widget_add_css_class(min_btn, "window-ctrl-btn");
        gtk_button_set_child(GTK_BUTTON(min_btn),
                             icon::load("minus-light", 18));
        gtk_widget_set_tooltip_text(min_btn, "最小化");
        g_signal_connect(min_btn, "clicked", G_CALLBACK(+[](GtkWidget* btn, gpointer) {
            gtk_window_minimize(GTK_WINDOW(gtk_widget_get_root(btn)));
        }), nullptr);
        adw_header_bar_pack_end(ADW_HEADER_BAR(header_bar), min_btn);
    }
}

}  // namespace pcl
