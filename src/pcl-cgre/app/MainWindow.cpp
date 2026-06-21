#include "app/MainWindow.hpp"
#include "app/NavigationController.hpp"
#include "app/WindowShell.hpp"
#include "app/WindowControls.hpp"
#include "app/HeaderSetup.hpp"
#include "app/PageAssembler.hpp"
#include "app/NotificationSetup.hpp"
#include "core/Styles.hpp"

#include <adwaita.h>

namespace pcl {

GtkWidget* create_main_window(GtkApplication* app)
{
    /* ═══════════════════════════════════════════════════════════════════
     * 1. 窗口外壳 — GtkWindow + Overlay + ToolbarView + 背景
     * ═══════════════════════════════════════════════════════════════════ */
    GtkWidget* window = create_window_shell(app);
    if (!window) return nullptr;

    /* 取出内层组件引用 */
    GtkWidget* overlay = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(window), "main-overlay"));
    GtkWidget* toolbar_view = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(window), "toolbar-view"));

    /* ═══════════════════════════════════════════════════════════════════
     * 2. ViewStack — 页面容器 (header / pages 都需要引用)
     * ═══════════════════════════════════════════════════════════════════ */
    AdwViewStack* stack = ADW_VIEW_STACK(adw_view_stack_new());
    adw_view_stack_set_hhomogeneous(stack, FALSE);
    adw_view_stack_set_vhomogeneous(stack, FALSE);
    adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(toolbar_view), GTK_WIDGET(stack));

    /* ═══════════════════════════════════════════════════════════════════
     * 3. 标题栏 — HeaderBar + 标题按钮 + 页签 + 返回导航
     * ═══════════════════════════════════════════════════════════════════ */
    GtkWidget* header = setup_header(toolbar_view, stack);

    /* 提取 header 组件并初始化 NavigationController */
    GtkWidget* header_tabs = header ? static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(header), "header-tabs")) : nullptr;
    GtkWidget* back_nav = header ? static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(header), "back-nav")) : nullptr;
    GtkWidget* title_btn = header ? static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(header), "title-btn")) : nullptr;

    NavigationController::instance().init(
        GTK_WINDOW(window), stack, header_tabs,
        back_nav, header, title_btn);

    /* 向后兼容: 保留 g_object_set_data */
    if (header_tabs) g_object_set_data(G_OBJECT(window), "header-tabs", header_tabs);
    if (back_nav)    g_object_set_data(G_OBJECT(window), "back-nav", back_nav);
    if (header)      g_object_set_data(G_OBJECT(window), "header-bar", header);
    if (title_btn)   g_object_set_data(G_OBJECT(window), "app-title", title_btn);
    g_object_set_data(G_OBJECT(window), "main-stack", stack);

    /* ═══════════════════════════════════════════════════════════════════
     * 4. 窗口控件 — 关闭 / 最小化按钮 (独立于 HeaderSetup, 可注释)
     * ═══════════════════════════════════════════════════════════════════ */
    build_window_controls(header);

    /* ═══════════════════════════════════════════════════════════════════
     * 5. 页面组装 — 启动 / 下载 / 设置 / 更多 (每页独立, 可单独注释)
     * ═══════════════════════════════════════════════════════════════════ */
    assemble_pages(stack);

    /* ═══════════════════════════════════════════════════════════════════
     * 6. 通知抽屉 — 遮罩层 + 抽屉面板 (独立模块, 可注释)
     * ═══════════════════════════════════════════════════════════════════ */
    setup_notifications(window, overlay);

    /* ═══════════════════════════════════════════════════════════════════
     * 7. 收尾 — 加载全局 CSS
     * ═══════════════════════════════════════════════════════════════════ */
    load_stylesheet_default();
    gtk_widget_queue_draw(window);

    return window;
}

}  // namespace pcl
