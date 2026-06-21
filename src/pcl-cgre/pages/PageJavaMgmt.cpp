#include "pages/PageJavaMgmt.hpp"
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
 * P1 — Java 管理 (工具栏 + 动态列表)
 * ============================================================================ */

namespace {

void java_refresh_list(GtkListBox* lb)
{
    /* clear */
    GtkWidget* row = gtk_widget_get_first_child(GTK_WIDGET(lb));
    while (row) {
        GtkWidget* next = gtk_widget_get_next_sibling(row);
        gtk_list_box_remove(lb, row);
        row = next;
    }

    auto& cfg = ConfigManager::instance();
    auto arr = cfg.get_json("java.runtimes");
    int def_idx = cfg.get_or<int>("java.default_runtime", 0);

    for (size_t i = 0; i < arr.size(); i++) {
        auto& rt = arr[i];
        std::string name = rt.value("name", "Unknown");
        std::string path = rt.value("path", "");

        GtkWidget* item_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_margin_start(item_row, 8);
        gtk_widget_set_margin_end(item_row, 8);
        gtk_widget_set_margin_top(item_row, 6);
        gtk_widget_set_margin_bottom(item_row, 6);

        /* info box */
        GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget* name_lbl = gtk_label_new(name.c_str());
        gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
        gtk_widget_add_css_class(name_lbl, "card-title");
        gtk_box_append(GTK_BOX(info), name_lbl);

        GtkWidget* path_lbl = gtk_label_new(path.c_str());
        gtk_widget_set_opacity(path_lbl, 0.55);
        gtk_widget_set_halign(path_lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(info), path_lbl);

        gtk_box_append(GTK_BOX(item_row), info);
        gtk_widget_set_hexpand(info, TRUE);

        /* [设为默认] */
        if ((int)i == def_idx) {
            GtkWidget* badge = gtk_label_new("默认");
            gtk_widget_add_css_class(badge, "resource-tag");
            gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(item_row), badge);
        } else {
            GtkWidget* sel_btn = gtk_button_new_with_label("选用");
            gtk_widget_set_valign(sel_btn, GTK_ALIGN_CENTER);
            auto* idx_ptr = new int((int)i);
            g_object_set_data(G_OBJECT(sel_btn), "lb", lb);
            g_object_set_data(G_OBJECT(sel_btn), "idx", idx_ptr);
            g_signal_connect(sel_btn, "clicked",
                G_CALLBACK(+[](GtkButton* btn, gpointer) {
                    auto* idx_ptr = static_cast<int*>(g_object_get_data(G_OBJECT(btn), "idx"));
                    ConfigManager::instance().set("java.default_runtime", *idx_ptr);
                    GtkListBox* lb = GTK_LIST_BOX(g_object_get_data(G_OBJECT(btn), "lb"));
                    if (lb) java_refresh_list(lb);
                }), nullptr);
            gtk_box_append(GTK_BOX(item_row), sel_btn);
        }

        /* [移除] */
        GtkWidget* rm_btn = gtk_button_new_with_label("移除");
        gtk_widget_set_valign(rm_btn, GTK_ALIGN_CENTER);
        gtk_widget_add_css_class(rm_btn, "destructive-action");
        auto* rm_idx = new size_t(i);
        g_object_set_data(G_OBJECT(rm_btn), "lb", lb);
        g_object_set_data(G_OBJECT(rm_btn), "rm-idx", rm_idx);
        g_signal_connect(rm_btn, "clicked",
            G_CALLBACK(+[](GtkButton* btn, gpointer) {
                auto* rm_idx = static_cast<size_t*>(
                    g_object_get_data(G_OBJECT(btn), "rm-idx"));
                GtkListBox* lb = GTK_LIST_BOX(
                    g_object_get_data(G_OBJECT(btn), "lb"));
                auto& cfg = ConfigManager::instance();
                auto arr = cfg.get_json("java.runtimes");
                size_t idx = *rm_idx;
                if (idx < arr.size()) {
                    arr.erase(arr.begin() + (long)idx);
                    cfg.set_json("java.runtimes", arr);
                    int def = cfg.get_or<int>("java.default_runtime", 0);
                    if ((size_t)def >= arr.size())
                        cfg.set("java.default_runtime", 0);
                }
                if (lb) java_refresh_list(lb);
            }), nullptr);
        gtk_box_append(GTK_BOX(item_row), rm_btn);

        gtk_list_box_append(lb, item_row);
    }
}

}  // anonymous namespace

