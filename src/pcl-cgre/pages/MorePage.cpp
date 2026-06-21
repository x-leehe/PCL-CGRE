#include "pages/MorePage.hpp"
#include "pages/LaunchPage.hpp"
#include "app/NavigationController.hpp"
#include "core/Colors.hpp"
#include "util/IconHelper.hpp"
#include "widgets/NotificationToast.hpp"
#include "pclcore/pclcore.hpp"

#include <cstdlib>
#include <string>
#include <vector>
#include <adwaita.h>

namespace pcl {

using namespace pcl::colors;

/* ── 声明自 DownloadPage.cpp ── */
GtkWidget* build_placeholder_page(const char* icon, const char* desc);

/* ── Blueprint 路径解析 (相对于二进制) ── */

/* ============================================================================
 * build_more_page — 更多页面主入口 (三栏层级布局)
 *
 *   左栏: 3 个一级分类 (帮助 / 工具箱 / 插件)
 *   中栏: 二级子项导航 (随左栏切换), nav-sidebar 风格
 *   右栏: 卡片内容 (随中栏切换)
 * ============================================================================ */
GtkWidget* build_more_page()
{
    /* ═══════════════════════════════════════════════════════════════════
     *  右栏 GtkStack — 先创建, 供中栏子项引用
     * ═══════════════════════════════════════════════════════════════════ */
    GtkWidget* right_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(right_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(right_stack), 150);
    gtk_widget_set_hexpand(right_stack, TRUE);

    /* ── 帮助与支持 ── */
    {
        GtkBuilder* b = icon::load_ui("help_support.ui");
        GtkWidget* p = GTK_WIDGET(gtk_builder_get_object(b, "help_support_page"));
        g_object_ref(p);

        GtkWidget* inner = GTK_WIDGET(gtk_builder_get_object(b, "help_support_inner"));
        auto helps = pclcore::local::get_help_content_provider().get_help_entries();
        for (auto& h : helps) {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_bottom(row, 18);
            gtk_box_append(GTK_BOX(inner), row);

            GtkWidget* text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
            gtk_widget_set_hexpand(text_box, TRUE);
            gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(row), text_box);

            GtkWidget* tl = gtk_label_new(h.title.c_str());
            gtk_label_set_xalign(GTK_LABEL(tl), 0.0f);
            {
                PangoAttrList* a = pango_attr_list_new();
                pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                gtk_label_set_attributes(GTK_LABEL(tl), a);
                pango_attr_list_unref(a);
            }
            gtk_box_append(GTK_BOX(text_box), tl);

            GtkWidget* dl = gtk_label_new(h.description.c_str());
            gtk_label_set_xalign(GTK_LABEL(dl), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(dl), TRUE);
            gtk_widget_set_opacity(dl, 0.6);
            gtk_box_append(GTK_BOX(text_box), dl);

            GtkWidget* btn = gtk_button_new_with_label(h.action_label.c_str());
            gtk_widget_add_css_class(btn, "suggested-action");
            gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
            g_signal_connect(btn, "clicked",
                G_CALLBACK(+[](GtkWidget*, gpointer d) {
                    GtkUriLauncher* ul = gtk_uri_launcher_new((const char*)d);
                    gtk_uri_launcher_launch(ul, nullptr, nullptr, nullptr, nullptr);
                    g_object_unref(ul);
                }), g_strdup(h.url.c_str()));
            gtk_box_append(GTK_BOX(row), btn);
        }
        g_object_unref(b);
        gtk_stack_add_named(GTK_STACK(right_stack), p, "page-help-support");
    }

    /* ── 常见问题 ── */
    {
        GtkBuilder* b = icon::load_ui("help_faq.ui");
        GtkWidget* p = GTK_WIDGET(gtk_builder_get_object(b, "help_faq_page"));
        g_object_ref(p);

        GtkWidget* inner = GTK_WIDGET(gtk_builder_get_object(b, "help_faq_inner"));
        auto faqs = pclcore::local::get_help_content_provider().get_faq_entries();
        for (auto& f : faqs) {
            GtkWidget* ql = gtk_label_new(f.question.c_str());
            gtk_label_set_xalign(GTK_LABEL(ql), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(ql), TRUE);
            {
                PangoAttrList* a = pango_attr_list_new();
                pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                gtk_label_set_attributes(GTK_LABEL(ql), a);
                pango_attr_list_unref(a);
            }
            gtk_box_append(GTK_BOX(inner), ql);

            GtkWidget* al = gtk_label_new(f.answer.c_str());
            gtk_label_set_xalign(GTK_LABEL(al), 0.0f);
            gtk_label_set_wrap(GTK_LABEL(al), TRUE);
            gtk_widget_set_opacity(al, 0.7);
            gtk_widget_set_margin_bottom(al, 16);
            gtk_box_append(GTK_BOX(inner), al);
        }
        g_object_unref(b);
        gtk_stack_add_named(GTK_STACK(right_stack), p, "page-help-faq");
    }

    /* ── 实用工具 ── */
    {
        GtkBuilder* b = icon::load_ui("tools.ui");
        GtkWidget* p = GTK_WIDGET(gtk_builder_get_object(b, "tools_page"));
        g_object_ref(p);

        GtkWidget* flow = GTK_WIDGET(gtk_builder_get_object(b, "tool_flow"));
        auto tools = pclcore::local::get_tool_provider().get_tools();
        for (auto& t : tools) {
            GtkWidget* btn = gtk_button_new_with_label(t.label.c_str());
            gtk_widget_set_size_request(btn, 120, 50);
            if (!t.css_class.empty())
                gtk_widget_add_css_class(btn, t.css_class.c_str());
            gtk_flow_box_insert(GTK_FLOW_BOX(flow), btn, -1);
        }
        g_object_unref(b);
        gtk_stack_add_named(GTK_STACK(right_stack), p, "page-tools");
    }

