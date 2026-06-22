#include "util/IconHelper.hpp"

#include <cstdlib>
#include <string>
#include <unordered_map>

#if defined(__linux__) && !defined(__ANDROID__)
#include <limits.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace pcl::icon {

namespace {

std::string g_icon_base;  // e.g. "/usr/share/icons" — the parent of "pcl-cgre/"

/* ═══════════════════════════════════════════════════════════════════════
 *  Icon cache — caches GdkTexture (shareable), not GtkWidget (single-parent).
 *  Each load() call creates a fresh GtkImage from the cached texture.
 * ═══════════════════════════════════════════════════════════════════════ */
std::unordered_map<std::string, GdkTexture*> g_icon_cache;

/** Check whether a directory contains pcl-cgre/index.theme. */
bool has_pcl_icon_theme(const std::string& dir)
{
    auto path = dir + "/pcl-cgre/index.theme";
    return g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR);
}

}  // anonymous namespace

/** Resolve the binary's own directory. */
std::string resolve_binary_dir()
{
#if defined(__linux__) && !defined(__ANDROID__)
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
        buf[len] = '\0';
        std::string exe(buf);
        auto slash = exe.rfind('/');
        if (slash != std::string::npos)
            return exe.substr(0, slash);
    }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::string exe(buf, len);
        auto slash = exe.rfind('\\');
        if (slash != std::string::npos)
            return exe.substr(0, slash);
    }
#endif
    return {};
}

void init_icon_theme()
{
    // 1.  Environment variable override
    const char* env = std::getenv("PCL_ICON_DIR");
    if (env && has_pcl_icon_theme(env)) {
        g_icon_base = env;
    }

    // 2.  Relative to binary (dev / build tree)
    if (g_icon_base.empty()) {
        std::string bin_dir = resolve_binary_dir();
        if (!bin_dir.empty()) {
            // binary is in build/, icons are in build/resources/icons/
            auto build_path = bin_dir + "/resources/icons";
            if (has_pcl_icon_theme(build_path)) {
                g_icon_base = build_path;
            } else {
                // binary is in build/, icons are in ../resources/icons/
                auto dev_path = bin_dir + "/../resources/icons";
                if (has_pcl_icon_theme(dev_path))
                    g_icon_base = dev_path;
            }
        }
    }

    // 3.  Compile-time install prefix
#ifdef PCL_ICON_THEME_DIR
    if (g_icon_base.empty() && has_pcl_icon_theme(PCL_ICON_THEME_DIR)) {
        g_icon_base = PCL_ICON_THEME_DIR;
    }
#endif

    // 4.  System install location (hardcoded fallback)
    if (g_icon_base.empty() && has_pcl_icon_theme("/usr/share/icons")) {
        g_icon_base = "/usr/share/icons";
    }

    if (g_icon_base.empty()) {
        g_warning("IconHelper: cannot find pcl-cgre icon theme.  "
                  "Set PCL_ICON_DIR or install icons to /usr/share/icons/");
        return;
    }

    // Register with GTK
    GtkIconTheme* theme = gtk_icon_theme_get_for_display(
        gdk_display_get_default());
    gtk_icon_theme_add_search_path(theme, g_icon_base.c_str());
}

