#pragma once

#include <gtk/gtk.h>

namespace pcl {

/**
 * 设置通知抽屉系统 — 遮罩层 + 抽屉面板 + Revealer
 *
 * 注册到 NavigationController。
 *
 * 失败保护: window / overlay 任一为 nullptr 时静默返回
 */
void setup_notifications(GtkWidget* window, GtkWidget* overlay);

}  // namespace pcl