    /* ── 皮肤 ── */
    {
        GtkBuilder* b = icon::load_ui("skin.ui");
        GtkWidget* p = GTK_WIDGET(gtk_builder_get_object(b, "skin_page"));
        g_object_ref(p);
        g_object_unref(b);
        gtk_stack_add_named(GTK_STACK(right_stack), p, "page-skin");
    }

    /* ── 插件占位 ── */
    {
        GtkWidget* p = build_placeholder_page("puzzle",
            "插件功能尚未实现，敬请期待。");
        gtk_stack_add_named(GTK_STACK(right_stack), p, "page-plugin");
    }

    /* ── CrashSpy 崩溃分析中心 ── */
    {
        auto crashes = pclcore::local::get_crash_provider().get_crash_reports();

        for (size_t ci = 0; ci < crashes.size(); ci++) {
            const auto& cd = crashes[ci];

            GtkWidget* page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
            gtk_widget_set_margin_start(page, 24);
            gtk_widget_set_margin_end(page, 24);
            gtk_widget_set_margin_top(page, 24);
            gtk_widget_set_margin_bottom(page, 24);

            GtkWidget* scroll = gtk_scrolled_window_new();
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                           GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
            gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), page);

            /* ── 卡片1: 崩溃概览 (来源 + 标题 + 实例 + 严重程度) ── */
            {
                GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_add_css_class(card, "card");
                gtk_widget_set_margin_bottom(card, 8);

                /* 第一行: 来源徽章 + 严重程度 */
                GtkWidget* hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_widget_set_margin_bottom(hdr, 4);

                GtkWidget* badge = gtk_label_new(
                    pclcore::local::crash_source_label(cd.source));
                gtk_widget_add_css_class(badge, "crash-badge");
                gtk_label_set_xalign(GTK_LABEL(badge), 0.0f);
                gtk_widget_set_halign(badge, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(hdr), badge);

                {
                    std::string sev = std::string(cd.severity);
                    GtkWidget* sev_lbl = gtk_label_new(sev.c_str());
                    if (cd.severity == "致命")
                        gtk_widget_add_css_class(sev_lbl, "crash-severity-fatal");
                    else if (cd.severity == "警告")
                        gtk_widget_add_css_class(sev_lbl, "crash-severity-warn");
                    gtk_widget_set_halign(sev_lbl, GTK_ALIGN_END);
                    gtk_widget_set_hexpand(sev_lbl, TRUE);
                    gtk_label_set_xalign(GTK_LABEL(sev_lbl), 1.0f);
                    gtk_widget_set_opacity(sev_lbl, 0.7);
                    gtk_box_append(GTK_BOX(hdr), sev_lbl);
                }
                gtk_box_append(GTK_BOX(card), hdr);

                /* 崩溃标题 */
                {
                    GtkWidget* title = gtk_label_new(cd.title.c_str());
                    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
                    gtk_label_set_wrap(GTK_LABEL(title), TRUE);
                    {
                        PangoAttrList* a = pango_attr_list_new();
                        pango_attr_list_insert(a,
                            pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                        pango_attr_list_insert(a,
                            pango_attr_size_new(13 * PANGO_SCALE));
                        gtk_label_set_attributes(GTK_LABEL(title), a);
                        pango_attr_list_unref(a);
                    }
                    gtk_widget_set_margin_bottom(title, 4);
                    gtk_box_append(GTK_BOX(card), title);
                }

                /* 关联实例 (如果有) */
                if (!cd.instance_name.empty()) {
                    GtkWidget* inst_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                    GtkWidget* inst_lbl = gtk_label_new("实例:");
                    gtk_widget_set_opacity(inst_lbl, 0.6);
                    gtk_box_append(GTK_BOX(inst_row), inst_lbl);
                    GtkWidget* inst_val = gtk_label_new(cd.instance_name.c_str());
                    gtk_box_append(GTK_BOX(inst_row), inst_val);
                    gtk_box_append(GTK_BOX(card), inst_row);
                }
                gtk_box_append(GTK_BOX(page), card);
            }

            /* ── 卡片2: 诊断 / 嫌疑组件 ── */
            if (!cd.diagnosis.empty()) {
                /* ═══ 诊断卡片 (diagnosis 非空时替换嫌疑组件卡片) ═══ */
                GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_add_css_class(card, "card");
                gtk_widget_set_margin_bottom(card, 8);

                GtkWidget* title = gtk_label_new(cd.diagnosis.c_str());
                gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(title), a);
                    pango_attr_list_unref(a);
                }
                gtk_box_append(GTK_BOX(card), title);

