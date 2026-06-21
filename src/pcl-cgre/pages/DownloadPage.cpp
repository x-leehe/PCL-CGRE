#include "pages/DownloadPage.hpp"
#include "app/NavigationController.hpp"
#include "widgets/ResourceItem.hpp"
#include "pages/ResourceDetailPage.hpp"
#include "pages/McVersionDetailPage.hpp"
#include "pages/LaunchPage.hpp"
#include "core/Colors.hpp"
#include "util/IconHelper.hpp"
#include "network/McVersionFetcher.hpp"
#include "network/ResourceFetcher.hpp"
#include "core/Log.hpp"
#include "pclcore/pclcore.hpp"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <adwaita.h>

namespace pcl {


/* ── Minecraft 版本页面的 widget 指针集 (异步回调直接使用) ──────────────── */
struct McWidgets {
    // "最新版本" 卡片中的两个子项 — [release, snapshot]
    GtkWidget* latest_ver[2];
    GtkWidget* latest_note[2];

    // 4 个可展开分类的 GtkListBox (在 GtkRevealer 内部)
    GtkWidget* group_list[4];

    // 4 个分类 header 的 label (用于显示计数)
    GtkWidget* group_header_label[4];

    // 加载 spinner + 其父容器 (直接引用, 避免 widget tree 遍历)
    GtkWidget* load_spinner;
    GtkWidget* spinner_parent;   // GtkBox header of "最新版本" card

    // 是否已经发起过 fetch
    bool fetch_started;
};

/* ── 触发版本列表拉取 (由导航回调调用) ────────────────────────────────── */
void do_mc_fetch(McWidgets* w, GtkWidget* mc_scroll_w);  // forward

/* ── 资源搜索页面的 widget 指针集 ────────────────────────────────────── */
struct ResourceWidgets {
    GtkWidget* search_entry;   // GtkEntry inside build_search_box
    GtkWidget* search_btn;     // "搜索" 按钮
    GtkWidget* source_dd;      // 来源 dropdown
    GtkWidget* version_dd;     // 版本 dropdown
    GtkWidget* category_dd;    // 分类 dropdown (如 科技/魔法/冒险 …)
    GtkWidget* loader_dd;      // 加载器 dropdown (Forge/Fabric/…)
    GtkWidget* sort_dd;        // 排序方式 dropdown
    GtkWidget* result_list;    // GtkListBox
    GtkWidget* empty_card;     // 空态卡片
    GtkWidget* spinner;        // GtkSpinner — 居中在结果区
    GtkWidget* hist_revealer;  // GtkRevealer — 历史记录
    GtkWidget* hist_list;      // GtkListBox inside revealer
    std::vector<std::string> search_history;  // 最近 ≤MAX_SEARCH_HISTORY 条
    resource::ProjectType project_type;  // 资源类别
    bool default_loaded = false;         // 是否已触发过默认浏览加载
    bool history_triggers_search = true; // 点击历史项是否触发搜索 (收藏页为 false)
    unsigned search_generation = 0;      // 每次搜索/翻页自增; 异步回调据此丢弃过期结果

    // 分页
    int current_page = 0;              // 0-based
    int total_hits   = 0;
    GtkWidget* page_nav   = nullptr;   // footer bar
    GtkWidget* page_label = nullptr;   // "第 X 页"
    GtkWidget* btn_first  = nullptr;
    GtkWidget* btn_prev   = nullptr;
    GtkWidget* btn_next   = nullptr;
    GtkWidget* btn_last   = nullptr;
    static constexpr int page_size = 20;
    static constexpr size_t MAX_SEARCH_HISTORY = 8;  // 搜索历史保留条数
};

/* ── 触发资源搜索 & 翻页 ─────────────────────────────────────────────── */
void do_resource_search(ResourceWidgets* rw);  // forward
static void do_page_nav(ResourceWidgets* rw, int new_page);

/* ── 将 category 字符串映射到 ProjectType ────────────────────────────── */
resource::ProjectType category_to_project_type(const char* cat)
{
    if (strcmp(cat, "Mod") == 0)     return resource::ProjectType::Mod;
    if (strcmp(cat, "整合包") == 0)  return resource::ProjectType::Modpack;
    if (strcmp(cat, "数据包") == 0)  return resource::ProjectType::Datapack;
    if (strcmp(cat, "资源包") == 0)  return resource::ProjectType::ResourcePack;
    if (strcmp(cat, "光影") == 0)    return resource::ProjectType::Shader;
    if (strcmp(cat, "世界") == 0)    return resource::ProjectType::World;
    return resource::ProjectType::Mod;
}

GtkWidget* build_placeholder_page(const char* lucide_icon_name,
                                  const char* description)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "placeholder");
    gtk_widget_set_vexpand(box, TRUE);

    GtkWidget* icon = icon::load(lucide_icon_name, 48);
    gtk_widget_set_opacity(icon, 0.35);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), icon);

    GtkWidget* label = gtk_label_new(description);
    gtk_widget_add_css_class(label, "ph-title");
    gtk_widget_set_margin_top(label, 16);
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), label);

    return box;
}

/* ============================================================================
 *  辅助: 创建一个带 label 的 GtkDropDown
 * ============================================================================ */
static GtkWidget* build_filter_dropdown(const char* label_text,
                                        const char* const* items,
                                        int n_items)
{
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_hexpand(hbox, TRUE);

    GtkWidget* lbl = gtk_label_new(label_text);
    gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(hbox), lbl);

    /* GtkStringList → GtkDropDown */
    GtkStringList* model = gtk_string_list_new(nullptr);
    for (int i = 0; i < n_items; i++)
        gtk_string_list_append(model, items[i]);

    GtkWidget* dd = gtk_drop_down_new(G_LIST_MODEL(model), nullptr);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), 0);
    gtk_widget_add_css_class(dd, "dl-filter-dropdown");
    gtk_widget_set_hexpand(dd, TRUE);
    gtk_widget_set_valign(dd, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(hbox), dd);

    return hbox;
}

/* ============================================================================
 *  辅助: 清空结果列表
 * ============================================================================ */
static void clear_result_list(GtkWidget* list)
{
    GtkWidget* child;
    while ((child = gtk_widget_get_first_child(list)))
        gtk_list_box_remove(GTK_LIST_BOX(list), child);
}

/* ============================================================================
 *  辅助: 资源类型对应的 Lucide 图标名 (预计算静态表)
 * ============================================================================ */
static const char* project_type_icon(resource::ProjectType t)
{
    static const char* const table[] = {
        "puzzle",       // Mod
        "package",      // Modpack
        "file-archive", // Datapack
        "layers",       // ResourcePack
        "sparkles",     // Shader
        "globe",        // World
    };
    auto idx = static_cast<int>(t);
    if (idx >= 0 && idx < static_cast<int>(sizeof(table)/sizeof(table[0])))
        return table[idx];
    return "package";
}

/* ============================================================================
 *  辅助: 从 UI 控件读取当前筛选条件 (do_resource_search / do_page_nav 共用)
 * ============================================================================ */
struct SearchFilters {
    std::string              query;
    resource::Source         source = resource::Source::All;
    std::string              version;
    resource::SortType       sort   = resource::SortType::Default;
    resource::CompLoaderType loader = resource::CompLoaderType::Any;
};

static SearchFilters read_search_filters(ResourceWidgets* rw)
{
    SearchFilters f;

    if (rw->search_entry) {
        const char* q = gtk_editable_get_text(GTK_EDITABLE(rw->search_entry));
        if (q) f.query = q;
    }

    if (rw->source_dd) {
        switch (gtk_drop_down_get_selected(GTK_DROP_DOWN(rw->source_dd))) {
            case 1: f.source = resource::Source::CurseForge; break;
            case 2: f.source = resource::Source::Modrinth;   break;
            default: f.source = resource::Source::All;       break;
        }
    }

    if (rw->version_dd) {
        guint idx = gtk_drop_down_get_selected(GTK_DROP_DOWN(rw->version_dd));
        if (idx > 0) {  // 0 = "全部版本"
            GListModel* model = gtk_drop_down_get_model(GTK_DROP_DOWN(rw->version_dd));
            if (model && GTK_IS_STRING_LIST(model)) {
                const char* v = gtk_string_list_get_string(GTK_STRING_LIST(model), idx);
                if (v) f.version = v;
            }
        }
    }

    if (rw->sort_dd) {
        switch (gtk_drop_down_get_selected(GTK_DROP_DOWN(rw->sort_dd))) {
            case 1: f.sort = resource::SortType::Relevance; break;
            case 2: f.sort = resource::SortType::Downloads; break;
            case 3: f.sort = resource::SortType::Follows;   break;
            case 4: f.sort = resource::SortType::Newest;    break;
            case 5: f.sort = resource::SortType::Updated;   break;
            default: f.sort = resource::SortType::Default;  break;
        }
    }

    if (rw->loader_dd) {
        switch (gtk_drop_down_get_selected(GTK_DROP_DOWN(rw->loader_dd))) {
            case 1: f.loader = resource::CompLoaderType::Forge;      break;
            case 2: f.loader = resource::CompLoaderType::Fabric;     break;
            case 3: f.loader = resource::CompLoaderType::Quilt;      break;
            case 4: f.loader = resource::CompLoaderType::NeoForge;   break;
            case 5: f.loader = resource::CompLoaderType::LiteLoader; break;
            default: f.loader = resource::CompLoaderType::Any;       break;
        }
    }

    return f;
}

/* ============================================================================
 *  辅助: 将下载到的图标字节应用到结果列表第 index 行的 logo 控件
 *  (do_resource_search / do_page_nav 的图标回调共用)
 * ============================================================================ */
static void apply_icon_to_row(GtkListBox* list, int index,
                              const std::vector<uint8_t>& data)
{
    if (data.empty()) return;

    /* 定位第 index 个 GtkListBoxRow → 取其 child (真正的 row) */
    GtkWidget* list_row = gtk_widget_get_first_child(GTK_WIDGET(list));
    for (int i = 0; list_row && i < index; i++)
        list_row = gtk_widget_get_next_sibling(list_row);
    if (!list_row) return;

    GtkWidget* row = gtk_widget_get_first_child(list_row);
    if (!row) return;

    GtkWidget* logo = static_cast<GtkWidget*>(
        g_object_get_data(G_OBJECT(row), "logo"));
    if (!logo) return;

    GBytes* gb = g_bytes_new(data.data(), data.size());
    GdkTexture* tex = gdk_texture_new_from_bytes(gb, nullptr);
    if (tex) {
        if (GTK_IS_PICTURE(logo)) {
            gtk_picture_set_paintable(GTK_PICTURE(logo), GDK_PAINTABLE(tex));
        } else {
            /* fallback logo 是 GtkImage — 整个替换 */
            GtkWidget* new_logo = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
            gtk_image_set_pixel_size(GTK_IMAGE(new_logo), 50);
            gtk_widget_set_valign(new_logo, GTK_ALIGN_CENTER);
            gtk_widget_set_halign(new_logo, GTK_ALIGN_CENTER);
            gtk_widget_set_size_request(new_logo, 50, 50);
            gtk_widget_set_hexpand(new_logo, FALSE);
            gtk_widget_set_vexpand(new_logo, FALSE);
            gtk_widget_add_css_class(new_logo, "resource-logo");
            GtkWidget* parent = gtk_widget_get_parent(logo);
            if (parent) {
                gtk_box_remove(GTK_BOX(parent), logo);
                gtk_widget_insert_after(new_logo, parent, nullptr);
            }
            g_object_set_data(G_OBJECT(row), "logo", new_logo);
        }
        g_object_unref(tex);
    }
    g_bytes_unref(gb);
}

/* ============================================================================
 *  辅助: 用搜索结果填充结果列表 + 启动异步图标加载
 *  (do_resource_search / do_page_nav 共用)。@gen 用于丢弃过期的图标回调。
 * ============================================================================ */
