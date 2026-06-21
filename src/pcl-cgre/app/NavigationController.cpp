#include "app/NavigationController.hpp"
#include "core/Log.hpp"

#include <adwaita.h>

namespace pcl {

/* ═══════════════════════════════════════════════════════════════════════
 * 单例
 * ═══════════════════════════════════════════════════════════════════════ */

NavigationController& NavigationController::instance()
{
    static NavigationController nav;
    return nav;
}

/* ═══════════════════════════════════════════════════════════════════════
 * 初始化
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::init(GtkWindow*       window,
                                AdwViewStack*    main_stack,
                                GtkWidget*       header_tabs,
                                GtkWidget*       back_nav,
                                GtkWidget*       header_bar,
                                GtkWidget*       app_title)
{
    m_window      = window;
    m_main_stack  = main_stack;
    m_header_tabs = header_tabs;
    m_back_nav    = back_nav;
    m_header_bar  = header_bar;
    m_app_title   = app_title;
}

/* ═══════════════════════════════════════════════════════════════════════
 * 通知抽屉
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::register_notification(GtkWidget* revealer,
                                                  GtkWidget* backdrop,
                                                  GtkWidget* outer,
                                                  GtkWidget* notif_list)
{
    m_notif_revealer = revealer;
    m_notif_backdrop = backdrop;
    m_notif_outer    = outer;
    m_notif_list     = notif_list;
}

GtkWidget* NavigationController::get_notif_list() const
{
    return m_notif_list;
}

void NavigationController::toggle_notification_drawer()
{
    if (!m_notif_revealer) return;

    if (m_notif_open) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(m_notif_revealer), FALSE);
        m_notif_open = false;
        if (m_notif_backdrop)
            gtk_widget_set_visible(m_notif_backdrop, FALSE);
        if (m_notif_outer)
            gtk_widget_set_can_target(m_notif_outer, FALSE);
        if (m_window)
            g_object_set_data(G_OBJECT(m_window), "notif-open", GINT_TO_POINTER(0));
    } else {
        gtk_revealer_set_reveal_child(GTK_REVEALER(m_notif_revealer), TRUE);
        m_notif_open = true;
        if (m_notif_backdrop)
            gtk_widget_set_visible(m_notif_backdrop, TRUE);
        if (m_notif_outer)
            gtk_widget_set_can_target(m_notif_outer, TRUE);
        if (m_window)
            g_object_set_data(G_OBJECT(m_window), "notif-open", GINT_TO_POINTER(1));
    }
}

void NavigationController::close_notification_drawer()
{
    if (!m_notif_revealer) return;

    gtk_revealer_set_reveal_child(GTK_REVEALER(m_notif_revealer), FALSE);
    m_notif_open = false;
    if (m_notif_backdrop)
        gtk_widget_set_visible(m_notif_backdrop, FALSE);
    if (m_notif_outer)
        gtk_widget_set_can_target(m_notif_outer, FALSE);
    if (m_window)
        g_object_set_data(G_OBJECT(m_window), "notif-open", GINT_TO_POINTER(0));
}

/* ═══════════════════════════════════════════════════════════════════════
 * 顶层页面切换
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::click_header_tab(const std::string& page_name)
{
    if (!m_header_tabs) return;

    for (GtkWidget* tab = gtk_widget_get_first_child(m_header_tabs);
         tab; tab = gtk_widget_get_next_sibling(tab))
    {
        const char* pn = static_cast<const char*>(
            g_object_get_data(G_OBJECT(tab), "page-name"));
        if (pn && g_strcmp0(pn, page_name.c_str()) == 0) {
            g_signal_emit_by_name(tab, "clicked");
            return;
        }
    }
}

void NavigationController::switch_to_page(const std::string& name)
{
    if (!m_main_stack) return;

    adw_view_stack_set_visible_child_name(m_main_stack, name.c_str());
    click_header_tab(name);
}

/* ═══════════════════════════════════════════════════════════════════════
 * 详情页进入/退出
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::enter_detail_view(const std::string& title)
{
    if (!m_window || !m_header_bar || !m_back_nav || !m_main_stack) return;

    /* 隐藏 header tabs */
    if (m_header_tabs)
        gtk_widget_set_visible(m_header_tabs, FALSE);

    /* pack back_nav 到 header 左侧 (先确保未重复 attach) */
    if (gtk_widget_get_parent(m_back_nav))
        adw_header_bar_remove(ADW_HEADER_BAR(m_header_bar), m_back_nav);

    if (m_app_title && gtk_widget_get_parent(m_app_title)) {
        g_object_ref(m_app_title);
        adw_header_bar_remove(ADW_HEADER_BAR(m_header_bar), m_app_title);
    }
    adw_header_bar_pack_start(ADW_HEADER_BAR(m_header_bar), m_back_nav);

    /* 设置标题栏中的名称 */
    GtkWidget* title_lbl = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(m_back_nav), "title-lbl"));
    if (title_lbl && GTK_IS_LABEL(title_lbl))
        gtk_label_set_text(GTK_LABEL(title_lbl), title.c_str());

    /* 隐藏下载页左侧导航栏 + 分隔线 */
    if (m_dl_sidebar)
        gtk_widget_set_visible(m_dl_sidebar, FALSE);
    if (m_dl_sep)
        gtk_widget_set_visible(m_dl_sep, FALSE);
}

