/**
 * 通用表单控件库 — 实现
 *
 * 内存所有权:
 *   - cfg_path 通过 g_strdup 复制, 挂到控件上, 由 g_free 自动释放
 *   - 不再使用 `new std::string(...)` 作为 signal user_data (会泄漏)
 */

#include "widgets/FormWidgets.hpp"
#include "core/ConfigManager.hpp"

#include <cstdio>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace pcl {

namespace {

const char* read_cfg_path(GObject* obj)
{
    return static_cast<const char*>(g_object_get_data(obj, "cfg-path"));
}

void attach_cfg_path(GtkWidget* w, const char* cfg_path)
{
    g_object_set_data_full(G_OBJECT(w), "cfg-path",
                           g_strdup(cfg_path), g_free);
}

void destroy_string_vector(gpointer p)
{
    delete static_cast<std::vector<std::string>*>(p);
}

void destroy_double(gpointer p)
{
    delete static_cast<double*>(p);
}

}  // anonymous namespace

ScrolledPage scrolled_content()
{
    ScrolledPage p;
    p.sw = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(p.sw, TRUE);
    gtk_widget_set_hexpand(p.sw, TRUE);

    p.content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(p.content, TRUE);
    gtk_widget_set_hexpand(p.content, TRUE);
    gtk_widget_set_margin_start(p.content, 25);
    gtk_widget_set_margin_end(p.content, 25);
    gtk_widget_set_margin_top(p.content, 10);
    gtk_widget_set_margin_bottom(p.content, 25);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(p.sw), p.content);
    return p;
}

GtkWidget* build_card(const char* title, GtkWidget* content)
{
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_widget_set_hexpand(card, TRUE);
    gtk_widget_set_margin_bottom(card, 16);

    GtkWidget* tl = gtk_label_new(title ? title : "");
    gtk_widget_add_css_class(tl, "card-title");
    gtk_widget_set_halign(tl, GTK_ALIGN_START);
    gtk_widget_set_margin_start(tl, 20);
    gtk_widget_set_margin_end(tl, 20);
    gtk_widget_set_margin_top(tl, 16);
    gtk_box_append(GTK_BOX(card), tl);

    if (content) {
        gtk_widget_set_margin_start(content, 20);
        gtk_widget_set_margin_end(content, 20);
        gtk_widget_set_margin_top(content, 12);
        gtk_widget_set_margin_bottom(content, 16);
        gtk_box_append(GTK_BOX(card), content);
    }
    return card;
}

GtkWidget* labeled_row(const char* label, GtkWidget* widget)
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(row, 7);

    GtkWidget* lbl = gtk_label_new(label ? label : "");
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_size_request(lbl, 110, -1);
    gtk_box_append(GTK_BOX(row), lbl);

    if (widget) {
        gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(row), widget);
    }
    return row;
}

GtkWidget* make_dropdown(const char* const* items, int n, int sel,
                         const char* cfg_path)
{
    GtkStringList* model = gtk_string_list_new(nullptr);
    for (int i = 0; i < n; i++)
        gtk_string_list_append(model, items && items[i] ? items[i] : "");
    GtkWidget* dd = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), sel);
    gtk_widget_set_hexpand(dd, TRUE);
    gtk_widget_set_valign(dd, GTK_ALIGN_CENTER);

    if (cfg_path) {
        attach_cfg_path(dd, cfg_path);
        g_signal_connect(dd, "notify::selected",
            G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer) {
                const char* path = read_cfg_path(o);
                if (!path) return;
                guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
                ConfigManager::instance().set(std::string(path), (int)idx);
            }), nullptr);
    }
    return dd;
}

GtkWidget* make_dropdown_str(const char* const* labels,
                             const char* const* values,
                             int n, int sel,
                             const char* cfg_path)
{
    GtkStringList* model = gtk_string_list_new(nullptr);
    for (int i = 0; i < n; i++)
        gtk_string_list_append(model, labels && labels[i] ? labels[i] : "");
    GtkWidget* dd = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), sel);
    gtk_widget_set_hexpand(dd, TRUE);
    gtk_widget_set_valign(dd, GTK_ALIGN_CENTER);

    auto* vals = new std::vector<std::string>();
    vals->reserve(n);
    for (int i = 0; i < n; i++)
        vals->emplace_back(values && values[i] ? values[i] : "");
    g_object_set_data_full(G_OBJECT(dd), "dd-vals",
                           vals, destroy_string_vector);

    if (cfg_path) {
        attach_cfg_path(dd, cfg_path);
        g_signal_connect(dd, "notify::selected",
            G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer) {
                guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(o));
                auto* vals = static_cast<std::vector<std::string>*>(
                    g_object_get_data(o, "dd-vals"));
                const char* path = read_cfg_path(o);
                if (vals && idx < vals->size() && path)
                    ConfigManager::instance().set(std::string(path),
                                                  (*vals)[idx]);
            }), nullptr);
    }
    return dd;
}