static void populate_results(ResourceWidgets* rw, unsigned gen,
                             const resource::SearchResult& result)
{
    const char* icon_name = project_type_icon(rw->project_type);
    for (auto& hit : result.hits) {
        const char* src_str = (hit.source == resource::Source::Modrinth)
            ? "Modrinth" : "CurseForge";

        std::string dls  = resource::format_download_count(hit.download_count);
        std::string date = resource::format_date(hit.date_modified.c_str());

        ResourceItemData data;
        data.title          = hit.title;
        data.description    = hit.description;
        data.source         = src_str;
        data.version_range  = hit.version_range;
        data.date_modified  = hit.date_modified;
        data.icon_url       = hit.icon_url;
        data.download_count = hit.download_count;
        data.project_id     = hit.project_id;
        data.author         = hit.author;
        data.license_name   = hit.license_name;
        data.project_url    = hit.project_url;
        data.wiki_url       = hit.wiki_url;
        data.source_url     = hit.source_url;
        data.categories     = hit.categories;
        data.game_versions  = hit.game_versions;
        data.followers      = hit.followers;

        std::vector<const char*> tag_ptrs;
        tag_ptrs.reserve(hit.categories.size());
        for (auto& c : hit.categories)
            tag_ptrs.push_back(c.c_str());

        GtkWidget* item = build_resource_item(
            icon_name,
            hit.title.c_str(),
            hit.description.c_str(),
            hit.version_range.c_str(),
            dls.c_str(),
            date.c_str(),
            src_str,
            &data,
            nullptr,        // subtitle: author 现内联在标题行
            tag_ptrs);
        gtk_list_box_append(GTK_LIST_BOX(rw->result_list), item);
    }

    gtk_widget_set_visible(rw->empty_card, FALSE);
    gtk_widget_set_visible(gtk_widget_get_parent(rw->result_list), TRUE);

    /* 异步加载图标: 收集 URL, 后台下载, 逐个替换 fallback 图标 */
    std::vector<std::string> icon_urls;
    icon_urls.reserve(result.hits.size());
    int url_count = 0;
    for (auto& h : result.hits) {
        icon_urls.push_back(h.icon_url);
        if (!h.icon_url.empty()) url_count++;
    }
    LOG_INFO("MainWindow: loading %d icons (%zu hits total)",
             url_count, result.hits.size());

    resource::load_icons_async(std::move(icon_urls),
        [rw, gen](int index, std::vector<uint8_t> data) {
            if (gen != rw->search_generation) return;  // 过期搜索的图标, 丢弃
            apply_icon_to_row(GTK_LIST_BOX(rw->result_list), index, data);
        });
}

/* ============================================================================
 *  辅助: 搜索框 + chevron + 历史记录 Revealer (build_resource_search /
 *  build_favorites_page 共用)。控件追加到 @card; 点击历史项时仅当
 *  rw->history_triggers_search 为真才触发搜索。
 * ============================================================================ */
static void build_search_box_with_history(GtkWidget* card, ResourceWidgets* rw)
{
    GtkWidget* srow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_hexpand(srow, TRUE);
    gtk_widget_set_halign(srow, GTK_ALIGN_FILL);

    /* 搜索框 — 占满宽度 */
    GtkWidget* search = build_search_box();
    gtk_widget_set_hexpand(search, TRUE);
    gtk_box_append(GTK_BOX(srow), search);

    /* Extract GtkEntry, hide unused clear button (chevron replaces it) */
    {
        GtkWidget* child = gtk_widget_get_first_child(search);   // icon
        if (child) {
            child = gtk_widget_get_next_sibling(child);           // entry
            if (child && GTK_IS_EDITABLE(child))
                rw->search_entry = child;
            GtkWidget* clear = gtk_widget_get_next_sibling(child); // clear btn
            if (clear) gtk_widget_set_visible(clear, FALSE);
        }
    }

    /* Chevron 按钮 — 展开/折叠历史记录 */
    GtkWidget* chev_btn = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(chev_btn), FALSE);
    gtk_widget_add_css_class(chev_btn, "version-action-btn");
    GtkWidget* chev_icon = icon::load("chevron-down", 16);
    gtk_button_set_child(GTK_BUTTON(chev_btn), chev_icon);
    gtk_box_append(GTK_BOX(srow), chev_btn);

    gtk_box_append(GTK_BOX(card), srow);

    /* 历史记录 Revealer — 在搜索框下方 */
    rw->hist_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_duration(GTK_REVEALER(rw->hist_revealer), 150);
    {
        GtkWidget* hlist = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(hlist), GTK_SELECTION_NONE);
        gtk_widget_add_css_class(hlist, "boxed-list");
        gtk_revealer_set_child(GTK_REVEALER(rw->hist_revealer), hlist);
        rw->hist_list = hlist;
    }
    gtk_box_append(GTK_BOX(card), rw->hist_revealer);

    /* Chevron 点击 → 切换 revealer + 刷新历史列表 */
    g_signal_connect(chev_btn, "clicked",
        (GCallback)(+[](GtkWidget* btn, gpointer data) {
            auto* r = static_cast<ResourceWidgets*>(data);
            gboolean vis = gtk_revealer_get_reveal_child(
                GTK_REVEALER(r->hist_revealer));
            gtk_revealer_set_reveal_child(GTK_REVEALER(r->hist_revealer), !vis);

            /* 更新图标方向 */
            GtkWidget* old = gtk_button_get_child(GTK_BUTTON(btn));
            GtkWidget* icn = icon::load(vis ? "chevron-down" : "chevron-up", 16);
            if (old) gtk_box_remove(GTK_BOX(gtk_widget_get_parent(old)), old);
            gtk_button_set_child(GTK_BUTTON(btn), icn);

            /* 展开时刷新历史列表 */
            if (!vis) {
                GtkWidget* ch;
                while ((ch = gtk_widget_get_first_child(r->hist_list)))
                    gtk_list_box_remove(GTK_LIST_BOX(r->hist_list), ch);
                for (auto& h : r->search_history) {
                    GtkWidget* hrow = gtk_button_new();
                    gtk_button_set_has_frame(GTK_BUTTON(hrow), FALSE);
                    gtk_widget_add_css_class(hrow, "flat");
                    GtkWidget* hlbl = gtk_label_new(h.c_str());
                    gtk_label_set_xalign(GTK_LABEL(hlbl), 0.0f);
                    gtk_button_set_child(GTK_BUTTON(hrow), hlbl);
                    gtk_widget_set_margin_start(hrow, 8);
                    gtk_widget_set_margin_end(hrow, 8);

                    /* 点击历史 → 填入搜索框 (并按需触发搜索)。
                     * pair 随行控件销毁而释放 (GDestroyNotify), 不在点击时 delete,
                     * 避免重复点击时 use-after-free。 */
                    auto* user_pair =
                        new std::pair<ResourceWidgets*, std::string>(r, h);
                    g_signal_connect_data(hrow, "clicked",
                        (GCallback)(+[](GtkButton*, gpointer d) {
                            auto* pair = static_cast<
                                std::pair<ResourceWidgets*, std::string>*>(d);
                            gtk_editable_set_text(
                                GTK_EDITABLE(pair->first->search_entry),
                                pair->second.c_str());
                            gtk_revealer_set_reveal_child(
                                GTK_REVEALER(pair->first->hist_revealer), FALSE);
                            if (pair->first->history_triggers_search)
                                do_resource_search(pair->first);
                        }), user_pair,
                        (GClosureNotify)(+[](gpointer d, GClosure*) {
                            delete static_cast<
                                std::pair<ResourceWidgets*, std::string>*>(d);
                        }), (GConnectFlags)0);

                    gtk_list_box_append(GTK_LIST_BOX(r->hist_list), hrow);
                }
            }
        }), rw);
}

/* ============================================================================
 * build_resource_search — Mod / 整合包 / 数据包 … 共用搜索页
 *   对标 PCL-CE PageSelectRight: 卡片内搜索框 + 筛选 + 空态提示 + 卡片列表区
 * ============================================================================ */
