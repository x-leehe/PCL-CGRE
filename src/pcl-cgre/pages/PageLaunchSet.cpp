#include "pages/PageLaunchSet.hpp"
#include "pages/LaunchPage.hpp"
#include "core/ConfigManager.hpp"
#include "util/IconHelper.hpp"
#include "widgets/FormWidgets.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <gtk/gtk.h>

namespace pcl {

/* ============================================================================
 * P1 — 启动设置 (3 卡片, ~18 配置项)
 * ============================================================================ */

GtkWidget* build_page_launch_set()
{
    using CM = ConfigManager;
    auto& cfg = CM::instance();

    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    /* ── Card 1: 启动选项 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 实例隔离 */
        {
            const char* items[] = {"关闭", "仅 Modded", "非发布版",
                                   "Modded + 非发布", "全部"};
            int sel = cfg.get_or<int>("game.launch.instance_isolation", 1);
            GtkWidget* cb = make_dropdown(items, 5, sel, "game.launch.instance_isolation");
            gtk_box_append(GTK_BOX(inner), labeled_row("实例隔离", cb));
        }

        /* 游戏窗口标题 */
        {
            std::string v = cfg.get_or<std::string>("game.window.title", "");
            GtkWidget* e = make_entry("game.window.title", "留空为默认");
            gtk_editable_set_text(GTK_EDITABLE(e), v.c_str());
            gtk_box_append(GTK_BOX(inner), labeled_row("窗口标题", e));
        }

        /* 自定义信息 */
        {
            std::string v = cfg.get_or<std::string>("game.launch.custom_info", "PCL");
            GtkWidget* e = make_entry("game.launch.custom_info");
            gtk_editable_set_text(GTK_EDITABLE(e), v.c_str());
            gtk_box_append(GTK_BOX(inner), labeled_row("自定义信息", e));
        }

        /* 启动器可见性 */
        {
            const char* items[] = {"关闭", "隐藏并关闭", "隐藏并保持",
                                   "最小化", "保留"};
            int sel = cfg.get_or<int>("game.window.visibility", 4);
            GtkWidget* cb = make_dropdown(items, 5, sel, "game.window.visibility");
            gtk_box_append(GTK_BOX(inner), labeled_row("启动器可见性", cb));
        }

        /* 进程优先级 */
        {
            const char* items[] = {"实时", "高", "高于标准", "标准", "低于标准"};
            int sel = cfg.get_or<int>("game.launch.priority", 3);
            GtkWidget* cb = make_dropdown(items, 5, sel, "game.launch.priority");
            gtk_box_append(GTK_BOX(inner), labeled_row("进程优先级", cb));
        }

        /* 窗口大小 (dropdown + 条件自定义 WxH) */
        {
            const char* items[] = {"全屏", "默认", "同启动器", "自定义", "最大化"};
            std::string mode = cfg.get_or<std::string>("game.window.size_mode", "default");
            int sel = 1;
            if (mode == "fullscreen") sel = 0;
            else if (mode == "default") sel = 1;
            else if (mode == "same_as_launcher") sel = 2;
            else if (mode == "custom") sel = 3;
            else if (mode == "maximized") sel = 4;

            GtkStringList* model = gtk_string_list_new(nullptr);
            for (int i = 0; i < 5; i++) gtk_string_list_append(model, items[i]);
            GtkWidget* dd = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
            gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), sel);
            gtk_widget_set_hexpand(dd, TRUE);
            gtk_box_append(GTK_BOX(inner), labeled_row("窗口大小", dd));

            /* 自定义宽高 (GtkEntry 并排, 条件显示) */
            int cw = cfg.get_or<int>("game.window.custom_width", 854);
            int ch = cfg.get_or<int>("game.window.custom_height", 480);

            GtkWidget* size_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_bottom(size_row, 7);
            gtk_widget_set_margin_start(size_row, 122);

