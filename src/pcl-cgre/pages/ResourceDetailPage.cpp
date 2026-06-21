#include "pages/ResourceDetailPage.hpp"
#include "app/NavigationController.hpp"
#include "network/ResourceFetcher.hpp"
#include "util/IconHelper.hpp"
#include "core/Log.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include <adwaita.h>

namespace pcl {

namespace {

/** Cached PangoAttrList factory — avoid repeated alloc/free. */
static PangoAttrList* bold_14pt_attr()
{
    static PangoAttrList* a = []() {
        auto* attr = pango_attr_list_new();
        pango_attr_list_insert(attr, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        pango_attr_list_insert(attr, pango_attr_size_new(14 * PANGO_SCALE));
        return attr;
    }();
    return a;
}

static PangoAttrList* size_9pt_attr()
{
    static PangoAttrList* a = []() {
        auto* attr = pango_attr_list_new();
        pango_attr_list_insert(attr, pango_attr_size_new(9 * PANGO_SCALE));
        return attr;
    }();
    return a;
}

}  // anonymous namespace

/* ═══════════════════════════════════════════════════════════════════════
 * build_resource_detail_page
 * ═══════════════════════════════════════════════════════════════════════ */

GtkWidget* build_resource_detail_page()
{
    /* ── 主容器: 左右双栏 (各自独立滚动，互不影响) ── */
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 32);
    gtk_widget_set_margin_start(main_box, 40);
    gtk_widget_set_margin_end(main_box, 40);
    gtk_widget_set_margin_top(main_box, 24);
    gtk_widget_set_margin_bottom(main_box, 24);

    /* 供 navigate 直接查找 */
    g_object_set_data(G_OBJECT(main_box), "detail-box", main_box);

