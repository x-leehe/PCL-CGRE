#pragma once

#include <gtk/gtk.h>

namespace pcl {

/**
 * 创建窗口外壳 — AdwApplicationWindow + GtkOverlay + ToolbarView + 背景
 *
 * 失败保护: 返回 nullptr 时调用方应终止 create_main_window
 *
 * @param app  GtkApplication 实例
 * @return     顶层窗口 widget (已设置 "main-overlay", "bg-overlay",
 *              "toolbar-view" data)
 */
GtkWidget* create_window_shell(GtkApplication* app);

}  // namespace pcl