static GtkWidget* build_resource_search(const char* category_title,
                                         ResourceWidgets* rw)
{
    (void) category_title;  // used when populating results later

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);

    /* ── 搜索卡片 (搜索框 + 下拉筛选 + 按钮) ── */
    {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(card, "card");
        gtk_widget_add_css_class(card, "search-card");
        gtk_widget_set_halign(card, GTK_ALIGN_FILL);

        /* 搜索框行 + chevron + 历史记录 Revealer */
        build_search_box_with_history(card, rw);

        /* 下拉筛选行: [版本^] [来源^] [分类^] [加载器^] [排序^] — 自然宽度 */
        {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
            gtk_widget_set_halign(row, GTK_ALIGN_FILL);

            const char* ver_items[] = {"全部版本", "1.21.x", "1.20.x", "1.19.x", "1.18.x", "1.17.x", "1.16.x", "1.15.x", "1.14.x", "1.13.x", "1.12.x", "1.11.x", "1.10.x", "1.9.x", "1.8.x", "1.7.x", "1.6.x", "1.5.x", "1.4.x", "1.3.x", "1.2.x", "1.1.x", "1.0.x"};
            const char* src_items[] = {"全部来源", "CurseForge", "Modrinth"};

            /* 分类 — 对标 PCL-CE ComboSearchTag (按资源类别不同) */
            const char* cat_items_mod[] = {"全部分类", "科技", "魔法", "冒险", "装饰", "世界生成", "生物群系", "维度", "生物", "储存", "交通", "农业", "食物", "装备", "优化", "图书馆", "服务器", "实用", "红石", "能源", "创造"};
            const char* cat_items_modpack[] = {"全部分类", "科技", "魔法", "冒险", "空岛", "探索", "原版增强", "任务", "困难", "轻量", "农业", "建筑"};
            const char* cat_items_pack[]  = {"全部分类", "经典", "写实", "卡通", "中世纪", "现代", "简约", "PvP", "GUI", "字体"};
            const char* cat_items_shader[] = {"全部分类", "写实", "梦幻", "低配", "复古", "卡通", "香草"};
            const char* cat_items_world[] = {"全部分类", "空岛", "生存", "冒险", "解谜", "城市", "红石", "创造"};

            const char* const* cat_items = cat_items_mod;
            int cat_count = 21;
            switch (rw->project_type) {
                case resource::ProjectType::Modpack:      cat_items = cat_items_modpack; cat_count = 12; break;
                case resource::ProjectType::ResourcePack: cat_items = cat_items_pack;    cat_count = 10; break;
                case resource::ProjectType::Shader:       cat_items = cat_items_shader;  cat_count = 7;  break;
                case resource::ProjectType::World:        cat_items = cat_items_world;   cat_count = 8;  break;
                case resource::ProjectType::Datapack:     cat_items = cat_items_mod;     cat_count = 21; break;
                default: break;  // Mod
            }

            /* 加载器 — 对标 PCL-CE ComboSearchLoader */
            const char* loader_items[] = {"全部加载器", "Forge", "Fabric", "Quilt", "NeoForge", "Rift", "LiteLoader"};

            GtkWidget* ver_dd_hbox =
                build_filter_dropdown("版本", ver_items, 23);
            rw->version_dd = gtk_widget_get_last_child(ver_dd_hbox);
            gtk_box_append(GTK_BOX(row), ver_dd_hbox);

            GtkWidget* src_dd_hbox =
                build_filter_dropdown("来源", src_items, 3);
            rw->source_dd = gtk_widget_get_last_child(src_dd_hbox);
            gtk_box_append(GTK_BOX(row), src_dd_hbox);

            GtkWidget* cat_dd_hbox =
                build_filter_dropdown("分类", cat_items, cat_count);
            rw->category_dd = gtk_widget_get_last_child(cat_dd_hbox);
            gtk_box_append(GTK_BOX(row), cat_dd_hbox);

            /* 加载器 — 仅 Mod / 整合包 需要 */
            bool show_loader = (rw->project_type == resource::ProjectType::Mod ||
                                rw->project_type == resource::ProjectType::Modpack);
            if (show_loader) {
                GtkWidget* ldr_dd_hbox =
                    build_filter_dropdown("加载器", loader_items, 7);
                rw->loader_dd = gtk_widget_get_last_child(ldr_dd_hbox);
                gtk_box_append(GTK_BOX(row), ldr_dd_hbox);
            }

            /* 排序方式 — 对标 PCL-CE ComboSearchSort */
            {
                const char* sort_items[] = {"默认排序", "相关性", "下载量", "关注量", "最新发布", "最近更新"};
                GtkWidget* sort_dd_hbox =
                    build_filter_dropdown("排序", sort_items, 6);
                GtkWidget* dd = gtk_widget_get_last_child(sort_dd_hbox);
                if (dd && GTK_IS_DROP_DOWN(dd))
                    gtk_drop_down_set_selected(GTK_DROP_DOWN(dd), 0);
                rw->sort_dd = dd;
                gtk_box_append(GTK_BOX(row), sort_dd_hbox);
            }

            gtk_box_append(GTK_BOX(card), row);
        }

        /* 按钮行: [搜索] [重置条件] — 均分宽度 */
        {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_halign(row, GTK_ALIGN_FILL);
            gtk_box_set_homogeneous(GTK_BOX(row), TRUE);

            GtkWidget* search_btn = gtk_button_new_with_label("搜索");
            gtk_widget_add_css_class(search_btn, "suggested-action");
            gtk_widget_set_hexpand(search_btn, TRUE);
            gtk_box_append(GTK_BOX(row), search_btn);
            rw->search_btn = search_btn;

            GtkWidget* reset_btn = gtk_button_new_with_label("重置条件");
            gtk_widget_set_hexpand(reset_btn, TRUE);
            gtk_box_append(GTK_BOX(row), reset_btn);

            /* ── 导入本地整合包 (仅整合包页面) ── */
            if (rw->project_type == resource::ProjectType::Modpack) {
                GtkWidget* import_btn = gtk_button_new_with_label("导入本地整合包");
                gtk_widget_set_hexpand(import_btn, TRUE);
                gtk_box_append(GTK_BOX(row), import_btn);
            }

            /* ── 搜索按钮 → do_resource_search ── */
            g_signal_connect(search_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    do_resource_search(static_cast<ResourceWidgets*>(data));
                }), rw);

            /* ── Enter 键 → 同上 ── */
            if (rw->search_entry) {
                g_signal_connect(rw->search_entry, "activate",
                    G_CALLBACK(+[](GtkEntry*, gpointer data) {
                        do_resource_search(static_cast<ResourceWidgets*>(data));
                    }), rw);
            }

            /* ── 重置按钮 → 清空所有条件 ── */
            g_signal_connect(reset_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* r = static_cast<ResourceWidgets*>(data);
                    gtk_editable_set_text(GTK_EDITABLE(r->search_entry), "");
                    if (r->source_dd)  gtk_drop_down_set_selected(GTK_DROP_DOWN(r->source_dd), 0);
                    if (r->version_dd) gtk_drop_down_set_selected(GTK_DROP_DOWN(r->version_dd), 0);
                    if (r->category_dd) gtk_drop_down_set_selected(GTK_DROP_DOWN(r->category_dd), 0);
                    if (r->loader_dd)   gtk_drop_down_set_selected(GTK_DROP_DOWN(r->loader_dd), 0);
                    if (r->sort_dd)     gtk_drop_down_set_selected(GTK_DROP_DOWN(r->sort_dd), 0);
                    clear_result_list(r->result_list);
                    gtk_widget_set_visible(r->empty_card, TRUE);
                    gtk_widget_set_visible(gtk_widget_get_parent(r->result_list), FALSE);
                    if (r->spinner) gtk_widget_set_visible(r->spinner, FALSE);
                    if (r->page_nav) gtk_widget_set_visible(r->page_nav, FALSE);
                    r->current_page = 0;
                    r->total_hits = 0;
                }), rw);

            gtk_box_append(GTK_BOX(card), row);
        }

        gtk_box_append(GTK_BOX(box), card);
    }

    /* 空态占位 — 对标 PCL-CE PanEmpty / PanEmptySearch */
    GtkWidget* empty_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(empty_card, "card");
    gtk_widget_set_halign(empty_card, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(empty_card, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(empty_card, 40);
    {
        GtkWidget* empty_title = gtk_label_new("暂无内容");
        gtk_widget_set_margin_top(empty_title, 20);
        gtk_widget_set_margin_start(empty_title, 20);
        gtk_widget_set_margin_end(empty_title, 20);
        gtk_widget_set_margin_bottom(empty_title, 10);
        gtk_box_append(GTK_BOX(empty_card), empty_title);
        {
            PangoAttrList* attrs = pango_attr_list_new();
            PangoAttribute* size = pango_attr_size_new(14 * PANGO_SCALE);
            PangoAttribute* weight = pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD);
            pango_attr_list_insert(attrs, size);
            pango_attr_list_insert(attrs, weight);
            gtk_label_set_attributes(GTK_LABEL(empty_title), attrs);
            pango_attr_list_unref(attrs);
        }

        GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_margin_start(sep, 20);
        gtk_widget_set_margin_end(sep, 20);
        gtk_box_append(GTK_BOX(empty_card), sep);

        GtkWidget* empty_msg = gtk_label_new("使用上方的搜索框查找资源，\n或从左侧选择其他资源分类。");
        gtk_label_set_xalign(GTK_LABEL(empty_msg), 0.5f);
        gtk_widget_set_margin_top(empty_msg, 10);
        gtk_widget_set_margin_start(empty_msg, 20);
        gtk_widget_set_margin_end(empty_msg, 20);
        gtk_widget_set_margin_bottom(empty_msg, 20);
        gtk_box_append(GTK_BOX(empty_card), empty_msg);
    }
    gtk_box_append(GTK_BOX(box), empty_card);
    rw->empty_card = empty_card;

    /* Spinner — 居中在结果区, 搜索时显示 */
    rw->spinner = gtk_spinner_new();
    gtk_widget_set_size_request(rw->spinner, 28, 28);
    gtk_widget_set_halign(rw->spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(rw->spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(rw->spinner, 24);
    gtk_widget_set_margin_bottom(rw->spinner, 24);
    gtk_widget_set_visible(rw->spinner, FALSE);
    gtk_box_append(GTK_BOX(box), rw->spinner);

    /* 结果列表 (初始为空, 由 do_resource_search 动态填充, 包裹在 card 内) */
    GtkWidget* result_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(result_card, "card");
    gtk_widget_add_css_class(result_card, "resource-card");
    gtk_widget_set_visible(result_card, FALSE);

    GtkWidget* result_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(result_list), GTK_SELECTION_NONE);
    gtk_widget_add_css_class(result_list, "boxed-list");
    gtk_box_append(GTK_BOX(result_card), result_list);
    gtk_box_append(GTK_BOX(box), result_card);
    rw->result_list = result_list;

    /* ── 翻页 Footer ── */
    GtkWidget* page_nav = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(page_nav, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(page_nav, 8);
    gtk_widget_set_visible(page_nav, FALSE);
    gtk_box_append(GTK_BOX(box), page_nav);
    rw->page_nav = page_nav;

    auto make_page_btn = [&](const char* icon_name, const char* tooltip) {
        GtkWidget* btn = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
        gtk_widget_add_css_class(btn, "version-action-btn");
        gtk_widget_set_tooltip_text(btn, tooltip);
        gtk_button_set_child(GTK_BUTTON(btn), icon::load(icon_name, 14));
        gtk_box_append(GTK_BOX(page_nav), btn);
        return btn;
    };

    rw->btn_first = make_page_btn("chevrons-left",  "第一页");
    rw->btn_prev  = make_page_btn("chevron-left",   "上一页");

    rw->page_label = gtk_label_new("第 1 页");
    gtk_widget_set_valign(rw->page_label, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(page_nav), rw->page_label);

    rw->btn_next  = make_page_btn("chevron-right",  "下一页");
    rw->btn_last  = make_page_btn("chevrons-right", "最后一页");

    /* 翻页按钮回调 */
    g_signal_connect(rw->btn_first, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            auto* r = static_cast<ResourceWidgets*>(d);
            if (r->current_page > 0) do_page_nav(r, 0);
        }), rw);
    g_signal_connect(rw->btn_prev, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            auto* r = static_cast<ResourceWidgets*>(d);
            if (r->current_page > 0) do_page_nav(r, r->current_page - 1);
        }), rw);
    g_signal_connect(rw->btn_next, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            auto* r = static_cast<ResourceWidgets*>(d);
            if ((r->current_page + 1) * r->page_size < r->total_hits)
                do_page_nav(r, r->current_page + 1);
        }), rw);
    g_signal_connect(rw->btn_last, "clicked",
        G_CALLBACK(+[](GtkButton*, gpointer d) {
            auto* r = static_cast<ResourceWidgets*>(d);
            int last = (r->total_hits - 1) / r->page_size;
            if (r->current_page < last) do_page_nav(r, last);
        }), rw);

    /* 初始状态: 显示空态, 隐藏结果卡片+翻页 */
    gtk_widget_set_visible(empty_card, TRUE);
    gtk_widget_set_visible(result_card, FALSE);

    return box;
}

/* ═══════════════════════════════════════════════════════════════════════
 * do_resource_search — 从 UI 读取筛选条件, 启动后台抓取
 * ═══════════════════════════════════════════════════════════════════════ */
/* ── 更新翻页 Footer 状态 ──────────────────────────────────────────── */
static void update_page_nav(ResourceWidgets* rw, int total_hits)
{
    rw->total_hits = total_hits;
    int total_pages = (total_hits + rw->page_size - 1) / rw->page_size;
    bool visible = total_hits > rw->page_size;

    if (rw->page_nav)
        gtk_widget_set_visible(rw->page_nav, visible);

    if (!visible) return;

    if (rw->page_label) {
        char buf[48];
        snprintf(buf, sizeof(buf), "第 %d / %d 页",
                 rw->current_page + 1, total_pages);
        gtk_label_set_text(GTK_LABEL(rw->page_label), buf);
    }

    bool can_prev = rw->current_page > 0;
    bool can_next = (rw->current_page + 1) * rw->page_size < total_hits;

    if (rw->btn_first) gtk_widget_set_sensitive(rw->btn_first, can_prev);
    if (rw->btn_prev)  gtk_widget_set_sensitive(rw->btn_prev,  can_prev);
    if (rw->btn_next)  gtk_widget_set_sensitive(rw->btn_next,  can_next);
    if (rw->btn_last)  gtk_widget_set_sensitive(rw->btn_last,  can_next);
}

/* ── 翻页导航 (复用当前筛选条件, 仅改 offset) ──────────────────────── */
static void do_page_nav(ResourceWidgets* rw, int new_page)
{
    rw->current_page = new_page;
    unsigned gen = ++rw->search_generation;

    SearchFilters f = read_search_filters(rw);
    int offset = new_page * rw->page_size;

    LOG_INFO("MainWindow: page nav — page=%d offset=%d", new_page, offset);

    /* 清空旧结果, 显示 spinner */
    clear_result_list(rw->result_list);
    gtk_widget_set_visible(rw->empty_card, FALSE);
    gtk_widget_set_visible(gtk_widget_get_parent(rw->result_list), FALSE);
    if (rw->spinner) {
        gtk_spinner_start(GTK_SPINNER(rw->spinner));
        gtk_widget_set_visible(rw->spinner, TRUE);
    }

    /* 后台抓取 */
    resource::search_resources(
        f.query,
        rw->project_type,
        f.source,
        f.version,
        offset,
        rw->page_size,
        f.sort,
        f.loader,
        [rw, gen](resource::SearchResult result) {
            if (gen != rw->search_generation) return;  // 过期翻页, 丢弃

            if (rw->spinner) {
                gtk_spinner_stop(GTK_SPINNER(rw->spinner));
                gtk_widget_set_visible(rw->spinner, FALSE);
            }

            if (!result.success || result.hits.empty()) {
                gtk_widget_set_visible(rw->empty_card, TRUE);
                gtk_widget_set_visible(
                    gtk_widget_get_parent(rw->result_list), FALSE);
                if (rw->page_nav)
                    gtk_widget_set_visible(rw->page_nav, FALSE);
                return;
            }

            /* 更新翻页 footer + 填充结果 (含异步图标) */
            update_page_nav(rw, static_cast<int>(result.total_hits));
            populate_results(rw, gen, result);
        });
}