GtkWidget* make_check(const char* label, const char* cfg_path, bool def)
{
    GtkWidget* cb = gtk_check_button_new_with_label(label ? label : "");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(cb), def);
    gtk_widget_set_margin_bottom(cb, 6);

    if (cfg_path) {
        attach_cfg_path(cb, cfg_path);
        g_signal_connect(cb, "toggled",
            G_CALLBACK(+[](GtkCheckButton* btn, gpointer) {
                const char* path = read_cfg_path(G_OBJECT(btn));
                if (!path) return;
                ConfigManager::instance().set(std::string(path),
                    gtk_check_button_get_active(btn) ? true : false);
            }), nullptr);
    }
    return cb;
}

GtkWidget* make_entry(const char* cfg_path, const char* placeholder)
{
    GtkWidget* e = gtk_entry_new();
    if (placeholder) gtk_entry_set_placeholder_text(GTK_ENTRY(e), placeholder);
    gtk_widget_set_hexpand(e, TRUE);

    if (cfg_path) {
        attach_cfg_path(e, cfg_path);
        g_signal_connect(e, "notify::text",
            G_CALLBACK(+[](GObject* o, GParamSpec*, gpointer) {
                const char* path = read_cfg_path(o);
                if (!path) return;
                ConfigManager::instance().set(std::string(path),
                    std::string(gtk_editable_get_text(GTK_EDITABLE(o))));
            }), nullptr);
    }
    return e;
}

GtkWidget* make_slider(const char* cfg_path, double min, double max,
                       double step, double def, const char* fmt)
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(row, 7);

    GtkAdjustment* adj = gtk_adjustment_new(def, min, max, step, step, 0);
    GtkWidget* scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_box_append(GTK_BOX(row), scale);

    GtkWidget* val_lbl = gtk_label_new("");
    gtk_widget_set_size_request(val_lbl, 60, -1);
    gtk_widget_set_halign(val_lbl, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(row), val_lbl);

    g_object_set_data(G_OBJECT(adj), "slider-label", val_lbl);
    g_object_set_data_full(G_OBJECT(adj), "slider-fmt",
                           g_strdup(fmt ? fmt : "%.0f"), g_free);
    g_signal_connect(adj, "value-changed",
        G_CALLBACK(+[](GtkAdjustment* a, gpointer) {
            GtkLabel* l = GTK_LABEL(g_object_get_data(G_OBJECT(a), "slider-label"));
            const char* f = static_cast<const char*>(
                g_object_get_data(G_OBJECT(a), "slider-fmt"));
            if (!l || !f) return;
            char buf[32];
            std::snprintf(buf, sizeof(buf), f, gtk_adjustment_get_value(a));
            gtk_label_set_text(l, buf);
        }), nullptr);

    if (cfg_path) {
        g_object_set_data_full(G_OBJECT(adj), "cfg-path",
                               g_strdup(cfg_path), g_free);
        g_signal_connect(adj, "value-changed",
            G_CALLBACK(+[](GtkAdjustment* a, gpointer) {
                const char* path = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(a), "cfg-path"));
                if (!path) return;
                ConfigManager::instance().set(std::string(path),
                    (int)gtk_adjustment_get_value(a));
            }), nullptr);
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), fmt ? fmt : "%.0f", def);
    gtk_label_set_text(GTK_LABEL(val_lbl), buf);

    return row;
}

