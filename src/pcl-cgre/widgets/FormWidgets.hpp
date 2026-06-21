#pragma once

/**
 * 通用表单控件库 — 设置页面共享工具
 *
 * 设计原则:
 *   - 所有 helper 接收 cfg_path (字符串) 自动绑定 ConfigManager
 *   - 配置路径生命周期挂在控件本身: g_object_set_data_full(g_strdup, g_free)
 *     不再使用 `new std::string(...)` 造成的隐式泄漏
 *   - 输入无效 (空指针 / 越界) 时返回最小可用控件, 绝不崩溃
 */

#include <gtk/gtk.h>
#include <initializer_list>

namespace pcl {

/* ── 滚动页面框架 ────────────────────────────────────────────────────── */
struct ScrolledPage {
    GtkWidget* sw;       // GtkScrolledWindow
    GtkWidget* content;  // 内部 GtkBox (vertical, 已设置外边距)
};

/** 创建标准设置子页面的滚动外壳, 返回 sw + content 容器 */
ScrolledPage scrolled_content();

/* ── 卡片容器 ────────────────────────────────────────────────────────── */

/** 创建圆角卡片 — 顶部标题 + 内容区, content 可为 nullptr */
GtkWidget* build_card(const char* title, GtkWidget* content);

/* ── 标签行 — 左侧文字标签 + 右侧 widget ──────────────────────────────── */

/** 创建 "label : widget" 横排, widget 可为 nullptr */
GtkWidget* labeled_row(const char* label, GtkWidget* widget);

/* ── 配置绑定输入控件 ────────────────────────────────────────────────── */

/** 下拉框 — 整数索引存储到 cfg_path */
GtkWidget* make_dropdown(const char* const* items, int n, int sel,
                         const char* cfg_path);

/** 下拉框 — 选项对应字符串配置值 (labels 显示, values 存储) */
GtkWidget* make_dropdown_str(const char* const* labels,
                             const char* const* values,
                             int n, int sel,
                             const char* cfg_path);

/** 复选框 — bool 存储到 cfg_path */
GtkWidget* make_check(const char* label, const char* cfg_path, bool def);

/** 文本输入框 — string 存储到 cfg_path */
GtkWidget* make_entry(const char* cfg_path, const char* placeholder = nullptr);

/** 数值滑块 — int 存储到 cfg_path, 右侧显示数值 */
GtkWidget* make_slider(const char* cfg_path, double min, double max,
                       double step, double def, const char* fmt = "%.0f");

/** 双向滑块 + 输入框 — 用户可手动输入数值 */
GtkWidget* make_slider_entry(const char* cfg_path, double min, double max,
                             double step, double def, const char* fmt = "%.0f");

/* ── 通用复合控件 (P2+) ──────────────────────────────────────────────── */

/** 数组型复选框 — 多个 cfg_path 共享的列表配置 (push/erase value) */
GtkWidget* make_array_check(const char* label, const char* cfg_path,
                            const char* value);

/** 小型分组标签 (功能隐藏卡片内的子分类标题) */
GtkWidget* section_label(const char* text);

/** 操作按钮行 — 一组水平排列的按钮 */
GtkWidget* action_buttons_row(std::initializer_list<const char*> labels);

}  // namespace pcl