void do_resource_search(ResourceWidgets* rw)
{
    rw->default_loaded = true;  // 标记已触发默认浏览
    rw->current_page = 0;       // 新搜索 → 回到第一页
    unsigned gen = ++rw->search_generation;

    SearchFilters f = read_search_filters(rw);

    LOG_INFO("MainWindow: resource search — query='%s', type=%d, source=%d, ver='%s'",
             f.query.c_str(), (int)rw->project_type, (int)f.source,
             f.version.c_str());

    /* 清空旧结果, 显示加载状态 */
    clear_result_list(rw->result_list);
    gtk_widget_set_visible(rw->empty_card, FALSE);
    gtk_widget_set_visible(gtk_widget_get_parent(rw->result_list), FALSE);

    /* 显示居中的 spinner */
    if (rw->spinner) {
        gtk_spinner_start(GTK_SPINNER(rw->spinner));
        gtk_widget_set_visible(rw->spinner, TRUE);
    }

    /* 保存到搜索历史 (去重, 最多 MAX_SEARCH_HISTORY 条) */
    if (!f.query.empty()) {
        auto& h = rw->search_history;
        h.erase(std::remove(h.begin(), h.end(), f.query), h.end());
        h.insert(h.begin(), f.query);
        if (h.size() > ResourceWidgets::MAX_SEARCH_HISTORY)
            h.resize(ResourceWidgets::MAX_SEARCH_HISTORY);
    }

    /* 启动后台抓取 */
    resource::search_resources(
        f.query,
        rw->project_type,
        f.source,
        f.version,
        0,                 // offset — 新搜索从第一页开始
        rw->page_size,
        f.sort,
        f.loader,
        [rw, gen](resource::SearchResult result) {
            if (gen != rw->search_generation) return;  // 过期搜索, 丢弃

            /* Hide centered spinner */
            if (rw->spinner) {
                gtk_spinner_stop(GTK_SPINNER(rw->spinner));
                gtk_widget_set_visible(rw->spinner, FALSE);
            }

            /* Error / empty */
            if (!result.success || result.hits.empty()) {
                const char* msg = result.success
                    ? "未找到匹配的资源" : result.error.c_str();
                /* Update empty card message */
                GtkWidget* title = gtk_widget_get_first_child(rw->empty_card);
                if (title) {
                    GtkWidget* sep = gtk_widget_get_next_sibling(title);
                    if (sep) {
                        GtkWidget* msg_lbl = gtk_widget_get_next_sibling(sep);
                        if (msg_lbl && GTK_IS_LABEL(msg_lbl))
                            gtk_label_set_text(GTK_LABEL(msg_lbl), msg);
                    }
                }
                gtk_widget_set_visible(rw->empty_card, TRUE);
                gtk_widget_set_visible(gtk_widget_get_parent(rw->result_list), FALSE);
                LOG_INFO("MainWindow: resource search — no results");
                return;
            }

            /* 更新翻页 footer + 填充结果 (含异步图标) */
            update_page_nav(rw, static_cast<int>(result.total_hits));
            populate_results(rw, gen, result);
            LOG_INFO("MainWindow: resource search — %lu results shown",
                     (unsigned long)result.hits.size());
        });
}

/* ═══════════════════════════════════════════════════════════════════════
 * build_favorites_page — 收藏夹页面
 *
 *   搜索框 + 搜索/重置按钮 (与其他页面一致, 无版本/来源/分类/加载器筛选),
 *   下方为 5 个可折叠分类卡片 (Mod / 整合包 / … / 光影)
 * ═══════════════════════════════════════════════════════════════════════ */
static GtkWidget* build_favorites_page()
{
    auto* rw = new ResourceWidgets{};
    rw->project_type = resource::ProjectType::Mod;  // dummy — no search action yet

    GtkWidget* scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(box, 32);
    gtk_widget_set_margin_end(box, 32);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), box);

    /* ── 搜索卡片 (搜索框 + chevron + 历史 + 按钮, 无筛选下拉框) ── */
    {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_add_css_class(card, "card");
        gtk_widget_add_css_class(card, "search-card");
        gtk_widget_set_halign(card, GTK_ALIGN_FILL);

        /* 搜索框行 + chevron + 历史记录 Revealer (收藏页: 历史项不触发搜索) */
        rw->history_triggers_search = false;
        build_search_box_with_history(card, rw);

        /* 按钮行: [搜索] [重置条件] — 均分宽度 */
        {
            GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
            gtk_widget_set_halign(row, GTK_ALIGN_FILL);
            gtk_box_set_homogeneous(GTK_BOX(row), TRUE);

            GtkWidget* search_btn = gtk_button_new_with_label("搜索");
            gtk_widget_add_css_class(search_btn, "suggested-action");
            gtk_widget_set_hexpand(search_btn, TRUE);
            gtk_box_append(GTK_BOX(row), search_btn);
            rw->search_btn = search_btn;

            GtkWidget* reset_btn = gtk_button_new_with_label("重置条件");
            gtk_widget_set_hexpand(reset_btn, TRUE);
            gtk_box_append(GTK_BOX(row), reset_btn);

            /* 搜索按钮 → 标记已加载, 未来按搜索词过滤收藏 */
            g_signal_connect(search_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* r = static_cast<ResourceWidgets*>(data);
                    r->default_loaded = true;
                }), rw);

            /* 重置按钮 → 清空搜索框 */
            g_signal_connect(reset_btn, "clicked",
                G_CALLBACK(+[](GtkButton*, gpointer data) {
                    auto* r = static_cast<ResourceWidgets*>(data);
                    gtk_editable_set_text(GTK_EDITABLE(r->search_entry), "");
                }), rw);

            gtk_box_append(GTK_BOX(card), row);
        }

        gtk_box_append(GTK_BOX(box), card);
    }

    /* ── 可折叠分类卡片 ── */
    struct FavSection { const char* label; const char* icon; };
    const FavSection sections[] = {
        {"Mod",    "puzzle"},
        {"整合包",  "package"},
        {"数据包",  "file-archive"},
        {"资源包",  "layers"},
        {"光影",    "sparkles"},
        {"世界",    "globe"},
    };

    for (int i = 0; i < 6; i++) {
        GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_add_css_class(card, "card");

        GtkWidget* header = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(header), FALSE);
        gtk_widget_add_css_class(header, "card-header-btn");
        {
            GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_start(hbox, 4);
            gtk_widget_set_margin_end(hbox, 4);

            GtkWidget* icon_w = icon::load(sections[i].icon, 18);
            gtk_widget_set_valign(icon_w, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(hbox), icon_w);

            GtkWidget* lbl = gtk_label_new(sections[i].label);
            gtk_widget_add_css_class(lbl, "card-title");
            gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
            gtk_widget_set_hexpand(lbl, TRUE);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
            gtk_box_append(GTK_BOX(hbox), lbl);

            GtkWidget* count_lbl = gtk_label_new("(0)");
            gtk_widget_set_opacity(count_lbl, 0.55);
            gtk_widget_set_valign(count_lbl, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(hbox), count_lbl);

            GtkWidget* arrow = icon::load("chevron-up", 18);
            gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
            gtk_box_append(GTK_BOX(hbox), arrow);
            g_object_set_data(G_OBJECT(header), "arrow", arrow);
            g_object_set_data(G_OBJECT(header), "count_lbl", count_lbl);

            gtk_button_set_child(GTK_BUTTON(header), hbox);
        }
        gtk_box_append(GTK_BOX(card), header);

        GtkWidget* revealer = gtk_revealer_new();
        gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
        {
            GtkWidget* list = gtk_list_box_new();
            gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
                                            GTK_SELECTION_NONE);
            gtk_widget_add_css_class(list, "boxed-list");
            gtk_revealer_set_child(GTK_REVEALER(revealer), list);
        }
        gtk_box_append(GTK_BOX(card), revealer);
        gtk_box_append(GTK_BOX(box), card);

        g_signal_connect(header, "clicked",
            G_CALLBACK(+[](GtkWidget* btn, gpointer data) {
                GtkWidget* rv = static_cast<GtkWidget*>(data);
                gboolean vis = gtk_revealer_get_reveal_child(
                    GTK_REVEALER(rv));
                gtk_revealer_set_reveal_child(GTK_REVEALER(rv), !vis);

                GtkWidget* ar = static_cast<GtkWidget*>(
                    g_object_get_data(G_OBJECT(btn), "arrow"));
                if (ar) {
                    GtkWidget* parent = gtk_widget_get_parent(ar);
                    GtkWidget* new_arrow = icon::load(
                        vis ? "chevron-up" : "chevron-down", 18);
                    gtk_widget_set_valign(new_arrow, GTK_ALIGN_CENTER);
                    gtk_box_append(GTK_BOX(parent), new_arrow);
                    gtk_box_remove(GTK_BOX(parent), ar);
                    g_object_set_data(G_OBJECT(btn), "arrow", new_arrow);
                }
            }), revealer);
    }

    return scroll;
}

/**
 * 构建"下载"页面 — 左侧导航 + GtkStack 右侧内容
 */
