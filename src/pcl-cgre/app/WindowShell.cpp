#include "app/WindowShell.hpp"
#include "core/BackgroundManager.hpp"
#include "core/FrameLimiter.hpp"

#include <adwaita.h>

namespace pcl {

GtkWidget* create_window_shell(GtkApplication* app)
{
    if (!app) return nullptr;

    /* ── 窗口 ── */
    GtkWidget* window = adw_application_window_new(app);
    gtk_widget_add_css_class(window, "pcl-app");
    gtk_window_set_title(GTK_WINDOW(window), "PCL-CGRE");
    gtk_window_set_default_size(GTK_WINDOW(window), 1050, 650);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    /* ── 应用帧率限制 (默认 60fps) ── */
    apply_fps_limit(GTK_WINDOW(window), 60);

    /* ── GtkOverlay: 主内容 + 抽屉覆盖层 ── */
    GtkWidget* overlay = gtk_overlay_new();
    adw_application_window_set_content(ADW_APPLICATION_WINDOW(window), overlay);
    g_object_set_data(G_OBJECT(window), "main-overlay", overlay);

    /* ── 内层 Overlay: 背景层(底) + UI 内容(上) 的 z-order 控制 ── */
    GtkWidget* bg_overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(bg_overlay, TRUE);
    gtk_widget_set_vexpand(bg_overlay, TRUE);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), bg_overlay);
    g_object_set_data(G_OBJECT(window), "bg-overlay", bg_overlay);

    /* ── ToolbarView 布局容器 (bg_overlay 上层) ── */
    GtkWidget* toolbar_view = adw_toolbar_view_new();
    gtk_widget_set_hexpand(toolbar_view, TRUE);
    gtk_widget_set_vexpand(toolbar_view, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(bg_overlay), toolbar_view);
    g_object_set_data(G_OBJECT(window), "toolbar-view", toolbar_view);

    /* ── 应用背景设置 ── */
    apply_background(bg_overlay);

    return window;
}

}  // namespace pcl
