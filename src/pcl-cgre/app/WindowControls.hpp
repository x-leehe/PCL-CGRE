#pragma once

#include <gtk/gtk.h>

namespace pcl {

/**
 * 构建自定义窗口控件按钮 (关闭 / 最小化)
 *
 * 失败保护: header_bar 为 nullptr 时静默返回
 */
void build_window_controls(GtkWidget* header_bar);

}  // namespace pcl
