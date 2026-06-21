#include "widgets/NotificationDrawer.hpp"
#include "app/NavigationController.hpp"
#include "core/Colors.hpp"
#include "util/IconHelper.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <adwaita.h>

namespace pcl
{

    namespace {

    /* Cached PangoAttrList for commonly used weights. */
    static PangoAttrList* semibold_attr()
    {
        static PangoAttrList* a = []() {
            auto* attr = pango_attr_list_new();
            pango_attr_list_insert(attr, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
            return attr;
        }();
        return a;
    }

    }  // anonymous namespace

    /* ============================================================================
     * build_notification_drawer — 通知抽屉 (左侧滑出)
     *
     *   返回 outer (GtkBox)，其中包含:
     *     - GtkRevealer (滑出动画控制)
     *     - 抽屉面板 (标题 + 清空按钮 + 关闭按钮 + 通知列表)
     *
     *   在 outer 上存储 "notif-list" (GtkListBox*) 供 toast 结束后添加通知。
     *   关闭按钮和清空按钮通过 window 上的 "notif-*" 数据来操作抽屉状态。
     * ============================================================================ */

    GtkWidget *build_notification_drawer()
    {
        /* 外层容器 — 靠左, 不接收事件 (事件穿透到主内容) */
        GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_halign(outer, GTK_ALIGN_START);
        gtk_widget_set_valign(outer, GTK_ALIGN_FILL);
        gtk_widget_set_can_target(outer, FALSE);

        /* Revealer — 控制滑出动画 */
        GtkWidget *revealer = gtk_revealer_new();
        gtk_revealer_set_transition_type(GTK_REVEALER(revealer),
                                         GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
        gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 250);
        gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
        gtk_box_append(GTK_BOX(outer), revealer);

        /* 抽屉面板 */
        GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(panel, "notif-panel");
        gtk_widget_set_size_request(panel, 360, -1);

        /* ── 标题栏 ── */
        GtkWidget *header_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(header_row, "notif-header");
        gtk_box_append(GTK_BOX(panel), header_row);

        GtkWidget *title_lbl = gtk_label_new("通知");
        gtk_widget_add_css_class(title_lbl, "notif-title");
        gtk_widget_set_hexpand(title_lbl, TRUE);
        gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
        gtk_box_append(GTK_BOX(header_row), title_lbl);

        /* 清理通知按钮 */
        GtkWidget *trash_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(trash_btn), FALSE);
        gtk_button_set_child(GTK_BUTTON(trash_btn), icon::load("trash-2", 18));
        gtk_widget_add_css_class(trash_btn, "flat");
        gtk_widget_set_tooltip_text(trash_btn, "清理通知");
        gtk_box_append(GTK_BOX(header_row), trash_btn);

        /* 关闭按钮 */
        GtkWidget *close_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);
        gtk_button_set_child(GTK_BUTTON(close_btn), icon::load("x", 18));
        gtk_widget_add_css_class(close_btn, "flat");
        gtk_widget_set_tooltip_text(close_btn, "关闭");
        gtk_box_append(GTK_BOX(header_row), close_btn);

        /* 分隔线 */
        GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_append(GTK_BOX(panel), sep);

        /* ── 通知列表 ── */
        GtkWidget *scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_vexpand(scroll, TRUE);
        /* 滚动条靠左: scroll 设 RTL + 自绘 CSS; 子控件保持 LTR */
        gtk_widget_set_direction(scroll, GTK_TEXT_DIR_RTL);
        gtk_widget_add_css_class(scroll, "notif-scroll");
        gtk_box_append(GTK_BOX(panel), scroll);

