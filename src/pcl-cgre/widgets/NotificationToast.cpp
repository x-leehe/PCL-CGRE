#include "widgets/NotificationToast.hpp"
#include "app/NavigationController.hpp"
#include "core/Colors.hpp"
#include "util/IconHelper.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <adwaita.h>

namespace pcl {

namespace {

/** Cached PangoAttrList for semibold weight — shared by all toasts. */
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
 * ToastData — 通知弹窗运行态数据
 * ============================================================================ */

struct ToastData {
    GtkWidget*    revealer;
    GtkWidget*    progress;
    GtkWidget*    desc_lbl;
    int           ticks_left;
    int           ticks_total;    // 初始 tick 数 (用于进度条)
    guint         timer_id;
    GtkWindow*    window;
    std::string   type;
    std::string   title;
    std::string   desc;
    bool          skip_center = false;
    bool          can_clear   = true;
    std::function<void()> on_click;
};

/* ============================================================================
 * Toast 堆叠管理 — 静态列表 + 重新定位逻辑
 * ============================================================================ */

static std::vector<ToastData*> active_toasts;

static constexpr int TOAST_EST_HEIGHT = 60;   // 单条 toast 估算高度 (含 ~4px 间距)
static constexpr int TOAST_BASE_MARGIN = 24;   // 最底部 toast 的 bottom margin

/** 重新计算所有活跃 toast 的 margin_bottom，保持从下往上堆叠 */
static void reposition_toasts()
{
    for (size_t i = 0; i < active_toasts.size(); i++) {
        int margin = TOAST_BASE_MARGIN + static_cast<int>(i) * TOAST_EST_HEIGHT;
        gtk_widget_set_margin_bottom(active_toasts[i]->revealer, margin);
    }
}

/** 从活跃列表中移除指定 toast 并重新定位剩余 */
static void unregister_toast(ToastData* td)
{
    auto it = std::find(active_toasts.begin(), active_toasts.end(), td);
    if (it != active_toasts.end()) {
        active_toasts.erase(it);
        reposition_toasts();
    }
}

/* ============================================================================
 * show_toast — 右下角弹窗通知 (统一入口)
 *
 *   进度条走完后自动缩回:
 *     add_to_center=true  → 放入通知中心
 *     add_to_center=false → 直接销毁
 * ============================================================================ */

void show_toast(GtkWindow* win, const ToastConfig& cfg)
{
    /* ── 定位主 overlay (从 window 数据直接获取) ── */
    GtkWidget* overlay = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(win), "main-overlay"));
    if (!overlay || !GTK_IS_OVERLAY(overlay)) return;

    int ticks = std::max(20, cfg.duration_ms) / 20;  // 20 ms / tick, min 1 tick

    auto* td = new ToastData{};
    td->window      = win;
    td->type        = cfg.type;
    td->title       = cfg.title;
    td->desc        = cfg.desc;
    td->skip_center = !cfg.add_to_center;
    td->can_clear   = cfg.can_clear;
    td->on_click    = cfg.on_click;
    td->ticks_left  = ticks;
    td->ticks_total = ticks;
    td->timer_id   = 0;

    /* ── 构建 Toast 内容 ── */
    GtkWidget* toast = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(toast, "toast-box");
    /* 按严重等级加背景色 */
    if (td->type == "info")       gtk_widget_add_css_class(toast, "toast-bg-info");
    else if (td->type == "warn")  gtk_widget_add_css_class(toast, "toast-bg-warn");
    else if (td->type == "error") gtk_widget_add_css_class(toast, "toast-bg-error");
    else if (td->type == "fatal") gtk_widget_add_css_class(toast, "toast-bg-fatal");
    gtk_widget_set_size_request(toast, 280, -1);

    /* 主行: 图标 + 文字 + 关闭按钮 */
    GtkWidget* main_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(main_row, 10);
    gtk_widget_set_margin_end(main_row, 8);
    gtk_widget_set_margin_top(main_row, 8);
    gtk_widget_set_margin_bottom(main_row, 6);
    gtk_box_append(GTK_BOX(toast), main_row);