    /* ═══════════════════════════════════════════════════════════════════
     * 左栏: 资源 Icon / 名称 / 简介 → 底部操作按钮 (全部居中)
     * ═══════════════════════════════════════════════════════════════════ */
    {
        GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_set_halign(left, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(left, GTK_ALIGN_FILL);
        gtk_widget_set_hexpand(left, FALSE);
        gtk_widget_set_vexpand(left, TRUE);
        gtk_widget_add_css_class(left, "detail-left");

        /* 资源 Icon (96px, 居中) */
        GtkWidget* logo = icon::load("package", 96);
        gtk_widget_set_halign(logo, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(logo, 96, 96);
        gtk_widget_set_hexpand(logo, FALSE);
        gtk_widget_set_vexpand(logo, FALSE);
        gtk_widget_add_css_class(logo, "resource-logo");
        gtk_widget_add_css_class(logo, "detail-icon");
        gtk_box_append(GTK_BOX(left), logo);
        g_object_set_data(G_OBJECT(main_box), "detail-logo", logo);

        /* 资源名称 (居中, 点击可复制) */
        GtkWidget* name_lbl = gtk_label_new("");
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_CENTER);
        gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(name_lbl), 28);
        gtk_widget_add_css_class(name_lbl, "card-title");
        gtk_widget_add_css_class(name_lbl, "detail-name");
        gtk_widget_set_tooltip_text(name_lbl, "点击复制名称");
        gtk_box_append(GTK_BOX(left), name_lbl);
        g_object_set_data(G_OBJECT(main_box), "detail-name", name_lbl);
        gtk_label_set_attributes(GTK_LABEL(name_lbl), bold_14pt_attr());

        /* 资源简介 (居中, 自动换行, 不截断) */
        GtkWidget* desc_lbl = gtk_label_new("");
        gtk_widget_set_halign(desc_lbl, GTK_ALIGN_CENTER);
        gtk_label_set_wrap(GTK_LABEL(desc_lbl), TRUE);
        gtk_label_set_lines(GTK_LABEL(desc_lbl), -1);
        gtk_label_set_max_width_chars(GTK_LABEL(desc_lbl), 32);
        gtk_widget_set_opacity(desc_lbl, 0.7);
        gtk_box_append(GTK_BOX(left), desc_lbl);
        g_object_set_data(G_OBJECT(main_box), "detail-desc", desc_lbl);

        /* 附加信息行 (作者 / 下载量 / 日期 / 来源 / 版本范围 / 协议, 9pt) */
        GtkWidget* info_lbl = gtk_label_new("");
        gtk_widget_set_halign(info_lbl, GTK_ALIGN_CENTER);
        gtk_widget_set_opacity(info_lbl, 0.5);
        gtk_box_append(GTK_BOX(left), info_lbl);
        g_object_set_data(G_OBJECT(main_box), "detail-info", info_lbl);
        gtk_label_set_attributes(GTK_LABEL(info_lbl), size_9pt_attr());

        /* 弹性留白 — 将按钮推至栏底 */
        {
            GtkWidget* sp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_set_vexpand(sp, TRUE);
            gtk_widget_set_hexpand(sp, FALSE);
            gtk_box_append(GTK_BOX(left), sp);
        }

        /* ── 底部操作按钮 ── */
        {
            GtkWidget* btn_group = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
            gtk_widget_set_halign(btn_group, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(btn_group, GTK_ALIGN_END);
            gtk_widget_set_vexpand(btn_group, FALSE);
            gtk_box_append(GTK_BOX(left), btn_group);

            /* 转到 Modrinth / CurseForge */
            GtkWidget* src_btn = gtk_button_new_with_label("转到 CurseForge");
            gtk_widget_set_halign(src_btn, GTK_ALIGN_CENTER);
            gtk_widget_set_hexpand(src_btn, FALSE);
            gtk_box_append(GTK_BOX(btn_group), src_btn);
            g_object_set_data(G_OBJECT(main_box), "detail-btn-source", src_btn);

            /* 转到 MC百科 */
            GtkWidget* mcmod_btn = gtk_button_new_with_label("转到 MC百科");
            gtk_widget_set_halign(mcmod_btn, GTK_ALIGN_CENTER);
            gtk_widget_set_hexpand(mcmod_btn, FALSE);
            gtk_box_append(GTK_BOX(btn_group), mcmod_btn);
            g_object_set_data(G_OBJECT(main_box), "detail-btn-mcmod", mcmod_btn);

            /* 翻译详情 */
            GtkWidget* trans_btn = gtk_button_new_with_label("翻译详情");
            gtk_widget_set_halign(trans_btn, GTK_ALIGN_CENTER);
            gtk_widget_set_hexpand(trans_btn, FALSE);
            gtk_box_append(GTK_BOX(btn_group), trans_btn);
            g_object_set_data(G_OBJECT(main_box), "detail-btn-translate", trans_btn);

            /* 复制名称 */
            GtkWidget* copy_btn = gtk_button_new_with_label("复制名称");
            gtk_widget_set_halign(copy_btn, GTK_ALIGN_CENTER);
            gtk_widget_set_hexpand(copy_btn, FALSE);
            gtk_box_append(GTK_BOX(btn_group), copy_btn);
            g_object_set_data(G_OBJECT(main_box), "detail-btn-copy", copy_btn);
        }

        /* 左栏独立滚动容器 */
        GtkWidget* left_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left_scroll),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_propagate_natural_width(
            GTK_SCROLLED_WINDOW(left_scroll), FALSE);
        gtk_scrolled_window_set_propagate_natural_height(
            GTK_SCROLLED_WINDOW(left_scroll), FALSE);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left_scroll), left);
        gtk_box_append(GTK_BOX(main_box), left_scroll);
    }

    /* ── 垂直分隔线 ── */
    {
        GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
        gtk_widget_set_valign(sep, GTK_ALIGN_FILL);
        gtk_widget_set_vexpand(sep, TRUE);
        gtk_widget_set_opacity(sep, 0.12);
        gtk_box_append(GTK_BOX(main_box), sep);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * 右栏: MC 版本标签 / 加载器标签 / 版本列表
     * ═══════════════════════════════════════════════════════════════════ */
    {
        GtkWidget* right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_halign(right, GTK_ALIGN_FILL);
        gtk_widget_set_valign(right, GTK_ALIGN_START);
        gtk_widget_set_hexpand(right, TRUE);
        gtk_widget_set_vexpand(right, TRUE);
        gtk_widget_add_css_class(right, "detail-right");

        /* ── 内容区 (始终可见，预留空间) ── */
        GtkWidget* content_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_set_hexpand(content_area, TRUE);
        gtk_widget_set_vexpand(content_area, TRUE);
        gtk_box_append(GTK_BOX(right), content_area);
        g_object_set_data(G_OBJECT(main_box), "detail-content", content_area);

        /* ── 加载占位: 齿轮图标 + 提示文字 ── */
        GtkWidget* loading_placeholder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_halign(loading_placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(loading_placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_hexpand(loading_placeholder, TRUE);
        gtk_widget_set_vexpand(loading_placeholder, TRUE);
        gtk_widget_add_css_class(loading_placeholder, "detail-loading");
        {
            GtkWidget* gear = icon::load("settings", 48);
            gtk_widget_set_halign(gear, GTK_ALIGN_CENTER);
            gtk_widget_set_opacity(gear, 0.35);
            gtk_box_append(GTK_BOX(loading_placeholder), gear);

            GtkWidget* hint = gtk_label_new("正在加载，稍安勿躁……");
            gtk_widget_set_halign(hint, GTK_ALIGN_CENTER);
            gtk_widget_set_opacity(hint, 0.45);
            gtk_box_append(GTK_BOX(loading_placeholder), hint);
            {
                PangoAttrList* a = pango_attr_list_new();
                pango_attr_list_insert(a, pango_attr_size_new(11 * PANGO_SCALE));
                gtk_label_set_attributes(GTK_LABEL(hint), a);
                pango_attr_list_unref(a);
            }
        }
        gtk_box_append(GTK_BOX(content_area), loading_placeholder);
        g_object_set_data(G_OBJECT(main_box), "detail-loading", loading_placeholder);

        /* 版本筛选标签栏 */
        GtkWidget* ver_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_add_css_class(ver_bar, "version-tab-bar");
        gtk_widget_set_hexpand(ver_bar, FALSE);
        gtk_widget_set_halign(ver_bar, GTK_ALIGN_START);

        /* 加载器筛选标签栏 */
        GtkWidget* loader_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        gtk_widget_add_css_class(loader_bar, "version-tab-bar");
        gtk_widget_set_hexpand(loader_bar, FALSE);
        gtk_widget_set_halign(loader_bar, GTK_ALIGN_START);
        /* 初始隐藏，回调中按需显示 */
        gtk_widget_set_visible(loader_bar, FALSE);

        /* 两行标签装入垂直 box */
        GtkWidget* bars_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_hexpand(bars_box, FALSE);
        gtk_widget_set_halign(bars_box, GTK_ALIGN_START);
        gtk_widget_set_margin_bottom(bars_box, 12);
        gtk_box_append(GTK_BOX(bars_box), ver_bar);
        gtk_box_append(GTK_BOX(bars_box), loader_bar);

        /* 单一 GtkScrolledWindow 包住两行 */
        GtkWidget* bar_scroll = gtk_scrolled_window_new();
        gtk_widget_add_css_class(bar_scroll, "ver-tab-scroll");
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(bar_scroll),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
        gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW(bar_scroll), FALSE);
        gtk_scrolled_window_set_propagate_natural_width(
            GTK_SCROLLED_WINDOW(bar_scroll), FALSE);
        gtk_scrolled_window_set_propagate_natural_height(
            GTK_SCROLLED_WINDOW(bar_scroll), FALSE);
        gtk_widget_set_hexpand(bar_scroll, TRUE);
        gtk_widget_set_vexpand(bar_scroll, FALSE);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(bar_scroll), bars_box);

        /* 鼠标滚轮 → 水平滚动 */
        {
            GtkEventController* wheel = gtk_event_controller_scroll_new(
                GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
            g_signal_connect(wheel, "scroll",
                G_CALLBACK(+[](GtkEventControllerScroll* ctrl,
                               double, double dy, gpointer) -> gboolean {
                    GtkWidget* w = gtk_event_controller_get_widget(
                        GTK_EVENT_CONTROLLER(ctrl));
                    GtkAdjustment* adj = gtk_scrolled_window_get_hadjustment(
                        GTK_SCROLLED_WINDOW(w));
                    double val = gtk_adjustment_get_value(adj);
                    double step = gtk_adjustment_get_step_increment(adj);
                    double max  = gtk_adjustment_get_upper(adj) -
                                  gtk_adjustment_get_page_size(adj);
                    double next = CLAMP(val + dy * step * 3, 0.0, max);
                    gtk_adjustment_set_value(adj, next);
                    return GDK_EVENT_STOP;
                }), nullptr);
            gtk_widget_add_controller(bar_scroll, GTK_EVENT_CONTROLLER(wheel));
        }

        GtkWidget* bar_wrap = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_hexpand(bar_wrap, TRUE);
        gtk_widget_set_vexpand(bar_wrap, FALSE);
        gtk_box_append(GTK_BOX(bar_wrap), bar_scroll);
        gtk_box_append(GTK_BOX(content_area), bar_wrap);

        g_object_set_data(G_OBJECT(main_box), "detail-ver-bar", ver_bar);
        g_object_set_data(G_OBJECT(main_box), "detail-loader-bar", loader_bar);
        g_object_set_data(G_OBJECT(main_box), "detail-ver-scroll", bar_scroll);

        /* 文件列表容器 (对标 PCL-CE PanResults，由回调动态填充) */
        GtkWidget* pan = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_hexpand(pan, TRUE);
        gtk_box_append(GTK_BOX(content_area), pan);
        g_object_set_data(G_OBJECT(main_box), "detail-pan-results", pan);

        /* 右栏独立滚动容器 */
        GtkWidget* right_scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(right_scroll),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_propagate_natural_width(
            GTK_SCROLLED_WINDOW(right_scroll), FALSE);
        gtk_scrolled_window_set_propagate_natural_height(
            GTK_SCROLLED_WINDOW(right_scroll), FALSE);
        gtk_widget_set_hexpand(right_scroll, TRUE);
        gtk_widget_set_vexpand(right_scroll, TRUE);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right_scroll), right);
        gtk_box_append(GTK_BOX(main_box), right_scroll);
    }

    return main_box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * build_back_nav — 详情页标题栏: [← icon] [资源名称]
 * ═══════════════════════════════════════════════════════════════════════ */

