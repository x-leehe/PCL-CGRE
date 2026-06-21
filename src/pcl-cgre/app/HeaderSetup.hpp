#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

namespace pcl {

/**
 * 设置标题栏 — HeaderBar + 标题按钮 + 页签 + 返回导航
 *
 * 注册到 NavigationController，连接返回按钮信号。
 *
 * 失败保护: 任一参数为 nullptr 时返回 nullptr
 *
 * @param toolbar_view  AdwToolbarView (用于 add_top_bar)
 * @param stack          AdwViewStack (供页签切换)
 * @return               HeaderBar widget (nullptr on failure)
 */
GtkWidget* setup_header(GtkWidget* toolbar_view, AdwViewStack* stack);

}  // namespace pcl
