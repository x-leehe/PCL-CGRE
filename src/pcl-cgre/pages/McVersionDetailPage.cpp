#include "pages/McVersionDetailPage.hpp"
#include "app/NavigationController.hpp"
#include "util/IconHelper.hpp"
#include "network/LoaderFetcher.hpp"
#include "network/ResourceFetcher.hpp"
#include "core/Log.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include <adwaita.h>

namespace pcl {

/* ═══════════════════════════════════════════════════════════════════════
 * 内部: 构建一个可展开的加载器卡片
 *
 *   对标 PCL-CE PageDownloadInstall 中的 MyCard (IsSwapped / CanSwap)
 *   - 标题行: [block icon 18px] [名称] [选中版本] [chevron]
 *   - 展开后: 懒加载版本列表 (首次展开时触发异步请求)
 * ═══════════════════════════════════════════════════════════════════════ */

namespace {

/* ═══════════════════════════════════════════════════════════════════════
 *  图标辅助函数
 * ═══════════════════════════════════════════════════════════════════════ */

const char* icon_for_version_type(const std::string& vt)
{
    if (vt == "snapshot")   return "Command";
    if (vt == "old_alpha")  return "Minecraft-Alpha";
    if (vt == "old_beta")   return "Minecraft-Beta";
    if (vt == "april_fool") return "Minecraft-AprilFool";
    return "Minecraft";
}

const char* icon_for_loader(const char* ldr)
{
    if (!ldr) return nullptr;
    if (std::strcmp(ldr, "forge") == 0)     return "Anvil";
    if (std::strcmp(ldr, "fabric") == 0)    return "Fabric";
    if (std::strcmp(ldr, "quilt") == 0)     return "Quilt";
    if (std::strcmp(ldr, "optifine") == 0)  return "OptiFine";
    if (std::strcmp(ldr, "liteloader") == 0) return "Egg";
    if (std::strcmp(ldr, "neoforge") == 0)  return "NeoForge";
    if (std::strcmp(ldr, "cleanroom") == 0) return "Cleanroom";
    return nullptr;
}

void update_detail_logo(GtkWidget* box, const char* block_name)
{
    GtkWidget* info_card = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(box), "detail-info-card"));
    if (!info_card) return;
    GtkWidget* old_logo = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(info_card), "detail-logo"));
    if (!old_logo) return;

    GtkWidget* new_logo = icon::load_block(block_name, 64);
    gtk_widget_set_valign(new_logo, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(new_logo, GTK_ALIGN_START);
    gtk_widget_set_size_request(new_logo, 64, 64);
    gtk_widget_set_hexpand(new_logo, FALSE);
    gtk_widget_set_vexpand(new_logo, FALSE);
    gtk_widget_set_margin_top(new_logo, 2);

    GtkWidget* parent = gtk_widget_get_parent(old_logo);
    if (parent) {
        gtk_box_remove(GTK_BOX(parent), old_logo);
        gtk_widget_insert_after(new_logo, parent, nullptr);
    }
    g_object_set_data(G_OBJECT(info_card), "detail-logo", new_logo);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  build_loader_card
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Build a single expandable loader card with lazy loading.
 *
 * @param title         Card title (e.g. "Forge", "Fabric")
 * @param loader_name   Fetch key (e.g. "forge", "fabric", "optifine")
 * @param block_icon    Block icon name for the header (e.g. "Anvil", "Fabric")
 * @param fallback_lucide  Lucide icon name if block icon not available
 * @param initially_visible  Whether the card starts visible
 * @param box           The outer content box (for finding the info-card entry)
 */
GtkWidget* build_loader_card(const char* title,
                             const char* loader_name,
                             const char* block_icon,
                             const char* fallback_lucide,
                             bool        initially_visible,
                             GtkWidget*  box)
{
    GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(card, "card");
    gtk_widget_set_margin_bottom(card, 4);
    if (!initially_visible)
        gtk_widget_set_visible(card, FALSE);

    /* ── 存储关键数据 ── */
    g_object_set_data_full(G_OBJECT(card), "loader-name",
                           g_strdup(loader_name), g_free);
    g_object_set_data(G_OBJECT(card), "fetch-state",
                      GINT_TO_POINTER(0));  // 0=idle, 1=loading, 2=loaded
    g_object_set_data(G_OBJECT(card), "detail-box", box);

    /* ── 可点击标题行 ── */
    GtkWidget* header = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(header), FALSE);
    gtk_widget_add_css_class(header, "card-header-btn");
    {
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_start(hbox, 4);
        gtk_widget_set_margin_end(hbox, 4);

        /* 图标 — 优先用 block icon，fallback 到 Lucide */
        GtkWidget* icon_w = nullptr;
        if (block_icon && *block_icon) {
            icon_w = icon::load_block(block_icon, 18);
            if (GTK_IS_IMAGE(icon_w)) {
                GdkPaintable* pt = gtk_image_get_paintable(GTK_IMAGE(icon_w));
                if (!pt) icon_w = nullptr;
            }
        }
        if (!icon_w)
            icon_w = icon::load(fallback_lucide, 18);
        gtk_widget_set_valign(icon_w, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(hbox), icon_w);

        /* 标题 */
        GtkWidget* lbl = gtk_label_new(title);
        gtk_widget_add_css_class(lbl, "card-title");
        gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
        gtk_widget_set_hexpand(lbl, TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_box_append(GTK_BOX(hbox), lbl);

        /* 选中版本提示 (初始隐藏) */
        GtkWidget* sel_lbl = gtk_label_new("");
        gtk_widget_set_opacity(sel_lbl, 0.55);
        gtk_widget_set_valign(sel_lbl, GTK_ALIGN_CENTER);
        gtk_widget_set_visible(sel_lbl, FALSE);
        gtk_box_append(GTK_BOX(hbox), sel_lbl);
        g_object_set_data(G_OBJECT(card), "sel-lbl", sel_lbl);

        /* 展开/折叠箭头 */
        GtkWidget* arrow = icon::load("chevron-down", 18);
        gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(hbox), arrow);
        g_object_set_data(G_OBJECT(header), "arrow", arrow);

        gtk_button_set_child(GTK_BUTTON(header), hbox);
    }
    gtk_box_append(GTK_BOX(card), header);

    /* ── 可展开内容区 (spinner + version-list + placeholder) ── */
    GtkWidget* revealer = gtk_revealer_new();
    gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
    {
        GtkWidget* content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_start(content, 20);
        gtk_widget_set_margin_end(content, 20);
        gtk_widget_set_margin_top(content, 8);
        gtk_widget_set_margin_bottom(content, 12);

        /* 加载 spinner — 初始隐藏 */
        GtkWidget* spinner = gtk_spinner_new();
        gtk_widget_set_size_request(spinner, 22, 22);
        gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(spinner, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(spinner, 8);
        gtk_widget_set_margin_bottom(spinner, 8);
        gtk_widget_set_visible(spinner, FALSE);
        gtk_box_append(GTK_BOX(content), spinner);
        g_object_set_data(G_OBJECT(card), "loading-spinner", spinner);

        /* 版本列表容器 — 初始为空 */
        GtkWidget* ver_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_visible(ver_list, FALSE);
        gtk_box_append(GTK_BOX(content), ver_list);
        g_object_set_data(G_OBJECT(card), "version-list", ver_list);

        /* 占位提示 — 初始显示 */
        GtkWidget* placeholder = gtk_label_new("展开以加载版本列表");
        gtk_widget_set_opacity(placeholder, 0.45);
        gtk_widget_set_halign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(placeholder, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(placeholder, 8);
        gtk_widget_set_margin_bottom(placeholder, 8);
        gtk_box_append(GTK_BOX(content), placeholder);
        g_object_set_data(G_OBJECT(card), "placeholder", placeholder);

        gtk_revealer_set_child(GTK_REVEALER(revealer), content);
    }
    gtk_box_append(GTK_BOX(card), revealer);

    /* ── 展开/折叠 + 懒加载回调 ── */
    g_signal_connect(header, "clicked",
        G_CALLBACK(+[](GtkWidget* btn, gpointer data) {
            GtkWidget* rv = static_cast<GtkWidget*>(data);
            gboolean vis = gtk_revealer_get_reveal_child(GTK_REVEALER(rv));

            /* 找到卡片 (card = parent of parent: revealer → card) */
            GtkWidget* card = gtk_widget_get_parent(rv);

            int state = GPOINTER_TO_INT(
                g_object_get_data(G_OBJECT(card), "fetch-state"));

            /* 懒加载: 首次展开时触发异步请求 */
            if (!vis && state == 0) {
                g_object_set_data(G_OBJECT(card), "fetch-state",
                                  GINT_TO_POINTER(1));

                GtkWidget* spinner = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(card), "loading-spinner"));
                GtkWidget* pl = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(card), "placeholder"));

                if (spinner) {
                    gtk_spinner_start(GTK_SPINNER(spinner));
                    gtk_widget_set_visible(spinner, TRUE);
                }
                if (pl) gtk_widget_set_visible(pl, FALSE);

                const char* ldr_name = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(card), "loader-name"));
                const char* mc_ver = static_cast<const char*>(
                    g_object_get_data(G_OBJECT(card), "mc-version"));

                if (ldr_name && mc_ver) {
                    /* ── 通用填充 lambda — 复用于 LoaderFetcher 和 ResourceFetcher ── */
                    auto populate = [card](mc::LoaderVersionList list) {
                        g_object_set_data(G_OBJECT(card), "fetch-state",
                                          GINT_TO_POINTER(2));

                        GtkWidget* spinner = static_cast<GtkWidget*>(
                            g_object_get_data(G_OBJECT(card), "loading-spinner"));
                        GtkWidget* ver_list = static_cast<GtkWidget*>(
                            g_object_get_data(G_OBJECT(card), "version-list"));
                        GtkWidget* pl = static_cast<GtkWidget*>(
                            g_object_get_data(G_OBJECT(card), "placeholder"));

                        if (spinner) {
                            gtk_spinner_stop(GTK_SPINNER(spinner));
                            gtk_widget_set_visible(spinner, FALSE);
                        }

                        /* 清空旧版本行 */
                        GtkWidget* child;
                        while ((child = gtk_widget_get_first_child(ver_list)))
                            gtk_box_remove(GTK_BOX(ver_list), child);

                        if (!list.loaded || list.versions.empty()) {
                            if (pl) {
                                gtk_label_set_text(GTK_LABEL(pl),
                                    list.error.empty() ? "暂无可选版本"
                                                       : list.error.c_str());
                                gtk_widget_set_visible(pl, TRUE);
                            }
                            gtk_widget_set_visible(ver_list, FALSE);
                            return;
                        }

                        if (pl) gtk_widget_set_visible(pl, FALSE);
                        gtk_widget_set_visible(ver_list, TRUE);

                        /* 填充版本行 */
                        for (size_t i = 0; i < list.versions.size(); i++) {
                            auto& v = list.versions[i];

                            GtkWidget* row = gtk_button_new();
                            gtk_button_set_has_frame(GTK_BUTTON(row), FALSE);
                            gtk_widget_add_css_class(row, "flat");
                            gtk_widget_add_css_class(row, "loader-ver-row");
                            gtk_widget_set_halign(row, GTK_ALIGN_FILL);

                            GtkWidget* rhbox = gtk_box_new(
                                GTK_ORIENTATION_HORIZONTAL, 10);
                            gtk_widget_set_margin_start(rhbox, 4);
                            gtk_widget_set_margin_end(rhbox, 4);
                            gtk_widget_set_margin_top(rhbox, 3);
                            gtk_widget_set_margin_bottom(rhbox, 3);

                            GtkWidget* ver_lbl = gtk_label_new(
                                v.version.c_str());
                            gtk_label_set_xalign(GTK_LABEL(ver_lbl), 0.0f);
                            gtk_widget_set_hexpand(ver_lbl, TRUE);
                            gtk_widget_set_valign(ver_lbl, GTK_ALIGN_CENTER);
                            gtk_box_append(GTK_BOX(rhbox), ver_lbl);
                            {
                                PangoAttrList* a = pango_attr_list_new();
                                pango_attr_list_insert(a,
                                    pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                                gtk_label_set_attributes(GTK_LABEL(ver_lbl), a);
                                pango_attr_list_unref(a);
                            }

                            if (!v.branch.empty()) {
                                GtkWidget* branch_lbl = gtk_label_new(
                                    v.branch.c_str());
                                gtk_widget_add_css_class(branch_lbl, "resource-tag");
                                gtk_widget_set_valign(branch_lbl, GTK_ALIGN_CENTER);
                                gtk_box_append(GTK_BOX(rhbox), branch_lbl);
                            }

                            if (!v.date.empty()) {
                                GtkWidget* date_lbl = gtk_label_new(
                                    v.date.c_str());
                                gtk_widget_set_opacity(date_lbl, 0.5);
                                gtk_widget_set_valign(date_lbl, GTK_ALIGN_CENTER);
                                gtk_box_append(GTK_BOX(rhbox), date_lbl);
                                {
                                    PangoAttrList* a = pango_attr_list_new();
                                    pango_attr_list_insert(a,
                                        pango_attr_size_new(9 * PANGO_SCALE));
                                    gtk_label_set_attributes(
                                        GTK_LABEL(date_lbl), a);
                                    pango_attr_list_unref(a);
                                }
                            }

                            gtk_button_set_child(GTK_BUTTON(row), rhbox);
                            gtk_box_append(GTK_BOX(ver_list), row);

                            g_object_set_data_full(G_OBJECT(row),
                                "loader-version", g_strdup(v.version.c_str()), g_free);
                            g_object_set_data_full(G_OBJECT(row),
                                "loader-url", g_strdup(v.url.c_str()), g_free);
                            g_object_set_data_full(G_OBJECT(row),
                                "loader-branch", g_strdup(v.branch.c_str()), g_free);

                            g_signal_connect(row, "clicked",
                                (GCallback)(+[](GtkWidget* r, gpointer cd) {
                                    GtkWidget* crd = static_cast<GtkWidget*>(cd);
                                    const char* ver = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(r), "loader-version"));
                                    const char* url = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(r), "loader-url"));
                                    const char* ldr_name = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(crd), "loader-name"));

                                    GtkWidget* bx = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(crd), "detail-box"));
                                    if (!bx) return;

                                    /* 检查是否重复点击同一项 → 取消选择 */
                                    const char* prev = static_cast<const char*>(
                                        g_object_get_data(G_OBJECT(crd), "selected-version"));
                                    if (prev && std::strcmp(prev, ver) == 0) {
                                        /* 移除选中样式 */
                                        GtkWidget* prev_row = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(crd), "selected-row"));
                                        if (prev_row)
                                            gtk_widget_remove_css_class(prev_row, "loader-ver-row-selected");
                                        g_object_set_data(G_OBJECT(crd), "selected-row", nullptr);

                                        /* 清空此卡片的选择 */
                                        GtkWidget* sl = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(crd), "sel-lbl"));
                                        if (sl) {
                                            gtk_label_set_text(GTK_LABEL(sl), "");
                                            gtk_widget_set_visible(sl, FALSE);
                                        }
                                        g_object_set_data(G_OBJECT(crd), "selected-version", nullptr);
                                        g_object_set_data(G_OBJECT(crd), "selected-url", nullptr);

                                        /* Fabric API 取消选择: 不影响 loader 卡片和主名称 */
                                        if (ldr_name && std::strcmp(ldr_name, "fabric-api") == 0)
                                            return;

                                        /* 恢复图标为版本类型默认值 */
                                        const char* ver_type = static_cast<const char*>(
                                            g_object_get_data(G_OBJECT(bx), "version-type"));
                                        update_detail_logo(bx, icon_for_version_type(
                                            ver_type ? ver_type : ""));

                                        /* 恢复 loader 卡片 (尊重版本可见性) */
                                        const char* mc_ver = static_cast<const char*>(
                                            g_object_get_data(G_OBJECT(crd), "mc-version"));
                                        int ma = 0, mi = 0;
                                        if (mc_ver) std::sscanf(mc_ver, "%d.%d", &ma, &mi);
                                        int dp = ma * 100 + mi;

                                        struct LdrVis { const char* key; bool vis; };
                                        const LdrVis vis_map[] = {
                                            {"card-optifine",   true},
                                            {"card-forge",      true},
                                            {"card-fabric",     dp >= 114},
                                            {"card-quilt",      dp >= 119},
                                            {"card-neoforge",   dp >= 120},
                                            {"card-liteloader", ma == 1 && mi <= 12},
                                            {"card-cleanroom",  mc_ver && std::strstr(mc_ver, "1.12.2")},
                                        };
                                        for (auto& v : vis_map) {
                                            GtkWidget* c = static_cast<GtkWidget*>(
                                                g_object_get_data(G_OBJECT(bx), v.key));
                                            if (c) gtk_widget_set_visible(c, v.vis);
                                        }
                                        /* 隐藏 Fabric API 并清空其选择 */
                                        GtkWidget* fapi = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(bx), "card-fabric-api"));
                                        if (fapi) {
                                            gtk_widget_set_visible(fapi, FALSE);
                                            GtkWidget* fapi_sl = static_cast<GtkWidget*>(
                                                g_object_get_data(G_OBJECT(fapi), "sel-lbl"));
                                            if (fapi_sl) {
                                                gtk_label_set_text(GTK_LABEL(fapi_sl), "");
                                                gtk_widget_set_visible(fapi_sl, FALSE);
                                            }
                                            g_object_set_data(G_OBJECT(fapi), "selected-version", nullptr);
                                            g_object_set_data(G_OBJECT(fapi), "selected-url", nullptr);
                                        }

                                        /* 重置主输入框为 MC 版本号 */
                                        GtkWidget* info_card = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(bx), "detail-info-card"));
                                        if (info_card && mc_ver) {
                                            GtkWidget* entry = static_cast<GtkWidget*>(
                                                g_object_get_data(G_OBJECT(info_card), "detail-entry"));
                                            if (entry)
                                                gtk_editable_set_text(GTK_EDITABLE(entry), mc_ver);
                                        }
                                        return;
                                    }

                                    /* 切换选中样式 */
                                    GtkWidget* prev_row = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(crd), "selected-row"));
                                    if (prev_row)
                                        gtk_widget_remove_css_class(prev_row, "loader-ver-row-selected");
                                    gtk_widget_add_css_class(r, "loader-ver-row-selected");
                                    g_object_set_data(G_OBJECT(crd), "selected-row", r);

                                    /* 更新卡片标题副标题 */
                                    GtkWidget* sl = static_cast<GtkWidget*>(
                                        g_object_get_data(G_OBJECT(crd), "sel-lbl"));
                                    if (sl) {
                                        gtk_label_set_text(GTK_LABEL(sl), ver);
                                        gtk_widget_set_visible(sl, TRUE);
                                    }

                                    /* 切换图标为 Loader 图标 */
                                    const char* ldr_icon = icon_for_loader(ldr_name);
                                    if (ldr_icon)
                                        update_detail_logo(bx, ldr_icon);

                                    /* 更新主输入框: {MC版本}-{加载器名称}{加载器版本} */
                                    if (ldr_name && std::strcmp(ldr_name, "fabric-api") != 0) {
                                        /* 映射 loader_name → 显示名 */
                                        const char* disp = ldr_name;
                                        if (std::strcmp(ldr_name, "optifine") == 0)   disp = "OptiFine";
                                        else if (std::strcmp(ldr_name, "forge") == 0)  disp = "Forge";
                                        else if (std::strcmp(ldr_name, "fabric") == 0) disp = "Fabric";
                                        else if (std::strcmp(ldr_name, "quilt") == 0)  disp = "Quilt";
                                        else if (std::strcmp(ldr_name, "neoforge") == 0)  disp = "NeoForge";
                                        else if (std::strcmp(ldr_name, "liteloader") == 0) disp = "LiteLoader";
                                        else if (std::strcmp(ldr_name, "cleanroom") == 0) disp = "Cleanroom";

                                        const char* mc_ver = static_cast<const char*>(
                                            g_object_get_data(G_OBJECT(crd), "mc-version"));
                                        std::string name = mc_ver ? mc_ver : "";
                                        name += "-";
                                        name += disp;
                                        name += " ";
                                        name += ver;

                                        GtkWidget* info_card = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(bx), "detail-info-card"));
                                        if (info_card) {
                                            GtkWidget* entry = static_cast<GtkWidget*>(
                                                g_object_get_data(G_OBJECT(info_card), "detail-entry"));
                                            if (entry)
                                                gtk_editable_set_text(GTK_EDITABLE(entry), name.c_str());
                                        }
                                    }

                                    g_object_set_data_full(G_OBJECT(crd),
                                        "selected-version", g_strdup(ver), g_free);
                                    g_object_set_data_full(G_OBJECT(crd),
                                        "selected-url", g_strdup(url), g_free);

                                    /* ── 互斥逻辑: 仅 Mod Loader 触发 (Fabric API 不参与) ── */
                                    if (ldr_name && std::strcmp(ldr_name, "fabric-api") != 0) {
                                        const char* all_ldrs[] = {
                                            "card-optifine", "card-forge", "card-fabric",
                                            "card-quilt", "card-neoforge",
                                            "card-liteloader", "card-cleanroom"
                                        };
                                        for (auto key : all_ldrs) {
                                            GtkWidget* c = static_cast<GtkWidget*>(
                                                g_object_get_data(G_OBJECT(bx), key));
                                            if (!c || c == crd) continue;
                                            gtk_widget_set_visible(c, FALSE);
                                        }

                                        /* Fabric 或 Quilt 选中时显示 Fabric API */
                                        bool show_fapi = (std::strcmp(ldr_name, "fabric") == 0 ||
                                                         std::strcmp(ldr_name, "quilt") == 0);
                                        GtkWidget* fapi = static_cast<GtkWidget*>(
                                            g_object_get_data(G_OBJECT(bx), "card-fabric-api"));
                                        if (fapi) {
                                            gtk_widget_set_visible(fapi, show_fapi);
                                            /* 若之前选中了 Fabric API 但当前 loader 不是 Fabric/Quilt，清空其选择 */
                                            if (!show_fapi) {
                                                GtkWidget* fapi_sl = static_cast<GtkWidget*>(
                                                    g_object_get_data(G_OBJECT(fapi), "sel-lbl"));
                                                if (fapi_sl) {
                                                    gtk_label_set_text(GTK_LABEL(fapi_sl), "");
                                                    gtk_widget_set_visible(fapi_sl, FALSE);
                                                }
                                                g_object_set_data(G_OBJECT(fapi), "selected-version", nullptr);
                                                g_object_set_data(G_OBJECT(fapi), "selected-url", nullptr);
                                            }
                                        }
                                    }
                                }), card);
                        }
                    };

                    /* Fabric API 特殊处理: 通过 Modrinth API 获取 */
                    if (std::strcmp(ldr_name, "fabric-api") == 0) {
                        std::string mc_ver_str = mc_ver;
                        resource::fetch_project_files("P7dR8mSH", false,
                            [populate, mc_ver_str](std::vector<resource::CompFile> files) {
                                mc::LoaderVersionList list;
                                list.loader_name = "Fabric API";
                                list.loaded = true;
                                for (auto& f : files) {
                                    /* 筛选匹配 MC 版本的文件 */
                                    bool matches = false;
                                    for (auto& gv : f.game_versions) {
                                        if (gv == mc_ver_str) { matches = true; break; }
                                    }
                                    if (!matches) continue;

                                    mc::LoaderVersion lv;
                                    lv.version = f.file_name;
                                    lv.date    = f.date;
                                    if (lv.date.size() >= 10) lv.date = lv.date.substr(0, 10);
                                    lv.url     = f.download_url;
                                    list.versions.push_back(std::move(lv));
                                }
                                if (list.versions.empty()) {
                                    list.loaded = false;
                                    list.error = "该版本暂无 Fabric API";
                                }
                                populate(std::move(list));
                            });
                    } else {
                        /* 其他加载器: 通过 LoaderFetcher 获取 */
                        mc::fetch_loader_versions(ldr_name, mc_ver, populate);
                    }
                }
            }

            /* 切换展开/折叠 */
            gtk_revealer_set_reveal_child(GTK_REVEALER(rv), !vis);

            /* 旋转箭头 */
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

    return card;
}