GtkWidget* build_page_java_mgmt()
{
    auto sp = scrolled_content();
    GtkWidget* sw = sp.sw;
    GtkWidget* content = sp.content;

    GtkWidget* inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

    /* 工具栏 */
    GtkWidget* toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(toolbar, 12);

    GtkWidget* add_btn = gtk_button_new_with_label("+ 添加 Java 运行时");
    gtk_widget_add_css_class(add_btn, "suggested-action");
    gtk_box_append(GTK_BOX(toolbar), add_btn);

    GtkWidget* refresh_btn = gtk_button_new_with_label("刷新");
    gtk_box_append(GTK_BOX(toolbar), refresh_btn);
    gtk_box_append(GTK_BOX(inner), toolbar);

    /* Java 运行时列表 */
    GtkWidget* list_box = gtk_list_box_new();
    gtk_widget_add_css_class(list_box, "rich-list");
    gtk_box_append(GTK_BOX(inner), list_box);

    java_refresh_list(GTK_LIST_BOX(list_box));

    g_object_set_data(G_OBJECT(refresh_btn), "lb", list_box);
    g_signal_connect(refresh_btn, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            java_refresh_list(GTK_LIST_BOX(d));
        }), list_box);

    /* [+] 添加 Java 运行时 */
    g_object_set_data(G_OBJECT(add_btn), "lb", list_box);
    g_signal_connect(add_btn, "clicked",
        G_CALLBACK(+[](GtkButton* btn, gpointer) {
            GtkListBox* lb = GTK_LIST_BOX(
                g_object_get_data(G_OBJECT(btn), "lb"));

            GtkFileDialog* dlg = gtk_file_dialog_new();
            gtk_file_dialog_set_title(dlg, "选择 Java 可执行文件");

            GtkFileFilter* filter = gtk_file_filter_new();
            gtk_file_filter_set_name(filter, "Java 可执行文件");
            gtk_file_filter_add_pattern(filter, "java");

            GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
            g_list_store_append(filters, filter);
            gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(filters));

            gtk_file_dialog_open(dlg,
                GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(btn))),
                nullptr,
                +[](GObject* src, GAsyncResult* res, gpointer data) {
                    GFile* file = gtk_file_dialog_open_finish(
                        GTK_FILE_DIALOG(src), res, nullptr);
                    if (!file) return;

                    char* path = g_file_get_path(file);
                    std::string java_path(path);
                    g_free(path);

                    // 检测 Java 版本
                    std::string version_name = "Java";
                    std::string cmd = "\"" + java_path + "\" --version 2>&1";
                    FILE* p = popen(cmd.c_str(), "r");
                    if (p) {
                        char buf[256];
                        if (fgets(buf, sizeof(buf), p)) {
                            std::string s(buf);
                            auto pos = s.find("\"");
                            if (pos != std::string::npos) {
                                auto end = s.find("\"", pos + 1);
                                if (end != std::string::npos)
                                    version_name = s.substr(pos + 1, end - pos - 1);
                            } else {
                                auto vpos = s.find("version ");
                                if (vpos != std::string::npos) {
                                    auto ver = s.substr(vpos + 8);
                                    auto sp = ver.find(' ');
                                    if (sp != std::string::npos)
                                        ver = ver.substr(0, sp);
                                    version_name = "Java " + ver;
                                }
                            }
                        }
                        pclose(p);
                    }

                    auto& cfg = ConfigManager::instance();
                    auto arr = cfg.get_json("java.runtimes");
                    nlohmann::json entry;
                    entry["name"] = version_name;
                    entry["path"] = java_path;
                    arr.push_back(entry);
                    cfg.set_json("java.runtimes", arr);

                    // Rebuild list
                    java_refresh_list(GTK_LIST_BOX(data));
                }, lb);

            g_object_unref(dlg);
        }), nullptr);

    gtk_box_append(GTK_BOX(content), build_card("Java 运行时管理", inner));

    return sw;
}

}  // namespace pcl