void NavigationController::leave_detail_view()
{
    if (!m_window || !m_header_bar || !m_back_nav) return;

    /* 移除 back_nav */
    if (gtk_widget_get_parent(m_back_nav))
        adw_header_bar_remove(ADW_HEADER_BAR(m_header_bar), m_back_nav);

    /* 恢复 header tabs */
    if (m_header_tabs)
        gtk_widget_set_visible(m_header_tabs, TRUE);

    /* 恢复 app title */
    if (m_app_title && !gtk_widget_get_parent(m_app_title)) {
        adw_header_bar_pack_start(ADW_HEADER_BAR(m_header_bar), m_app_title);
        g_object_unref(m_app_title);
    }

    /* 恢复下载页侧栏 */
    if (m_dl_sidebar)
        gtk_widget_set_visible(m_dl_sidebar, TRUE);
    if (m_dl_sep)
        gtk_widget_set_visible(m_dl_sep, TRUE);
}

/* ═══════════════════════════════════════════════════════════════════════
 * 子页面注册
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::register_download(GtkWidget* sidebar,
                                              GtkWidget* sep,
                                              GtkWidget* stack)
{
    m_dl_sidebar = sidebar;
    m_dl_sep     = sep;
    m_dl_stack   = stack;
}

void NavigationController::register_settings(GtkWidget* left_nav,
                                              GtkWidget* mid_stack)
{
    m_settings_left = left_nav;
    m_settings_mid  = mid_stack;
}

void NavigationController::register_settings_right(GtkWidget* right_stack)
{
    m_settings_right = right_stack;
}

void NavigationController::register_more(GtkWidget* left_nav,
                                          GtkWidget* mid_stack,
                                          GtkWidget* right_stack)
{
    m_more_left  = left_nav;
    m_more_mid   = mid_stack;
    m_more_right = right_stack;
}

/* ═══════════════════════════════════════════════════════════════════════
 * 子页面导航 (内部辅助)
 * ═══════════════════════════════════════════════════════════════════════ */

void NavigationController::activate_settings_left(const std::string& mid_page)
{
    if (!m_settings_left) return;

    GtkWidget* inner = gtk_scrolled_window_get_child(
        GTK_SCROLLED_WINDOW(m_settings_left));
    if (!inner) return;

    for (GtkWidget* sib = gtk_widget_get_first_child(inner);
         sib; sib = gtk_widget_get_next_sibling(sib))
    {
        if (!gtk_widget_has_css_class(sib, "nav-item")) continue;

        const char* mp = static_cast<const char*>(
            g_object_get_data(G_OBJECT(sib), "mid-page"));
        if (mp && g_strcmp0(mp, mid_page.c_str()) == 0) {
            /* 更新中栏 */
            GtkWidget* mstk = static_cast<GtkWidget*>(
                g_object_get_data(G_OBJECT(sib), "mid-stack"));
            if (mstk)
                gtk_stack_set_visible_child_name(GTK_STACK(mstk), mid_page.c_str());

            /* 互斥高亮 */
            gtk_widget_add_css_class(sib, "nav-item-active");
            for (GtkWidget* other = gtk_widget_get_first_child(inner);
                 other; other = gtk_widget_get_next_sibling(other))
            {
                if (!gtk_widget_has_css_class(other, "nav-item")) continue;
                if (other != sib)
                    gtk_widget_remove_css_class(other, "nav-item-active");
            }
            return;
        }
    }
}

void NavigationController::activate_settings_mid(const std::string& right_page)
{
    if (!m_settings_mid) return;

    const char* cur_mid = gtk_stack_get_visible_child_name(GTK_STACK(m_settings_mid));
    if (!cur_mid) return;

    GtkWidget* mid_list = gtk_stack_get_child_by_name(
        GTK_STACK(m_settings_mid), cur_mid);
    if (!mid_list) return;

    /* 中栏每个组可能被 wrap 在 GtkBox 中 (包含添加按钮等)，取第一个子控件 */
    GtkWidget* nav_box = gtk_widget_get_first_child(mid_list);
    if (!nav_box || !gtk_widget_has_css_class(nav_box, "settings-mid"))
        nav_box = mid_list;

    for (GtkWidget* sib = gtk_widget_get_first_child(nav_box);
         sib; sib = gtk_widget_get_next_sibling(sib))
    {
        if (!gtk_widget_has_css_class(sib, "nav-item")) continue;

        const char* rp = static_cast<const char*>(
            g_object_get_data(G_OBJECT(sib), "right-page"));
        if (rp && g_strcmp0(rp, right_page.c_str()) == 0) {
            GtkWidget* rstk = static_cast<GtkWidget*>(
                g_object_get_data(G_OBJECT(sib), "right-stack"));
            if (rstk)
                gtk_stack_set_visible_child_name(GTK_STACK(rstk), right_page.c_str());

            /* 互斥高亮 */
            gtk_widget_add_css_class(sib, "nav-item-active");
            for (GtkWidget* other = gtk_widget_get_first_child(nav_box);
                 other; other = gtk_widget_get_next_sibling(other))
            {
                if (!gtk_widget_has_css_class(other, "nav-item")) continue;
                if (other != sib)
                    gtk_widget_remove_css_class(other, "nav-item-active");
            }
            return;
        }
    }
}