                /* 正文 — 若以 "TIPS:" 开头则随机抽取一行 tip */
                {
                    std::string body_text;
                    std::string raw = cd.advice;
                    if (raw.rfind("TIPS:", 0) == 0) {
                        /* 提取 TIPS: 之后的行，随机选一条 */
                        std::vector<std::string> tips;
                        size_t pos = 5;  // skip "TIPS:"
                        while (pos < raw.size()) {
                            size_t nl = raw.find('\n', pos);
                            std::string line = raw.substr(pos, nl - pos);
                            pos = (nl == std::string::npos) ? raw.size() : nl + 1;
                            /* trim */
                            while (!line.empty() && line[0] == ' ') line.erase(0, 1);
                            while (!line.empty() && line.back() == '\r') line.pop_back();
                            if (!line.empty()) tips.push_back(line);
                        }
                        if (!tips.empty())
                            body_text = tips[std::rand() % tips.size()];

                        /* 存储原始 advice 和 label，供每次导航时重新随机 tip */
                        if (!body_text.empty()) {
                            GtkWidget* body = gtk_label_new(body_text.c_str());
                            gtk_label_set_xalign(GTK_LABEL(body), 0.0f);
                            gtk_label_set_wrap(GTK_LABEL(body), TRUE);
                            gtk_widget_set_opacity(body, 0.7);
                            gtk_widget_set_margin_bottom(body, 8);
                            gtk_box_append(GTK_BOX(card), body);

                            g_object_set_data_full(G_OBJECT(scroll), "crash-tip-advice",
                                g_strdup(raw.c_str()), g_free);
                            g_object_set_data(G_OBJECT(scroll), "crash-tip-label", body);
                        }
                    } else {
                        body_text = raw;
                        if (!body_text.empty()) {
                            GtkWidget* body = gtk_label_new(body_text.c_str());
                            gtk_label_set_xalign(GTK_LABEL(body), 0.0f);
                            gtk_label_set_wrap(GTK_LABEL(body), TRUE);
                            gtk_widget_set_opacity(body, 0.7);
                            gtk_widget_set_margin_bottom(body, 8);
                            gtk_box_append(GTK_BOX(card), body);
                        }
                    }
                }