GtkWidget* load(const char* name, int px_size)
{
    // Build cache key: "name:size"
    std::string key(name);
    key += ':';
    key += std::to_string(px_size);

    GdkTexture* tex = nullptr;
    auto it = g_icon_cache.find(key);
    if (it != g_icon_cache.end()) {
        tex = it->second;
    } else {
        GError* err = nullptr;

        /* 1. Try GResource (embedded) */
        char* rpath = g_strdup_printf(
            "/pcl/cgre/icons/scalable/actions/pcl-%s.svg", name);
        GBytes* bytes = g_resources_lookup_data(rpath,
            G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if (bytes) {
            GdkTexture* t = gdk_texture_new_from_bytes(bytes, &err);
            g_bytes_unref(bytes);
            if (t) {
                g_icon_cache.emplace(std::move(key), t);
                tex = t;
            }
        }
        g_clear_error(&err);
        g_free(rpath);

        /* 2. Fallback to disk */
        if (!tex) {
            std::string file_path = g_icon_base + "/pcl-cgre/scalable/actions/pcl-";
            file_path += name;
            file_path += ".svg";
            tex = gdk_texture_new_from_filename(file_path.c_str(), &err);
            if (tex) {
                g_icon_cache.emplace(std::move(key), tex);
            } else {
                g_warning("IconHelper: cannot load '%s': %s", file_path.c_str(),
                          err ? err->message : "unknown error");
                g_clear_error(&err);
                return gtk_image_new();
            }
        }
    }

    // Create a fresh GtkImage from the shared texture
    GtkWidget* image = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
    gtk_image_set_pixel_size(GTK_IMAGE(image), px_size);
    return image;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Block image cache — GdkTexture (shareable)
 * ═══════════════════════════════════════════════════════════════════════ */
static std::unordered_map<std::string, GdkTexture*> g_block_cache;

GtkWidget* load_block(const char* name, int px_size)
{
    std::string key = std::string(name) + ":" + std::to_string(px_size);

    GdkTexture* tex = nullptr;
    auto it = g_block_cache.find(key);
    if (it != g_block_cache.end()) {
        tex = it->second;
    } else {
        GError* err = nullptr;

        /* 1. Try GResource (embedded) */
        char* rpath = g_strdup_printf("/pcl/cgre/blocks/%s.png", name);
        GBytes* bytes = g_resources_lookup_data(rpath,
            G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if (bytes) {
            tex = gdk_texture_new_from_bytes(bytes, &err);
            g_bytes_unref(bytes);
        }
        g_clear_error(&err);
        g_free(rpath);

        /* 2. Fallback to disk */
        if (!tex) {
            std::string block_path;
            const char* env = std::getenv("PCL_RES_DIR");
            if (env) {
                block_path = std::string(env) + "/blocks/" + name + ".png";
                if (!g_file_test(block_path.c_str(), G_FILE_TEST_IS_REGULAR))
                    block_path.clear();
            }
            if (block_path.empty()) {
                std::string bin_dir = resolve_binary_dir();
                if (!bin_dir.empty()) {
                    auto build = bin_dir + "/resources/blocks/" + name + ".png";
                    if (g_file_test(build.c_str(), G_FILE_TEST_IS_REGULAR))
                        block_path = build;
                    else {
                        auto dev = bin_dir + "/../resources/blocks/" + name + ".png";
                        if (g_file_test(dev.c_str(), G_FILE_TEST_IS_REGULAR))
                            block_path = dev;
                    }
                }
            }
            if (!block_path.empty()) {
                tex = gdk_texture_new_from_filename(block_path.c_str(), &err);
            }
        }

        if (!tex) {
            g_warning("IconHelper: cannot load block '%s'", name);
            return gtk_image_new();
        }
        g_block_cache.emplace(std::move(key), tex);
    }

    GtkWidget* image = gtk_image_new_from_paintable(GDK_PAINTABLE(tex));
    gtk_image_set_pixel_size(GTK_IMAGE(image), px_size);
    return image;
}

GtkBuilder* load_ui(const char* name)
{
    /* 1. Try GResource (embedded) */
    char* rpath = g_strdup_printf("/pcl/cgre/data/ui/%s", name);
    GtkBuilder* b = gtk_builder_new_from_resource(rpath);
    if (b) {
        g_free(rpath);
        return b;
    }
    g_free(rpath);

    /* 2. Fallback to disk */
    std::string path = resolve_binary_dir() + "/data/ui/" + name;
    return gtk_builder_new_from_file(path.c_str());
}

}  // namespace pcl::icon