GtkWidget* build_download_page()
{
    /* ---- 左侧导航列表 ---- */
    GtkWidget* left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(left, "nav-sidebar");
    gtk_widget_set_valign(left, GTK_ALIGN_FILL);

    /* ---- 右侧 GtkStack ---- */
    GtkWidget* stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack),
                                  GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_hhomogeneous(GTK_STACK(stack), FALSE);
    gtk_stack_set_vhomogeneous(GTK_STACK(stack), FALSE);

    struct NavDef { const char* label; const char* icon; int page; };
    const NavDef navs[] = {
        {"Minecraft",  "boxes",         0},
        {"Mod",        "puzzle",        1},
        {"整合包",      "package",       2},
        {"数据包",      "file-archive",  3},
        {"资源包",      "layers",        4},
        {"光影",        "sparkles",      5},
        {"收藏夹",      "heart",         6},
    };

    /* 用 new 分配，lambda 捕获指针，回调末尾 delete */
    auto* w = new McWidgets{};
    w->fetch_started = false;

    /* ---- 右侧页面 0: Minecraft 版本卡片列表 ---- */
    GtkWidget* mc_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mc_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    {
        GtkWidget* mc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_widget_set_margin_start(mc_box, 32);
        gtk_widget_set_margin_end(mc_box, 32);
        gtk_widget_set_margin_top(mc_box, 24);
        gtk_widget_set_margin_bottom(mc_box, 24);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(mc_scroll), mc_box);

        /* ── 最新版本卡片 (包含正式版 + 预览版两个子项) ── */
        {
            GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_add_css_class(card, "card");

            /* 卡片标题 */
            GtkWidget* header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_margin_start(header, 4);
            gtk_widget_set_margin_end(header, 4);
            gtk_widget_set_margin_top(header, 8);
            gtk_widget_set_margin_bottom(header, 4);
            GtkWidget* hdr_label = gtk_label_new("最新版本");
            gtk_widget_add_css_class(hdr_label, "card-title");
            gtk_widget_set_hexpand(hdr_label, TRUE);
            gtk_label_set_xalign(GTK_LABEL(hdr_label), 0.0f);
            gtk_box_append(GTK_BOX(header), hdr_label);

            /* 加载 spinner — 永久挂载在 header 中, 用 visible 控制显隐 */
            w->load_spinner = gtk_spinner_new();
            gtk_widget_set_size_request(w->load_spinner, 16, 16);
            gtk_widget_set_valign(w->load_spinner, GTK_ALIGN_CENTER);
            gtk_widget_set_visible(w->load_spinner, FALSE);
            gtk_box_append(GTK_BOX(header), w->load_spinner);
            w->spinner_parent = header;   // 直接引用, 刷新时无需遍历 widget tree

            gtk_box_append(GTK_BOX(card), header);

            /* 子项列表 */
            GtkWidget* list = gtk_list_box_new();
            gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
                                            GTK_SELECTION_NONE);
            gtk_widget_add_css_class(list, "mc-latest-list");
            gtk_box_append(GTK_BOX(card), list);

            const char* latest_blocks[] = { "Minecraft", "Command" };
            for (int i = 0; i < 2; i++) {
                GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                gtk_widget_set_margin_start(row, 8);
                gtk_widget_set_margin_end(row, 8);
                gtk_widget_set_margin_top(row, 6);
                gtk_widget_set_margin_bottom(row, 6);

                /* 图标 */
                GtkWidget* icn = icon::load_block(latest_blocks[i], 32);
                gtk_widget_set_valign(icn, GTK_ALIGN_START);
                gtk_widget_set_margin_top(icn, 2);
                gtk_box_append(GTK_BOX(row), icn);

                /* 中间: 版本号 + 注释 */
                GtkWidget* mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                gtk_widget_set_hexpand(mid, TRUE);
                gtk_widget_set_valign(mid, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(row), mid);

                GtkWidget* ver = gtk_label_new("未知");
                gtk_label_set_xalign(GTK_LABEL(ver), 0.0f);
                gtk_box_append(GTK_BOX(mid), ver);
                {
                    PangoAttrList* attrs = pango_attr_list_new();
                    pango_attr_list_insert(attrs,
                        pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(ver), attrs);
                    pango_attr_list_unref(attrs);
                }
                w->latest_ver[i] = ver;   // ★ 直接记录指针

                GtkWidget* note = gtk_label_new("正式版 · 发布于 —");
                gtk_label_set_xalign(GTK_LABEL(note), 0.0f);
                gtk_widget_set_opacity(note, 0.55);
                gtk_box_append(GTK_BOX(mid), note);
                w->latest_note[i] = note;  // ★ 直接记录指针

                /* 悬停操作按钮 */
                GtkWidget* actions = build_version_actions();
                gtk_box_append(GTK_BOX(row), actions);
                attach_row_hover(row, actions);

                /* 点击整行 → 导航到 MC 版本详情页 */
                {
                    /* i=0 → release, i=1 → snapshot */
                    const char* vtype = (i == 0) ? "release" : "snapshot";
                    GtkGesture* click = gtk_gesture_click_new();

                    /* 按下时 — 添加按下阴影反馈 */
                    g_signal_connect(click, "pressed",
                        G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
                            GtkWidget* target = gtk_event_controller_get_widget(
                                GTK_EVENT_CONTROLLER(g));
                            GtkWidget* list_row = gtk_widget_get_parent(target);
                            if (list_row)
                                gtk_widget_add_css_class(list_row, "mc-latest-row-pressed");
                        }), nullptr);

                    /* 释放/取消时 — 移除按下阴影反馈 */
                    auto remove_pressed_class = +[](GtkGesture* g, int, double, double, gpointer) {
                        GtkWidget* target = gtk_event_controller_get_widget(
                            GTK_EVENT_CONTROLLER(g));
                        GtkWidget* list_row = gtk_widget_get_parent(target);
                        if (list_row)
                            gtk_widget_remove_css_class(list_row, "mc-latest-row-pressed");
                    };
                    g_signal_connect(click, "released", G_CALLBACK(remove_pressed_class), nullptr);
                    g_signal_connect(click, "cancel",   G_CALLBACK(remove_pressed_class), nullptr);

                    g_signal_connect(click, "pressed",
                        G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer d) {
                            GtkWidget* target = gtk_event_controller_get_widget(
                                GTK_EVENT_CONTROLLER(g));
                            const char* vt = static_cast<const char*>(d);
                            GtkWidget* mid = gtk_widget_get_first_child(target);
                            if (mid) mid = gtk_widget_get_next_sibling(mid);
                            if (mid) {
                                GtkWidget* ver_lbl = gtk_widget_get_first_child(mid);
                                if (ver_lbl && GTK_IS_LABEL(ver_lbl)) {
                                    const char* vid = gtk_label_get_text(GTK_LABEL(ver_lbl));
                                    if (vid && strcmp(vid, "未知") != 0)
                                        navigate_to_mc_version_detail(target, vid, vt);
                                }
                            }
                        }), (gpointer)vtype);
                    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
                }

                gtk_list_box_append(GTK_LIST_BOX(list), row);
            }
            gtk_box_append(GTK_BOX(mc_box), card);
        }

        /* ── 可展开分类卡片 ── */
        struct VerGroup {
            const char* label;
            const char* block;
            const char* key;  // internal id
        };
        const VerGroup groups[] = {
            {"正式版",   "Minecraft",           "release"},
            {"预览版",   "Command",             "snapshot"},
            {"远古版本", "Minecraft-Alpha",      "old_alpha"},
            {"愚人节版本","Minecraft-AprilFool", "april_fool"},
        };

        for (int i = 0; i < 4; i++) {
            /* ── 卡片容器 (对标 PCL-CE MyCard) ── */
            GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_widget_add_css_class(card, "card");

            /* 卡片标题 (点击展开/折叠) */
            GtkWidget* header = gtk_button_new();
            gtk_button_set_has_frame(GTK_BUTTON(header), FALSE);
            gtk_widget_add_css_class(header, "card-header-btn");
            {
                GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_widget_set_margin_start(hbox, 4);
                gtk_widget_set_margin_end(hbox, 4);

                GtkWidget* lbl = gtk_label_new(groups[i].label);
                gtk_widget_add_css_class(lbl, "card-title");
                gtk_widget_set_valign(lbl, GTK_ALIGN_CENTER);
                gtk_widget_set_hexpand(lbl, TRUE);
                gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
                gtk_box_append(GTK_BOX(hbox), lbl);
                w->group_header_label[i] = lbl;  // ★ 直接记录指针

                GtkWidget* arrow = icon::load("chevron-up", 18);
                gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);
                gtk_box_append(GTK_BOX(hbox), arrow);
                g_object_set_data(G_OBJECT(header), "arrow", arrow);

                gtk_button_set_child(GTK_BUTTON(header), hbox);
            }
            gtk_box_append(GTK_BOX(card), header);

            /* 版本列表 (初始隐藏) — 在 card 内部 */
            GtkWidget* revealer = gtk_revealer_new();
            gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 200);
            {
                GtkWidget* list = gtk_list_box_new();
                gtk_list_box_set_selection_mode(GTK_LIST_BOX(list),
                                                GTK_SELECTION_NONE);
                gtk_widget_add_css_class(list, "boxed-list");
                gtk_widget_add_css_class(list, "ver-group-list");
                gtk_revealer_set_child(GTK_REVEALER(revealer), list);
                w->group_list[i] = list;    // ★ 直接记录指针
            }
            gtk_box_append(GTK_BOX(card), revealer);

            gtk_box_append(GTK_BOX(mc_box), card);

            /* 展开/折叠回调 */
            g_signal_connect(header, "clicked",
                G_CALLBACK(+[](GtkWidget* btn, gpointer data) {
                    GtkWidget* rv = static_cast<GtkWidget*>(data);
                    gboolean vis = gtk_revealer_get_reveal_child(
                        GTK_REVEALER(rv));
                    gtk_revealer_set_reveal_child(GTK_REVEALER(rv), !vis);
                    // rotate chevron
                    GtkWidget* ar = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(btn), "arrow"));
                    if (ar) {
                        GtkWidget* parent = gtk_widget_get_parent(ar);
                        GtkWidget* new_arrow = icon::load(
                            vis ? "chevron-up" : "chevron-down", 18);
                        gtk_widget_set_valign(new_arrow, GTK_ALIGN_CENTER);
                        gtk_box_append(GTK_BOX(parent), new_arrow);
                        gtk_box_remove(GTK_BOX(parent), ar);
                        g_object_set_data(G_OBJECT(btn), "arrow", new_arrow);
                    }
                }), revealer);
        }

    }
    gtk_stack_add_named(GTK_STACK(stack), mc_scroll, "page0");

    /* ---- 右侧页面 1–5: 各资源分类独立页面 ---- */
    const char* res_categories[] = {
        "Mod", "整合包", "数据包", "资源包", "光影",
    };
    for (int i = 0; i < 5; i++) {
        auto* rw = new ResourceWidgets{};
        rw->project_type = category_to_project_type(res_categories[i]);

        GtkWidget* scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        GtkWidget* page = build_resource_search(res_categories[i], rw);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), page);

        // 将 ResourceWidgets 挂到 scroll 上，供导航回调触发默认搜索
        g_object_set_data(G_OBJECT(scroll), "res_widgets", rw);

        char name[8];
        snprintf(name, sizeof(name), "page%d", i + 1);
        gtk_stack_add_named(GTK_STACK(stack), scroll, name);
    }

    /* ---- 右侧页面 6: 收藏夹 (无筛选下拉框, 仅可折叠分类) ---- */
    {
        GtkWidget* scroll = build_favorites_page();
        gtk_stack_add_named(GTK_STACK(stack), scroll, "page6");
    }

    /* ---- 右侧页面 7: 下载管理器 ---- */
    {
        /* ── 下载任务数据模型 (通过 libpclcore Provider) ── */
        using DmTask = pclcore::local::DmTask;
        auto& task_provider = pclcore::local::get_download_task_provider();

        /* DmItemCb: tasks* + pos + refresh fn (副本), 用于单个条目的停止/删除按钮 */
        struct DmItemCb {
            std::vector<DmTask>* tasks;
            size_t pos;
            std::shared_ptr<std::function<void()>> refresh;
        };
        /* DmCbPair: tasks* + refresh fn, 用于暂停/继续/清除等操作 */
        using DmCbPair = std::pair<std::vector<DmTask>*,
                                   std::shared_ptr<std::function<void()>>>;
        auto tasks = std::make_shared<std::vector<DmTask>>(task_provider.get_tasks());

        /* ── 示例任务由 HardcodedDownloadTaskProvider 提供 ── */

        /* ── 格式化大小 ── */
        auto fmt_size = [](int64_t bytes) -> std::string {
            if (bytes >= 1000000000LL) {
                char b[32]; snprintf(b, sizeof(b), "%.1f GB", bytes / 1e9);
                return b;
            }
            if (bytes >= 1000000) {
                char b[32]; snprintf(b, sizeof(b), "%.1f MB", bytes / 1e6);
                return b;
            }
            if (bytes >= 1000) {
                char b[32]; snprintf(b, sizeof(b), "%.0f KB", bytes / 1e3);
                return b;
            }
            return std::to_string(bytes) + " B";
        };

        /* ── 界面构件 ── */
        GtkWidget* scroll = gtk_scrolled_window_new();
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_widget_add_css_class(scroll, "dm-scroll");
        GtkWidget* dm_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_widget_set_margin_start(dm_box, 32);
        gtk_widget_set_margin_end(dm_box, 32);
        gtk_widget_set_margin_top(dm_box, 24);
        gtk_widget_set_margin_bottom(dm_box, 24);
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), dm_box);

        /* ── 标题行 ── */
        GtkWidget* hdr_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(dm_box), hdr_row);
        GtkWidget* title = gtk_label_new("下载管理器");
        gtk_widget_add_css_class(title, "card-title");
        gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
        gtk_widget_set_hexpand(title, TRUE);
        gtk_box_append(GTK_BOX(hdr_row), title);
        GtkWidget* new_btn = gtk_button_new_with_label("新建下载");
        gtk_widget_add_css_class(new_btn, "suggested-action");
        gtk_box_append(GTK_BOX(hdr_row), new_btn);

        gtk_box_append(GTK_BOX(dm_box),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

        /* ── 任务列表 / 空态 用 GtkStack 切换 ── */
        GtkWidget* list_stack = gtk_stack_new();
        gtk_stack_set_transition_type(GTK_STACK(list_stack),
                                      GTK_STACK_TRANSITION_TYPE_CROSSFADE);
        gtk_widget_set_vexpand(list_stack, TRUE);
        gtk_box_append(GTK_BOX(dm_box), list_stack);

        GtkWidget* list = gtk_list_box_new();
        gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
        gtk_stack_add_named(GTK_STACK(list_stack), list, "list");

        GtkWidget* empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_valign(empty_box, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_top(empty_box, 40);
        GtkWidget* e_icon = icon::load("download", 48);
        gtk_widget_set_opacity(e_icon, 0.2);
        gtk_widget_set_halign(e_icon, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(empty_box), e_icon);
        GtkWidget* e_lbl = gtk_label_new("暂无下载任务");
        gtk_widget_set_opacity(e_lbl, 0.4);
        gtk_widget_set_halign(e_lbl, GTK_ALIGN_CENTER);
        gtk_box_append(GTK_BOX(empty_box), e_lbl);
        gtk_stack_add_named(GTK_STACK(list_stack), empty_box, "empty");

        gtk_stack_set_visible_child_name(GTK_STACK(list_stack), "empty");

        /* ── 底部操作栏 ── */
        GtkWidget* footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_box_append(GTK_BOX(dm_box), footer);

        GtkWidget* sel_all_btn = gtk_button_new_with_label("全选");
        gtk_box_append(GTK_BOX(footer), sel_all_btn);

        GtkWidget* clear_btn = gtk_button_new_with_label("清除已完成");
        gtk_box_append(GTK_BOX(footer), clear_btn);

        GtkWidget* del_sel_btn = gtk_button_new_with_label("删除选中");
        gtk_widget_add_css_class(del_sel_btn, "destructive-action");
        gtk_box_append(GTK_BOX(footer), del_sel_btn);

        /* ── 刷新列表 (用 shared_ptr 让回调能调用它) ── */
        auto p_refresh = std::make_shared<std::function<void()>>();
        *p_refresh = [tasks, list, list_stack, fmt_size, p_refresh]() {
            GtkWidget* child;
            while ((child = gtk_widget_get_first_child(list)))
                gtk_list_box_remove(GTK_LIST_BOX(list), child);

            bool has = !tasks->empty();
            gtk_stack_set_visible_child_name(GTK_STACK(list_stack),
                has ? "list" : "empty");

            for (size_t idx = 0; idx < tasks->size(); idx++) {
                auto& t = (*tasks)[idx];

                GtkWidget* card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
                gtk_widget_add_css_class(card, "card");
                gtk_widget_set_margin_bottom(card, 6);

                GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
                gtk_widget_set_margin_start(row, 10);
                gtk_widget_set_margin_end(row, 14);
                gtk_widget_set_margin_top(row, 10);
                gtk_widget_set_margin_bottom(row, 10);
                gtk_box_append(GTK_BOX(card), row);

                /* 复选框 */
                GtkWidget* cb = gtk_check_button_new();
                gtk_widget_add_css_class(cb, "dm-check");
                gtk_check_button_set_active(GTK_CHECK_BUTTON(cb), t.checked);
                gtk_widget_set_valign(cb, GTK_ALIGN_START);
                gtk_widget_set_margin_top(cb, 2);
                g_object_set_data(G_OBJECT(cb), "dm-idx", GSIZE_TO_POINTER(idx));
                g_object_set_data(G_OBJECT(cb), "dm-tasks", tasks.get());
                g_signal_connect(cb, "toggled", G_CALLBACK(+[](GtkCheckButton* btn, gpointer) {
                    size_t i = GPOINTER_TO_SIZE(g_object_get_data(G_OBJECT(btn), "dm-idx"));
                    auto* ts = static_cast<std::vector<DmTask>*>(
                        g_object_get_data(G_OBJECT(btn), "dm-tasks"));
                    if (ts && i < ts->size())
                        (*ts)[i].checked = gtk_check_button_get_active(btn);
                }), nullptr);
                gtk_box_append(GTK_BOX(row), cb);

                /* 状态图标 */
                const char* sicon = "download";
                if (t.status == 1)      sicon = "download";
                else if (t.status == 2) sicon = "pause";
                else if (t.status == 3) sicon = "circle-check";
                else if (t.status == 4) sicon = "x";
                GtkWidget* icn = icon::load(sicon, 22);
                gtk_widget_set_valign(icn, GTK_ALIGN_START);
                gtk_widget_set_margin_top(icn, 2);
                gtk_box_append(GTK_BOX(row), icn);

                /* 信息区 */
                GtkWidget* info = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
                gtk_widget_set_hexpand(info, TRUE);
                gtk_box_append(GTK_BOX(row), info);

                /* 文件名 */
                GtkWidget* nm = gtk_label_new(t.name.c_str());
                gtk_label_set_xalign(GTK_LABEL(nm), 0.0f);
                gtk_label_set_ellipsize(GTK_LABEL(nm), PANGO_ELLIPSIZE_MIDDLE);
                {
                    PangoAttrList* a = pango_attr_list_new();
                    pango_attr_list_insert(a, pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                    gtk_label_set_attributes(GTK_LABEL(nm), a);
                    pango_attr_list_unref(a);
                }
                gtk_box_append(GTK_BOX(info), nm);

                /* 状态 + 大小 */
                const char* stxt = "排队中";
                if (t.status == 1) stxt = "下载中";
                else if (t.status == 2) stxt = "已暂停";
                else if (t.status == 3) stxt = "已完成";
                else if (t.status == 4) stxt = "错误";

                GtkWidget* srow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
                gtk_box_append(GTK_BOX(info), srow);

                GtkWidget* sl = gtk_label_new(stxt);
                gtk_widget_set_opacity(sl, 0.6);
                gtk_box_append(GTK_BOX(srow), sl);

                /* 大小信息 */
                std::string sz;
                if (t.status == 3 || t.status == 0)
                    sz = fmt_size(t.total);
                else if (t.status == 1 || t.status == 2)
                    sz = fmt_size(t.done) + " / " + fmt_size(t.total);
                GtkWidget* sz_lbl = gtk_label_new(sz.c_str());
                gtk_widget_set_opacity(sz_lbl, 0.5);
                gtk_box_append(GTK_BOX(srow), sz_lbl);

                /* 线程信息 */
                if (t.status == 1 && t.max_threads > 0) {
                    char th[32];
                    snprintf(th, sizeof(th), "线程 %d/%d", t.threads, t.max_threads);
                    GtkWidget* th_lbl = gtk_label_new(th);
                    gtk_widget_set_opacity(th_lbl, 0.45);
                    gtk_box_append(GTK_BOX(srow), th_lbl);
                }

                /* 下载速度 + ETA */
                if (t.status == 1 && t.speed > 0) {
                    /* ── 速度字符串 ── */
                    std::string spd;
                    if (t.speed >= 1000000) {
                        char b[32];
                        snprintf(b, sizeof(b), "%.1f MB/s", t.speed / 1e6);
                        spd = b;
                    } else if (t.speed >= 1000) {
                        char b[32];
                        snprintf(b, sizeof(b), "%.0f KB/s", t.speed / 1e3);
                        spd = b;
                    } else {
                        spd = std::to_string(t.speed) + " B/s";
                    }

                    GtkWidget* spd_lbl = gtk_label_new(spd.c_str());
                    gtk_widget_set_opacity(spd_lbl, 0.55);
                    gtk_box_append(GTK_BOX(srow), spd_lbl);

                    /* ── ETA 字符串 ── */
                    int64_t remaining = t.total > t.done ? t.total - t.done : 0;
                    int64_t eta_s = t.eta > 0 ? t.eta
                        : (t.speed > 0 && remaining > 0
                            ? std::max((int64_t)1, remaining / t.speed)
                            : 0);

                    if (eta_s > 0) {
                        std::string eta_str;
                        if (eta_s >= 3600) {
                            int h = (int)(eta_s / 3600);
                            int m = (int)((eta_s % 3600) / 60);
                            char b[32];
                            snprintf(b, sizeof(b), "剩余 %d 小时 %d 分", h, m);
                            eta_str = b;
                        } else if (eta_s >= 60) {
                            int m = (int)(eta_s / 60);
                            int s = (int)(eta_s % 60);
                            char b[32];
                            snprintf(b, sizeof(b), "剩余 %d 分 %d 秒", m, s);
                            eta_str = b;
                        } else {
                            char b[32];
                            snprintf(b, sizeof(b), "剩余 %lld 秒", (long long)eta_s);
                            eta_str = b;
                        }
                        GtkWidget* eta_lbl = gtk_label_new(eta_str.c_str());
                        gtk_widget_set_opacity(eta_lbl, 0.45);
                        gtk_box_append(GTK_BOX(srow), eta_lbl);
                    }
                }

                /* 保存路径 (小字灰色) */
                if (!t.save_path.empty()) {
                    GtkWidget* path_lbl = gtk_label_new(t.save_path.c_str());
                    gtk_label_set_xalign(GTK_LABEL(path_lbl), 0.0f);
                    gtk_label_set_ellipsize(GTK_LABEL(path_lbl), PANGO_ELLIPSIZE_MIDDLE);
                    gtk_widget_set_opacity(path_lbl, 0.35);
                    {
                        PangoAttrList* a = pango_attr_list_new();
                        pango_attr_list_insert(a, pango_attr_scale_new(0.85));
                        gtk_label_set_attributes(GTK_LABEL(path_lbl), a);
                        pango_attr_list_unref(a);
                    }
                    gtk_box_append(GTK_BOX(info), path_lbl);
                }

                /* ── 操作按钮 ── */
                auto mkb = [](const char* icn, const char* tt,
                              const char* css = nullptr) {
                    GtkWidget* b = gtk_button_new();
                    gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
                    gtk_widget_add_css_class(b, "version-action-btn");
                    gtk_widget_set_tooltip_text(b, tt);
                    gtk_button_set_child(GTK_BUTTON(b), icon::load(icn, 15));
                    if (css) gtk_widget_add_css_class(b, css);
                    return b;
                };

                /* 暂停: 状态 1→2 + 刷新 */
                if (t.status == 1) {
                    GtkWidget* pb = mkb("pause", "暂停");
                    auto* cb = new DmCbPair(tasks.get(), p_refresh);
                    g_signal_connect(pb, "clicked",
                        G_CALLBACK(+[](GtkWidget*, gpointer d) {
                            auto* pair = static_cast<DmCbPair*>(d);
                            auto* ts = pair->first;
                            for (auto& tk : *ts)
                                if (tk.status == 1) tk.status = 2;
                            (*pair->second)();
                            delete pair;
                        }), cb);
                    gtk_box_append(GTK_BOX(row), pb);
                }
                /* 继续: 状态 2→1 + 刷新 */
                else if (t.status == 2) {
                    GtkWidget* rb = mkb("play", "继续");
                    auto* cb = new DmCbPair(tasks.get(), p_refresh);
                    g_signal_connect(rb, "clicked",
                        G_CALLBACK(+[](GtkWidget*, gpointer d) {
                            auto* pair = static_cast<DmCbPair*>(d);
                            auto* ts = pair->first;
                            for (auto& tk : *ts)
                                if (tk.status == 2) tk.status = 1;
                            (*pair->second)();
                            delete pair;
                        }), cb);
                    gtk_box_append(GTK_BOX(row), rb);
                }

                if (t.status <= 2) {
                    GtkWidget* sb = mkb("x", "停止");
                    gtk_widget_add_css_class(sb, "dm-danger-btn");
                    auto* cb = new DmItemCb{tasks.get(), idx, p_refresh};
                    g_signal_connect(sb, "clicked",
                        G_CALLBACK(+[](GtkWidget*, gpointer d) {
                            auto* cb = static_cast<DmItemCb*>(d);
                            auto* ts = cb->tasks;
                            if (cb->pos < ts->size())
                                ts->erase(ts->begin() + cb->pos);
                            (*cb->refresh)();
                            delete cb;
                        }), cb);
                    gtk_box_append(GTK_BOX(row), sb);
                }
                if (t.status >= 3) {
                    GtkWidget* db = mkb("trash-2", "删除");
                    gtk_widget_add_css_class(db, "dm-danger-btn");
                    auto* cb = new DmItemCb{tasks.get(), idx, p_refresh};
                    g_signal_connect(db, "clicked",
                        G_CALLBACK(+[](GtkWidget*, gpointer d) {
                            auto* cb = static_cast<DmItemCb*>(d);
                            auto* ts = cb->tasks;
                            if (cb->pos < ts->size())
                                ts->erase(ts->begin() + cb->pos);
                            (*cb->refresh)();
                            delete cb;
                        }), cb);
                    gtk_box_append(GTK_BOX(row), db);
                }

                /* 打开所在文件夹 */
                {
                    GtkWidget* of = mkb("folder-open", "打开所在文件夹");
                    std::string path = t.save_path;
                    g_signal_connect(of, "clicked",
                        G_CALLBACK(+[](GtkWidget*, gpointer d) {
                            auto* p = static_cast<std::string*>(d);
                            /* 将 ~ 展开为 $HOME */
                            std::string uri = "file://";
                            if (!p->empty() && (*p)[0] == '~')
                                uri += std::getenv("HOME")
                                    ? (std::getenv("HOME") + p->substr(1))
                                    : *p;
                            else
                                uri += *p;
                            {
                                GtkUriLauncher* l = gtk_uri_launcher_new(uri.c_str());
                                gtk_uri_launcher_launch(l, nullptr, nullptr, nullptr, nullptr);
                                g_object_unref(l);
                            }
                            delete p;
                        }), new std::string(path));
                    gtk_box_append(GTK_BOX(row), of);
                }

                /* ── 底部进度条：Fluent Design 风格细线 ── */
                if (t.status == 1 || t.status == 2) {
                    double frac = t.total > 0 ? (double)t.done / t.total : 0;
                    GtkWidget* prog = gtk_progress_bar_new();
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(prog), frac);
                    gtk_widget_set_hexpand(prog, TRUE);
                    gtk_widget_add_css_class(prog, "dm-progress");
                    gtk_box_append(GTK_BOX(card), prog);
                }

                gtk_list_box_append(GTK_LIST_BOX(list), card);
            }
        };

        /* ── 按钮回调 ── */
        g_signal_connect(new_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
            auto* p = static_cast<std::shared_ptr<std::function<void()>>*>(d);
            (*p)->operator()();
        }), new auto(p_refresh));

        /* DmCbPair 存储 shared_ptr 的副本（非裸指针），避免悬空 */
        using DmCbPair = std::pair<std::vector<DmTask>*,
                                   std::shared_ptr<std::function<void()>>>;
        {
            auto* cb = new DmCbPair(tasks.get(), p_refresh);
            g_signal_connect(clear_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
                auto* pair = static_cast<DmCbPair*>(d);
                auto* ts = pair->first;
                ts->erase(std::remove_if(ts->begin(), ts->end(),
                    [](const DmTask& t) { return t.status >= 3; }), ts->end());
                (*pair->second)();
                /* cb 与按钮同生命周期, 不 delete */
            }), cb);
        }
        {
            auto* cb = new DmCbPair(tasks.get(), p_refresh);
            g_signal_connect(del_sel_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
                auto* pair = static_cast<DmCbPair*>(d);
                auto* ts = pair->first;

                std::set<size_t> sel;
                for (size_t i = 0; i < ts->size(); i++)
                    if ((*ts)[i].checked) sel.insert(i);

                if (sel.empty()) return;

                for (auto it = sel.rbegin(); it != sel.rend(); ++it)
                    if (*it < ts->size()) ts->erase(ts->begin() + *it);

                (*pair->second)();
            }), cb);
        }

        g_signal_connect(sel_all_btn, "clicked", G_CALLBACK(+[](GtkWidget*, gpointer d) {
            GtkWidget* lst = static_cast<GtkWidget*>(d);
            GtkWidget* child = gtk_widget_get_first_child(lst);
            while (child) {
                GtkWidget* card = gtk_widget_get_first_child(child);
                if (card) {
                    GtkWidget* row = gtk_widget_get_first_child(card);
                    if (row) {
                        GtkWidget* cb = gtk_widget_get_first_child(row);
                        if (cb && GTK_IS_CHECK_BUTTON(cb))
                            gtk_check_button_set_active(GTK_CHECK_BUTTON(cb), TRUE);
                    }
                }
                child = gtk_widget_get_next_sibling(child);
            }
        }), list);

        (*p_refresh)();

        g_object_set_data(G_OBJECT(scroll), "dm-tasks", tasks.get());

        gtk_stack_add_named(GTK_STACK(stack), scroll, "page7");
    }

    /* ---- 资源详情页 (点击资源项后进入) ---- */
    {
        GtkWidget* detail = build_resource_detail_page();
        gtk_stack_add_named(GTK_STACK(stack), detail, "detail");
    }

    /* ---- MC 版本详情/安装页 (点击版本项后进入) ---- */
    {
        GtkWidget* mc_detail = build_mc_version_detail_page();
        gtk_stack_add_named(GTK_STACK(stack), mc_detail, "mc-detail");
    }

    /* ---- 导航点击: 切换 Stack 页面 ---- */
    for (int i = 0; i < 7; i++) {
        /* 行容器: nav_item (hexpand) + 浮动刷新按钮 */
        GtkWidget* wrapper = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(wrapper, "nav-item");

        GtkWidget* row = build_nav_item(navs[i].label, navs[i].icon, false);
        gtk_widget_remove_css_class(row, "nav-item");  /* wrapper 接管 styling */
        gtk_widget_set_hexpand(row, TRUE);
        gtk_box_append(GTK_BOX(wrapper), row);
        g_object_set_data(G_OBJECT(row), "stack", stack);
        g_object_set_data(G_OBJECT(row), "page", GINT_TO_POINTER(navs[i].page));

        /* 初始激活第一个 wrapper (而非内层 nav-item) */
        if (i == 0)
            gtk_widget_add_css_class(wrapper, "nav-item-active");

        /* 刷新按钮 — 所有资源入口 (Minecraft ~ 光影, 即 page 0–5) */
        if (i <= 5) {
            GtkWidget* refresh_btn = gtk_button_new();
            gtk_button_set_has_frame(GTK_BUTTON(refresh_btn), FALSE);
            gtk_widget_add_css_class(refresh_btn, "nav-refresh-btn");
            gtk_widget_set_tooltip_text(refresh_btn, "刷新");
            gtk_widget_set_valign(refresh_btn, GTK_ALIGN_CENTER);
            gtk_widget_set_size_request(refresh_btn, 26, 26);

            GtkWidget* ref_icon = icon::load("refresh-cw", 14);
            gtk_button_set_child(GTK_BUTTON(refresh_btn), ref_icon);
            gtk_box_append(GTK_BOX(wrapper), refresh_btn);

            // 储存上下文供刷新回调使用
            g_object_set_data(G_OBJECT(refresh_btn), "stack", stack);
            g_object_set_data(G_OBJECT(refresh_btn), "page",
                              GINT_TO_POINTER(navs[i].page));

            g_signal_connect(refresh_btn, "clicked",
                G_CALLBACK(+[](GtkWidget* btn, gpointer) {
                    auto* stk = static_cast<GtkWidget*>(
                        g_object_get_data(G_OBJECT(btn), "stack"));
                    int pg = GPOINTER_TO_INT(
                        g_object_get_data(G_OBJECT(btn), "page"));

                    if (pg == 0) {
                        /* Minecraft — 重新拉取版本清单 */
                        auto* w = static_cast<McWidgets*>(
                            g_object_get_data(G_OBJECT(stk), "mc_widgets"));
                        if (w) {
                            w->fetch_started = false;
                            do_mc_fetch(w, nullptr);
                        }
                    } else {
                        /* 资源分类 — 重新搜索 */
                        char page_name[16];
                        snprintf(page_name, sizeof(page_name), "page%d", pg);
                        GtkWidget* child = gtk_stack_get_child_by_name(
                            GTK_STACK(stk), page_name);
                        if (child) {
                            auto* rw = static_cast<ResourceWidgets*>(
                                g_object_get_data(G_OBJECT(child),
                                                  "res_widgets"));
                            if (rw) do_resource_search(rw);
                        }
                    }
                }), nullptr);
        }

        /* 手势点击 — 切换页面 (挂在整个 wrapper 上，但点击刷新按钮不会触发) */
        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
            GtkWidget* target = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
            GtkWidget* stk = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(target), "stack"));
            int page = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(target), "page"));

            char page_name[16];
            snprintf(page_name, sizeof(page_name), "page%d", page);
            gtk_stack_set_visible_child_name(GTK_STACK(stk), page_name);

            // 互斥高亮 — 激活态作用在 wrapper 上 (覆盖整行包括刷新按钮)
            GtkWidget* wrapper = gtk_widget_get_parent(target);
            GtkWidget* left_nav = gtk_widget_get_parent(wrapper);
            for (GtkWidget* sib = gtk_widget_get_first_child(left_nav);
                 sib; sib = gtk_widget_get_next_sibling(sib))
            {
                if (sib == wrapper)
                    gtk_widget_add_css_class(sib, "nav-item-active");
                else
                    gtk_widget_remove_css_class(sib, "nav-item-active");
            }

            /* 资源页面首次访问时自动触发默认浏览搜索
             * (匹配 PCL-CE PageComp 在 PageComp_IsVisibleChanged 时的行为) */
            if (page >= 1 && page <= 5) {
                GtkWidget* child = gtk_stack_get_child_by_name(
                    GTK_STACK(stk), page_name);
                if (child) {
                    auto* rw = static_cast<ResourceWidgets*>(
                        g_object_get_data(G_OBJECT(child), "res_widgets"));
                    if (rw && !rw->default_loaded)
                        do_resource_search(rw);
                }
            }

        }), nullptr);
        // 只把 click gesture 挂在 nav_item row 上，避免刷新按钮误触发页面切换
        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));

        gtk_box_append(GTK_BOX(left), wrapper);
    }

    /* ── 下载管理器 — 固定在侧边栏底部 ── */
    {
        /* 弹性空间将下方内容推到底部 */
        GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_vexpand(spacer, TRUE);
        gtk_box_append(GTK_BOX(left), spacer);

        /* 分隔线 */
        GtkWidget* sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_margin_start(sep, 8);
        gtk_widget_set_margin_end(sep, 8);
        gtk_box_append(GTK_BOX(left), sep);

        GtkWidget* wrapper = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(wrapper, "nav-item");

        GtkWidget* item = build_nav_item("下载管理器", "download", false);
        gtk_widget_remove_css_class(item, "nav-item");  /* wrapper 接管 styling */
        gtk_widget_set_hexpand(item, TRUE);
        gtk_box_append(GTK_BOX(wrapper), item);
        g_object_set_data(G_OBJECT(item), "stack", stack);
        g_object_set_data(G_OBJECT(item), "page", GINT_TO_POINTER(7));

        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(+[](GtkGesture* g, int, double, double, gpointer) {
            GtkWidget* target = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(g));
            GtkWidget* stk = static_cast<GtkWidget*>(g_object_get_data(G_OBJECT(target), "stack"));
            gtk_stack_set_visible_child_name(GTK_STACK(stk), "page7");

            GtkWidget* wrapper2 = gtk_widget_get_parent(target);
            GtkWidget* left_nav = gtk_widget_get_parent(wrapper2);
            for (GtkWidget* sib = gtk_widget_get_first_child(left_nav);
                 sib; sib = gtk_widget_get_next_sibling(sib))
            {
                if (!gtk_widget_has_css_class(sib, "nav-item")) continue;
                if (sib == wrapper2)
                    gtk_widget_add_css_class(sib, "nav-item-active");
                else
                    gtk_widget_remove_css_class(sib, "nav-item-active");
            }
        }), nullptr);
        gtk_widget_add_controller(item, GTK_EVENT_CONTROLLER(click));

        gtk_box_append(GTK_BOX(left), wrapper);
    }

    /* 把 McWidgets 和 mc_scroll 挂到 stack 上供导航回调使用 */
    g_object_set_data(G_OBJECT(stack), "mc_widgets", w);
    g_object_set_data(G_OBJECT(stack), "mc_scroll", mc_scroll);

    GtkWidget* page = build_two_panel_page(left, stack);
    /* 也挂到外层 page 供 create_main_window 的 AdwViewStack 信号使用 */
    g_object_set_data(G_OBJECT(page), "mc_widgets", w);
    g_object_set_data(G_OBJECT(page), "mc_scroll", mc_scroll);
    /* 内部引用 — 供详情页导航使用 (NavigationController + 向后兼容) */
    GtkWidget* sep = gtk_widget_get_next_sibling(left);
    NavigationController::instance().register_download(left, sep, stack);
    g_object_set_data(G_OBJECT(page), "dl-stack", stack);
    g_object_set_data(G_OBJECT(page), "dl-sidebar", left);
    if (sep) g_object_set_data(G_OBJECT(page), "dl-sep", sep);
    return page;
}