            GtkWidget* lbl_w = gtk_label_new("宽");
            gtk_box_append(GTK_BOX(size_row), lbl_w);
            GtkWidget* ew = gtk_entry_new();
            gtk_entry_set_input_purpose(GTK_ENTRY(ew), GTK_INPUT_PURPOSE_DIGITS);
            {
                char buf[16]; std::snprintf(buf, sizeof(buf), "%d", cw);
                gtk_editable_set_text(GTK_EDITABLE(ew), buf);
            }
            gtk_widget_set_size_request(ew, 72, -1);
            gtk_box_append(GTK_BOX(size_row), ew);

            GtkWidget* lbl_h = gtk_label_new("高");
            gtk_box_append(GTK_BOX(size_row), lbl_h);
            GtkWidget* eh = gtk_entry_new();
            gtk_entry_set_input_purpose(GTK_ENTRY(eh), GTK_INPUT_PURPOSE_DIGITS);
            {
                char buf[16]; std::snprintf(buf, sizeof(buf), "%d", ch);
                gtk_editable_set_text(GTK_EDITABLE(eh), buf);
            }
            gtk_widget_set_size_request(eh, 72, -1);
            gtk_box_append(GTK_BOX(size_row), eh);

            if (sel != 3)
                gtk_widget_set_visible(size_row, FALSE);
            gtk_box_append(GTK_BOX(inner), size_row);

            /* 连接 dropdown → 条件显示 + config */
            g_object_set_data(G_OBJECT(dd), "size-row", size_row);
            {
                auto cb = +[](GObject* o, GParamSpec*, gpointer) -> void {
                    guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
                    GtkWidget* sr = GTK_WIDGET(g_object_get_data(G_OBJECT(o), "size-row"));
                    const char* modes[] = {"fullscreen", "default",
                        "same_as_launcher", "custom", "maximized"};
                    ConfigManager::instance().set("game.window.size_mode",
                        std::string(modes[idx]));
                    gtk_widget_set_visible(sr, idx == 3);
                };
                g_signal_connect_data(dd, "notify::selected",
                    (GCallback)cb, nullptr, nullptr, G_CONNECT_DEFAULT);
            }