GtkWidget* build_back_nav()
{
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);

    /* 返回按钮 — 纯图标 */
    GtkWidget* back_btn = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(back_btn), FALSE);
    gtk_widget_add_css_class(back_btn, "header-tab");
    gtk_widget_set_tooltip_text(back_btn, "返回");
    gtk_button_set_child(GTK_BUTTON(back_btn),
                         icon::load("arrow-left-light", 16));
    gtk_box_append(GTK_BOX(bar), back_btn);
    g_object_set_data(G_OBJECT(bar), "back-btn", back_btn);

    /* 资源名称 */
    GtkWidget* title_lbl = gtk_label_new("");
    gtk_widget_add_css_class(title_lbl, "app-title");
    gtk_widget_set_valign(title_lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(bar), title_lbl);
    g_object_set_data(G_OBJECT(bar), "title-lbl", title_lbl);

    return bar;
}

/* ═══════════════════════════════════════════════════════════════════════
 * navigate_to_resource_detail
 * ═══════════════════════════════════════════════════════════════════════ */

void navigate_to_resource_detail(GtkWidget*        trigger_widget,
                                 const std::string& name,
                                 const std::string& description,
                                 const std::string& source,
                                 const std::string& version_range,
                                 uint64_t           download_count,
                                 const std::string& date_modified,
                                 const std::string& /*icon_url*/,
                                 const std::string& project_id,
                                 const std::string& author,
                                 const std::string& license_name,
                                 const std::string& project_url,
                                 const std::string& wiki_url,
                                 const std::string& /*source_url*/,
                                 const std::vector<std::string>& /*categories*/,
                                 const std::vector<std::string>& /*game_versions*/,
                                 uint64_t           /*followers*/)
{
    auto& nav = NavigationController::instance();
    if (!nav.window()) return;

    AdwViewStack* main_stk = nav.main_stack();
    if (!main_stk) return;

    GtkWidget* dl_page = adw_view_stack_get_child_by_name(main_stk, "download");
    if (!dl_page) return;

    GtkWidget* dl_stk = nav.dl_stack();
    if (!dl_stk) return;

    /* 保存当前页面名称 */
    const char* cur_name = gtk_stack_get_visible_child_name(GTK_STACK(dl_stk));
    if (cur_name && strcmp(cur_name, "detail") != 0) {
        g_object_set_data_full(G_OBJECT(dl_page), "prev-page",
                               g_strdup(cur_name), g_free);
    }

    /* 委托 NavigationController 处理 header/sidebar 切换 */
    nav.enter_detail_view(name);

    /* 切换到详情页 */
    gtk_stack_set_visible_child_name(GTK_STACK(dl_stk), "detail");

    /* 填充详情页数据 — 通过 g_object_data 直接查找, 不依赖 widget tree */
    GtkWidget* detail = gtk_stack_get_child_by_name(GTK_STACK(dl_stk), "detail");
    if (!detail) { LOG_ERR("navigate_detail: detail child not found in stack"); return; }

    GtkWidget* box = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(detail), "detail-box"));
    if (!box) { LOG_ERR("navigate_detail: detail-box not found on scrolled window"); return; }

    /* 打印当前 logo 的信息 */
    {
        GtkWidget* cur_logo = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-logo"));
        if (cur_logo) {
            int w = gtk_widget_get_width(cur_logo);
            int h = gtk_widget_get_height(cur_logo);
            int sw = 0, sh = 0;
            gtk_widget_get_size_request(cur_logo, &sw, &sh);
            LOG_INFO("navigate_detail: cur logo actual=%dx%d size_req=%dx%d type=%s",
                     w, h, sw, sh, G_OBJECT_TYPE_NAME(cur_logo));
        }
    }

    /* 填充左栏数据 */
    {
        GtkWidget* name_lbl = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-name"));
        if (name_lbl) {
            gtk_label_set_text(GTK_LABEL(name_lbl), name.c_str());
            /* 点击名称复制到剪贴板 */
            std::string* name_for_gesture = new std::string(name);
            GtkGesture* click = gtk_gesture_click_new();
            g_signal_connect(click, "pressed",
                G_CALLBACK(+[](GtkGestureClick*, int, double, double, gpointer d) {
                    auto* txt = static_cast<std::string*>(d);
                    GdkClipboard* cb = gdk_display_get_clipboard(
                        gdk_display_get_default());
                    gdk_clipboard_set_text(cb, txt->c_str());
                }), name_for_gesture);
            gtk_widget_add_controller(name_lbl, GTK_EVENT_CONTROLLER(click));
        }

        GtkWidget* desc_lbl = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-desc"));
        if (desc_lbl)
            gtk_label_set_text(GTK_LABEL(desc_lbl), description.c_str());

        GtkWidget* info_lbl = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-info"));
        if (info_lbl) {
            std::string info;
            if (!author.empty()) {
                info += author;
                info += " · ";
            }
            info += resource::format_download_count(download_count);
            info += " · ";
            info += resource::format_date(date_modified.c_str());
            info += " · ";
            info += source;
            info += " · ";
            info += version_range;
            if (!license_name.empty()) {
                info += " · ";
                info += license_name;
            }
            gtk_label_set_text(GTK_LABEL(info_lbl), info.c_str());
        }
    }

    /* 来源按钮文字 + 点击打开链接 */
    {
        GtkWidget* src_btn = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-btn-source"));
        if (src_btn) {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), "转到 %s", source.c_str());
            gtk_button_set_label(GTK_BUTTON(src_btn), lbl);

            /* 点击打开项目页面 */
            std::string url_copy = project_url;
            g_signal_connect(src_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer d) {
                    auto* url = static_cast<std::string*>(d);
                    if (!url->empty()) {
                        GtkUriLauncher* launcher = gtk_uri_launcher_new(url->c_str());
                        gtk_uri_launcher_launch(launcher, nullptr, nullptr, nullptr, nullptr);
                        g_object_unref(launcher);
                    }
                }), new std::string(project_url));
        }
    }

    /* MC百科按钮 */
    {
        GtkWidget* mcmod_btn = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-btn-mcmod"));
        if (mcmod_btn && !wiki_url.empty()) {
            std::string* wu = new std::string(wiki_url);
            g_signal_connect(mcmod_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer d) {
                    auto* url = static_cast<std::string*>(d);
                    GtkUriLauncher* launcher2 = gtk_uri_launcher_new(url->c_str());
                    gtk_uri_launcher_launch(launcher2, nullptr, nullptr, nullptr, nullptr);
                    g_object_unref(launcher2);
                }), wu);
        }
    }

    /* 翻译详情按钮 */
    {
        GtkWidget* trans_btn = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-btn-translate"));
        if (trans_btn && !wiki_url.empty()) {
            std::string* wu = new std::string(wiki_url);
            g_signal_connect(trans_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer d) {
                    auto* url = static_cast<std::string*>(d);
                    GtkUriLauncher* launcher = gtk_uri_launcher_new(url->c_str());
                    gtk_uri_launcher_launch(launcher, nullptr, nullptr, nullptr, nullptr);
                    g_object_unref(launcher);
                }), wu);
        }
    }

    /* 复制名称按钮 */
    {
        GtkWidget* copy_btn = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-btn-copy"));
        if (copy_btn) {
            std::string* name_copy = new std::string(name);
            g_signal_connect(copy_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer d) {
                    auto* txt = static_cast<std::string*>(d);
                    GdkClipboard* cb = gdk_display_get_clipboard(
                        gdk_display_get_default());
                    gdk_clipboard_set_text(cb, txt->c_str());
                }), name_copy);
        }
    }

    /* ── 从列表项复用已加载的图标 (避免重复下载) ── */
    {
        GtkWidget* logo = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "detail-logo"));
        /* trigger_widget 是列表项 row → 取其 "logo" */
        GtkWidget* src_logo = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(trigger_widget), "logo"));
        LOG_INFO("navigate_detail: logo=%p type=%s src_logo=%p type=%s is_pic=%d is_img=%d",
                 (void*)logo, logo ? G_OBJECT_TYPE_NAME(logo) : "null",
                 (void*)src_logo, src_logo ? G_OBJECT_TYPE_NAME(src_logo) : "null",
                 src_logo ? (int)GTK_IS_PICTURE(src_logo) : -1,
                 src_logo ? (int)GTK_IS_IMAGE(src_logo) : -1);
        GdkPaintable* pt = nullptr;
        if (src_logo) {
            if (GTK_IS_PICTURE(src_logo))
                pt = gtk_picture_get_paintable(GTK_PICTURE(src_logo));
            else if (GTK_IS_IMAGE(src_logo))
                pt = gtk_image_get_paintable(GTK_IMAGE(src_logo));
        }
        if (logo && pt) {
            double pw = gdk_paintable_get_intrinsic_width(pt);
            double ph = gdk_paintable_get_intrinsic_height(pt);
            LOG_INFO("navigate_detail: paintable intrinsic size = %.0fx%.0f", pw, ph);
            /* GtkImage respects pixel_size, unlike GtkPicture which uses intrinsic size */
            GtkWidget* new_logo = gtk_image_new_from_paintable(pt);
            gtk_image_set_pixel_size(GTK_IMAGE(new_logo), 96);
            gtk_widget_set_halign(new_logo, GTK_ALIGN_CENTER);
            gtk_widget_set_size_request(new_logo, 96, 96);
            gtk_widget_set_hexpand(new_logo, FALSE);
            gtk_widget_set_vexpand(new_logo, FALSE);
            gtk_widget_add_css_class(new_logo, "resource-logo");
            gtk_widget_add_css_class(new_logo, "detail-icon");
            GtkWidget* parent = gtk_widget_get_parent(logo);
            if (parent) {
                gtk_box_remove(GTK_BOX(parent), logo);
                gtk_widget_insert_after(new_logo, parent, nullptr);
            }
            g_object_set_data(G_OBJECT(box), "detail-logo", new_logo);
            LOG_INFO("navigate_detail: replaced detail logo, new=%p size_req=96x96", (void*)new_logo);
            /* 强制触发布局重算 */
            gtk_widget_queue_resize(box);
        }
    }

    /* ── 拉取版本文件列表 ── */
    if (!project_id.empty()) {
        bool from_cf = (source == "CurseForge");
        LOG_INFO("navigate_detail: fetching files for project %s (cf=%d)",
                 project_id.c_str(), from_cf);

        /* 显示加载占位 */
        {
            GtkWidget* loading = static_cast<GtkWidget*>(
                g_object_get_data(G_OBJECT(box), "detail-loading"));
            if (loading) gtk_widget_set_visible(loading, TRUE);
        }

        GtkWidget* box_ptr = box;
        resource::fetch_project_files(project_id, from_cf,
            [box_ptr](std::vector<resource::CompFile> files) {
                /* 隐藏加载占位 */
                {
                    GtkWidget* loading = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(box_ptr), "detail-loading"));
                    if (loading) gtk_widget_set_visible(loading, FALSE);
                }
                GtkWidget* pan_results = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(box_ptr), "detail-pan-results"));
                GtkWidget* ver_bar = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(box_ptr), "detail-ver-bar"));
                GtkWidget* loader_bar = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(box_ptr), "detail-loader-bar"));

                /* ── loader 名称映射 ── */
                auto ldr_name = [](resource::CompLoaderType lt) -> const char* {
                    switch (lt) {
                        case resource::CompLoaderType::Forge:      return "Forge";
                        case resource::CompLoaderType::Fabric:     return "Fabric";
                        case resource::CompLoaderType::Quilt:      return "Quilt";
                        case resource::CompLoaderType::NeoForge:   return "NeoForge";
                        case resource::CompLoaderType::LiteLoader: return "LiteLoader";
                        case resource::CompLoaderType::Vanilla:    return "Vanilla";
                        default: return nullptr;
                    }
                };

                /* ── 提取版本号的纯数字前缀 ── */
                auto clean_ver = [](const std::string& gv) -> std::string {
                    /* 'w' 版本 (快照/开发版) 保留原样 */
                    if (gv.find('w') != std::string::npos)
                        return gv;
                    size_t ndots = 0;
                    for (size_t i = 0; i < gv.size(); i++) {
                        char c = gv[i];
                        if (c == '.') {
                            ndots++;
                            if (ndots > 2) return gv.substr(0, i);
                        } else if (c < '0' || c > '9') {
                            return gv.substr(0, i);
                        }
                    }
                    return gv;
                };
                auto major_ver = [&](const std::string& gv) -> std::string {
                    std::string n = clean_ver(gv);
                    auto d1 = n.find('.');
                    if (d1 == std::string::npos) return "";
                    auto d2 = n.find('.', d1 + 1);
                    return (d2 != std::string::npos) ? n.substr(0, d2) : n;
                };

                /* ── 提取大版本号 + 开发/远古版本标记 ── */
                std::vector<std::string> majors;
                bool has_w = false, has_ancient = false;
                {
                    std::set<std::string> seen;
                    for (auto& f : files)
                        for (auto& gv : f.game_versions) {
                            if (gv.find('w') != std::string::npos) {
                                has_w = true;
                            } else {
                                std::string m = major_ver(gv);
                                if (!m.empty() && seen.insert(m).second)
                                    majors.push_back(m);
                                else if (m.empty() && !gv.empty())
                                    has_ancient = true;
                            }
                        }
                }
                std::sort(majors.begin(), majors.end(),
                    [](const std::string& a, const std::string& b) {
                        auto va = strtod(a.c_str(), nullptr);
                        auto vb = strtod(b.c_str(), nullptr);
                        if (va != vb) return va > vb;
                        return a > b;
                    });

                /* ── 提取加载器类型并排序 ── */
                std::vector<resource::CompLoaderType> all_loaders;
                {
                    std::set<resource::CompLoaderType> seen;
                    for (auto& f : files)
                        for (auto& lt : f.mod_loaders)
                            if (seen.insert(lt).second)
                                all_loaders.push_back(lt);
                }
                std::sort(all_loaders.begin(), all_loaders.end(),
                    [](auto a, auto b) { return (int)a < (int)b; });

                /* ── 构建版本标签栏 ── */
                if (ver_bar) {
                    GtkWidget* ch;
                    while ((ch = gtk_widget_get_first_child(ver_bar)))
                        gtk_box_remove(GTK_BOX(ver_bar), ch);

                    /* 添加标签的 lambda */
                    auto add_ver_tab = [&](const char* label, bool is_default) {
                        GtkWidget* btn = gtk_button_new_with_label(label);
                        gtk_widget_add_css_class(btn, "version-tab");
                        if (is_default) gtk_widget_add_css_class(btn, "version-tab-active");
                        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
                        gtk_widget_set_hexpand(btn, FALSE);
                        g_object_set_data(G_OBJECT(btn), "box-ptr", box_ptr);
                        g_object_set_data_full(G_OBJECT(btn), "is-all",
                            g_strdup(is_default ? "1" : "0"), g_free);
                        g_signal_connect(btn, "clicked",
                            G_CALLBACK(+[](GtkWidget* b, gpointer) {
                                GtkWidget* bx = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(b), "box-ptr"));
                                GtkWidget* bar = gtk_widget_get_parent(b);
                                for (GtkWidget* s = gtk_widget_get_first_child(bar);
                                     s; s = gtk_widget_get_next_sibling(s))
                                    gtk_widget_remove_css_class(s, "version-tab-active");
                                gtk_widget_add_css_class(b, "version-tab-active");
                                g_object_set_data_full(G_OBJECT(bx), "cur-ver",
                                    g_strdup(gtk_button_get_label(GTK_BUTTON(b))), g_free);
                                /* 合并筛选 */
                                const char* cv = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ver"));
                                const char* cl = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ldr"));
                                bool av = !cv || strcmp(cv, "全部") == 0;
                                bool aw = !cv || strcmp(cv, "开发版本") == 0;
                                bool ao = !cv || strcmp(cv, "远古版本") == 0;
                                bool al = !cl || strcmp(cl, "全部") == 0;
                                GtkWidget* pan = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(bx), "detail-pan-results"));
                                if (!pan) return;
                                std::vector<std::string>* mj = static_cast<std::vector<std::string>*>(
                                    g_object_get_data(G_OBJECT(bx), "majors-ref"));
                                for (GtkWidget* card = gtk_widget_get_first_child(pan);
                                     card; card = gtk_widget_get_next_sibling(card)) {
                                    const char* key = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(card), "group-key"));
                                    if (!key) { gtk_widget_set_visible(card, TRUE); continue; }
                                    bool vis = true;
                                    if (!av) {
                                        const char* sp = strrchr(key, ' ');
                                        const char* ver = sp ? sp + 1 : key;
                                        if (aw) {
                                            vis = (strchr(ver, 'w') != nullptr);
                                        } else if (ao) {
                                            vis = true;
                                            if (mj && strchr(ver, 'w') == nullptr) {
                                                for (auto& m : *mj) {
                                                    size_t mlen = m.size();
                                                    if (strncmp(ver, m.c_str(), mlen) == 0 &&
                                                        !isdigit((unsigned char)ver[mlen])) {
                                                        vis = false; break;
                                                    }
                                                }
                                            }
                                        } else {
                                            size_t sl = strlen(cv);
                                            vis = (strncmp(ver, cv, sl) == 0 &&
                                                   !isdigit((unsigned char)ver[sl]));
                                        }
                                    }
                                    if (vis && !al) {
                                        const char* sp = strchr(key, ' ');
                                        vis = sp && (strncmp(key, cl, sp - key) == 0);
                                    }
                                    gtk_widget_set_visible(card, vis);
                                }
                            }), nullptr);
                        gtk_box_append(GTK_BOX(ver_bar), btn);
                    };

                    /* "全部" — 默认选中 */
                    add_ver_tab("全部", true);
                    for (auto& m : majors)
                        add_ver_tab(m.c_str(), false);
                    /* 开发版本 (含 'w' 的版本号) */
                    if (has_w)
                        add_ver_tab("开发版本", false);
                    /* 远古版本 (无法归入大版本且不含 'w') */
                    if (has_ancient)
                        add_ver_tab("远古版本", false);
                    /* 默认选中 "全部" 已在 add_ver_tab("全部", true) 中处理 */
                }

                /* ── 构建加载器标签栏，无加载器时隐藏 ── */
                if (loader_bar) {
                    /* 数据包/资源包/光影等没有 mod_loaders，隐藏加载器栏 */
                    gtk_widget_set_visible(loader_bar, !all_loaders.empty());
                    GtkWidget* ch;
                    while ((ch = gtk_widget_get_first_child(loader_bar)))
                        gtk_box_remove(GTK_BOX(loader_bar), ch);

                    /* 全部 + 各 loader，均无捕获 */
                    {
                        GtkWidget* btn = gtk_button_new_with_label("全部");
                        gtk_widget_add_css_class(btn, "version-tab");
                        gtk_widget_add_css_class(btn, "version-tab-active");
                        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
                        gtk_widget_set_hexpand(btn, FALSE);
                        g_object_set_data(G_OBJECT(btn), "box-ptr", box_ptr);
                        g_signal_connect(btn, "clicked",
                            G_CALLBACK(+[](GtkWidget* b, gpointer) {
                                GtkWidget* bx = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(b), "box-ptr"));
                                GtkWidget* bar = gtk_widget_get_parent(b);
                                for (GtkWidget* s = gtk_widget_get_first_child(bar);
                                     s; s = gtk_widget_get_next_sibling(s))
                                    gtk_widget_remove_css_class(s, "version-tab-active");
                                gtk_widget_add_css_class(b, "version-tab-active");
                                g_object_set_data_full(G_OBJECT(bx), "cur-ldr",
                                    g_strdup(gtk_button_get_label(GTK_BUTTON(b))), g_free);
                                /* 合并筛选 */
                                const char* cv = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ver"));
                                const char* cl = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ldr"));
                                bool av = !cv || strcmp(cv, "远古版本") == 0;
                                bool al_sel = !cl || strcmp(cl, "全部") == 0;
                                GtkWidget* pan = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(bx), "detail-pan-results"));
                                if (!pan) return;
                                for (GtkWidget* card = gtk_widget_get_first_child(pan);
                                     card; card = gtk_widget_get_next_sibling(card)) {
                                    const char* key = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(card), "group-key"));
                                    if (!key) { gtk_widget_set_visible(card, TRUE); continue; }
                                    bool vis = true;
                                    if (!av) {
                                        const char* sp = strrchr(key, ' ');
                                        const char* ver = sp ? sp + 1 : key;
                                        size_t sl = strlen(cv);
                                        vis = (strncmp(ver, cv, sl) == 0 &&
                                               !isdigit((unsigned char)ver[sl]));
                                    }
                                    if (vis && !al_sel) {
                                        const char* sp = strchr(key, ' ');
                                        vis = sp && (strncmp(key, cl, sp - key) == 0);
                                    }
                                    gtk_widget_set_visible(card, vis);
                                }
                            }), nullptr);
                        gtk_box_append(GTK_BOX(loader_bar), btn);
                    }
                    for (auto& lt : all_loaders) {
                        const char* nm = ldr_name(lt);
                        if (!nm) continue;
                        GtkWidget* btn = gtk_button_new_with_label(nm);
                        gtk_widget_add_css_class(btn, "version-tab");
                        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
                        gtk_widget_set_hexpand(btn, FALSE);
                        g_object_set_data(G_OBJECT(btn), "box-ptr", box_ptr);
                        g_signal_connect(btn, "clicked",
                            G_CALLBACK(+[](GtkWidget* b, gpointer) {
                                GtkWidget* bx = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(b), "box-ptr"));
                                GtkWidget* bar = gtk_widget_get_parent(b);
                                for (GtkWidget* s = gtk_widget_get_first_child(bar);
                                     s; s = gtk_widget_get_next_sibling(s))
                                    gtk_widget_remove_css_class(s, "version-tab-active");
                                gtk_widget_add_css_class(b, "version-tab-active");
                                g_object_set_data_full(G_OBJECT(bx), "cur-ldr",
                                    g_strdup(gtk_button_get_label(GTK_BUTTON(b))), g_free);
                                const char* cv = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ver"));
                                const char* cl = static_cast<const char*>(
                                    g_object_get_data(G_OBJECT(bx), "cur-ldr"));
                                bool av = !cv || strcmp(cv, "远古版本") == 0;
                                bool al_sel = !cl || strcmp(cl, "全部") == 0;
                                GtkWidget* pan = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(bx), "detail-pan-results"));
                                if (!pan) return;
                                for (GtkWidget* card = gtk_widget_get_first_child(pan);
                                     card; card = gtk_widget_get_next_sibling(card)) {
                                    const char* key = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(card), "group-key"));
                                    if (!key) { gtk_widget_set_visible(card, TRUE); continue; }
                                    bool vis = true;
                                    if (!av) {
                                        const char* sp = strrchr(key, ' ');
                                        const char* ver = sp ? sp + 1 : key;
                                        size_t sl = strlen(cv);
                                        vis = (strncmp(ver, cv, sl) == 0 &&
                                               !isdigit((unsigned char)ver[sl]));
                                    }
                                    if (vis && !al_sel) {
                                        const char* sp = strchr(key, ' ');
                                        vis = sp && (strncmp(key, cl, sp - key) == 0);
                                    }
                                    gtk_widget_set_visible(card, vis);
                                }
                            }), nullptr);
                        gtk_box_append(GTK_BOX(loader_bar), btn);
                    }
                }

                /* 存储 majors 引用供筛选回调使用 */
                auto* majors_ref = new std::vector<std::string>(majors);
                g_object_set_data_full(G_OBJECT(box_ptr), "majors-ref",
                    majors_ref, [](void* p) { delete static_cast<std::vector<std::string>*>(p); });

                /* ── 存储文件数据 ── */
                auto* files_ptr = new std::vector<resource::CompFile>(std::move(files));
                g_object_set_data_full(G_OBJECT(box_ptr), "detail-files",
                    files_ptr, [](void* p) { delete static_cast<std::vector<resource::CompFile>*>(p); });

                /* ── 按 [加载器] [清理后版本] 分组 ── */
                std::map<std::string, std::vector<resource::CompFile*>> groups;
                std::vector<std::string> group_order;
                {
                    std::set<std::string> seen_keys;
                    for (auto& f : *files_ptr) {
                        for (auto& gv : f.game_versions) {
                            std::string cv = clean_ver(gv);
                            if (cv.empty()) cv = gv;
                            if (f.mod_loaders.empty()) {
                                if (seen_keys.insert(cv).second)
                                    group_order.push_back(cv);
                                groups[cv].push_back(&f);
                            } else {
                                for (auto& lt : f.mod_loaders) {
                                    const char* nm = ldr_name(lt);
                                    if (!nm) continue;
                                    std::string key = std::string(nm) + " " + cv;
                                    if (seen_keys.insert(key).second)
                                        group_order.push_back(key);
                                    groups[key].push_back(&f);
                                }
                            }
                        }
                    }
                }
                /* 按版本号降序排列组 */
                std::sort(group_order.begin(), group_order.end(),
                    [](const std::string& a, const std::string& b) {
                        /* 提取版本号部分: "Forge 1.21.11" -> "1.21.11" */
                        auto extract_ver = [](const std::string& s) -> std::string {
                            auto sp = s.rfind(' ');
                            return (sp != std::string::npos) ? s.substr(sp + 1) : s;
                        };
                        std::string va = extract_ver(a);
                        std::string vb = extract_ver(b);
                        double na = strtod(va.c_str(), nullptr);
                        double nb = strtod(vb.c_str(), nullptr);
                        if (na != nb) return na > nb;
                        return a > b;
                    });

                /* ── 构建分组卡片 ── */
                if (pan_results) {
                    GtkWidget* ch;
                    while ((ch = gtk_widget_get_first_child(pan_results)))
                        gtk_box_remove(GTK_BOX(pan_results), ch);

                    for (size_t gi = 0; gi < group_order.size(); gi++) {
                        auto& key = group_order[gi];
                        auto& vec = groups[key];

                        /* 卡片容器 */
                        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                        gtk_widget_add_css_class(card, "card");
                        gtk_widget_set_margin_bottom(card, 15);

                        /* 卡片标题 (可点击展开) */
                        GtkWidget* header = gtk_button_new();
                        gtk_button_set_has_frame(GTK_BUTTON(header), FALSE);
                        gtk_widget_add_css_class(header, "card-header-btn");
                        {
                            GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                            gtk_widget_set_margin_start(hbox, 4);
                            gtk_widget_set_margin_end(hbox, 4);

                            GtkWidget* lbl = gtk_label_new(key.c_str());
                            gtk_widget_add_css_class(lbl, "card-title");
                            gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
                            gtk_widget_set_hexpand(lbl, TRUE);
                            gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
                            gtk_box_append(GTK_BOX(hbox), lbl);

                            char cnt[32];
                            snprintf(cnt, sizeof(cnt), "(%zu)", vec.size());
                            GtkWidget* cnt_lbl = gtk_label_new(cnt);
                            gtk_widget_set_opacity(cnt_lbl, 0.55);
                            gtk_widget_set_valign(cnt_lbl, GTK_ALIGN_CENTER);
                            gtk_box_append(GTK_BOX(hbox), cnt_lbl);

                            GtkWidget* arrow = icon::load("chevron-down", 18);
                            gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
                            gtk_box_append(GTK_BOX(hbox), arrow);
                            g_object_set_data(G_OBJECT(header), "arrow", arrow);

                            gtk_button_set_child(GTK_BUTTON(header), hbox);
                        }
                        gtk_box_append(GTK_BOX(card), header);
                        g_object_set_data_full(G_OBJECT(card), "group-key",
                            g_strdup(key.c_str()), g_free);

                        /* 文件列表 (在 revealer 内) */
                        GtkWidget* revealer = gtk_revealer_new();
                        gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
                        {
                            GtkWidget* list = gtk_list_box_new();
                            gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
                                                            GTK_SELECTION_NONE);
                            gtk_widget_add_css_class(list, "boxed-list");
                            gtk_widget_set_margin_start(list, 4);
                            gtk_widget_set_margin_end(list, 4);

                            /* 填充文件条目 */
                            for (auto* f : vec) {
                                std::string loader_str;
                                for (size_t li = 0; li < f->mod_loaders.size(); li++) {
                                    if (li > 0) loader_str += " / ";
                                    const char* nm = ldr_name(f->mod_loaders[li]);
                                    loader_str += nm ? nm : "?";
                                }

                                GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                                gtk_widget_set_margin_start(row, 8);
                                gtk_widget_set_margin_end(row, 8);
                                gtk_widget_set_margin_top(row, 5);
                                gtk_widget_set_margin_bottom(row, 5);

                                /* 左侧: 版本号 + 加载器标签 */
                                GtkWidget* info_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                                gtk_widget_set_valign(info_box, GTK_ALIGN_CENTER);

                                GtkWidget* ver_lbl = gtk_label_new(f->game_version.c_str());
                                gtk_widget_set_valign(ver_lbl, GTK_ALIGN_CENTER);
                                gtk_box_append(GTK_BOX(info_box), ver_lbl);
                                {
                                    PangoAttrList* a = pango_attr_list_new();
                                    pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                                    gtk_label_set_attributes(GTK_LABEL(ver_lbl), a);
                                    pango_attr_list_unref(a);
                                }

                                if (!loader_str.empty()) {
                                    GtkWidget* ldr_lbl = gtk_label_new(loader_str.c_str());
                                    gtk_widget_add_css_class(ldr_lbl, "resource-tag");
                                    gtk_widget_set_valign(ldr_lbl, GTK_ALIGN_CENTER);
                                    gtk_box_append(GTK_BOX(info_box), ldr_lbl);
                                }
                                gtk_box_append(GTK_BOX(row), info_box);

                                /* 文件名 */
                                GtkWidget* name_lbl = gtk_label_new(f->file_name.c_str());
                                gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
                                gtk_label_set_ellipsize(GTK_LABEL(name_lbl), PANGO_ELLIPSIZE_END);
                                gtk_widget_set_hexpand(name_lbl, TRUE);
                                gtk_widget_set_valign(name_lbl, GTK_ALIGN_CENTER);
                                gtk_widget_set_opacity(name_lbl, 0.55);
                                gtk_box_append(GTK_BOX(row), name_lbl);
                                {
                                    PangoAttrList* a = pango_attr_list_new();
                                    pango_attr_list_insert(a, pango_attr_size_new(10 * PANGO_SCALE));
                                    gtk_label_set_attributes(GTK_LABEL(name_lbl), a);
                                    pango_attr_list_unref(a);
                                }

                                /* 右侧箭头 */
                                GtkWidget* arw = icon::load("chevron-right", 16);
                                gtk_widget_set_valign(arw, GTK_ALIGN_CENTER);
                                gtk_widget_set_opacity(arw, 0.3);
                                gtk_box_append(GTK_BOX(row), arw);

                                gtk_list_box_append(GTK_LIST_BOX(list), row);
                            }

                            gtk_revealer_set_child(GTK_REVEALER(revealer), list);
                        }
                        gtk_box_append(GTK_BOX(card), revealer);

                        /* 展开/折叠回调 */
                        g_signal_connect(header, "clicked",
                            G_CALLBACK(+[](GtkWidget* btn, gpointer data) {
                                GtkWidget* rv = static_cast<GtkWidget*>(data);
                                gboolean vis = gtk_revealer_get_reveal_child(GTK_REVEALER(rv));
                                gtk_revealer_set_reveal_child(GTK_REVEALER(rv), !vis);
                                GtkWidget* ar = static_cast<GtkWidget*>(
                                    g_object_get_data(G_OBJECT(btn), "arrow"));
                                if (ar) {
                                    GtkWidget* parent = gtk_widget_get_parent(ar);
                                    GtkWidget* new_arrow = icon::load(
                                        vis ? "chevron-down" : "chevron-up", 18);
                                    gtk_widget_set_valign(new_arrow, GTK_ALIGN_CENTER);
                                    gtk_box_append(GTK_BOX(parent), new_arrow);
                                    gtk_box_remove(GTK_BOX(parent), ar);
                                    g_object_set_data(G_OBJECT(btn), "arrow", new_arrow);
                                }
                            }), revealer);

                        gtk_box_append(GTK_BOX(pan_results), card);

                        /* 第一个卡片自动展开 */
                        if (gi == 0)
                            gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), TRUE);
                    }

                    /* 仅一个卡片时自动展开 */
                    if (group_order.size() == 1 && !groups.empty()) {
                        GtkWidget* first_card = gtk_widget_get_first_child(pan_results);
                        if (first_card) {
                            GtkWidget* hdr = gtk_widget_get_first_child(first_card);
                            GtkWidget* rv = gtk_widget_get_next_sibling(hdr);
                            if (rv && GTK_IS_REVEALER(rv))
                                gtk_revealer_set_reveal_child(GTK_REVEALER(rv), TRUE);
                        }
                    }
                }

                LOG_INFO("navigate_detail: loaded %zu files, %zu major versions, %zu loaders, %zu groups",
                         files_ptr->size(), majors.size(), all_loaders.size(), group_order.size());
            });
    }
}

}  // namespace pcl
