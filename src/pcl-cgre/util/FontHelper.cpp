#include "util/FontHelper.hpp"

#include <cstdlib>
#include <string>

#include <fontconfig/fontconfig.h>
#include <glib.h>

#if defined(__linux__) && !defined(__ANDROID__)
#include <limits.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace pcl::font {

namespace {

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

/** Check whether a directory exists and contains at least one .ttf file. */
bool has_fonts(const std::string& dir)
{
    auto path = dir + "/HarmonyOS_Sans_Regular.ttf";
    return g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR);
}

}  // anonymous namespace

void load_custom_fonts()
{
    std::string font_dir;

    // 1.  Environment variable override
    const char* env = std::getenv("PCL_FONT_DIR");
    if (env && has_fonts(env)) {
        font_dir = env;
    }

    // 2.  Relative to binary (build tree)
    if (font_dir.empty()) {
        std::string bin_dir = resolve_binary_dir();
        if (!bin_dir.empty()) {
            auto build_path = bin_dir + "/resources/fonts";
            if (has_fonts(build_path)) {
                font_dir = build_path;
            } else {
                auto dev_path = bin_dir + "/../resources/fonts";
                if (has_fonts(dev_path))
                    font_dir = dev_path;
            }
        }
    }

    if (font_dir.empty()) {
        g_warning("FontHelper: cannot find HarmonyOS Sans fonts.  "
                  "Set PCL_FONT_DIR or install fonts to resources/fonts/");
        return;
    }

    // Register each .ttf file with Fontconfig
    FcConfig* config = FcConfigGetCurrent();
    GDir* dir = g_dir_open(font_dir.c_str(), 0, nullptr);
    if (!dir) return;

    int count = 0;
    const char* name;
    while ((name = g_dir_read_name(dir))) {
        std::string path = font_dir + "/" + name;
        if (path.size() > 4 && path.compare(path.size() - 4, 4, ".ttf") == 0) {
            if (FcConfigAppFontAddFile(config,
                    reinterpret_cast<const FcChar8*>(path.c_str()))) {
                count++;
            }
        }
    }
    g_dir_close(dir);

    g_message("FontHelper: registered %d custom font(s) from %s",
              count, font_dir.c_str());
}

}  // namespace pcl::font
