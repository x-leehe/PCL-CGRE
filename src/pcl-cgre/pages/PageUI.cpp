#include "pages/PageUI.hpp"
#include "pages/LaunchPage.hpp"
#include "core/ConfigManager.hpp"
#include "util/IconHelper.hpp"
#include "widgets/FormWidgets.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <gtk/gtk.h>

namespace pcl {

GtkWidget* build_page_ui()
{
    using CM = ConfigManager;
    auto& cfg = CM::instance();

    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    /* ── Card 1: 基础外观 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 启动器不透明度 */
        {
            double v = cfg.get_or<double>("launcher.ui.opacity", 1.0);
            GtkAdjustment* adj = gtk_adjustment_new(v, 0.0, 1.0, 0.05, 0.1, 0);
            GtkWidget* scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
            gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
            gtk_widget_set_hexpand(scale, TRUE);

            GtkWidget* val_lbl = gtk_label_new("");
            {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%.0f%%", v * 100);
                gtk_label_set_text(GTK_LABEL(val_lbl), buf);
            }
            gtk_widget_set_size_request(val_lbl, 48, -1);
            gtk_widget_set_halign(val_lbl, GTK_ALIGN_END);

            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_margin_bottom(row, 7);
            gtk_box_append(GTK_BOX(row), scale);
            gtk_box_append(GTK_BOX(row), val_lbl);

            g_object_set_data(G_OBJECT(adj), "opacity-label", val_lbl);
            auto* sp = new std::string("launcher.ui.opacity");
            g_signal_connect(adj, "value-changed",
                G_CALLBACK(+[](GtkAdjustment* a, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    double val = gtk_adjustment_get_value(a);
                    ConfigManager::instance().set(*p, val);
                    GtkLabel* l = GTK_LABEL(g_object_get_data(G_OBJECT(a), "opacity-label"));
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "%.0f%%", val * 100);
                    gtk_label_set_text(l, buf);
                }), sp);

            gtk_box_append(GTK_BOX(inner), labeled_row("不透明度", row));
        }

        /* 主题模式 */
        {
            const char* labels[] = {"跟随系统", "浅色", "深色"};
            const char* vals[]   = {"follow_system", "light", "dark"};
            std::string cur = cfg.get_or<std::string>("launcher.ui.theme", "follow_system");
            int sel = (cur == "light") ? 1 : (cur == "dark") ? 2 : 0;
            gtk_box_append(GTK_BOX(inner),
                labeled_row("主题模式", make_dropdown_str(labels, vals, 3, sel, "launcher.ui.theme")));
        }