    const char* icon_name = "info";
    const char* icon_css  = "notif-info";
    if (td->type == "warn")  { icon_name = "info"; icon_css = "notif-warn"; }
    else if (td->type == "error") { icon_name = "x"; icon_css = "notif-error"; }
    else if (td->type == "fatal") { icon_name = "x"; icon_css = "notif-fatal"; }

    GtkWidget* icn = icon::load(icon_name, 18);
    gtk_widget_add_css_class(icn, icon_css);
    gtk_widget_set_valign(icn, GTK_ALIGN_START);
    gtk_widget_set_margin_top(icn, 2);
    gtk_box_append(GTK_BOX(main_row), icn);

    GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_hexpand(text_box, TRUE);
    gtk_box_append(GTK_BOX(main_row), text_box);

    GtkWidget* title_lbl = gtk_label_new(td->title.c_str());
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_label_set_attributes(GTK_LABEL(title_lbl), semibold_attr());
    gtk_box_append(GTK_BOX(text_box), title_lbl);

    GtkWidget* desc_lbl = gtk_label_new(td->desc.c_str());
    gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(desc_lbl), TRUE);
    gtk_widget_set_opacity(desc_lbl, 0.6);
    gtk_box_append(GTK_BOX(text_box), desc_lbl);
    td->desc_lbl = desc_lbl;

    /* 关闭按钮 (fatal 类型无) */
    if (td->type != "fatal") {
        GtkWidget* close_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(close_btn), FALSE);
        gtk_button_set_child(GTK_BUTTON(close_btn), icon::load("x", 13));
        gtk_widget_add_css_class(close_btn, "flat");
        gtk_widget_set_valign(close_btn, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(main_row), close_btn);

        /* 捕获手势 — 阻止 × 按钮的点击冒泡到 toast 的 gesture */
        {
            GtkGesture* cap = gtk_gesture_click_new();
            gtk_event_controller_set_propagation_phase(
                GTK_EVENT_CONTROLLER(cap), GTK_PHASE_CAPTURE);
            g_signal_connect(cap, "pressed",
                G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                    gtk_gesture_set_state(g, GTK_EVENT_SEQUENCE_CLAIMED);
                }), nullptr);
            gtk_widget_add_controller(close_btn, GTK_EVENT_CONTROLLER(cap));
        }

        g_signal_connect(close_btn, "clicked",
            G_CALLBACK(+[](GtkWidget*, gpointer d) {
                auto* td2 = static_cast<ToastData*>(d);
                if (td2->timer_id) g_source_remove(td2->timer_id);
                gtk_revealer_set_reveal_child(GTK_REVEALER(td2->revealer), FALSE);
            }), td);
    }

    /* 进度条 */
    GtkWidget* prog = gtk_progress_bar_new();
    gtk_widget_add_css_class(prog, "toast-progress");
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog), 1.0);
    gtk_widget_set_margin_start(prog, 2);
    gtk_widget_set_margin_end(prog, 2);
    gtk_widget_set_margin_bottom(prog, 2);
    gtk_box_append(GTK_BOX(toast), prog);
    td->progress = prog;

    /* 点击回调 */
    if (td->on_click) {
        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed",
            G_CALLBACK(+[](GtkGesture*, int, double, double, gpointer d) {
                auto* td2 = static_cast<ToastData*>(d);
                if (td2->on_click) td2->on_click();
            }), td);
        gtk_widget_add_controller(toast, GTK_EVENT_CONTROLLER(click));
    }

    /* ── 悬停暂停倒计时 ── */
    {
        GtkEventController* motion = gtk_event_controller_motion_new();
        g_signal_connect(motion, "enter",
            G_CALLBACK(+[](GtkEventController*, double, double, gpointer d) {
                auto* td2 = static_cast<ToastData*>(d);
                if (td2->timer_id) {
                    g_source_remove(td2->timer_id);
                    td2->timer_id = 0;
                }
            }), td);
        g_signal_connect(motion, "leave",
            G_CALLBACK(+[](GtkEventController*, gpointer d) {
                auto* td2 = static_cast<ToastData*>(d);
                if (!td2->timer_id && td2->ticks_left > 0) {
                    td2->timer_id = g_timeout_add(20,
                        [](gpointer dd) -> gboolean {
                            auto* t = static_cast<ToastData*>(dd);
                            t->ticks_left--;
                            double frac = (double)t->ticks_left / t->ticks_total;
                            if (frac < 0.0) frac = 0.0;
                            if (t->progress && GTK_IS_PROGRESS_BAR(t->progress))
                                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(t->progress), frac);
                            if (t->ticks_left <= 0) {
                                t->timer_id = 0;
                                if (t->revealer && GTK_IS_REVEALER(t->revealer))
                                    gtk_revealer_set_reveal_child(GTK_REVEALER(t->revealer), FALSE);
                                return G_SOURCE_REMOVE;
                            }
                            return G_SOURCE_CONTINUE;
                        }, td2);
                }
            }), td);
        gtk_widget_add_controller(toast, GTK_EVENT_CONTROLLER(motion));
    }

    /* ── Toast 点击: 非 fatal 直接关闭, fatal 需 3 连击 ── */
    {
        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect_data(click, "pressed",
            G_CALLBACK(+[](GtkGesture*, int, double, double, gpointer d) {
                auto* td2 = static_cast<ToastData*>(d);
                if (td2->type != "fatal") {
                    /* 安全模式通知 → 导航到 CrashSpy Launcher 崩溃 (第 6 项) */
                    if (td2->title == "已进入安全模式" && td2->window) {
                        auto& nav = NavigationController::instance();

                        /* 1. 切换到 More 顶层页面 */
                        GtkWidget* htabs = nav.header_tabs();
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

                        /* 2. 获取 More 页面的内部 mid / right stack */
                        GtkWidget* mid_stk = nav.more_mid();
                        GtkWidget* right_stk = nav.more_right();

                        if (mid_stk && right_stk) {
                            /* 3. 切换 mid stack 到 CrashSpy */
                            gtk_stack_set_visible_child_name(
                                GTK_STACK(mid_stk), "mid-crash");

                            /* 4. 找到 Launcher 崩溃对应的 mid-crash 子项 (第 6 项, index 5) */
                            GtkWidget* cn = gtk_stack_get_child_by_name(
                                GTK_STACK(mid_stk), "mid-crash");
                            if (cn) {
                                GtkWidget* r6 = gtk_widget_get_first_child(cn);
                                for (int k = 0; k < 5 && r6; k++)
                                    r6 = gtk_widget_get_next_sibling(r6);

                                if (r6 && gtk_widget_has_css_class(r6, "nav-item")) {
                                    const char* rp = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(r6), "right-page"));
                                    GtkWidget* ml = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(r6), "mid-list"));

                                    /* 5. 切换 right stack 到目标页面 */
                                    if (rp)
                                        gtk_stack_set_visible_child_name(
                                            GTK_STACK(right_stk), rp);

                                    /* 6. 高亮 mid-crash 子项 (互斥) */
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

                            /* 7. 高亮 More 左栏的 CrashSpy 项 */
                            GtkWidget* left_nav = nav.more_left();
                            if (left_nav) {
                                GtkWidget* inner = gtk_scrolled_window_get_child(
                                    GTK_SCROLLED_WINDOW(left_nav));
                                if (inner) {
                                    for (GtkWidget* s = gtk_widget_get_first_child(inner);
                                         s; s = gtk_widget_get_next_sibling(s)) {
                                        if (!gtk_widget_has_css_class(s, "nav-item")) continue;
                                        const char* mp = static_cast<const char*>(
                                            g_object_get_data(G_OBJECT(s), "mid-page"));
                                        if (mp && g_strcmp0(mp, "mid-crash") == 0)
                                            gtk_widget_add_css_class(s, "nav-item-active");
                                        else
                                            gtk_widget_remove_css_class(s, "nav-item-active");
                                    }
                                }
                            }
                        }
                    }
                    if (td2->timer_id) g_source_remove(td2->timer_id);
                    gtk_revealer_set_reveal_child(
                        GTK_REVEALER(td2->revealer), FALSE);
                    return;
                }
                /* fatal: 3 连击 */
                guint tid = GPOINTER_TO_UINT(
                    g_object_get_data(G_OBJECT(td2->revealer), "toast-timeout-id"));
                if (tid) {
                    g_source_remove(tid);
                    g_object_set_data(G_OBJECT(td2->revealer), "toast-timeout-id", nullptr);
                }
                int cnt = GPOINTER_TO_INT(
                    g_object_get_data(G_OBJECT(td2->revealer), "toast-click-cnt"));
                cnt++;
                if (cnt == 1)
                    g_object_set_data_full(G_OBJECT(td2->revealer),
                        "toast-orig-desc", g_strdup(td2->desc.c_str()), g_free);
                if (cnt < 3) {
                    g_object_set_data(G_OBJECT(td2->revealer),
                        "toast-click-cnt", GINT_TO_POINTER(cnt));
                    char hint[128];
                    snprintf(hint, sizeof(hint),
                        "现在只需要 %d 个步骤即可清除此通知", 3 - cnt);
                    gtk_label_set_text(GTK_LABEL(td2->desc_lbl), hint);
                    guint nt = g_timeout_add(800,
                        [](gpointer dd) -> gboolean {
                            auto* t = static_cast<ToastData*>(dd);
                            g_object_set_data(G_OBJECT(t->revealer),
                                "toast-click-cnt", GINT_TO_POINTER(0));
                            g_object_set_data(G_OBJECT(t->revealer),
                                "toast-timeout-id", nullptr);
                            gtk_label_set_text(GTK_LABEL(t->desc_lbl),
                                t->desc.c_str());
                            return G_SOURCE_REMOVE;
                        }, td2);
                    g_object_set_data(G_OBJECT(td2->revealer),
                        "toast-timeout-id", GUINT_TO_POINTER(nt));
                    return;
                }
                g_object_set_data(G_OBJECT(td2->revealer), "toast-timeout-id", nullptr);
                if (td2->timer_id) g_source_remove(td2->timer_id);
                gtk_revealer_set_reveal_child(
                    GTK_REVEALER(td2->revealer), FALSE);
            }), td,
            [](gpointer, GClosure*) {}, GConnectFlags(0));
        gtk_widget_add_controller(toast, GTK_EVENT_CONTROLLER(click));
    }

    /* ── Revealer (从左侧滑入) ── */
    GtkWidget* rev = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(rev),
        GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(rev), 200);
    gtk_revealer_set_child(GTK_REVEALER(rev), toast);
    td->revealer = rev;

    /* 定位: 左下角, 新通知排在已有通知上方 */
    gtk_widget_set_halign(rev, GTK_ALIGN_START);
    gtk_widget_set_valign(rev, GTK_ALIGN_END);
    gtk_widget_set_margin_start(rev, 24);

    /* 注册到活跃列表 (新 toast 总是在最上面) */
    active_toasts.insert(active_toasts.begin(), td);
    reposition_toasts();

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), rev);
    gtk_revealer_set_reveal_child(GTK_REVEALER(rev), TRUE);

    /* ── 进度条定时器 (20 ms / tick, 共 150 ticks = 3 s) ── */
    td->timer_id = g_timeout_add(20,
        [](gpointer d) -> gboolean {
            auto* td2 = static_cast<ToastData*>(d);
            td2->ticks_left--;
            double frac = td2->ticks_left / (double)td2->ticks_total;
            if (frac < 0.0) frac = 0.0;
            if (td2->progress && GTK_IS_PROGRESS_BAR(td2->progress))
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(td2->progress), frac);
            if (td2->ticks_left <= 0) {
                td2->timer_id = 0;
                if (td2->revealer && GTK_IS_REVEALER(td2->revealer))
                    gtk_revealer_set_reveal_child(GTK_REVEALER(td2->revealer), FALSE);
                return G_SOURCE_REMOVE;
            }
            return G_SOURCE_CONTINUE;
        },
        td);

    /* ── 缩回完成后: 加入通知中心 + 清理 ── */
    g_signal_connect(rev, "notify::child-revealed",
        G_CALLBACK(+[](GObject* obj, GParamSpec*, gpointer d) {
            auto* td2 = static_cast<ToastData*>(d);
            if (gtk_revealer_get_child_revealed(GTK_REVEALER(obj)))
                return;

            /* 断开此信号，防止后续 cleanup 操作（如 remove_overlay）
             * 再次触发 child-revealed 导致无限递归。 */
            g_signal_handlers_disconnect_by_data(obj, d);

            /* 取消任何仍在运行的定时器 */
            if (td2->timer_id) {
                g_source_remove(td2->timer_id);
                td2->timer_id = 0;
            }

            /* 如果标记为 skip_center，直接清理，不加入通知中心 */
            if (td2->skip_center) {
                GtkWidget* ol = gtk_widget_get_parent(GTK_WIDGET(obj));
                if (ol && GTK_IS_OVERLAY(ol))
                    gtk_overlay_remove_overlay(GTK_OVERLAY(ol), GTK_WIDGET(obj));
                auto it = std::find(active_toasts.begin(), active_toasts.end(), td2);
                if (it != active_toasts.end()) active_toasts.erase(it);
                delete td2;
                return;
            }

            GtkWidget* notif_list = static_cast<GtkWidget*>(
                g_object_get_data(G_OBJECT(td2->window), "notif-list"));
            if (notif_list) {
                const char* icon_n = "info";
                const char* css_c  = "notif-info";
                const char* bg_c   = "notif-bg-info";
                if (td2->type == "warn") {
                    icon_n = "circle-help"; css_c = "notif-warn"; bg_c = "notif-bg-warn";
                } else if (td2->type == "error") {
                    icon_n = "wrench"; css_c = "notif-error"; bg_c = "notif-bg-error";
                } else if (td2->type == "fatal") {
                    icon_n = "x"; css_c = "notif-fatal"; bg_c = "notif-bg-fatal";
                }

                GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                gtk_widget_add_css_class(row, "notif-item");
                gtk_widget_add_css_class(row, bg_c);
                gtk_widget_set_can_target(row, TRUE);

                g_object_set_data_full(G_OBJECT(row), "notif-type",
                    g_strdup(td2->type.c_str()), g_free);

                GtkWidget* ic = icon::load(icon_n, 20);
                gtk_widget_add_css_class(ic, css_c);
                gtk_widget_set_valign(ic, GTK_ALIGN_START);
                gtk_widget_set_margin_top(ic, 2);
                gtk_box_append(GTK_BOX(row), ic);

                GtkWidget* tb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
                gtk_widget_set_hexpand(tb, TRUE);
                gtk_box_append(GTK_BOX(row), tb);

                GtkWidget* tl = gtk_label_new(td2->title.c_str());
                gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a,
                        pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(tl), a);
                    pango_attr_list_unref(a);
                }
                gtk_box_append(GTK_BOX(tb), tl);

                GtkWidget* dl = gtk_label_new(td2->desc.c_str());
                gtk_label_set_xalign(GTK_LABEL(dl), 0.0f);
                gtk_label_set_wrap(GTK_LABEL(dl), TRUE);
                gtk_widget_set_opacity(dl, 0.65);
                gtk_box_append(GTK_BOX(tb), dl);

                if (td2->type != "fatal") {
                    GtkWidget* del_btn = gtk_button_new();
                    gtk_button_set_has_frame(GTK_BUTTON(del_btn), FALSE);
                    gtk_button_set_child(GTK_BUTTON(del_btn),
                                         icon::load("x", 16));
                    gtk_widget_add_css_class(del_btn, "flat");
                    gtk_widget_add_css_class(del_btn, "notif-del-btn");
                    gtk_widget_set_valign(del_btn, GTK_ALIGN_START);
                    gtk_box_append(GTK_BOX(row), del_btn);

                    g_signal_connect(del_btn, "clicked",
                        G_CALLBACK(+[](GtkWidget* btn, gpointer) {
                            GtkWidget* r = gtk_widget_get_parent(btn);
                            GtkWidget* lr = gtk_widget_get_parent(r);
                            if (lr)
                                gtk_list_box_remove(
                                    GTK_LIST_BOX(gtk_widget_get_parent(lr)), lr);
                        }), nullptr);
                }

                gtk_list_box_prepend(GTK_LIST_BOX(notif_list), row);

                /* 存储 can_clear 标志 — 供垃圾桶按钮判断 */
                g_object_set_data(G_OBJECT(row),
                    "notif-can-clear", GINT_TO_POINTER(td2->can_clear ? 1 : 0));

                /* 添加点击消除手势 (fatal 需点 3 次) */
                {
                    const char* typ = g_strdup(td2->type.c_str());
                    GtkGesture* click = gtk_gesture_click_new();
                    g_signal_connect_data(click, "pressed",
                        G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer d) {
                            GtkWidget* rw = gtk_event_controller_get_widget(
                                GTK_EVENT_CONTROLLER(g));
                            const char* typ = static_cast<const char*>(d);

                            if (g_strcmp0(typ, "fatal") == 0) {
                                guint tid = GPOINTER_TO_UINT(
                                    g_object_get_data(G_OBJECT(rw), "notif-timeout-id"));
                                if (tid) {
                                    g_source_remove(tid);
                                    g_object_set_data(G_OBJECT(rw), "notif-timeout-id", nullptr);
                                }

                                int cnt = GPOINTER_TO_INT(
                                    g_object_get_data(G_OBJECT(rw), "notif-click-cnt"));
                                cnt++;

                                /* 首次点击时保存原始描述 */
                                if (cnt == 1) {
                                    GtkWidget* tb0 = gtk_widget_get_next_sibling(
                                        gtk_widget_get_first_child(rw));
                                    if (tb0) {
                                        GtkWidget* d0 = gtk_widget_get_next_sibling(
                                            gtk_widget_get_first_child(tb0));
                                        if (d0 && GTK_IS_LABEL(d0))
                                            g_object_set_data_full(G_OBJECT(rw),
                                                "notif-orig-desc",
                                                g_strdup(gtk_label_get_text(GTK_LABEL(d0))),
                                                g_free);
                                    }
                                }

                                if (cnt < 3) {
                                    g_object_set_data(G_OBJECT(rw),
                                        "notif-click-cnt", GINT_TO_POINTER(cnt));
                                    GtkWidget* tb = gtk_widget_get_next_sibling(
                                        gtk_widget_get_first_child(rw));
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
                                    guint new_tid = g_timeout_add(800,
                                        [](gpointer data) -> gboolean {
                                            GtkWidget* rw2 = GTK_WIDGET(data);
                                            g_object_set_data(G_OBJECT(rw2),
                                                "notif-click-cnt", GINT_TO_POINTER(0));
                                            g_object_set_data(G_OBJECT(rw2),
                                                "notif-timeout-id", nullptr);
                                            GtkWidget* tb2 = gtk_widget_get_next_sibling(
                                                gtk_widget_get_first_child(rw2));
                                            if (tb2) {
                                                GtkWidget* d2 = gtk_widget_get_next_sibling(
                                                    gtk_widget_get_first_child(tb2));
                                                if (d2 && GTK_IS_LABEL(d2)) {
                                                    const char* orig = static_cast<const char*>(
                                                        g_object_get_data(G_OBJECT(rw2), "notif-orig-desc"));
                                                    gtk_label_set_text(GTK_LABEL(d2),
                                                        orig ? orig : "严重错误");
                                                }
                                            }
                                            return G_SOURCE_REMOVE;
                                        }, rw);
                                    g_object_set_data(G_OBJECT(rw),
                                        "notif-timeout-id", GUINT_TO_POINTER(new_tid));
                                    return;
                                }
                                g_object_set_data(G_OBJECT(rw), "notif-timeout-id", nullptr);
                            }

                            GtkWidget* lr = gtk_widget_get_parent(rw);
                            if (lr)
                                gtk_list_box_remove(
                                    GTK_LIST_BOX(gtk_widget_get_parent(lr)), lr);
                        }), (gpointer)typ,
                        [](gpointer d, GClosure*) { g_free((gpointer)d); },
                        GConnectFlags(0));
                    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
                }
            }

            GtkWidget* parent = gtk_widget_get_parent(
                GTK_WIDGET(td2->revealer));
            if (parent)
                gtk_overlay_remove_overlay(GTK_OVERLAY(parent),
                                           td2->revealer);

            /* 从堆叠列表移除, 上方 toast 自动下沉 */
            unregister_toast(td2);
            delete td2;
        }), td);
}

}  // namespace pcl
