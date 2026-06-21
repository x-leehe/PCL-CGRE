#include "app/NotificationSetup.hpp"
#include "app/NavigationController.hpp"
#include "widgets/NotificationDrawer.hpp"

#include <adwaita.h>

namespace pcl {

void setup_notifications(GtkWidget* window, GtkWidget* overlay)
{
    if (!window || !overlay || !GTK_IS_OVERLAY(overlay)) return;

    /* ── 半透明背景遮罩 — 抽屉打开时显示, 点击关闭抽屉 ── */
    GtkWidget* backdrop = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(backdrop, "notif-backdrop");
    gtk_widget_set_visible(backdrop, FALSE);
    gtk_widget_set_can_target(backdrop, TRUE);
    gtk_widget_set_hexpand(backdrop, TRUE);
    gtk_widget_set_vexpand(backdrop, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), backdrop);

    /* 点击遮罩 → 关闭抽屉 */
    GtkGesture* bg_click = gtk_gesture_click_new();
    g_signal_connect(bg_click, "pressed", G_CALLBACK(+[](GtkGesture*, int, double, double, gpointer) {
        NavigationController::instance().close_notification_drawer();
    }), nullptr);
    gtk_widget_add_controller(backdrop, GTK_EVENT_CONTROLLER(bg_click));

    /* ── 抽屉面板 ── */
    GtkWidget* drawer = build_notification_drawer();
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), drawer);

    /* 提取 revealer 并注册到 NavigationController */
    GtkWidget* revealer = gtk_widget_get_first_child(drawer);

    /* 提取通知列表 (供 toast 结束后添加条目) */
    GtkWidget* notif_list = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(drawer), "notif-list"));

    NavigationController::instance().register_notification(
        revealer, backdrop, drawer, notif_list);

    /* 向后兼容: NotificationDrawer 中的旧代码仍通过 window data 查找 */
    if (revealer)  g_object_set_data(G_OBJECT(window), "notif-revealer", revealer);
    if (backdrop)  g_object_set_data(G_OBJECT(window), "notif-backdrop", backdrop);
    if (drawer)    g_object_set_data(G_OBJECT(window), "notif-outer", drawer);
    if (notif_list) g_object_set_data(G_OBJECT(window), "notif-list", notif_list);
    g_object_set_data(G_OBJECT(window), "notif-open", GINT_TO_POINTER(0));
}

}  // namespace pcl