        /* 浅色主题色 */
        {
            const char* labels[] = {"蓝", "绿", "橙", "紫", "红"};
            const char* vals[]   = {"blue", "green", "orange", "purple", "red"};
            std::string cur = cfg.get_or<std::string>("launcher.ui.light_accent", "blue");
            int sel = 0;
            for (int i = 0; i < 5; i++) if (cur == vals[i]) { sel = i; break; }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("浅色主题色", make_dropdown_str(labels, vals, 5, sel, "launcher.ui.light_accent")));
        }

        /* 深色主题色 */
        {
            const char* labels[] = {"蓝", "绿", "橙", "紫", "红"};
            const char* vals[]   = {"blue", "green", "orange", "purple", "red"};
            std::string cur = cfg.get_or<std::string>("launcher.ui.dark_accent", "blue");
            int sel = 0;
            for (int i = 0; i < 5; i++) if (cur == vals[i]) { sel = i; break; }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("深色主题色", make_dropdown_str(labels, vals, 5, sel, "launcher.ui.dark_accent")));
        }

        gtk_box_append(GTK_BOX(inner),
            make_check("显示启动器图标", "launcher.ui.show_logo",
                       cfg.get_or<bool>("launcher.ui.show_logo", true)));
        gtk_box_append(GTK_BOX(inner),
            make_check("锁定窗口大小", "launcher.ui.lock_window",
                       cfg.get_or<bool>("launcher.ui.lock_window", false)));
        gtk_box_append(GTK_BOX(inner),
            make_check("显示启动趣闻", "launcher.ui.show_trivia",
                       cfg.get_or<bool>("launcher.ui.show_trivia", true)));

        /* 模糊 — 复选框 + 条件子项 */
        {
            bool blur_on = cfg.get_or<bool>("launcher.ui.blur_enabled", false);
            GtkWidget* blur_cb = make_check("启用高级材质 / 模糊", "launcher.ui.blur_enabled", blur_on);
            gtk_box_append(GTK_BOX(inner), blur_cb);

            /* 模糊参数区域 (条件显示) */
            GtkWidget* blur_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_margin_start(blur_box, 20);

            /* 模糊半径 */
            {
                int v = cfg.get_or<int>("launcher.ui.blur_radius", 20);
                GtkWidget* sr = make_slider("launcher.ui.blur_radius", 0, 40, 1, v, "%.0f");
                gtk_box_append(GTK_BOX(blur_box), labeled_row("模糊半径", sr));
            }
            /* 模糊采样率 */
            {
                int v = cfg.get_or<int>("launcher.ui.blur_sampling", 50);
                GtkWidget* sr = make_slider("launcher.ui.blur_sampling", 0, 100, 1, v, "%.0f %%");
                gtk_box_append(GTK_BOX(blur_box), labeled_row("采样率", sr));
            }
            /* 模糊方式 */
            {
                const char* labels[] = {"高斯模糊", "方框模糊"};
                const char* vals[]   = {"gaussian", "box"};
                std::string cur = cfg.get_or<std::string>("launcher.ui.blur_method", "gaussian");
                int sel = (cur == "box") ? 1 : 0;
                gtk_box_append(GTK_BOX(blur_box),
                    labeled_row("模糊方式", make_dropdown_str(labels, vals, 2, sel, "launcher.ui.blur_method")));
            }

            if (!blur_on) gtk_widget_set_visible(blur_box, FALSE);
            gtk_box_append(GTK_BOX(inner), blur_box);

            /* blur checkbox → 条件显示 */
            g_object_set_data(G_OBJECT(blur_cb), "blur-box", blur_box);
            g_signal_connect(blur_cb, "toggled",
                G_CALLBACK(+[](GtkCheckButton* btn, gpointer) {
                    GtkWidget* bb = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "blur-box"));
                    if (bb) gtk_widget_set_visible(bb, gtk_check_button_get_active(btn));
                }), nullptr);
        }

        gtk_box_append(GTK_BOX(content), build_card("基础外观", inner));
    }

    /* ── Card 2: 字体 ──────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 全局字体 */
        {
            std::string fam = cfg.get_or<std::string>("launcher.ui.font_family", "");
            GtkWidget* e = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), "HarmonyOS Sans SC");
            gtk_editable_set_text(GTK_EDITABLE(e), fam.c_str());
            gtk_widget_set_hexpand(e, TRUE);

            auto* sp = new std::string("launcher.ui.font_family");
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);
            gtk_box_append(GTK_BOX(inner), labeled_row("全局字体", e));
        }
        /* MOTD 字体 */
        {
            std::string fam = cfg.get_or<std::string>("launcher.ui.motd_font_family", "");
            GtkWidget* e = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(e), "等宽字体");
            gtk_editable_set_text(GTK_EDITABLE(e), fam.c_str());
            gtk_widget_set_hexpand(e, TRUE);

            auto* sp = new std::string("launcher.ui.motd_font_family");
            g_signal_connect(e, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), sp);
            gtk_box_append(GTK_BOX(inner), labeled_row("MOTD 字体", e));
        }

        gtk_box_append(GTK_BOX(content), build_card("字体", inner));
    }

    /* ── Card 3: 背景图 ────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        /* 内容适应 */
        {
            const char* labels[] = {"智能", "居中", "包含", "拉伸", "平铺",
                                     "左上角", "右上角", "左下角", "右下角"};
            const char* vals[]   = {"smart", "center", "contain", "stretch", "tile",
                                     "corner_left_up", "corner_right_up",
                                     "corner_left_down", "corner_right_down"};
            std::string cur = cfg.get_or<std::string>("launcher.ui.background.fit", "smart");
            int sel = 0;
            for (int i = 0; i < 9; i++) if (cur == vals[i]) { sel = i; break; }
            gtk_box_append(GTK_BOX(inner),
                labeled_row("内容适应", make_dropdown_str(labels, vals, 9, sel, "launcher.ui.background.fit")));
        }
        /* 背景不透明度 */
        {
            double v = cfg.get_or<double>("launcher.ui.background.opacity", 1.0);
            GtkWidget* sr = make_slider("launcher.ui.background.opacity", 0.0, 1.0, 0.05, v, "%.0f%%");
            gtk_box_append(GTK_BOX(inner), labeled_row("背景不透明度", sr));
        }
        /* 背景模糊 */
        {
            int v = cfg.get_or<int>("launcher.ui.background.blur", 0);
            GtkWidget* sr = make_slider("launcher.ui.background.blur", 0, 40, 1, v, "%.0f");
            gtk_box_append(GTK_BOX(inner), labeled_row("背景模糊", sr));
        }

        gtk_box_append(GTK_BOX(inner),
            make_check("失去焦点时暂停背景视频", "launcher.ui.background.pause_blur",
                       cfg.get_or<bool>("launcher.ui.background.pause_blur", true)));
        gtk_box_append(GTK_BOX(inner),
            make_check("彩色覆盖层", "launcher.ui.background.color_overlay",
                       cfg.get_or<bool>("launcher.ui.background.color_overlay", false)));

        /* 操作按钮 */
        gtk_box_append(GTK_BOX(inner), action_buttons_row({"打开文件夹", "刷新", "清除"}));

        gtk_box_append(GTK_BOX(content), build_card("背景图", inner));
    }

    /* ── Card 4: Logo ──────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        std::string logo_type = cfg.get_or<std::string>("launcher.ui.logo.type", "default");

        /* Logo 类型 (互斥按钮组) */
        {
            const char* items[] = {"无", "默认", "文字", "图片"};
            GtkWidget* toggle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            gtk_widget_add_css_class(toggle, "linked");
            gtk_widget_set_margin_bottom(toggle, 10);

            GtkWidget* btns[4];
            for (int i = 0; i < 4; i++) {
                btns[i] = gtk_toggle_button_new_with_label(items[i]);
                gtk_widget_set_size_request(btns[i], 70, -1);
                gtk_box_append(GTK_BOX(toggle), btns[i]);
            }

            int active = (logo_type == "none") ? 0 : (logo_type == "text") ? 2 : (logo_type == "image") ? 3 : 1;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btns[active]), TRUE);

            /* 互斥逻辑 */
            auto toggle_cb = +[](GtkToggleButton* self, gpointer data) {
                if (!gtk_toggle_button_get_active(self)) return;
                GtkWidget* group = GTK_WIDGET(data);
                for (GtkWidget* sib = gtk_widget_get_first_child(group); sib;
                     sib = gtk_widget_get_next_sibling(sib)) {
                    if (sib != GTK_WIDGET(self) && GTK_IS_TOGGLE_BUTTON(sib))
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sib), FALSE);
                }
            };
            for (int i = 0; i < 4; i++)
                g_signal_connect(btns[i], "toggled", G_CALLBACK(toggle_cb), toggle);

            gtk_box_append(GTK_BOX(inner), labeled_row("Logo 类型", toggle));

            /* 条件控件 */
            GtkWidget* text_entry = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(text_entry), "自定义文字");
            gtk_editable_set_text(GTK_EDITABLE(text_entry),
                cfg.get_or<std::string>("launcher.ui.logo.text", "").c_str());
            gtk_widget_set_hexpand(text_entry, TRUE);

            GtkWidget* image_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            GtkWidget* img_entry = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(img_entry), "选择图片路径...");
            gtk_editable_set_text(GTK_EDITABLE(img_entry),
                cfg.get_or<std::string>("launcher.ui.logo.image_path", "").c_str());
            gtk_widget_set_hexpand(img_entry, TRUE);
            gtk_box_append(GTK_BOX(image_row), img_entry);
            GtkWidget* browse_btn = gtk_button_new_with_label("浏览");
            gtk_box_append(GTK_BOX(image_row), browse_btn);

            gtk_box_append(GTK_BOX(inner), text_entry);
            gtk_box_append(GTK_BOX(inner), image_row);

            if (logo_type != "text")  gtk_widget_set_visible(text_entry, FALSE);
            if (logo_type != "image") gtk_widget_set_visible(image_row, FALSE);

            /* Config bindings */
            auto* tex_p = new std::string("launcher.ui.logo.text");
            auto* img_p = new std::string("launcher.ui.logo.image_path");

            const char* type_vals[] = {"none", "default", "text", "image"};

            for (int i = 0; i < 4; i++) {
                g_object_set_data(G_OBJECT(btns[i]), "logo-type", (void*)type_vals[i]);
                g_object_set_data(G_OBJECT(btns[i]), "logo-text-entry", text_entry);
                g_object_set_data(G_OBJECT(btns[i]), "logo-image-row", image_row);

                g_signal_connect(btns[i], "toggled",
                    G_CALLBACK(+[](GtkToggleButton* btn, gpointer) {
                        if (!gtk_toggle_button_get_active(btn)) return;
                        const char* t = static_cast<const char*>(
                            g_object_get_data(G_OBJECT(btn), "logo-type"));
                        ConfigManager::instance().set("launcher.ui.logo.type", std::string(t));
                        GtkWidget* te = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "logo-text-entry"));
                        GtkWidget* ir = GTK_WIDGET(g_object_get_data(G_OBJECT(btn), "logo-image-row"));
                        if (te) gtk_widget_set_visible(te, std::string(t) == "text");
                        if (ir) gtk_widget_set_visible(ir, std::string(t) == "image");
                    }), nullptr);
            }

            g_signal_connect(text_entry, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), tex_p);

            g_signal_connect(img_entry, "notify::text",
                G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                    auto* p = static_cast<std::string*>(d);
                    ConfigManager::instance().set(*p,
                        std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                }), img_p);
        }

        gtk_box_append(GTK_BOX(content), build_card("Logo", inner));
    }

    /* ── Card 5: 主页 ──────────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        std::string hp_type = cfg.get_or<std::string>("launcher.ui.homepage.type", "preset");

        /* 主页类型 */
        {
            const char* items[] = {"空白", "预设", "本地", "联网"};
            GtkWidget* toggle = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
            gtk_widget_add_css_class(toggle, "linked");
            gtk_widget_set_margin_bottom(toggle, 10);

            GtkWidget* btns[4];
            for (int i = 0; i < 4; i++) {
                btns[i] = gtk_toggle_button_new_with_label(items[i]);
                gtk_widget_set_size_request(btns[i], 70, -1);
                gtk_box_append(GTK_BOX(toggle), btns[i]);
            }

            int active = (hp_type == "blank") ? 0 : (hp_type == "local") ? 2 : (hp_type == "remote") ? 3 : 1;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btns[active]), TRUE);

            auto toggle_cb = +[](GtkToggleButton* self, gpointer data) {
                if (!gtk_toggle_button_get_active(self)) return;
                GtkWidget* group = GTK_WIDGET(data);
                for (GtkWidget* sib = gtk_widget_get_first_child(group); sib;
                     sib = gtk_widget_get_next_sibling(sib)) {
                    if (sib != GTK_WIDGET(self) && GTK_IS_TOGGLE_BUTTON(sib))
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sib), FALSE);
                }
            };
            for (int i = 0; i < 4; i++)
                g_signal_connect(btns[i], "toggled", G_CALLBACK(toggle_cb), toggle);

            gtk_box_append(GTK_BOX(inner), labeled_row("主页内容", toggle));

            /* 条件: 预设下拉 */
            {
                const char* presets[] = {"Trivia", "McNews", "SimpleHomepage",
                    "DailyModpack", "McSkin", "OpenBmclapi",
                    "PclMarket", "PclNews", "PclManual",
                    "Magazine", "GitHub", "McUpdateSummary",
                    "PclAnnouncement", "McOfficialFeed", nullptr};
                int n = 14;
                std::string cur = cfg.get_or<std::string>("launcher.ui.homepage.preset", "Trivia");
                GtkStringList* model = gtk_string_list_new(nullptr);
                int sel = 0;
                for (int i = 0; i < n; i++) {
                    gtk_string_list_append(model, presets[i]);
                    if (cur == presets[i]) sel = i;
                }
                GtkWidget* dd = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
                gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), sel);
                gtk_widget_set_hexpand(dd, TRUE);

                GtkWidget* row = labeled_row("预设页面", dd);
                gtk_box_append(GTK_BOX(inner), row);
                if (hp_type != "preset") gtk_widget_set_visible(row, FALSE);
                g_object_set_data(G_OBJECT(toggle), "hp-preset-row", row);

                g_object_set_data(G_OBJECT(dd), "dd-path",
                    new std::string("launcher.ui.homepage.preset"));
                g_object_set_data_full(G_OBJECT(dd), "dd-presets",
                    g_strdupv((char**)presets), (GDestroyNotify)g_strfreev);
                g_signal_connect(dd, "notify::selected",
                    G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer) {
                        guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
                        auto* path = static_cast<std::string*>(g_object_get_data(G_OBJECT(o), "dd-path"));
                        char** presets = (char**)g_object_get_data(G_OBJECT(o), "dd-presets");
                        if (path && idx < 14 && presets)
                            ConfigManager::instance().set(*path, std::string(presets[idx]));
                    }), nullptr);
            }

            /* 条件: 本地路径 */
            {
                std::string v = cfg.get_or<std::string>("launcher.ui.homepage.local_path", "");
                GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                GtkWidget* e = gtk_entry_new();
                gtk_entry_set_placeholder_text(GTK_ENTRY(e), "HTML 文件路径");
                gtk_editable_set_text(GTK_EDITABLE(e), v.c_str());
                gtk_widget_set_hexpand(e, TRUE);
                gtk_box_append(GTK_BOX(row), e);
                gtk_box_append(GTK_BOX(row), gtk_button_new_with_label("刷新"));
                gtk_box_append(GTK_BOX(row), gtk_button_new_with_label("教程"));

                GtkWidget* lr = labeled_row("本地路径", row);
                gtk_box_append(GTK_BOX(inner), lr);
                if (hp_type != "local") gtk_widget_set_visible(lr, FALSE);
                g_object_set_data(G_OBJECT(toggle), "hp-local-row", lr);

                auto* sp = new std::string("launcher.ui.homepage.local_path");
                g_signal_connect(e, "notify::text",
                    G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                        auto* p = static_cast<std::string*>(d);
                        ConfigManager::instance().set(*p,
                            std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                    }), sp);
            }

            /* 条件: 联网 URL */
            {
                std::string v = cfg.get_or<std::string>("launcher.ui.homepage.remote_url", "");
                GtkWidget* e = gtk_entry_new();
                gtk_entry_set_placeholder_text(GTK_ENTRY(e), "https://...");
                gtk_editable_set_text(GTK_EDITABLE(e), v.c_str());
                gtk_widget_set_hexpand(e, TRUE);

                GtkWidget* lr = labeled_row("网页 URL", e);
                gtk_box_append(GTK_BOX(inner), lr);
                if (hp_type != "remote") gtk_widget_set_visible(lr, FALSE);
                g_object_set_data(G_OBJECT(toggle), "hp-remote-row", lr);

                auto* sp = new std::string("launcher.ui.homepage.remote_url");
                g_signal_connect(e, "notify::text",
                    G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer d) {
                        auto* p = static_cast<std::string*>(d);
                        ConfigManager::instance().set(*p,
                            std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
                    }), sp);
            }

            /* 类型切换 → config + 条件显示 */
            const char* hp_type_vals[] = {"blank", "preset", "local", "remote"};
            for (int i = 0; i < 4; i++) {
                g_object_set_data(G_OBJECT(btns[i]), "hp-type", (void*)hp_type_vals[i]);
                g_signal_connect(btns[i], "toggled",
                    G_CALLBACK(+[](GtkToggleButton* btn, gpointer) {
                        if (!gtk_toggle_button_get_active(btn)) return;
                        const char* t = static_cast<const char*>(
                            g_object_get_data(G_OBJECT(btn), "hp-type"));
                        ConfigManager::instance().set("launcher.ui.homepage.type", std::string(t));

                        GtkWidget* group = gtk_widget_get_parent(GTK_WIDGET(btn));
                        GtkWidget* preset_r = GTK_WIDGET(g_object_get_data(G_OBJECT(group), "hp-preset-row"));
                        GtkWidget* local_r  = GTK_WIDGET(g_object_get_data(G_OBJECT(group), "hp-local-row"));
                        GtkWidget* remote_r = GTK_WIDGET(g_object_get_data(G_OBJECT(group), "hp-remote-row"));
                        std::string ts(t);
                        if (preset_r) gtk_widget_set_visible(preset_r, ts == "preset");
                        if (local_r)  gtk_widget_set_visible(local_r,  ts == "local");
                        if (remote_r) gtk_widget_set_visible(remote_r, ts == "remote");
                    }), nullptr);
            }
        }

        gtk_box_append(GTK_BOX(content), build_card("主页", inner));
    }

    /* ── Card 6: 功能隐藏 ──────────────────────────────────────────────── */
    {
        GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

        /* 主页标签 */
        gtk_box_append(GTK_BOX(inner), section_label("主页标签"));
        {
            GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
            GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_box_append(GTK_BOX(col), make_array_check("下载",  "launcher.ui.hidden.pages", "download"));
            gtk_box_append(GTK_BOX(col), make_array_check("设置",  "launcher.ui.hidden.pages", "settings"));
            gtk_box_append(GTK_BOX(col), make_array_check("工具",  "launcher.ui.hidden.pages", "tools"));
            gtk_box_append(GTK_BOX(grid), col);
            gtk_box_append(GTK_BOX(inner), grid);
        }

        /* 设置子页面 */
        gtk_box_append(GTK_BOX(inner), section_label("设置子页面"));
        {
            GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            GtkWidget* c1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            GtkWidget* c2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_box_append(GTK_BOX(c1), make_array_check("启动",    "launcher.ui.hidden.setup", "launch"));
            gtk_box_append(GTK_BOX(c1), make_array_check("Java",   "launcher.ui.hidden.setup", "java"));
            gtk_box_append(GTK_BOX(c1), make_array_check("游戏管理","launcher.ui.hidden.setup", "game_manage"));
            gtk_box_append(GTK_BOX(c1), make_array_check("界面",    "launcher.ui.hidden.setup", "ui"));
            gtk_box_append(GTK_BOX(c1), make_array_check("语言",    "launcher.ui.hidden.setup", "language"));
            gtk_box_append(GTK_BOX(c2), make_array_check("杂项",    "launcher.ui.hidden.setup", "misc"));
            gtk_box_append(GTK_BOX(c2), make_array_check("关于",    "launcher.ui.hidden.setup", "about"));
            gtk_box_append(GTK_BOX(c2), make_array_check("反馈",    "launcher.ui.hidden.setup", "feedback"));
            gtk_box_append(GTK_BOX(c2), make_array_check("日志",    "launcher.ui.hidden.setup", "log"));
            gtk_widget_set_hexpand(c2, TRUE);
            gtk_box_append(GTK_BOX(grid), c1);
            gtk_box_append(GTK_BOX(grid), c2);
            gtk_box_append(GTK_BOX(inner), grid);
        }

        /* 工具子页面 */
        gtk_box_append(GTK_BOX(inner), section_label("工具子页面"));
        {
            GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
            GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_box_append(GTK_BOX(col), make_array_check("工具箱",   "launcher.ui.hidden.tools", "toolbox"));
            gtk_box_append(GTK_BOX(grid), col);
            gtk_box_append(GTK_BOX(inner), grid);
        }

        /* 实例功能 */
        gtk_box_append(GTK_BOX(inner), section_label("实例功能"));
        {
            GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            GtkWidget* c1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            GtkWidget* c2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_box_append(GTK_BOX(c1), make_array_check("编辑",    "launcher.ui.hidden.instance", "edit"));
            gtk_box_append(GTK_BOX(c1), make_array_check("导出",    "launcher.ui.hidden.instance", "export"));
            gtk_box_append(GTK_BOX(c1), make_array_check("存档",    "launcher.ui.hidden.instance", "save"));
            gtk_box_append(GTK_BOX(c1), make_array_check("截图",    "launcher.ui.hidden.instance", "screenshot"));
            gtk_box_append(GTK_BOX(c1), make_array_check("Mod",    "launcher.ui.hidden.instance", "mod"));
            gtk_box_append(GTK_BOX(c2), make_array_check("资源包",   "launcher.ui.hidden.instance", "resource_pack"));
            gtk_box_append(GTK_BOX(c2), make_array_check("光影",    "launcher.ui.hidden.instance", "shader"));
            gtk_box_append(GTK_BOX(c2), make_array_check("原理图",   "launcher.ui.hidden.instance", "schematic"));
            gtk_box_append(GTK_BOX(c2), make_array_check("服务器",   "launcher.ui.hidden.instance", "server"));
            gtk_widget_set_hexpand(c2, TRUE);
            gtk_box_append(GTK_BOX(grid), c1);
            gtk_box_append(GTK_BOX(grid), c2);
            gtk_box_append(GTK_BOX(inner), grid);
        }

        /* 特定功能 */
        gtk_box_append(GTK_BOX(inner), section_label("特定功能"));
        {
            GtkWidget* grid = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
            GtkWidget* col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_box_append(GTK_BOX(col), make_array_check("版本选择",    "launcher.ui.hidden.functions", "version_select"));
            gtk_box_append(GTK_BOX(col), make_array_check("Mod 更新",   "launcher.ui.hidden.functions", "mod_update"));
            gtk_box_append(GTK_BOX(col), make_array_check("功能隐藏本身", "launcher.ui.hidden.functions", "feature_hide"));
            gtk_box_append(GTK_BOX(grid), col);
            gtk_box_append(GTK_BOX(inner), grid);
        }

        gtk_box_append(GTK_BOX(content), build_card("功能隐藏", inner));
    }

    return sw;
}

}  // namespace pcl