/* ── 重置卡片为初始状态 (navigate 时调用) ── */
void reset_loader_card(GtkWidget* card)
{
    g_object_set_data(G_OBJECT(card), "fetch-state", GINT_TO_POINTER(0));

    GtkWidget* spinner = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(card), "loading-spinner"));
    GtkWidget* ver_list = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(card), "version-list"));
    GtkWidget* pl = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(card), "placeholder"));
    GtkWidget* sl = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(card), "sel-lbl"));

    if (spinner) {
        gtk_spinner_stop(GTK_SPINNER(spinner));
        gtk_widget_set_visible(spinner, FALSE);
    }
    if (ver_list) {
        GtkWidget* child;
        while ((child = gtk_widget_get_first_child(ver_list)))
            gtk_box_remove(GTK_BOX(ver_list), child);
        gtk_widget_set_visible(ver_list, FALSE);
    }
    if (pl && GTK_IS_LABEL(pl)) {
        gtk_label_set_text(GTK_LABEL(pl), "展开以加载版本列表");
        gtk_widget_set_visible(pl, TRUE);
    }
    if (sl && GTK_IS_LABEL(sl)) {
        gtk_label_set_text(GTK_LABEL(sl), "");
        gtk_widget_set_visible(sl, FALSE);
    }

    g_object_set_data(G_OBJECT(card), "selected-version", nullptr);
    g_object_set_data(G_OBJECT(card), "selected-url", nullptr);
    g_object_set_data(G_OBJECT(card), "selected-row", nullptr);
}

}  // anonymous namespace