GtkWidget* make_slider_entry(const char* cfg_path, double min, double max,
                             double step, double def, const char* fmt)
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_bottom(row, 7);

    GtkAdjustment* adj = gtk_adjustment_new(def, min, max, step, step, 0);
    GtkWidget* scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
    gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_box_append(GTK_BOX(row), scale);

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
    gtk_widget_set_size_request(entry, 72, -1);
    gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(row), entry);

    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), fmt ? fmt : "%.0f", def);
        gtk_editable_set_text(GTK_EDITABLE(entry), buf);
    }

    g_object_set_data(G_OBJECT(adj), "se-entry", entry);
    g_object_set_data_full(G_OBJECT(adj), "se-fmt",
                           g_strdup(fmt ? fmt : "%.0f"), g_free);
    g_object_set_data_full(G_OBJECT(adj), "se-min",
                           new double(min), destroy_double);
    g_object_set_data_full(G_OBJECT(adj), "se-max",
                           new double(max), destroy_double);

    g_signal_connect(adj, "value-changed",
        G_CALLBACK(+[](GtkAdjustment* a, gpointer) {
            GtkEditable* e = GTK_EDITABLE(g_object_get_data(G_OBJECT(a), "se-entry"));
            const char* f = static_cast<const char*>(
                g_object_get_data(G_OBJECT(a), "se-fmt"));
            if (!e || !f) return;
            char buf[32];
            std::snprintf(buf, sizeof(buf), f, gtk_adjustment_get_value(a));
            gtk_editable_set_text(e, buf);
        }), nullptr);

    if (cfg_path) {
        g_object_set_data_full(G_OBJECT(adj), "cfg-path",
                               g_strdup(cfg_path), g_free);
        g_signal_connect(adj, "value-changed",
            G_CALLBACK(+[](GtkAdjustment* a, gpointer) {
                const char* path = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(a), "cfg-path"));
                if (!path) return;
                ConfigManager::instance().set(std::string(path),
                    (int)gtk_adjustment_get_value(a));
            }), nullptr);
    }

    auto entry_cb = +[](GtkEntry* e, gpointer data) {
        GtkAdjustment* a = GTK_ADJUSTMENT(data);
        const char* text = gtk_editable_get_text(GTK_EDITABLE(e));
        double val = g_strtod(text, nullptr);
        double* mn = static_cast<double*>(g_object_get_data(G_OBJECT(a), "se-min"));
        double* mx = static_cast<double*>(g_object_get_data(G_OBJECT(a), "se-max"));
        if (mn && val < *mn) val = *mn;
        if (mx && val > *mx) val = *mx;
        gtk_adjustment_set_value(a, val);
    };
    g_signal_connect(entry, "activate", G_CALLBACK(entry_cb), adj);

    return row;
}

GtkWidget* make_array_check(const char* label, const char* cfg_path,
                            const char* value)
{
    auto& cfg = ConfigManager::instance();
    auto arr = cfg_path ? cfg.get_json(cfg_path) : nlohmann::json::array();
    bool active = false;
    if (arr.is_array() && value) {
        for (auto& v : arr)
            if (v.is_string() && v.get<std::string>() == value) {
                active = true;
                break;
            }
    }

    GtkWidget* cb = gtk_check_button_new_with_label(label ? label : "");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(cb), active);
    gtk_widget_set_margin_bottom(cb, 4);

    if (cfg_path && value) {
        g_object_set_data_full(G_OBJECT(cb), "arr-path",
                               g_strdup(cfg_path), g_free);
        g_object_set_data_full(G_OBJECT(cb), "arr-val",
                               g_strdup(value), g_free);
        g_signal_connect(cb, "toggled",
            G_CALLBACK(+[](GtkCheckButton* btn, gpointer) {
                const char* path = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(btn), "arr-path"));
                const char* val = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(btn), "arr-val"));
                if (!path || !val) return;
                bool checked = gtk_check_button_get_active(btn);
                auto& cfg = ConfigManager::instance();
                auto arr = cfg.get_json(path);
                if (!arr.is_array()) arr = nlohmann::json::array();
                if (checked) {
                    bool found = false;
                    for (auto& v : arr)
                        if (v.is_string() && v.get<std::string>() == val) {
                            found = true; break;
                        }
                    if (!found) arr.push_back(val);
                } else {
                    for (auto it = arr.begin(); it != arr.end(); ++it) {
                        if (it->is_string() && it->get<std::string>() == val) {
                            arr.erase(it); break;
                        }
                    }
                }
                cfg.set_json(path, arr);
            }), nullptr);
    }
    return cb;
}

GtkWidget* section_label(const char* text)
{
    GtkWidget* lbl = gtk_label_new(text ? text : "");
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_top(lbl, 8);
    gtk_widget_set_margin_bottom(lbl, 2);
    gtk_widget_set_opacity(lbl, 0.60);
    return lbl;
}

GtkWidget* action_buttons_row(std::initializer_list<const char*> labels)
{
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(row, 8);
    gtk_widget_set_margin_start(row, 122);
    for (auto lbl : labels) {
        GtkWidget* btn = gtk_button_new_with_label(lbl ? lbl : "");
        gtk_box_append(GTK_BOX(row), btn);
    }
    return row;
}

}  // namespace pcl