            /* 宽高 entry 变化 → config */
            auto* pw = new std::string("game.window.custom_width");
            auto* ph = new std::string("game.window.custom_height");
            g_signal_connect(ew, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    int v = std::atoi(gtk_editable_get_text(GTK_EDITABLE(o)));
                    ConfigManager::instance().set(*p, v);
                }), pw);
            g_signal_connect(eh, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    int v = std::atoi(gtk_editable_get_text(GTK_EDITABLE(o)));
                    ConfigManager::instance().set(*p, v);
                }), ph);
        }

        /* 微软登录方式 */
        {
            const char* items[] = {"网页", "设备代码"};
            int sel = cfg.get_or<int>("account.microsoft.auth_method", 0);
            GtkWidget* cb = make_dropdown(items, 2, sel, "account.microsoft.auth_method");
            gtk_box_append(GTK_BOX(inner), labeled_row("微软登录方式", cb));
        }

        /* IP 协议栈 */
        {
            const char* items[] = {"IPv4", "Java 默认", "IPv6"};
            int sel = cfg.get_or<int>("game.launch.ip_stack", 0);
            GtkWidget* cb = make_dropdown(items, 3, sel, "game.launch.ip_stack");
            gtk_box_append(GTK_BOX(inner), labeled_row("IP 协议栈", cb));
        }

        gtk_box_append(GTK_BOX(content), build_card("启动选项", inner));
    }

    /* ── Card 2: 内存 ────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        std::string mode = cfg.get_or<std::string>("game.java.memory.mode", "auto");
        int max_mb = cfg.get_or<int>("game.java.memory.max_mb", 2048);

        /* 模式切换按钮 (auto / custom) */
        const char* modes[] = {"自动分配", "自定义"};
        GtkWidget* toggle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_widget_add_css_class(toggle, "linked");
        gtk_widget_set_margin_bottom(toggle, 7);

        GtkWidget* btn_auto = gtk_toggle_button_new_with_label(modes[0]);
        GtkWidget* btn_custom = gtk_toggle_button_new_with_label(modes[1]);
        gtk_widget_set_size_request(btn_auto, 90, -1);
        gtk_widget_set_size_request(btn_custom, 90, -1);

        if (mode == "auto")
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_auto), TRUE);
        else
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_custom), TRUE);

        gtk_box_append(GTK_BOX(toggle), btn_auto);
        gtk_box_append(GTK_BOX(toggle), btn_custom);
        gtk_box_append(GTK_BOX(inner), labeled_row("内存模式", toggle));

        /* 内存滑块 (custom 模式下可见) + 输入框 */
        GtkWidget* scale_row = make_slider_entry("game.java.memory.max_mb", 512, 16384, 128, max_mb, "%.0f");
        gtk_widget_set_margin_start(scale_row, 122);

        if (mode == "auto")
            gtk_widget_set_visible(scale_row, FALSE);
        gtk_box_append(GTK_BOX(inner), scale_row);

        /* 内存可视化条 */
        GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_size_request(bar, -1, 20);
        gtk_widget_set_margin_start(bar, 122);
        gtk_widget_set_margin_bottom(bar, 7);

        GtkWidget* used_seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(used_seg, "mem-used");
        gtk_widget_set_size_request(used_seg, 0, -1);
        gtk_box_append(GTK_BOX(bar), used_seg);

        GtkWidget* free_seg = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(free_seg, "mem-free");
        gtk_widget_set_hexpand(free_seg, TRUE);
        gtk_box_append(GTK_BOX(bar), free_seg);
        gtk_box_append(GTK_BOX(inner), bar);

        /* 自定义 Java 警告 — 始终显示为提示 */
        GtkWidget* hint = gtk_label_new(
            "检测到自定义 Java 运行时\n"
            "可能无法正确分配超过 1024 MB 的内存");
        gtk_widget_set_margin_start(hint, 122);
        gtk_widget_set_margin_bottom(hint, 7);
        gtk_widget_set_opacity(hint, 0.55);
        gtk_widget_set_halign(hint, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(inner), hint);

        /* ── 信号连接 ── */

        // mode toggle → config + visibility
        auto toggle_cb = +[](GtkToggleButton* self, gpointer data) {
            if (!gtk_toggle_button_get_active(self)) return;
            GtkWidget* other = GTK_WIDGET(data);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(other), FALSE);
        };
        g_signal_connect(btn_auto, "toggled",
            G_CALLBACK(toggle_cb), btn_custom);
        g_signal_connect(btn_custom, "toggled",
            G_CALLBACK(toggle_cb), btn_auto);

        g_object_set_data(G_OBJECT(btn_auto), "scale-row", scale_row);
        g_object_set_data(G_OBJECT(btn_custom), "scale-row", scale_row);

        auto mode_cb = +[](GtkToggleButton* btn, gpointer) {
            bool active = gtk_toggle_button_get_active(btn);
            if (!active) return;
            bool is_auto = (std::string(gtk_button_get_label(GTK_BUTTON(btn))) == "自动分配");
            ConfigManager::instance().set("game.java.memory.mode",
                std::string(is_auto ? "auto" : "custom"));
            GtkWidget* sr = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "scale-row"));
            if (sr) gtk_widget_set_visible(sr, !is_auto);
        };
        g_signal_connect(btn_auto, "toggled", G_CALLBACK(mode_cb), nullptr);
        g_signal_connect(btn_custom, "toggled", G_CALLBACK(mode_cb), nullptr);

        gtk_box_append(GTK_BOX(content), build_card("内存", inner));
    }

    /* ── Card 3: 高级选项 (可折叠) ────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 渲染器 */
        {
            const char* items[] = {"自动", "Software", "D3D12", "Vulkan"};
            int sel = cfg.get_or<int>("game.java.renderer", 0);
            GtkWidget* cb = make_dropdown(items, 4, sel, "game.java.renderer");
            gtk_box_append(GTK_BOX(inner), labeled_row("渲染器", cb));
        }

        /* JVM 参数 + [重置] */
        {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_bottom(row, 7);
            GtkWidget* lbl = gtk_label_new("JVM 参数");
            gtk_widget_set_size_request(lbl, 110, -1);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row), lbl);

            GtkWidget* e = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), "-Xmx2048m -XX:+UseG1GC");
            gtk_editable_set_text(GTK_EDITABLE(e),
                cfg.get_or<std::string>("game.java.jvm_args", "").c_str());
            gtk_widget_set_hexpand(e, TRUE);
            gtk_box_append(GTK_BOX(row), e);

            GtkWidget* reset = gtk_button_new_with_label("重置");
            g_object_set_data(G_OBJECT(reset), "entry", e);
            g_signal_connect(reset, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer d) {
                    GtkEntry* en = GTK_ENTRY(d);
                    gtk_editable_set_text(GTK_EDITABLE(en), "");
                }), e);
            gtk_box_append(GTK_BOX(row), reset);

            std::string path = "game.java.jvm_args";
            auto* sp = new std::string(path);
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);

            gtk_box_append(GTK_BOX(inner), row);
        }

        /* 游戏追加参数 */
        {
            GtkWidget* e = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), "--arg1 --arg2");
            gtk_editable_set_text(GTK_EDITABLE(e),
                cfg.get_or<std::string>("game.java.game_args", "").c_str());
            gtk_widget_set_hexpand(e, TRUE);

            auto* sp = new std::string("game.java.game_args");
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);

            gtk_box_append(GTK_BOX(inner), labeled_row("游戏参数", e));
        }

        /* 预启动命令 + 等待 */
        {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_bottom(row, 7);
            GtkWidget* lbl = gtk_label_new("预启动命令");
            gtk_widget_set_size_request(lbl, 110, -1);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(row), lbl);

            GtkWidget* e = gtk_entry_new();
            gtk_editable_set_text(GTK_EDITABLE(e),
                cfg.get_or<std::string>("game.launch.pre_launch_cmd", "").c_str());
            gtk_widget_set_hexpand(e, TRUE);
            gtk_box_append(GTK_BOX(row), e);

            auto* sp = new std::string("game.launch.pre_launch_cmd");
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);
            gtk_box_append(GTK_BOX(inner), row);

            bool wait = cfg.get_or<bool>("game.launch.pre_launch_wait", false);
            gtk_box_append(GTK_BOX(inner),
                make_check("等待命令执行完成", "game.launch.pre_launch_wait", wait));
        }

        /* 5 个复选框 */
        gtk_box_append(GTK_BOX(inner),
            make_check("禁用 JavaLaunchWrapper",
                       "game.java.disable_jlw",
                       cfg.get_or<bool>("game.java.disable_jlw", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("禁用 LaunchFramework",
                       "game.java.disable_lf",
                       cfg.get_or<bool>("game.java.disable_lf", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("偏好独立 GPU",
                       "game.java.prefer_dedicated_gpu",
                       cfg.get_or<bool>("game.java.prefer_dedicated_gpu", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("禁用 LWJGL Unsafe Agent",
                       "game.java.disable_lwjgl_unsafe",
                       cfg.get_or<bool>("game.java.disable_lwjgl_unsafe", false)));

        GtkWidget* expander = gtk_expander_new("高级选项");
        gtk_expander_set_child(GTK_EXPANDER(expander), inner);
        gtk_box_append(GTK_BOX(content), build_card("高级选项", expander));
    }

    return sw;
}

}  // namespace pcl
