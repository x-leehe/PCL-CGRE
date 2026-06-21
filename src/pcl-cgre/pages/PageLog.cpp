#include "pages/PageLog.hpp"
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
 * P3 — build_page_log: 日志 (GtkTextView)
 * ============================================================================ */

GtkWidget* build_page_log()
{
    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 工具栏 */
        GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_bottom(toolbar, 8);

        GtkWidget* clear_btn = gtk_button_new_with_label("清除");
        GtkWidget* copy_btn  = gtk_button_new_with_label("复制全部");
        GtkWidget* scroll_btn = gtk_check_button_new_with_label("自动滚动");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(scroll_btn), TRUE);
        gtk_box_append(GTK_BOX(toolbar), clear_btn);
        gtk_box_append(GTK_BOX(toolbar), copy_btn);
        gtk_box_append(GTK_BOX(toolbar), scroll_btn);
        gtk_box_append(GTK_BOX(inner), toolbar);

        /* 文本视图 */
        GtkWidget* tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
        gtk_widget_set_vexpand(tv, TRUE);
        gtk_widget_set_hexpand(tv, TRUE);
        gtk_widget_add_css_class(tv, "card");

        GtkWidget* tv_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tv_scroll), tv);
        gtk_widget_set_vexpand(tv_scroll, TRUE);
        gtk_widget_set_size_request(tv_scroll, -1, 400);
        gtk_box_append(GTK_BOX(inner), tv_scroll);

        /* 初始日志文本 */
        GtkTextBuffer* buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
        gtk_text_buffer_set_text(buf, "PCL-CGRE 日志输出...\n", -1);

        /* 清除按钮 */
        {
            auto cb = +[](GtkButton*, gpointer d) {
                GtkTextBuffer* b = GTK_TEXT_BUFFER(d);
                gtk_text_buffer_set_text(b, "", -1);
            };
            g_signal_connect_data(clear_btn, "clicked", (GCallback)cb, buf, nullptr, G_CONNECT_DEFAULT);
        }

        /* 复制按钮 */
        g_object_set_data(G_OBJECT(copy_btn), "log-buf", buf);
        {
            auto cb = +[](GtkButton* btn, gpointer) {
                GtkTextBuffer* b = GTK_TEXT_BUFFER(
                    g_object_get_data(G_OBJECT(btn), "log-buf"));
                GtkTextIter start, end;
                gtk_text_buffer_get_bounds(b, &start, &end);
                char* text = gtk_text_buffer_get_text(b, &start, &end, FALSE);
                GdkClipboard* clip = gdk_display_get_clipboard(
                    gdk_display_get_default());
                gdk_clipboard_set_text(clip, text);
                g_free(text);
            };
            g_signal_connect_data(copy_btn, "clicked", (GCallback)cb, nullptr, nullptr, G_CONNECT_DEFAULT);
        }

        /* 自动滚动 — 滚动到底部 */
        g_object_set_data(G_OBJECT(scroll_btn), "log-scroll", tv_scroll);
        g_object_set_data(G_OBJECT(scroll_btn), "log-tv", tv);

        gtk_box_append(GTK_BOX(content), build_card("运行日志", inner));
    }

    return sw;
}

}  // namespace pcl
