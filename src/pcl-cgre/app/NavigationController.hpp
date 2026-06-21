#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include <string>

namespace pcl {

/**
 * 导航控制器 — 集中管理页面间导航，消除 g_object_set_data/get_data 爬树
 *
 * 隔离原则: 各页面通过 NavigationController 单例交互，无需了解彼此的
 * 内部 widget 结构。
 *
 * 失败保护: 未调用 init() 时所有方法为 no-op。页面仍可通过 HeaderTabs
 * 直接切换 (功能降级，不崩溃)。
 */
class NavigationController {
public:
    static NavigationController& instance();

    /* ── 初始化 ────────────────────────────────────────────────────── */

    /** 注册顶层窗口组件 (由 create_main_window 调用) */
    void init(GtkWindow*       window,
              AdwViewStack*    main_stack,
              GtkWidget*       header_tabs,
              GtkWidget*       back_nav,
              GtkWidget*       header_bar,
              GtkWidget*       app_title);

    /* ── 通知抽屉 ──────────────────────────────────────────────────── */

    void register_notification(GtkWidget* revealer,
                               GtkWidget* backdrop,
                               GtkWidget* outer,
                               GtkWidget* notif_list);

    void toggle_notification_drawer();
    void close_notification_drawer();

    /** 获取通知列表，供 toast 缩回后添加条目 */
    GtkWidget* get_notif_list() const;

    /* ── 顶层页面切换 ──────────────────────────────────────────────── */

    /** 切换到指定顶层页面 (launch / download / settings / more) */
    void switch_to_page(const std::string& name);

    /* ── 详情页进入/退出 ───────────────────────────────────────────── */

    /** 进入详情页: 隐藏页签 + 侧栏，切换 header 为返回导航 */
    void enter_detail_view(const std::string& title);

    /** 退出详情页: 恢复页签 + 侧栏 + 应用标题 */
    void leave_detail_view();

    /* ── 子页面注册 (各页面 builder 调用) ──────────────────────────── */

    void register_download(GtkWidget* sidebar,
                           GtkWidget* sep,
                           GtkWidget* stack);

    void register_settings(GtkWidget* left_nav,
                           GtkWidget* mid_stack);

    void register_settings_right(GtkWidget* right_stack);

    void register_more(GtkWidget* left_nav,
                       GtkWidget* mid_stack,
                       GtkWidget* right_stack);

    /* ── 子页面导航 ────────────────────────────────────────────────── */

    /** 导航到设置子页面 (如 "page-launch-set", "page-java") */
    void navigate_settings_to(const std::string& sub);

    /** 导航到更多子页面 (如 "mid-crash", "mid-help") */
    void navigate_more_to(const std::string& mid_group,
                          const std::string& right_page = "");

    /* ── 访问器 (部分页面仍需直接引用组件) ────────────────────────── */

    GtkWindow*    window()      const { return m_window; }
    AdwViewStack* main_stack()  const { return m_main_stack; }
    GtkWidget*    header_bar()  const { return m_header_bar; }
    GtkWidget*    header_tabs() const { return m_header_tabs; }
    GtkWidget*    back_nav()    const { return m_back_nav; }
    GtkWidget*    app_title()   const { return m_app_title; }

    /* download page internals */
    GtkWidget* dl_sidebar() const { return m_dl_sidebar; }
    GtkWidget* dl_sep()     const { return m_dl_sep; }
    GtkWidget* dl_stack()   const { return m_dl_stack; }

    /* settings page internals */
    GtkWidget* settings_left() const { return m_settings_left; }
    GtkWidget* settings_mid()  const { return m_settings_mid; }
    GtkWidget* settings_right() const { return m_settings_right; }

    /* more page internals */
    GtkWidget* more_left()  const { return m_more_left; }
    GtkWidget* more_mid()   const { return m_more_mid; }
    GtkWidget* more_right() const { return m_more_right; }

private:
    NavigationController() = default;
    NavigationController(const NavigationController&) = delete;
    NavigationController& operator=(const NavigationController&) = delete;

    /* ── 内部辅助 ──────────────────────────────────────────────────── */

    /** 点击 header_tabs 中对应 page_name 的页签按钮 */
    void click_header_tab(const std::string& page_name);

    /** 在 settings 左栏中查找并激活 mid_page 组 */
    void activate_settings_left(const std::string& mid_page);

    /** 在 settings 中栏中查找并激活 right_page 项 */
    void activate_settings_mid(const std::string& right_page);

    /** 在 more 左栏中查找并激活 mid_page 组 + 首个 right_page */
    void activate_more_left(const std::string& mid_page,
                            const std::string& first_right);

    /* ── 顶层窗口组件 ──────────────────────────────────────────────── */
    GtkWindow*    m_window      = nullptr;
    AdwViewStack* m_main_stack  = nullptr;
    GtkWidget*    m_header_tabs = nullptr;
    GtkWidget*    m_back_nav    = nullptr;
    GtkWidget*    m_header_bar  = nullptr;
    GtkWidget*    m_app_title   = nullptr;

    /* ── 通知抽屉 ──────────────────────────────────────────────────── */
    GtkWidget* m_notif_revealer = nullptr;
    GtkWidget* m_notif_backdrop = nullptr;
    GtkWidget* m_notif_outer    = nullptr;
    GtkWidget* m_notif_list     = nullptr;
    bool       m_notif_open     = false;

    /* ── 下载页内部 ────────────────────────────────────────────────── */
    GtkWidget* m_dl_sidebar = nullptr;
    GtkWidget* m_dl_sep     = nullptr;
    GtkWidget* m_dl_stack   = nullptr;

    /* ── 设置页内部 ────────────────────────────────────────────────── */
    GtkWidget* m_settings_left  = nullptr;
    GtkWidget* m_settings_mid   = nullptr;
    GtkWidget* m_settings_right = nullptr;

    /* ── 更多页内部 ────────────────────────────────────────────────── */
    GtkWidget* m_more_left  = nullptr;
    GtkWidget* m_more_mid   = nullptr;
    GtkWidget* m_more_right = nullptr;
};

}  // namespace pcl
