#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

namespace pcl {

/**
 * 组装所有页面 — 创建启动/下载/设置/更多页面并注册到 ViewStack
 *
 * 失败保护: stack 为 nullptr 时静默返回。
 * 单个页面 builder 失败不影响其他页面。
 */
void assemble_pages(AdwViewStack* stack);

}  // namespace pcl