                /* 操作按钮 — 根据来源类型 */
                GtkWidget* btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                if (cd.source == pclcore::local::CrashSource::Plugin) {
                    GtkWidget* b1 = gtk_button_new_with_label("重新启用");
                    gtk_widget_add_css_class(b1, "suggested-action");
                    gtk_box_append(GTK_BOX(btn_row), b1);

                    GtkWidget* b2 = gtk_button_new_with_label("复制详情");
                    gtk_box_append(GTK_BOX(btn_row), b2);

                    GtkWidget* b3 = gtk_button_new_with_label("重置插件");
                    gtk_widget_add_css_class(b3, "destructive-action");
                    gtk_box_append(GTK_BOX(btn_row), b3);
                } else if (cd.source == pclcore::local::CrashSource::Launcher) {
                    GtkWidget* b1 = gtk_button_new_with_label("查看插件列表");
                    gtk_widget_add_css_class(b1, "suggested-action");
                    gtk_box_append(GTK_BOX(btn_row), b1);

                    GtkWidget* b2 = gtk_button_new_with_label("复制详情");
                    gtk_box_append(GTK_BOX(btn_row), b2);

                    GtkWidget* b3 = gtk_button_new_with_label("我要提 Issue");
                    gtk_widget_add_css_class(b3, "destructive-action");
                    gtk_box_append(GTK_BOX(btn_row), b3);
                } else {
                    GtkWidget* b = gtk_button_new_with_label("复制详情");
                    gtk_box_append(GTK_BOX(btn_row), b);
                }
                gtk_box_append(GTK_BOX(card), btn_row);
                gtk_box_append(GTK_BOX(page), card);

            } else if (!cd.suspect.empty()) {
                /* ═══ 嫌疑组件卡片 (diagnosis 为空且有 suspect 时) ═══ */
                GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_add_css_class(card, "card");
                gtk_widget_set_margin_bottom(card, 8);

                GtkWidget* title = gtk_label_new("嫌疑组件");
                gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(title), a);
                    pango_attr_list_unref(a);
                }
                gtk_box_append(GTK_BOX(card), title);

                GtkWidget* hint = gtk_label_new("以下组件疑似导致了此次崩溃");
                gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
                gtk_widget_set_opacity(hint, 0.6);
                gtk_box_append(GTK_BOX(card), hint);

                GtkWidget* entry = gtk_entry_new();
                gtk_editable_set_text(GTK_EDITABLE(entry), cd.suspect.c_str());
                gtk_editable_set_editable(GTK_EDITABLE(entry), FALSE);
                gtk_widget_add_css_class(entry, "crash-jar-entry");
                gtk_box_append(GTK_BOX(card), entry);

                GtkWidget* tip = gtk_label_new(
                    "你可以在下方展开日志文件以查看完整的崩溃详情");
                gtk_label_set_xalign(GTK_LABEL(tip), 0.0f);
                gtk_label_set_wrap(GTK_LABEL(tip), TRUE);
                gtk_widget_set_opacity(tip, 0.55);
                gtk_box_append(GTK_BOX(card), tip);

                GtkWidget* btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                {
                    GtkWidget* b1 = gtk_button_new_with_label("禁用组件");
                    gtk_widget_add_css_class(b1, "destructive-action");
                    gtk_box_append(GTK_BOX(btn_row), b1);
                    GtkWidget* b2 = gtk_button_new_with_label("重试启动");
                    gtk_widget_add_css_class(b2, "suggested-action");
                    gtk_box_append(GTK_BOX(btn_row), b2);
                    GtkWidget* b3 = gtk_button_new_with_label("复制详情");
                    gtk_box_append(GTK_BOX(btn_row), b3);
                }
                gtk_box_append(GTK_BOX(card), btn_row);
                gtk_box_append(GTK_BOX(page), card);
            }

            /* ── 卡片3+: 日志文件 (可展开, 变长) ── */
            auto log_contents = pclcore::local::get_crash_provider().get_log_contents((int)ci);

            for (size_t li = 0; li < cd.log_files.size() && li < log_contents.size(); li++) {
                GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
                gtk_widget_add_css_class(card, "card");
                gtk_widget_set_margin_bottom(card, 8);

                /* 整行可点击展开 */
                GtkWidget* hdr_btn = gtk_button_new();
                gtk_button_set_has_frame(GTK_BUTTON(hdr_btn), FALSE);
                gtk_widget_add_css_class(hdr_btn, "flat");
                gtk_widget_add_css_class(hdr_btn, "card-header-btn");

                GtkWidget* hdr = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                GtkWidget* log_title = gtk_label_new(cd.log_files[li].filename.c_str());
                gtk_label_set_xalign(GTK_LABEL(log_title), 0.0f);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a,
                        pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(log_title), a);
                    pango_attr_list_unref(a);
                }
                gtk_widget_set_hexpand(log_title, TRUE);
                gtk_box_append(GTK_BOX(hdr), log_title);

                /* 日志描述 (副标题) */
                GtkWidget* log_desc = gtk_label_new(cd.log_files[li].description.c_str());
                gtk_label_set_xalign(GTK_LABEL(log_desc), 0.0f);
                gtk_widget_set_opacity(log_desc, 0.5);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a,
                        pango_attr_size_new(9 * PANGO_SCALE));
                    gtk_label_set_attributes(GTK_LABEL(log_desc), a);
                    pango_attr_list_unref(a);
                }
                gtk_box_append(GTK_BOX(hdr), log_desc);

                GtkWidget* chevron = icon::load("chevron-down", 14);
                GtkWidget* hdr_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
                gtk_box_append(GTK_BOX(hdr_row), hdr);
                gtk_box_append(GTK_BOX(hdr_row), chevron);
                gtk_button_set_child(GTK_BUTTON(hdr_btn), hdr_row);
                gtk_box_append(GTK_BOX(card), hdr_btn);

                /* 日志内容 (初始隐藏, 可选中复制) */
                GtkWidget* content = gtk_text_view_new();
                gtk_text_view_set_editable(GTK_TEXT_VIEW(content), FALSE);
                gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(content), TRUE);
                gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(content), GTK_WRAP_WORD_CHAR);
                {
                    GtkTextBuffer* buf = gtk_text_view_get_buffer(
                        GTK_TEXT_VIEW(content));
                    gtk_text_buffer_set_text(buf, log_contents[li].c_str(), -1);
                }
                gtk_widget_add_css_class(content, "log-content");
                gtk_widget_set_visible(content, FALSE);
                gtk_widget_set_margin_start(content, 8);
                gtk_widget_set_size_request(content, -1, 140);
                gtk_box_append(GTK_BOX(card), content);

                g_signal_connect(hdr_btn, "clicked",
                    G_CALLBACK(+[](GtkWidget* btn, gpointer d) {
                        GtkWidget* c = GTK_WIDGET(d);
                        gboolean vis = gtk_widget_get_visible(c);
                        gtk_widget_set_visible(c, !vis);
                        /* 更新按钮内最右侧 chevron 图标 */
                        GtkWidget* row = gtk_button_get_child(GTK_BUTTON(btn));
                        if (row) {
                            GtkWidget* last = gtk_widget_get_last_child(row);
                            if (last) gtk_box_remove(GTK_BOX(row), last);
                            gtk_box_append(GTK_BOX(row),
                                icon::load(vis ? "chevron-down" : "chevron-up", 14));
                        }
                    }), content);
                gtk_box_append(GTK_BOX(page), card);
            }

            char pname[32];
            snprintf(pname, sizeof(pname), "page-crash-%zu", ci + 1);
            gtk_stack_add_named(GTK_STACK(right_stack), scroll, pname);
        }
    }

    gtk_stack_set_visible_child_name(GTK_STACK(right_stack), "page-help-support");

    /* ═══════════════════════════════════════════════════════════════════
     *  中栏 GtkBox — nav-sidebar 风格, 承载二级子项导航
     * ═══════════════════════════════════════════════════════════════════ */
    GtkWidget* mid_box_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(mid_box_content, "settings-mid");
    gtk_widget_set_valign(mid_box_content, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(mid_box_content, TRUE);
    gtk_widget_set_halign(mid_box_content, GTK_ALIGN_FILL);

    GtkWidget* mid_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(mid_stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(mid_stack), 120);
    gtk_widget_set_vexpand(mid_stack, TRUE);
    gtk_box_append(GTK_BOX(mid_box_content), mid_stack);

    /* 包裹滚动窗口 — 内容过多时可滚动，默认不显示滚动条 */
    GtkWidget* mid_box = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mid_box),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mid_box), mid_box_content);
    gtk_widget_set_valign(mid_box, GTK_ALIGN_FILL);

    /* ── 二级子项定义 ── */
    struct SubItem {
        const char* label;
        const char* icon;
        const char* right_page;
    };

    /* 帮助 */
    const SubItem sub_help[] = {
        {"帮助与支持", nullptr, "page-help-support"},
        {"常见问题",   nullptr, "page-help-faq"},
    };

    /* 工具箱 */
    const SubItem sub_tools[] = {
        {"实用工具", nullptr, "page-tools"},
        {"皮肤",     nullptr, "page-skin"},
    };

    /* 插件 */
    const SubItem sub_plugin[] = {
        {"插件市场",     nullptr, "page-plugin"},
        {"已安装的插件", nullptr, "page-plugin"},
        {"示例插件01",   nullptr, "page-plugin"},
        {"示例插件02",   nullptr, "page-plugin"},
    };

    /* CrashSpy — 通过 libpclcore 接口获取, 预构建字符串防止悬空指针 */
    auto crash_reports = pclcore::local::get_crash_provider().get_crash_reports();
    std::vector<std::string> crash_page_names, crash_labels;
    for (size_t i = 0; i < crash_reports.size(); i++) {
        crash_page_names.push_back("page-crash-" + std::to_string(i + 1));
        std::string label = std::string(pclcore::local::crash_source_label(
            crash_reports[i].source)) + " 崩溃";
        crash_labels.push_back(label);
    }
    std::vector<SubItem> sub_crash;
    for (size_t i = 0; i < crash_reports.size(); i++) {
        sub_crash.push_back({crash_labels[i].c_str(), nullptr,
                             crash_page_names[i].c_str()});
    }

    /* ── 构建中栏子项列表 ── */
    auto build_mid_list = [&](const SubItem* items, int count) {
        GtkWidget* list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_valign(list, GTK_ALIGN_FILL);

        for (int i = 0; i < count; i++) {
            GtkWidget* item = build_nav_item(items[i].label, items[i].icon, i == 0);
            g_object_set_data(G_OBJECT(item), "right-stack", right_stack);
            g_object_set_data(G_OBJECT(item), "right-page", (gpointer)items[i].right_page);
            g_object_set_data(G_OBJECT(item), "mid-list", list);

            GtkGesture* click = gtk_gesture_click_new();
            g_signal_connect(click, "pressed",
                G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                    GtkWidget* tgt = gtk_event_controller_get_widget(
                        GTK_EVENT_CONTROLLER(g));
                    GtkWidget* rstk = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(tgt), "right-stack"));
                    const char* rp = static_cast<const char*>(
                        g_object_get_data(G_OBJECT(tgt), "right-page"));
                    GtkWidget* mlist = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(tgt), "mid-list"));

                    gtk_stack_set_visible_child_name(GTK_STACK(rstk), rp);

                    for (GtkWidget* sib = gtk_widget_get_first_child(mlist);
                         sib; sib = gtk_widget_get_next_sibling(sib))
                    {
                        if (!gtk_widget_has_css_class(sib, "nav-item")) continue;
                        if (sib == tgt)
                            gtk_widget_add_css_class(sib, "nav-item-active");
                        else
                            gtk_widget_remove_css_class(sib, "nav-item-active");
                    }
                }), nullptr);
            gtk_widget_add_controller(item, GTK_EVENT_CONTROLLER(click));
            gtk_box_append(GTK_BOX(list), item);
        }
        return list;
    };

    GtkWidget* mid_help   = build_mid_list(sub_help,   2);
    GtkWidget* mid_tools  = build_mid_list(sub_tools,  2);
    GtkWidget* mid_plugin = build_mid_list(sub_plugin, 4);

    /* CrashSpy 中栏 — 按来源类型显示不同高度 */
    GtkWidget* mid_crash = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(mid_crash, GTK_ALIGN_FILL);
    for (size_t i = 0; i < crash_reports.size(); i++) {
        const auto& cr = crash_reports[i];
        GtkWidget* item;

        if (cr.source == pclcore::local::CrashSource::JDK ||
            cr.source == pclcore::local::CrashSource::Launcher) {
            /* JDK / 启动器: 单行 slim 项 */
            item = build_nav_item(sub_crash[i].label, nullptr, i == 0);
            gtk_widget_set_size_request(item, -1, 36);
        } else if (cr.source == pclcore::local::CrashSource::Plugin) {
            /* 插件: 标题 + 插件名 (副标题) + 插件路径 */
            std::string plugin_name = cr.suspect;
            /* 提取文件名部分 */
            auto slash = plugin_name.rfind('/');
            if (slash != std::string::npos)
                plugin_name = plugin_name.substr(slash + 1);

            item = build_nav_item_subtitle(sub_crash[i].label,
                plugin_name.c_str(), i == 0);
            /* 加高 */
            {
                GtkWidget* title = gtk_widget_get_first_child(item);
                if (title) {
                    gtk_widget_set_valign(title, GTK_ALIGN_CENTER);
                    gtk_widget_set_margin_top(title, 0);
                    GtkWidget* sub = gtk_widget_get_next_sibling(title);
                    if (sub) {
                        gtk_widget_set_valign(sub, GTK_ALIGN_CENTER);
                        gtk_widget_set_margin_bottom(sub, 0);
                        PangoAttrList* a = pango_attr_list_new();
                        pango_attr_list_insert(a,
                            pango_attr_size_new(10 * PANGO_SCALE));
                        gtk_label_set_attributes(GTK_LABEL(sub), a);
                        pango_attr_list_unref(a);
                    }
                }
            }
            /* 追加插件路径 */
            GtkWidget* path_lbl = gtk_label_new(cr.suspect.c_str());
            gtk_label_set_xalign(GTK_LABEL(path_lbl), 0.0f);
            gtk_widget_set_valign(path_lbl, GTK_ALIGN_CENTER);
            gtk_widget_set_opacity(path_lbl, 0.45);
            gtk_label_set_ellipsize(GTK_LABEL(path_lbl), PANGO_ELLIPSIZE_MIDDLE);
            gtk_widget_set_margin_start(path_lbl, 8);
            gtk_box_append(GTK_BOX(item), path_lbl);
            gtk_widget_set_size_request(item, -1, 72);
        } else {
            /* Minecraft / 其他: 标题 + 实例名 (副标题) + 实例路径 */
            item = build_nav_item_subtitle(sub_crash[i].label,
                cr.instance_name.c_str(), i == 0);
            {
                GtkWidget* title = gtk_widget_get_first_child(item);
                if (title) {
                    gtk_widget_set_valign(title, GTK_ALIGN_CENTER);
                    gtk_widget_set_margin_top(title, 0);
                    GtkWidget* sub = gtk_widget_get_next_sibling(title);
                    if (sub) {
                        gtk_widget_set_valign(sub, GTK_ALIGN_CENTER);
                        gtk_widget_set_margin_bottom(sub, 0);
                        PangoAttrList* a = pango_attr_list_new();
                        pango_attr_list_insert(a,
                            pango_attr_size_new(10 * PANGO_SCALE));
                        gtk_label_set_attributes(GTK_LABEL(sub), a);
                        pango_attr_list_unref(a);
                    }
                }
            }
            GtkWidget* path_lbl = gtk_label_new(cr.instance_path.c_str());
            gtk_label_set_xalign(GTK_LABEL(path_lbl), 0.0f);
            gtk_widget_set_valign(path_lbl, GTK_ALIGN_CENTER);
            gtk_widget_set_opacity(path_lbl, 0.45);
            gtk_label_set_ellipsize(GTK_LABEL(path_lbl), PANGO_ELLIPSIZE_MIDDLE);
            gtk_widget_set_margin_start(path_lbl, 8);
            gtk_box_append(GTK_BOX(item), path_lbl);
            gtk_widget_set_size_request(item, -1, 72);
        }

        /* ═══ 用 GtkOverlay 包裹, 浮层放置 hover 才显示的 '×' 删除按钮 ═══ */
        GtkWidget* overlay = gtk_overlay_new();
        gtk_widget_add_css_class(overlay, "nav-item");
        if (i == 0) gtk_widget_add_css_class(overlay, "nav-item-active");
        gtk_widget_set_valign(overlay, GTK_ALIGN_FILL);

        /* 内层 item 不可作为事件目标，CSS 交由 overlay 管理，移除内层冲突 */
        gtk_widget_set_can_target(item, FALSE);
        gtk_widget_remove_css_class(item, "nav-item");
        gtk_widget_remove_css_class(item, "nav-item-active");
        gtk_overlay_set_child(GTK_OVERLAY(overlay), item);

        /* 数据放在 overlay 上 (取代原始 item) */
        g_object_set_data(G_OBJECT(overlay), "right-stack", right_stack);
        g_object_set_data_full(G_OBJECT(overlay), "right-page",
            g_strdup(sub_crash[i].right_page), g_free);
        g_object_set_data(G_OBJECT(overlay), "mid-list", mid_crash);

        /* ── 浮层 '×' 按钮 (初始隐藏, hover 时显示) ── */
        GtkWidget* x_btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(x_btn), FALSE);
        gtk_button_set_child(GTK_BUTTON(x_btn), icon::load("x", 14));
        gtk_widget_add_css_class(x_btn, "flat");
        gtk_widget_add_css_class(x_btn, "crash-dismiss-btn");
        gtk_widget_set_halign(x_btn, GTK_ALIGN_END);
        gtk_widget_set_valign(x_btn, GTK_ALIGN_START);
        gtk_widget_set_visible(x_btn, FALSE);
        gtk_widget_set_margin_top(x_btn, 4);
        gtk_widget_set_margin_end(x_btn, 4);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), x_btn);

        /* ── Hover 检测: 鼠标进入 overlay 显示 ×, 离开隐藏 × ── */
        {
            GtkEventController* motion = gtk_event_controller_motion_new();
            g_signal_connect(motion, "enter",
                G_CALLBACK(+[](GtkEventController*, gdouble, gdouble, gpointer d) {
                    gtk_widget_set_visible(GTK_WIDGET(d), TRUE);
                }), x_btn);
            g_signal_connect(motion, "leave",
                G_CALLBACK(+[](GtkEventController*, gpointer d) {
                    gtk_widget_set_visible(GTK_WIDGET(d), FALSE);
                }), x_btn);
            gtk_widget_add_controller(overlay, GTK_EVENT_CONTROLLER(motion));
        }

        /* ── × 按钮: 双击确认后删除 ── */
        g_signal_connect(x_btn, "clicked",
            G_CALLBACK(+[](GtkWidget* btn, gpointer d) {
                GtkWidget* ov = GTK_WIDGET(d);
                int cnt = GPOINTER_TO_INT(
                    g_object_get_data(G_OBJECT(btn), "dismiss-cnt"));
                cnt++;

                if (cnt == 1) {
                    /* 首次点击: 弹出确认通知 */
                    g_object_set_data(G_OBJECT(btn), "dismiss-cnt",
                        GINT_TO_POINTER(cnt));
                    GtkWindow* win = GTK_WINDOW(gtk_widget_get_root(btn));
                    show_toast(GTK_WINDOW(win), {
                        "warn",
                        "你真的要删除这个项目吗？",
                        "本操作无法恢复。要继续删除，请再次单击！",
                        false,   // add_to_center
                        false,   // can_clear
                    });

                    /* 2 秒后重置计数 */
                    guint tid = g_timeout_add(2000,
                        [](gpointer ud) -> gboolean {
                            g_object_set_data(G_OBJECT(ud), "dismiss-cnt",
                                GINT_TO_POINTER(0));
                            g_object_set_data(G_OBJECT(ud), "dismiss-tid", nullptr);
                            return G_SOURCE_REMOVE;
                        }, btn);
                    g_object_set_data(G_OBJECT(btn), "dismiss-tid",
                        GUINT_TO_POINTER(tid));
                } else {
                    /* 第二次点击: 执行删除 */
                    guint tid = GPOINTER_TO_UINT(
                        g_object_get_data(G_OBJECT(btn), "dismiss-tid"));
                    if (tid) g_source_remove(tid);
                    g_object_set_data(G_OBJECT(btn), "dismiss-cnt",
                        GINT_TO_POINTER(0));
                    g_object_set_data(G_OBJECT(btn), "dismiss-tid", nullptr);

                    /* 移除右侧 crash 页面 */
                    GtkWidget* rstk = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(ov), "right-stack"));
                    const char* rp = static_cast<const char*>(
                        g_object_get_data(G_OBJECT(ov), "right-page"));
                    if (rstk && rp) {
                        GtkWidget* old = gtk_stack_get_child_by_name(
                            GTK_STACK(rstk), rp);
                        if (old) gtk_stack_remove(GTK_STACK(rstk), old);
                    }

                    /* 从中栏移除 overlay */
                    GtkWidget* parent = gtk_widget_get_parent(ov);
                    if (parent) gtk_box_remove(GTK_BOX(parent), ov);
                }
            }), overlay);

        /* ── 点击 overlay (导航) ── */
        {
            GtkGesture* click = gtk_gesture_click_new();
            g_signal_connect(click, "pressed",
                G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                    GtkWidget* tgt = gtk_event_controller_get_widget(
                        GTK_EVENT_CONTROLLER(g));
                    GtkWidget* rstk = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(tgt), "right-stack"));
                    const char* rp = static_cast<const char*>(
                        g_object_get_data(G_OBJECT(tgt), "right-page"));
                    GtkWidget* mlist = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(tgt), "mid-list"));

                    /* 导航到目标页面 */
                    gtk_stack_set_visible_child_name(GTK_STACK(rstk), rp);

                    /* 刷新随机 tip */
                    {
                        GtkWidget* page = gtk_stack_get_visible_child(
                            GTK_STACK(rstk));
                        if (page) {
                            const char* raw = static_cast<const char*>(
                                g_object_get_data(G_OBJECT(page), "crash-tip-advice"));
                            GtkWidget* lbl = static_cast<GtkWidget*>(
                                g_object_get_data(G_OBJECT(page), "crash-tip-label"));
                            if (raw && lbl && GTK_IS_LABEL(lbl)) {
                                std::string tips_str(raw);
                                std::vector<std::string> tips;
                                size_t pos = 5;
                                while (pos < tips_str.size()) {
                                    size_t nl = tips_str.find('\n', pos);
                                    std::string line = tips_str.substr(pos, nl - pos);
                                    pos = (nl == std::string::npos) ? tips_str.size() : nl + 1;
                                    while (!line.empty() && line[0] == ' ') line.erase(0, 1);
                                    while (!line.empty() && line.back() == '\r') line.pop_back();
                                    if (!line.empty()) tips.push_back(line);
                                }
                                if (!tips.empty())
                                    gtk_label_set_text(GTK_LABEL(lbl),
                                        tips[std::rand() % tips.size()].c_str());
                            }
                        }
                    }

                    /* 互斥高亮: overlay 的兄弟都是 overlay，它们带 "nav-item" class */
                    for (GtkWidget* sib = gtk_widget_get_first_child(mlist);
                         sib; sib = gtk_widget_get_next_sibling(sib))
                    {
                        if (!gtk_widget_has_css_class(sib, "nav-item")) continue;
                        if (sib == tgt)
                            gtk_widget_add_css_class(sib, "nav-item-active");
                        else
                            gtk_widget_remove_css_class(sib, "nav-item-active");
                    }
                }), nullptr);
            gtk_widget_add_controller(overlay, GTK_EVENT_CONTROLLER(click));
        }

        gtk_box_append(GTK_BOX(mid_crash), overlay);
    }

    gtk_stack_add_named(GTK_STACK(mid_stack), mid_help,   "mid-help");
    gtk_stack_add_named(GTK_STACK(mid_stack), mid_tools,  "mid-tools");
    gtk_stack_add_named(GTK_STACK(mid_stack), mid_plugin, "mid-plugin");
    /* 删除所有日志 按钮 */
    {
        GtkWidget* btn = gtk_button_new_with_label("删除所有日志");
        gtk_widget_add_css_class(btn, "suggested-action");
        gtk_widget_set_margin_start(btn, 8);
        gtk_widget_set_margin_end(btn, 8);
        gtk_widget_set_margin_top(btn, 8);
        gtk_box_append(GTK_BOX(mid_crash), btn);
    }

    gtk_stack_add_named(GTK_STACK(mid_stack), mid_crash,  "mid-crash");
    gtk_stack_set_visible_child_name(GTK_STACK(mid_stack), "mid-help");

    /* ═══════════════════════════════════════════════════════════════════
     *  左栏 — 一级分类
     * ═══════════════════════════════════════════════════════════════════ */
    GtkWidget* left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(left_box, "nav-sidebar");
    gtk_widget_set_valign(left_box, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(left_box, TRUE);
    gtk_widget_set_halign(left_box, GTK_ALIGN_FILL);

    struct LeftItem {
        const char* label;
        const char* icon;
        const char* mid_page;
        const char* first_right;
    };
    const LeftItem left_items[] = {
        {"帮助",     "link-2",    "mid-help",   "page-help-support"},
        {"工具箱",   "tool-case", "mid-tools",  "page-tools"},
        {"CrashSpy", "glasses",    "mid-crash",  "page-crash-1"},
        {"插件",     "puzzle",    "mid-plugin", "page-plugin"},
    };

    for (int i = 0; i < 4; i++) {
        GtkWidget* item = build_nav_item(left_items[i].label,
                                         left_items[i].icon, i == 0);
        g_object_set_data(G_OBJECT(item), "mid-stack", mid_stack);
        g_object_set_data(G_OBJECT(item), "mid-page", (gpointer)left_items[i].mid_page);
        g_object_set_data(G_OBJECT(item), "right-stack", right_stack);
        g_object_set_data(G_OBJECT(item), "first-right", (gpointer)left_items[i].first_right);
        g_object_set_data(G_OBJECT(item), "left-nav", left_box);

        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed",
            G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                GtkWidget* tgt = gtk_event_controller_get_widget(
                    GTK_EVENT_CONTROLLER(g));
                GtkWidget* mstk = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(tgt), "mid-stack"));
                const char* mp = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(tgt), "mid-page"));
                GtkWidget* rstk = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(tgt), "right-stack"));
                const char* fr = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(tgt), "first-right"));
                GtkWidget* nav = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(tgt), "left-nav"));

                gtk_stack_set_visible_child_name(GTK_STACK(mstk), mp);
                gtk_stack_set_visible_child_name(GTK_STACK(rstk), fr);

                /* 刷新随机 tip */
                {
                    GtkWidget* rpage = gtk_stack_get_visible_child(GTK_STACK(rstk));
                    if (rpage) {
                        const char* raw = static_cast<const char*>(
                            g_object_get_data(G_OBJECT(rpage), "crash-tip-advice"));
                        GtkWidget* lbl = static_cast<GtkWidget*>(
                            g_object_get_data(G_OBJECT(rpage), "crash-tip-label"));
                        if (raw && lbl && GTK_IS_LABEL(lbl)) {
                            std::string tips_str(raw);
                            std::vector<std::string> tips;
                            size_t pos = 5;
                            while (pos < tips_str.size()) {
                                size_t nl = tips_str.find('\n', pos);
                                std::string line = tips_str.substr(pos, nl - pos);
                                pos = (nl == std::string::npos) ? tips_str.size() : nl + 1;
                                while (!line.empty() && line[0] == ' ') line.erase(0, 1);
                                while (!line.empty() && line.back() == '\r') line.pop_back();
                                if (!line.empty()) tips.push_back(line);
                            }
                            if (!tips.empty())
                                gtk_label_set_text(GTK_LABEL(lbl),
                                    tips[std::rand() % tips.size()].c_str());
                        }
                    }
                }

                /* 激活中栏第一个子项 */
                GtkWidget* mchild = gtk_stack_get_child_by_name(
                    GTK_STACK(mstk), mp);
                if (mchild) {
                    GtkWidget* first = gtk_widget_get_first_child(mchild);
                    if (first && gtk_widget_has_css_class(first, "nav-item"))
                        gtk_widget_add_css_class(first, "nav-item-active");
                    for (GtkWidget* sib = gtk_widget_get_next_sibling(first);
                         sib; sib = gtk_widget_get_next_sibling(sib))
                        if (gtk_widget_has_css_class(sib, "nav-item"))
                            gtk_widget_remove_css_class(sib, "nav-item-active");
                }

                for (GtkWidget* sib = gtk_widget_get_first_child(nav);
                     sib; sib = gtk_widget_get_next_sibling(sib))
                {
                    if (!gtk_widget_has_css_class(sib, "nav-item")) continue;
                    if (sib == tgt)
                        gtk_widget_add_css_class(sib, "nav-item-active");
                    else
                        gtk_widget_remove_css_class(sib, "nav-item-active");
                }
            }), nullptr);
        gtk_widget_add_controller(item, GTK_EVENT_CONTROLLER(click));
        gtk_box_append(GTK_BOX(left_box), item);
    }

    /* 包裹滚动窗口 — 内容过多时可滚动，默认不显示滚动条 */
    GtkWidget* left = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(left),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(left), left_box);
    gtk_widget_set_valign(left, GTK_ALIGN_FILL);

    /* ── 三栏固定布局 — 左中固定宽度, 右侧弹性 ── */
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(panel, TRUE);
    gtk_widget_set_vexpand(panel, TRUE);

    /* 左栏 — 固定宽度 */
    gtk_widget_set_size_request(left, 180, -1);
    gtk_box_append(GTK_BOX(panel), left);
    gtk_box_append(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* 中栏 — 固定宽度 */
    gtk_widget_set_size_request(mid_box, 180, -1);
    gtk_box_append(GTK_BOX(panel), mid_box);
    gtk_box_append(GTK_BOX(panel), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* 右栏 — 弹性宽度 */
    gtk_widget_add_css_class(right_stack, "settings-right");
    gtk_widget_set_size_request(right_stack, 280, -1);
    gtk_widget_set_hexpand(right_stack, TRUE);
    gtk_box_append(GTK_BOX(panel), right_stack);

    /* 存储关键控件 — NavigationController 集中管理 + 向后兼容 */
    NavigationController::instance().register_more(left, mid_stack, right_stack);
    g_object_set_data(G_OBJECT(panel), "more-left-nav", left);
    g_object_set_data(G_OBJECT(panel), "more-mid-stack", mid_stack);
    g_object_set_data(G_OBJECT(panel), "more-right-stack", right_stack);

    return panel;
}

}  // namespace pcl
