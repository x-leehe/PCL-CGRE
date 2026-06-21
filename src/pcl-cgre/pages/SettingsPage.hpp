#pragma once

#include <gtk/gtk.h>

/* ── 各设置子页面 builder (每个模块独立编译) ─────────────────────────── */
#include "pages/PageLaunchSet.hpp"
#include "pages/PageJavaMgmt.hpp"
#include "pages/PageGameManage.hpp"
#include "pages/PageUI.hpp"
#include "pages/PageLanguage.hpp"
#include "pages/PageMisc.hpp"
#include "pages/PageAbout.hpp"
#include "pages/PageUpdate.hpp"
#include "pages/PageFeedback.hpp"
#include "pages/PageLog.hpp"

namespace pcl {

/**
 * 构建设置页面 — 三栏层级布局 (左/中/右) + 互斥切换
 *
 * 左栏: 5 个一级分类 (全局游戏设置 / 实例设置 / 账号设置 / 启动器设置 / 关于此软件)
 * 中栏: 二级子项导航 (随左栏切换)
 * 右栏: 卡片内容 (随中栏切换, 目前均为占位)
 */
GtkWidget* build_settings_page();

}  // namespace pcl
