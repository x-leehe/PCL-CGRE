#include "app/HeaderSetup.hpp"
#include "app/NavigationController.hpp"
#include "pages/ResourceDetailPage.hpp"  // build_back_nav
#include "widgets/HeaderTabs.hpp"
#include "util/IconHelper.hpp"

#include <adwaita.h>

namespace pcl {

GtkWidget* setup_header(GtkWidget* toolbar_view, AdwViewStack* stack)
{
    if (!toolbar_view || !stack) return nullptr;

    /* ── Header bar (48px, 匹配 PCL-CE 标题栏) ── */
    GtkWidget* header = adw_header_bar_new();
    adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(toolbar_view), header);

    /* 应用标题按钮 (左侧, 点击打开通知抽屉)
     * 进入详情页时隐藏, 返回时恢复 */
    GtkWidget* title_btn = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(title_btn), FALSE);
    gtk_widget_add_css_class(title_btn, "app-title-btn");
    gtk_button_set_child(GTK_BUTTON(title_btn),
                         gtk_label_new("PCL-CGRE"));
    adw_header_bar_pack_start(ADW_HEADER_BAR(header), title_btn);

    /* 点击切换抽屉 — 委托给 NavigationController */
    g_signal_connect(title_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer) {
        NavigationController::instance().toggle_notification_drawer();
    }), nullptr);

    /* ── 自定义标题栏页签 (替代 AdwViewSwitcher) ── */
    GtkWidget* header_tabs = build_header_tabs(stack);
    if (header_tabs) {
        g_object_ref(header_tabs);
        adw_header_bar_set_title_widget(ADW_HEADER_BAR(header), header_tabs);
    }

    /* ── 详情页返回导航栏 — 初始隐藏, 进入资源详情时替换 header_tabs ── */
    GtkWidget* back_nav = build_back_nav();
    g_object_ref(back_nav);

    /* ── 返回按钮 → 退出详情页, 恢复标题栏页签 ── */
    {
        GtkWidget* back_btn = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(back_nav), "back-btn"));
        if (back_btn) {
            g_signal_connect(back_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer) {
                auto& nav = NavigationController::instance();
                nav.leave_detail_view();

                /* 切回下载页的内部 stack 到上一页 */
                AdwViewStack* main_stk = nav.main_stack();
                if (main_stk) {
                    GtkWidget* dl_page = adw_view_stack_get_child_by_name(
                        main_stk, "download");
                    if (dl_page) {
                        GtkWidget* dl_stk = nav.dl_stack();
                        const char* prev_page = static_cast<const char*>(
                            g_object_get_data(G_OBJECT(dl_page), "prev-page"));
                        if (dl_stk && prev_page)
                            gtk_stack_set_visible_child_name(
                                GTK_STACK(dl_stk), prev_page);
                    }
                }
            }), nullptr);
        }
    }

    /* 存储组件供 NavigationController 后续初始化 */
    g_object_set_data(G_OBJECT(header), "header-tabs", header_tabs);
    g_object_set_data(G_OBJECT(header), "back-nav", back_nav);
    g_object_set_data(G_OBJECT(header), "title-btn", title_btn);

    return header;
}

}  // namespace pcl