        GtkWidget *list = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
        gtk_widget_add_css_class(list, "notif-list");
        gtk_widget_set_direction(list, GTK_TEXT_DIR_LTR);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list);

        /* ── 空列表占位文字 ── */
        {
            GtkWidget* placeholder = gtk_label_new("本来无一物，何处惹尘埃");
            gtk_widget_set_opacity(placeholder, 0.35);
            gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
            gtk_label_set_wrap(GTK_LABEL(placeholder), TRUE);
            gtk_list_box_set_placeholder(GTK_LIST_BOX(list), placeholder);
        }

        /* 存储 list 到 outer, 供 toast 结束后添加通知 */
        g_object_set_data(G_OBJECT(outer), "notif-list", list);

        /* ── 预置通知条目 ── */
        struct NotifData
        {
            const char *icon;
            const char *css_class;
            const char *bg_class;
            const char *type;
            const char *title;
            const char *desc;
            bool can_delete;
        };

        const NotifData notifs[] = {
            {"refresh-cw", "notif-info", "notif-bg-info", "info", "更新",
             "更新可用！点击本通知进行更新。", true},
            {"info", "notif-info", "notif-bg-info", "info", "提示",
             "目前为设计预览版本，暂未设计实际业务逻辑。玩得开心！", true},
            {"circle-help", "notif-warn", "notif-bg-warn", "warn", "警告",
             "未知的整合包类型！", true},
            {"wrench", "notif-error", "notif-bg-error", "error", "错误",
             "检测到Minecraft发生崩溃，日志分析完成。点击查看日志分析结果！", true},
            {"x", "notif-fatal", "notif-bg-fatal", "fatal", "严重错误",
             "检测到程序发生严重错误。我们正收集错误信息，然后为你重新启动PCL-CGRE。\n"
             "错误代码：Config_ReadOnly（33%）",
             false},
        };

        for (auto &n : notifs)
        {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_add_css_class(row, "notif-item");
            gtk_widget_add_css_class(row, n.bg_class);
            gtk_widget_set_can_target(row, TRUE);

            g_object_set_data_full(G_OBJECT(row), "notif-type",
                                   g_strdup(n.type), g_free);

            GtkWidget *icn = icon::load(n.icon, 20);
            gtk_widget_add_css_class(icn, n.css_class);
            gtk_widget_set_valign(icn, GTK_ALIGN_START);
            gtk_widget_set_margin_top(icn, 2);
            gtk_box_append(GTK_BOX(row), icn);

            GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
            gtk_widget_set_hexpand(text_box, TRUE);
            gtk_box_append(GTK_BOX(row), text_box);

            GtkWidget *t = gtk_label_new(n.title);
            gtk_label_set_xalign(GTK_LABEL(t), 0.0f);
            gtk_label_set_attributes(GTK_LABEL(t), semibold_attr());
            gtk_box_append(GTK_BOX(text_box), t);

            GtkWidget *d = gtk_label_new(n.desc);
            gtk_label_set_xalign(GTK_LABEL(d), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(d), TRUE);
            gtk_widget_set_opacity(d, 0.65);
            gtk_box_append(GTK_BOX(text_box), d);

            if (n.can_delete)
            {
                GtkWidget *del_btn = gtk_button_new();
                gtk_button_set_has_frame(GTK_BUTTON(del_btn), FALSE);
                gtk_button_set_child(GTK_BUTTON(del_btn), icon::load("x", 16));
                gtk_widget_add_css_class(del_btn, "flat");
                gtk_widget_add_css_class(del_btn, "notif-del-btn");
                gtk_widget_set_valign(del_btn, GTK_ALIGN_START);
                gtk_widget_set_tooltip_text(del_btn, "删除此通知");
                gtk_box_append(GTK_BOX(row), del_btn);

                /* Capture 手势: 点击删除按钮时阻止行手势触发 */
                {
                    GtkGesture* cap = gtk_gesture_click_new();
                    gtk_event_controller_set_propagation_phase(
                        GTK_EVENT_CONTROLLER(cap), GTK_PHASE_CAPTURE);
                    g_signal_connect(cap, "pressed",
                        G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                            gtk_gesture_set_state(g, GTK_EVENT_SEQUENCE_CLAIMED);
                        }), nullptr);
                    gtk_widget_add_controller(del_btn, GTK_EVENT_CONTROLLER(cap));
                }

                g_signal_connect(del_btn, "clicked",
                                 G_CALLBACK(+[](GtkWidget *btn, gpointer)
                                            {
                                                GtkWidget *r = gtk_widget_get_parent(btn);
                                                GtkWidget *lr = gtk_widget_get_parent(r);
                                                if (lr)
                                                    gtk_list_box_remove(
                                                        GTK_LIST_BOX(gtk_widget_get_parent(lr)), lr);
                                            }),
                                 nullptr);
            }

            /* ── 行点击手势 (bubble 阶段, 删除按钮的 capture 优先) ── */
            {
                GtkGesture* click = gtk_gesture_click_new();
                gtk_event_controller_set_propagation_phase(
                    GTK_EVENT_CONTROLLER(click), GTK_PHASE_BUBBLE);
                g_signal_connect_data(click, "pressed",
                    G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer d) {
                        GtkWidget* row_w = gtk_event_controller_get_widget(
                            GTK_EVENT_CONTROLLER(g));
                        const char* typ = static_cast<const char*>(d);

                        /* fatal 需连续点击 3 次 (2 秒超时) */
                        if (g_strcmp0(typ, "fatal") == 0) {
                            /* 取消之前的超时定时器 */
                            guint tid = GPOINTER_TO_UINT(
                                g_object_get_data(G_OBJECT(row_w), "notif-timeout-id"));
                            if (tid) {
                                g_source_remove(tid);
                                g_object_set_data(G_OBJECT(row_w), "notif-timeout-id", nullptr);
                            }

                            int cnt = GPOINTER_TO_INT(
                                g_object_get_data(G_OBJECT(row_w), "notif-click-cnt"));
                            cnt++;

                            /* 首次点击保存原始描述 */
                            if (cnt == 1) {
                                GtkWidget* tb0 = gtk_widget_get_next_sibling(
                                    gtk_widget_get_first_child(row_w));
                                if (tb0) {
                                    GtkWidget* d0 = gtk_widget_get_next_sibling(
                                        gtk_widget_get_first_child(tb0));
                                    if (d0 && GTK_IS_LABEL(d0))
                                        g_object_set_data_full(G_OBJECT(row_w),
                                            "notif-orig-desc",
                                            g_strdup(gtk_label_get_text(GTK_LABEL(d0))),
                                            g_free);
                                }
                            }

                            if (cnt < 3) {
                                g_object_set_data(G_OBJECT(row_w),
                                    "notif-click-cnt", GINT_TO_POINTER(cnt));
                                /* 更新描述文字 */
                                GtkWidget* tb = gtk_widget_get_next_sibling(
                                    gtk_widget_get_first_child(row_w));
                                if (tb) {
                                    GtkWidget* desc = gtk_widget_get_next_sibling(
                                        gtk_widget_get_first_child(tb));
                                    if (desc && GTK_IS_LABEL(desc)) {
                                        char hint[128];
                                        snprintf(hint, sizeof(hint),
                                            "现在只需要 %d 个步骤即可清除此通知", 3 - cnt);
                                        gtk_label_set_text(GTK_LABEL(desc), hint);
                                    }
                                }
                                /* 启动 2 秒超时: 过期则重置 */
                                guint new_tid = g_timeout_add(800,
                                    [](gpointer data) -> gboolean {
                                        GtkWidget* rw = GTK_WIDGET(data);
                                        g_object_set_data(G_OBJECT(rw),
                                            "notif-click-cnt", GINT_TO_POINTER(0));
                                        g_object_set_data(G_OBJECT(rw),
                                            "notif-timeout-id", nullptr);
                                        /* 恢复原始描述 */
                                        GtkWidget* tb2 = gtk_widget_get_next_sibling(
                                            gtk_widget_get_first_child(rw));
                                        if (tb2) {
                                            GtkWidget* d2 = gtk_widget_get_next_sibling(
                                                gtk_widget_get_first_child(tb2));
                                            if (d2 && GTK_IS_LABEL(d2)) {
                                                const char* orig = static_cast<const char*>(
                                                    g_object_get_data(G_OBJECT(rw), "notif-orig-desc"));
                                                if (orig)
                                                    gtk_label_set_text(GTK_LABEL(d2), orig);
                                            }
                                        }
                                        return G_SOURCE_REMOVE;
                                    }, row_w);
                                g_object_set_data(G_OBJECT(row_w),
                                    "notif-timeout-id", GUINT_TO_POINTER(new_tid));
                                return;
                            }
                            /* cnt >= 3: 清除定时器并继续删除 */
                            g_object_set_data(G_OBJECT(row_w), "notif-timeout-id", nullptr);
                        }

                        if (g_strcmp0(typ, "error") == 0) {
                            /* 跳转 CrashSpy 1.21.5 */
                            GObject* win = G_OBJECT(gtk_widget_get_root(row_w));
                            GtkWidget* htabs = static_cast<GtkWidget*>(
                                g_object_get_data(win, "header-tabs"));
                            if (htabs) {
                                for (GtkWidget* tab = gtk_widget_get_first_child(htabs);
                                     tab; tab = gtk_widget_get_next_sibling(tab)) {
                                    const char* pn = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(tab), "page-name"));
                                    if (pn && g_strcmp0(pn, "more") == 0) {
                                        g_signal_emit_by_name(tab, "clicked");
                                        break;
                                    }
                                }
                            }
                            AdwViewStack* main_stk = ADW_VIEW_STACK(
                                g_object_get_data(win, "main-stack"));
                            if (main_stk) {
                                GtkWidget* mp = adw_view_stack_get_child_by_name(
                                    main_stk, "more");
                                if (mp) {
                                    GtkWidget* left_nav = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(mp), "more-left-nav"));
                                    GtkWidget* mid_stk = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(mp), "more-mid-stack"));
                                    if (left_nav && mid_stk) {
                                        gtk_stack_set_visible_child_name(
                                            GTK_STACK(mid_stk), "mid-crash");
                                        for (GtkWidget* s = gtk_widget_get_first_child(left_nav);
                                             s; s = gtk_widget_get_next_sibling(s)) {
                                            if (!gtk_widget_has_css_class(s, "nav-item")) continue;
                                            const char* mp2 = static_cast<const char*>(
                                                g_object_get_data(G_OBJECT(s), "mid-page"));
                                            if (mp2 && g_strcmp0(mp2, "mid-crash") == 0)
                                                gtk_widget_add_css_class(s, "nav-item-active");
                                            else
                                                gtk_widget_remove_css_class(s, "nav-item-active");
                                        }
                                        /* 选中 1.21.5 (第二个子项) */
                                        GtkWidget* cn = gtk_stack_get_child_by_name(
                                            GTK_STACK(mid_stk), "mid-crash");
                                        if (cn) {
                                            GtkWidget* r2 = gtk_widget_get_first_child(cn);
                                            if (r2) r2 = gtk_widget_get_next_sibling(r2);
                                            if (r2 && gtk_widget_has_css_class(r2, "nav-item")) {
                                                GtkWidget* rs = static_cast<GtkWidget*>(
                                                    g_object_get_data(G_OBJECT(r2), "right-stack"));
                                                const char* rp = static_cast<const char*>(
                                                    g_object_get_data(G_OBJECT(r2), "right-page"));
                                                GtkWidget* ml = static_cast<GtkWidget*>(
                                                    g_object_get_data(G_OBJECT(r2), "mid-list"));
                                                if (rs && rp)
                                                    gtk_stack_set_visible_child_name(
                                                        GTK_STACK(rs), rp);
                                                if (ml) {
                                                    for (GtkWidget* s = gtk_widget_get_first_child(ml);
                                                         s; s = gtk_widget_get_next_sibling(s)) {
                                                        if (!gtk_widget_has_css_class(s, "nav-item")) continue;
                                                        if (s == r2)
                                                            gtk_widget_add_css_class(s, "nav-item-active");
                                                        else
                                                            gtk_widget_remove_css_class(s, "nav-item-active");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            /* 关闭抽屉 */
                            GtkWidget* rev = static_cast<GtkWidget*>(
                                g_object_get_data(win, "notif-revealer"));
                            if (rev) gtk_revealer_set_reveal_child(GTK_REVEALER(rev), FALSE);
                            g_object_set_data(win, "notif-open", GINT_TO_POINTER(0));
                            GtkWidget* bd = static_cast<GtkWidget*>(
                                g_object_get_data(win, "notif-backdrop"));
                            GtkWidget* outer = static_cast<GtkWidget*>(
                                g_object_get_data(win, "notif-outer"));
                            if (bd) gtk_widget_set_visible(bd, FALSE);
                            if (outer) gtk_widget_set_can_target(outer, FALSE);
                        }

                        if (g_strcmp0(typ, "warn") == 0) {
                            /* 检查是否为安全模式通知 → 跳转 Launcher 崩溃 */
                            GtkWidget* tb = gtk_widget_get_next_sibling(
                                gtk_widget_get_first_child(row_w));
                            const char* title_text = nullptr;
                            if (tb) {
                                GtkWidget* tl = gtk_widget_get_first_child(tb);
                                if (tl && GTK_IS_LABEL(tl))
                                    title_text = gtk_label_get_text(GTK_LABEL(tl));
                            }
                            if (title_text && g_strcmp0(title_text, "已进入安全模式") == 0) {
                                GObject* win = G_OBJECT(gtk_widget_get_root(row_w));
                                /* 切换到 More tab */
                                GtkWidget* htabs = static_cast<GtkWidget*>(
                                    g_object_get_data(win, "header-tabs"));
                                if (htabs) {
                                    for (GtkWidget* tab = gtk_widget_get_first_child(htabs);
                                         tab; tab = gtk_widget_get_next_sibling(tab)) {
                                        const char* pn = static_cast<const char*>(
                                            g_object_get_data(G_OBJECT(tab), "page-name"));
                                        if (pn && g_strcmp0(pn, "more") == 0) {
                                            g_signal_emit_by_name(tab, "clicked");
                                            break;
                                        }
                                    }
                                }
                                /* 导航到 CrashSpy → 启动器 崩溃 (第 6 项) */
                                AdwViewStack* main_stk = ADW_VIEW_STACK(
                                    g_object_get_data(win, "main-stack"));
                                if (main_stk) {
                                    GtkWidget* mp = adw_view_stack_get_child_by_name(
                                        main_stk, "more");
                                    if (mp) {
                                        GtkWidget* left_nav = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(mp), "more-left-nav"));
                                        GtkWidget* mid_stk = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(mp), "more-mid-stack"));
                                        if (left_nav && mid_stk) {
                                            gtk_stack_set_visible_child_name(
                                                GTK_STACK(mid_stk), "mid-crash");
                                            for (GtkWidget* s = gtk_widget_get_first_child(left_nav);
                                                 s; s = gtk_widget_get_next_sibling(s)) {
                                                if (!gtk_widget_has_css_class(s, "nav-item")) continue;
                                                const char* mp2 = static_cast<const char*>(
                                                    g_object_get_data(G_OBJECT(s), "mid-page"));
                                                if (mp2 && g_strcmp0(mp2, "mid-crash") == 0)
                                                    gtk_widget_add_css_class(s, "nav-item-active");
                                                else
                                                    gtk_widget_remove_css_class(s, "nav-item-active");
                                            }
                                            /* 选中第 6 个子项 (Launcher 崩溃, index 5) */
                                            GtkWidget* cn = gtk_stack_get_child_by_name(
                                                GTK_STACK(mid_stk), "mid-crash");
                                            if (cn) {
                                                GtkWidget* r6 = gtk_widget_get_first_child(cn);
                                                for (int k = 0; k < 5 && r6; k++)
                                                    r6 = gtk_widget_get_next_sibling(r6);
                                                if (r6 && gtk_widget_has_css_class(r6, "nav-item")) {
                                                    GtkWidget* rs = static_cast<GtkWidget*>(
                                                        g_object_get_data(G_OBJECT(r6), "right-stack"));
                                                    const char* rp = static_cast<const char*>(
                                                        g_object_get_data(G_OBJECT(r6), "right-page"));
                                                    GtkWidget* ml = static_cast<GtkWidget*>(
                                                        g_object_get_data(G_OBJECT(r6), "mid-list"));
                                                    if (rs && rp)
                                                        gtk_stack_set_visible_child_name(
                                                            GTK_STACK(rs), rp);
                                                    if (ml) {
                                                        for (GtkWidget* s = gtk_widget_get_first_child(ml);
                                                             s; s = gtk_widget_get_next_sibling(s)) {
                                                            if (!gtk_widget_has_css_class(s, "nav-item")) continue;
                                                            if (s == r6)
                                                                gtk_widget_add_css_class(s, "nav-item-active");
                                                            else
                                                                gtk_widget_remove_css_class(s, "nav-item-active");
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                /* 关闭抽屉 */
                                GtkWidget* rev = static_cast<GtkWidget*>(
                                    g_object_get_data(win, "notif-revealer"));
                                if (rev) gtk_revealer_set_reveal_child(GTK_REVEALER(rev), FALSE);
                                g_object_set_data(win, "notif-open", GINT_TO_POINTER(0));
                                GtkWidget* bd = static_cast<GtkWidget*>(
                                    g_object_get_data(win, "notif-backdrop"));
                                GtkWidget* outer = static_cast<GtkWidget*>(
                                    g_object_get_data(win, "notif-outer"));
                                if (bd) gtk_widget_set_visible(bd, FALSE);
                                if (outer) gtk_widget_set_can_target(outer, FALSE);
                            }
                        }

                        /* 清除当前通知项 */
                        GtkWidget* lr = gtk_widget_get_parent(row_w);
                        if (lr)
                            gtk_list_box_remove(
                                GTK_LIST_BOX(gtk_widget_get_parent(lr)), lr);
                    }),
                    (gpointer)g_strdup(n.type),
                    [](gpointer d, GClosure*) { g_free(d); },
                    GConnectFlags(0));
                gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
            }

            gtk_list_box_append(GTK_LIST_BOX(list), row);
        }

        /* ── 清理通知按钮 — 清除所有 can_clear=true 的通知 ── */
        g_signal_connect(trash_btn, "clicked", G_CALLBACK(+[](GtkWidget *, gpointer data)
                                                          {
                                                              GtkWidget *lst = static_cast<GtkWidget *>(data);
                                                              std::vector<GtkWidget *> to_remove;
                                                              GtkWidget *child = gtk_widget_get_first_child(lst);
                                                              while (child)
                                                              {
                                                                  GtkWidget *row_w = gtk_widget_get_first_child(child);
                                                                  if (row_w)
                                                                  {
                                                                      int cc = GPOINTER_TO_INT(
                                                                          g_object_get_data(G_OBJECT(row_w), "notif-can-clear"));
                                                                      if (cc)
                                                                          to_remove.push_back(child);
                                                                  }
                                                                  child = gtk_widget_get_next_sibling(child);
                                                              }
                                                              for (auto *r : to_remove)
                                                                  gtk_list_box_remove(GTK_LIST_BOX(lst), r);
                                                          }),
                         list);

        gtk_revealer_set_child(GTK_REVEALER(revealer), panel);

        /* 抽屉缩回时重置所有 fatal 通知的计数器 */
        g_signal_connect(revealer, "notify::child-revealed",
            G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer data) {
                if (gtk_revealer_get_child_revealed(GTK_REVEALER(obj)))
                    return;
                GtkWidget* lst = GTK_WIDGET(data);
                for (GtkWidget* ch = gtk_widget_get_first_child(lst);
                     ch; ch = gtk_widget_get_next_sibling(ch)) {
                    GtkWidget* row_w = gtk_widget_get_first_child(ch);
                    if (!row_w) continue;
                    const char* typ = static_cast<const char*>(
                        g_object_get_data(G_OBJECT(row_w), "notif-type"));
                    if (typ && g_strcmp0(typ, "fatal") == 0) {
                        g_object_set_data(G_OBJECT(row_w),
                            "notif-click-cnt", GINT_TO_POINTER(0));
                        guint tid = GPOINTER_TO_UINT(
                            g_object_get_data(G_OBJECT(row_w), "notif-timeout-id"));
                        if (tid) {
                            g_source_remove(tid);
                            g_object_set_data(G_OBJECT(row_w), "notif-timeout-id", nullptr);
                        }
                        /* 恢复原始描述文字 */
                        GtkWidget* tb = gtk_widget_get_next_sibling(
                            gtk_widget_get_first_child(row_w));
                        if (tb) {
                            GtkWidget* desc = gtk_widget_get_next_sibling(
                                gtk_widget_get_first_child(tb));
                            if (desc && GTK_IS_LABEL(desc)) {
                                const char* orig = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(row_w), "notif-orig-desc"));
                                if (orig)
                                    gtk_label_set_text(GTK_LABEL(desc), orig);
                            }
                        }
                    }
                }
            }), list);

        /* 关闭按钮 → 委托 NavigationController 收起抽屉 */
        g_signal_connect(close_btn, "clicked",
                         G_CALLBACK(+[](GtkWidget*, gpointer) {
                             NavigationController::instance().close_notification_drawer();
                         }),
                         nullptr);

        return outer;
    }

} // namespace pcl