/* ═══════════════════════════════════════════════════════════════════════
 * build_mc_version_detail_page
 * ═══════════════════════════════════════════════════════════════════════ */

GtkWidget* build_mc_version_detail_page()
{
    /* ── 外层 Overlay: 内容 + 浮动安装按钮 ── */
    GtkWidget* overlay = gtk_overlay_new();

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(
        GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_scrolled_window_set_propagate_natural_height(
        GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), scroll);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(box, 40);
    gtk_widget_set_margin_end(box, 40);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);

    /* 把 box 存到 overlay 上，供 navigate 直接查找 */
    g_object_set_data(G_OBJECT(overlay), "detail-box", box);

    /* ═══════════════════════════════════════════════════════════════════
     * 顶部信息卡片: [版本图标 64px] [名称输入框]
     * ═══════════════════════════════════════════════════════════════════ */
    {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        gtk_widget_add_css_class(card, "card");

        /* 版本图标 */
        GtkWidget* logo = icon::load_block("Minecraft", 64);
        gtk_widget_set_valign(logo, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(logo, GTK_ALIGN_START);
        gtk_widget_set_size_request(logo, 64, 64);
        gtk_widget_set_hexpand(logo, FALSE);
        gtk_widget_set_vexpand(logo, FALSE);
        gtk_widget_set_margin_top(logo, 2);
        gtk_box_append(GTK_BOX(card), logo);
        g_object_set_data(G_OBJECT(card), "detail-logo", logo);

        /* 名称输入框 */
        GtkWidget* entry = gtk_entry_new();
        gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_widget_set_margin_start(entry, 8);
        gtk_widget_set_margin_end(entry, 8);
        gtk_entry_set_max_length(GTK_ENTRY(entry), 70);
        gtk_box_append(GTK_BOX(card), entry);
        g_object_set_data(G_OBJECT(card), "detail-entry", entry);
        {
            PangoAttrList* attrs = pango_attr_list_new();
            pango_attr_list_insert(attrs,
                pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
            pango_attr_list_insert(attrs,
                pango_attr_size_new(14 * PANGO_SCALE));
            gtk_entry_set_attributes(GTK_ENTRY(entry), attrs);
            pango_attr_list_unref(attrs);
        }

        gtk_box_append(GTK_BOX(box), card);
        g_object_set_data(G_OBJECT(box), "detail-info-card", card);
    }

    /* ═══════════════════════════════════════════════════════════════════
     * 加载器卡片列表 — 传入 box 供选中回调更新 entry
     * ═══════════════════════════════════════════════════════════════════ */

    GtkWidget* card_optifine = build_loader_card(
        "OptiFine", "optifine", "OptiFine", "gauge", true, box);
    gtk_box_append(GTK_BOX(box), card_optifine);
    g_object_set_data(G_OBJECT(box), "card-optifine", card_optifine);

    GtkWidget* card_forge = build_loader_card(
        "Forge", "forge", "Anvil", "anvil", true, box);
    gtk_box_append(GTK_BOX(box), card_forge);
    g_object_set_data(G_OBJECT(box), "card-forge", card_forge);

    GtkWidget* card_fabric = build_loader_card(
        "Fabric", "fabric", "Fabric", "scroll", true, box);
    gtk_box_append(GTK_BOX(box), card_fabric);
    g_object_set_data(G_OBJECT(box), "card-fabric", card_fabric);

    GtkWidget* card_quilt = build_loader_card(
        "Quilt", "quilt", "Quilt", "grid-2x2", true, box);
    gtk_box_append(GTK_BOX(box), card_quilt);
    g_object_set_data(G_OBJECT(box), "card-quilt", card_quilt);

    GtkWidget* card_neoforge = build_loader_card(
        "NeoForge", "neoforge", "NeoForge", "cat", true, box);
    gtk_box_append(GTK_BOX(box), card_neoforge);
    g_object_set_data(G_OBJECT(box), "card-neoforge", card_neoforge);

    GtkWidget* card_fabric_api = build_loader_card(
        "Fabric API", "fabric-api", "Fabric", "scroll", false, box);
    gtk_box_append(GTK_BOX(box), card_fabric_api);
    g_object_set_data(G_OBJECT(box), "card-fabric-api", card_fabric_api);

    GtkWidget* card_liteloader = build_loader_card(
        "LiteLoader", "liteloader", "Egg", "egg", true, box);
    gtk_box_append(GTK_BOX(box), card_liteloader);
    g_object_set_data(G_OBJECT(box), "card-liteloader", card_liteloader);

    GtkWidget* card_cleanroom = build_loader_card(
        "Cleanroom", "cleanroom", "Cleanroom", "flask-conical", false, box);
    gtk_box_append(GTK_BOX(box), card_cleanroom);
    g_object_set_data(G_OBJECT(box), "card-cleanroom", card_cleanroom);

    /* ═══════════════════════════════════════════════════════════════════
     * 浮动安装按钮 (右下角圆形 FAB，对标启动页)
     * ═══════════════════════════════════════════════════════════════════ */
    {
        GtkWidget* fab = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(fab), FALSE);
        gtk_button_set_child(GTK_BUTTON(fab), icon::load("chevron-right-light", 20));
        gtk_widget_set_size_request(fab, 48, 48);
        gtk_widget_add_css_class(fab, "fab");
        gtk_widget_add_css_class(fab, "fab-blue");
        gtk_widget_set_tooltip_text(fab, "安装");
        gtk_widget_set_halign(fab, GTK_ALIGN_END);
        gtk_widget_set_valign(fab, GTK_ALIGN_END);
        gtk_widget_set_margin_end(fab, 24);
        gtk_widget_set_margin_bottom(fab, 24);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), fab);

        g_signal_connect(fab, "clicked",
            G_CALLBACK(+[](GtkButton*, gpointer) {
                LOG_INFO("McVersionDetail: install button clicked (not yet implemented)");
            }), nullptr);
    }

    return overlay;
}