/* ═══════════════════════════════════════════════════════════════════════
 * do_mc_fetch — 触发版本列表拉取 (用户首次切换到 Minecraft 页面时调用)
 * ═══════════════════════════════════════════════════════════════════════ */
void do_mc_fetch(McWidgets* w, GtkWidget* /*mc_scroll_w*/)
{
    if (w->fetch_started) return;
    w->fetch_started = true;

    /* 显示 spinner (已在构造时挂载在 header 中, 直接显示 + 启动即可) */
    if (w->load_spinner) {
        gtk_widget_set_visible(w->load_spinner, TRUE);
        gtk_spinner_start(GTK_SPINNER(w->load_spinner));
    }

    LOG_INFO("MainWindow: starting version manifest fetch");

    /* ★ w 指针直接捕获，回调里用 w->latest_ver[i] 等，跳过 key 查找 */
    mc::fetch_version_manifest([w](mc::VersionManifest m) {
        LOG_INFO("MainWindow: manifest callback (loaded=%d, error=%s)",
                 m.loaded, m.error.c_str());

        /* Hide spinner — 永久挂载, 不 remove */
        if (w->load_spinner) {
            gtk_spinner_stop(GTK_SPINNER(w->load_spinner));
            gtk_widget_set_visible(w->load_spinner, FALSE);
        }

        if (!m.loaded || m.groups.empty()) {
            LOG_WARN("MainWindow: fetch failed — showing error");
            for (int i = 0; i < 2; i++) {
                if (w->latest_ver[i])
                    gtk_label_set_text(GTK_LABEL(w->latest_ver[i]),
                        m.error.empty() ? "加载失败" : m.error.c_str());
                if (w->latest_note[i])
                    gtk_label_set_text(GTK_LABEL(w->latest_note[i]),
                        "请检查网络连接后重试");
            }
            return;
        }

        LOG_INFO("MainWindow: populating UI — %lu groups",
                 (unsigned long)m.groups.size());

        /* ── 更新最新版本行 ── */
        for (int i = 0; i < 2; i++) {
            const auto& group = m.groups[i];   // 0=release, 1=snapshot
            if (!group.versions.empty()) {
                const auto& v = group.versions[0];

                if (w->latest_ver[i])
                    gtk_label_set_text(GTK_LABEL(w->latest_ver[i]),
                                       v.id.c_str());

                if (w->latest_note[i]) {
                    char buf[128];
                    struct tm* lt = localtime(&v.timestamp);
                    char date[48] = "—";
                    if (lt) strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", lt);
                    const char* label = (i == 0) ? "最新正式版" : "最新开发版";
                    snprintf(buf, sizeof(buf), "%s · 发布于 %s", label, date);
                    gtk_label_set_text(GTK_LABEL(w->latest_note[i]), buf);
                }
            }
        }

        /* ── 加载本地愚人节版本列表 (API 不返回 april_fool 类型) ── */
        {
            auto local_af = mc::load_lirpa_loof_versions();
            if (!local_af.empty()) {
                // Group 3 = "愚人节版本"
                auto& af_group = m.groups[3];
                for (auto& v : local_af)
                    af_group.versions.push_back(std::move(v));
                // Re-sort: newest first (they're already sorted, but be safe)
                std::sort(af_group.versions.begin(), af_group.versions.end(),
                          [](const mc::VersionEntry& a, const mc::VersionEntry& b) {
                              return a.timestamp > b.timestamp;
                          });
                LOG_INFO("MainWindow: merged %lu local april fool versions",
                         (unsigned long)local_af.size());
            }
        }

        /* ── 分批填充可展开分类列表 ──
         *
         *  一次性创建 200+ 个复杂 widget 行 (每行含图标、Pango 属性、
         *  手势控制器、悬停按钮等) 会阻塞主线程 0.5–2 s。在平铺式 WM
         *  中, 如果用户切换桌面后这批 widget 才在主线程创建, GTK 会累
         *  积大量推迟的渲染操作 — 切回时窗口需要数秒才能恢复响应。
         *
         *  因此将版本行创建拆分为小批次 (每批 ≤25 行), 批次之间通过
         *  g_idle_add 交还主循环, 确保 UI 始终能及时处理输入事件。 ── */
        {
            struct BatchState {
                McWidgets* w;
                mc::VersionManifest m;
                int        group;       // 0–3
                size_t     idx;         // index within current group
                enum : size_t { CHUNK = 25 };
            };

            auto* bs = new BatchState{w, std::move(m), 0, 0};

            /* 清空 4 个分类列表的旧数据 (一次性, 很快) */
            for (int g = 0; g < 4; g++) {
                GtkWidget* list = w->group_list[g];
                if (!list) continue;
                GtkWidget* child;
                while ((child = gtk_widget_get_first_child(list)))
                    gtk_list_box_remove(GTK_LIST_BOX(list), child);
            }

            auto populate_chunk = [](gpointer data) -> gboolean {
                auto* s = static_cast<BatchState*>(data);

                /* 找到第一个非空的分组 */
                while (s->group < 4) {
                    GtkWidget* list = s->w->group_list[s->group];
                    if (list && s->idx < s->m.groups[s->group].versions.size())
                        break;
                    /* 此分组已处理完 → 更新 header, 前进到下一分组 */
                    if (s->w->group_header_label[s->group]) {
                        char title[64];
                        int total = static_cast<int>(
                            s->m.groups[s->group].versions.size());
                        snprintf(title, sizeof(title), "%s (%d)",
                                 s->m.groups[s->group].label.c_str(), total);
                        gtk_label_set_text(GTK_LABEL(
                            s->w->group_header_label[s->group]), title);
                    }
                    s->group++;
                    s->idx = 0;
                }

                if (s->group >= 4) {
                    /* 全部分组处理完毕 */
                    LOG_INFO("MainWindow: UI population complete");
                    delete s;
                    return G_SOURCE_REMOVE;
                }

                auto& grp = s->m.groups[s->group];
                GtkWidget* list = s->w->group_list[s->group];
                const char* block_icon = grp.block_icon.c_str();

                /* 处理一批 (最多 CHUNK 个) */
                size_t end = std::min(s->idx + BatchState::CHUNK,
                                      grp.versions.size());
                for (; s->idx < end; s->idx++) {
                    auto& v = grp.versions[s->idx];

                    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
                    gtk_widget_set_margin_start(row, 8);
                    gtk_widget_set_margin_end(row, 8);
                    gtk_widget_set_margin_top(row, 4);
                    gtk_widget_set_margin_bottom(row, 4);

                    /* 图标 */
                    GtkWidget* icn = icon::load_block(block_icon, 24);
                    gtk_widget_set_valign(icn, GTK_ALIGN_START);
                    gtk_widget_set_margin_top(icn, 2);
                    gtk_box_append(GTK_BOX(row), icn);

                    /* 版本号 + 日期 */
                    GtkWidget* mid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
                    gtk_widget_set_hexpand(mid, TRUE);
                    gtk_widget_set_valign(mid, GTK_ALIGN_CENTER);
                    gtk_box_append(GTK_BOX(row), mid);

                    GtkWidget* ver_lbl = gtk_label_new(v.id.c_str());
                    gtk_label_set_xalign(GTK_LABEL(ver_lbl), 0.0f);
                    gtk_box_append(GTK_BOX(mid), ver_lbl);
                    {
                        PangoAttrList* attrs = pango_attr_list_new();
                        pango_attr_list_insert(attrs,
                            pango_attr_weight_new(PANGO_WEIGHT_SEMIBOLD));
                        gtk_label_set_attributes(GTK_LABEL(ver_lbl), attrs);
                        pango_attr_list_unref(attrs);
                    }

                    GtkWidget* date_lbl;
                    const char* af_prefix = "april_fool_local:";
                    if (v.url.compare(0, strlen(af_prefix), af_prefix) == 0) {
                        std::string desc = v.url.substr(strlen(af_prefix));
                        char date_buf[48] = "—";
                        if (v.timestamp > 0) {
                            struct tm* lt = localtime(&v.timestamp);
                            if (lt) strftime(date_buf, sizeof(date_buf),
                                             "%Y-%m-%d", lt);
                        }
                        std::string combined = std::string(date_buf) + " | " + desc;
                        date_lbl = gtk_label_new(combined.c_str());
                    } else {
                        char date_buf[48] = "—";
                        if (v.timestamp > 0) {
                            struct tm* lt = localtime(&v.timestamp);
                            if (lt) strftime(date_buf, sizeof(date_buf),
                                             "%Y-%m-%d %H:%M:%S", lt);
                        }
                        date_lbl = gtk_label_new(date_buf);
                    }
                    gtk_label_set_xalign(GTK_LABEL(date_lbl), 0.0f);
                    gtk_widget_set_opacity(date_lbl, 0.55);
                    gtk_box_append(GTK_BOX(mid), date_lbl);

                    /* 悬停操作按钮 */
                    GtkWidget* actions = build_version_actions();
                    gtk_box_append(GTK_BOX(row), actions);
                    attach_row_hover(row, actions);

                    /* 点击整行 → 导航到 MC 版本详情页 */
                    {
                        auto* nd = new std::pair<std::string, std::string>(v.id, v.type);
                        GtkGesture* click = gtk_gesture_click_new();

                        /* 按下时 — 添加按下阴影反馈 */
                        g_signal_connect(click, "pressed",
                            (GCallback)(+[](GtkGesture* g, int, double, double, gpointer) {
                                GtkWidget* target = gtk_event_controller_get_widget(
                                    GTK_EVENT_CONTROLLER(g));
                                GtkWidget* list_row = gtk_widget_get_parent(target);
                                if (list_row)
                                    gtk_widget_add_css_class(list_row, "ver-group-row-pressed");
                            }), nullptr);

                        /* 释放/取消时 — 移除按下阴影反馈 */
                        auto remove_pressed_class = +[](GtkGesture* g, int, double, double, gpointer) {
                            GtkWidget* target = gtk_event_controller_get_widget(
                                GTK_EVENT_CONTROLLER(g));
                            GtkWidget* list_row = gtk_widget_get_parent(target);
                            if (list_row)
                                gtk_widget_remove_css_class(list_row, "ver-group-row-pressed");
                        };
                        g_signal_connect(click, "released", (GCallback)remove_pressed_class, nullptr);
                        g_signal_connect(click, "cancel",   (GCallback)remove_pressed_class, nullptr);

                        g_signal_connect(click, "pressed",
                            (GCallback)(+[](GtkGesture* g, int, double, double, gpointer d) {
                                auto* p = static_cast<std::pair<std::string, std::string>*>(d);
                                GtkWidget* target = gtk_event_controller_get_widget(
                                    GTK_EVENT_CONTROLLER(g));
                                navigate_to_mc_version_detail(target, p->first, p->second);
                                delete p;
                            }), nd);
                        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
                    }

                    gtk_list_box_append(GTK_LIST_BOX(list), row);
                }

                if (s->idx >= grp.versions.size()) {
                    /* 当前分组处理完 → 更新 header */
                    if (s->w->group_header_label[s->group]) {
                        char title[64];
                        int total = static_cast<int>(grp.versions.size());
                        snprintf(title, sizeof(title), "%s (%d)",
                                 grp.label.c_str(), total);
                        gtk_label_set_text(GTK_LABEL(
                            s->w->group_header_label[s->group]), title);
                    }
                    s->group++;
                    s->idx = 0;
                }

                if (s->group >= 4) {
                    LOG_INFO("MainWindow: UI population complete");
                    delete s;
                    return G_SOURCE_REMOVE;
                }

                /* 还有更多版本 → 继续下一批 */
                return G_SOURCE_CONTINUE;
            };

            g_idle_add(+populate_chunk, bs);
        }
    });
}



void trigger_download_page_mc_fetch(GtkWidget* download_page)
{
    auto* w = static_cast<McWidgets*>(
        g_object_get_data(G_OBJECT(download_page), "mc_widgets"));
    if (w) do_mc_fetch(w, nullptr);
}

}  // namespace pcl