void NavigationController::activate_more_left(const std::string& mid_page,
                                               const std::string& first_right)
{
    if (!m_more_left) return;

    GtkWidget* inner = gtk_scrolled_window_get_child(
        GTK_SCROLLED_WINDOW(m_more_left));
    if (!inner) return;

    for (GtkWidget* sib = gtk_widget_get_first_child(inner);
         sib; sib = gtk_widget_get_next_sibling(sib))
    {
        if (!gtk_widget_has_css_class(sib, "nav-item")) continue;

        const char* mp = static_cast<const char*>(
            g_object_get_data(G_OBJECT(sib), "mid-page"));
        if (mp && g_strcmp0(mp, mid_page.c_str()) == 0) {
            /* 更新中栏 */
            GtkWidget* mstk = static_cast<GtkWidget*>(
                g_object_get_data(G_OBJECT(sib), "mid-stack"));
            if (mstk)
                gtk_stack_set_visible_child_name(GTK_STACK(mstk), mid_page.c_str());

            /* 互斥高亮左栏 */
            gtk_widget_add_css_class(sib, "nav-item-active");
            for (GtkWidget* other = gtk_widget_get_first_child(inner);
                 other; other = gtk_widget_get_next_sibling(other))
            {
                if (!gtk_widget_has_css_class(other, "nav-item")) continue;
                if (other != sib)
                    gtk_widget_remove_css_class(other, "nav-item-active");
            }

            /* 激活右栏目标页面 */
            if (!first_right.empty() && m_more_right) {
                gtk_stack_set_visible_child_name(
                    GTK_STACK(m_more_right), first_right.c_str());
            }

            /* 高亮中栏中指向 first_right 的 nav-item (而非总是第一个) */
            GtkWidget* mchild = gtk_stack_get_child_by_name(
                GTK_STACK(mstk), mid_page.c_str());
            if (mchild) {
                GtkWidget* matched = nullptr;
                for (GtkWidget* cs = gtk_widget_get_first_child(mchild);
                     cs; cs = gtk_widget_get_next_sibling(cs))
                {
                    if (!gtk_widget_has_css_class(cs, "nav-item")) continue;
                    const char* rp = static_cast<const char*>(
                        g_object_get_data(G_OBJECT(cs), "right-page"));
                    if (rp && g_strcmp0(rp, first_right.c_str()) == 0) {
                        matched = cs;
                        gtk_widget_add_css_class(cs, "nav-item-active");
                    } else {
                        gtk_widget_remove_css_class(cs, "nav-item-active");
                    }
                }
                /* 若未匹配到, 回退到高亮第一个 */
                if (!matched) {
                    GtkWidget* first = gtk_widget_get_first_child(mchild);
                    if (first && gtk_widget_has_css_class(first, "nav-item"))
                        gtk_widget_add_css_class(first, "nav-item-active");
                }
            }
            return;
        }
    }
}

void NavigationController::navigate_settings_to(const std::string& sub)
{
    if (!m_window) return;

    /* 1. 切换到设置顶层页面 */
    switch_to_page("settings");

    /* 2. 在左栏中激活对应的中栏组 */
    /*    根据 sub 推断 mid_page:
     *      page-launch-set / page-java / page-game-manage → mid-global
     *      page-instance-*                                → mid-instance
     *      page-account-*                                 → mid-account
     *      page-ui / page-language / page-misc            → mid-launcher
     *      page-about / page-update / page-feedback / page-log → mid-about
     */
    const char* mid_group = "mid-global";
    if (sub.compare(0, 14, "page-instance-") == 0)     mid_group = "mid-instance";
    else if (sub.compare(0, 13, "page-account-") == 0) mid_group = "mid-account";
    else if (sub == "page-ui" || sub == "page-language" || sub == "page-misc")
        mid_group = "mid-launcher";
    else if (sub == "page-about" || sub == "page-update" ||
             sub == "page-feedback" || sub == "page-log")
        mid_group = "mid-about";

    activate_settings_left(mid_group);
    activate_settings_mid(sub);
}

void NavigationController::navigate_more_to(const std::string& mid_group,
                                             const std::string& right_page)
{
    if (!m_window) return;

    /* 1. 切换到更多顶层页面 */
    switch_to_page("more");

    /* 2. 推断 first_right */
    std::string first_right = right_page;
    if (first_right.empty()) {
        if (mid_group == "mid-help")        first_right = "page-help-support";
        else if (mid_group == "mid-tools")  first_right = "page-tools";
        else if (mid_group == "mid-crash")  first_right = "page-crash-1";
        else if (mid_group == "mid-plugin") first_right = "page-plugin";
    }

    /* 3. 激活左栏 + 中栏第一个子项 */
    activate_more_left(mid_group, first_right);
}

}  // namespace pcl