/* ═══════════════════════════════════════════════════════════════════════
 * navigate_to_mc_version_detail
 * ═══════════════════════════════════════════════════════════════════════ */

void navigate_to_mc_version_detail(GtkWidget*        trigger_widget,
                                   const std::string& version_id,
                                   const std::string& version_type)
{
    auto& nav = NavigationController::instance();
    if (!nav.window()) return;

    AdwViewStack* main_stk = nav.main_stack();
    if (!main_stk) return;

    GtkWidget* dl_page = adw_view_stack_get_child_by_name(main_stk, "download");
    if (!dl_page) return;

    GtkWidget* dl_stk = nav.dl_stack();
    if (!dl_stk) return;

    /* 保存当前页面名称 (供返回时恢复) */
    const char* cur_name = gtk_stack_get_visible_child_name(GTK_STACK(dl_stk));
    if (cur_name && strcmp(cur_name, "mc-detail") != 0) {
        g_object_set_data_full(G_OBJECT(dl_page), "prev-page",
                               g_strdup(cur_name), g_free);
    }

    /* 委托 NavigationController 处理 header/sidebar 切换 */
    nav.enter_detail_view(version_id);

    /* 切换到 MC 详情页 */
    gtk_stack_set_visible_child_name(GTK_STACK(dl_stk), "mc-detail");

    /* 填充详情页数据 */
    GtkWidget* detail = gtk_stack_get_child_by_name(GTK_STACK(dl_stk), "mc-detail");
    if (!detail) { LOG_ERR("navigate_mc_detail: detail child not found"); return; }

    GtkWidget* box = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(detail), "detail-box"));
    if (!box) { LOG_ERR("navigate_mc_detail: detail-box not found"); return; }

    /* 存储当前 MC 版本号 + 类型 */
    GtkWidget* overlay = static_cast<GtkWidget*>(detail);
    g_object_set_data_full(G_OBJECT(overlay), "mc-version",
                           g_strdup(version_id.c_str()), g_free);
    g_object_set_data_full(G_OBJECT(box), "version-type",
                           g_strdup(version_type.c_str()), g_free);

    /* 设置初始图标 (根据版本类型) */
    update_detail_logo(box, icon_for_version_type(version_type));

    /* 设置输入框默认值为版本号 */
    GtkWidget* info_card = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(box), "detail-info-card"));
    if (info_card) {
        GtkWidget* entry = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(info_card), "detail-entry"));
        if (entry)
            gtk_editable_set_text(GTK_EDITABLE(entry), version_id.c_str());
    }

    /* ── 根据 MC 版本控制卡片可见性 (对标 PCL-CE PageDownloadInstall) ── */
    {
        /* Parse major.minor from version string for comparison */
        int major = 0, minor = 0;
        std::sscanf(version_id.c_str(), "%d.%d", &major, &minor);
        int drop = major * 100 + minor;  // rough: 1.12 → 112, 1.21 → 121

        /* LiteLoader: 仅 1.12.x 及以下 (drop < 113) */
        GtkWidget* card_ll = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "card-liteloader"));
        if (card_ll)
            gtk_widget_set_visible(card_ll, major == 1 && minor <= 12);

        /* Cleanroom: 仅 1.12.2 */
        GtkWidget* card_cr = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "card-cleanroom"));
        if (card_cr)
            gtk_widget_set_visible(card_cr, version_id.find("1.12.2") != std::string::npos);

        /* Fabric: 1.14+ (drop >= 114) */
        GtkWidget* card_fb = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "card-fabric"));
        if (card_fb)
            gtk_widget_set_visible(card_fb, drop >= 114);

        /* Quilt: 1.19+ (drop >= 119) */
        GtkWidget* card_qu = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "card-quilt"));
        if (card_qu)
            gtk_widget_set_visible(card_qu, drop >= 119);

        /* NeoForge: 1.20.1+ (drop >= 120) */
        GtkWidget* card_nf = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), "card-neoforge"));
        if (card_nf)
            gtk_widget_set_visible(card_nf, drop >= 120);
    }

    /* ── 重置所有卡片为初始状态 (清空旧版本 + 重置 lazy-load 标记) ── */
    const char* card_keys[] = {
        "card-optifine", "card-forge", "card-fabric", "card-quilt",
        "card-neoforge", "card-fabric-api", "card-liteloader", "card-cleanroom"
    };
    for (auto key : card_keys) {
        GtkWidget* card = static_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(box), key));
        if (card) {
            reset_loader_card(card);
            /* 设置当前 MC 版本号到卡片 */
            g_object_set_data_full(G_OBJECT(card), "mc-version",
                                   g_strdup(version_id.c_str()), g_free);
            /* 折叠 revealer */
            GtkWidget* header = gtk_widget_get_first_child(card);
            if (header) {
                GtkWidget* revealer = gtk_widget_get_next_sibling(header);
                if (revealer && GTK_IS_REVEALER(revealer))
                    gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), FALSE);
            }
        }
    }

    LOG_INFO("navigate_mc_detail: version=%s", version_id.c_str());
}

}  // namespace pcl
