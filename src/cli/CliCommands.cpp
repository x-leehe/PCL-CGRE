#include "CliCommands.hpp"

#include "pclcore/network/Dispatcher.hpp"
#include "pclcore/local/Instance.hpp"
#include "pclcore/local/CrashReport.hpp"
#include "pclcore/local/Account.hpp"
#include "pclcore/local/DownloadTask.hpp"

#include "network/McVersionFetcher.hpp"
#include "network/LoaderFetcher.hpp"
#include "network/ResourceFetcher.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif
#include <future>
#include <string>
#include <vector>

/* ──────────────────────────────────────────────────────────────────────────
 *  Forward declarations of all subcommand handlers
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_help(bool json_output);
static int cmd_version(bool json_output);

static int cmd_search(int argc, char* argv[]);
static int cmd_list(int argc, char* argv[]);

static int cmd_install(int argc, char* argv[]);

static int cmd_instance(int argc, char* argv[]);
static int cmd_crashspy();
static int cmd_account(int argc, char* argv[]);
static int cmd_settings();
static int cmd_plugin(int argc, char* argv[]);
static int cmd_run(int argc, char* argv[]);

/* ──────────────────────────────────────────────────────────────────────────
 *  Utilities
 * ──────────────────────────────────────────────────────────────────────── */

/** Print to stderr and return 1. */
static int fail(const char* msg)
{
    std::fprintf(stderr, "Error: %s\n", msg);
    return 1;
}

/** Check whether @a s matches a long-form flag. */
static inline bool is_opt(const char* s, const char* long_name)
{
    return std::strcmp(s, long_name) == 0;
}

/** String → ProjectType (user-facing category names). */
static pcl::resource::ProjectType parse_project_type(const char* s)
{
    if (std::strcmp(s, "mod") == 0)       return pcl::resource::ProjectType::Mod;
    if (std::strcmp(s, "modpack") == 0)   return pcl::resource::ProjectType::Modpack;
    if (std::strcmp(s, "respack") == 0)   return pcl::resource::ProjectType::ResourcePack;
    if (std::strcmp(s, "datapack") == 0)  return pcl::resource::ProjectType::Datapack;
    if (std::strcmp(s, "shader") == 0)    return pcl::resource::ProjectType::Shader;
    return pcl::resource::ProjectType::Mod;  // default
}

static const char* project_type_name(pcl::resource::ProjectType t)
{
    switch (t) {
        case pcl::resource::ProjectType::Mod:          return "mod";
        case pcl::resource::ProjectType::Modpack:      return "modpack";
        case pcl::resource::ProjectType::ResourcePack: return "respack";
        case pcl::resource::ProjectType::Datapack:     return "datapack";
        case pcl::resource::ProjectType::Shader:       return "shader";
        case pcl::resource::ProjectType::World:        return "world";
        default: return "unknown";
    }
}

static pcl::resource::Source parse_source(const char* s)
{
    if (std::strcmp(s, "modrinth") == 0)   return pcl::resource::Source::Modrinth;
    if (std::strcmp(s, "curseforge") == 0) return pcl::resource::Source::CurseForge;
    return pcl::resource::Source::All;
}

static const char* source_name(pcl::resource::Source s)
{
    switch (s) {
        case pcl::resource::Source::Modrinth:   return "Modrinth";
        case pcl::resource::Source::CurseForge: return "CurseForge";
        default: return "All";
    }
}

static pcl::resource::CompLoaderType parse_loader(const char* s)
{
    if (!s || !*s) return pcl::resource::CompLoaderType::Any;
    if (std::strcmp(s, "forge") == 0)      return pcl::resource::CompLoaderType::Forge;
    if (std::strcmp(s, "fabric") == 0)     return pcl::resource::CompLoaderType::Fabric;
    if (std::strcmp(s, "quilt") == 0)      return pcl::resource::CompLoaderType::Quilt;
    if (std::strcmp(s, "neoforge") == 0)   return pcl::resource::CompLoaderType::NeoForge;
    if (std::strcmp(s, "liteloader") == 0) return pcl::resource::CompLoaderType::LiteLoader;
    return pcl::resource::CompLoaderType::Any;
}

static const char* loader_name(pcl::resource::CompLoaderType l)
{
    switch (l) {
        case pcl::resource::CompLoaderType::Forge:      return "Forge";
        case pcl::resource::CompLoaderType::Fabric:     return "Fabric";
        case pcl::resource::CompLoaderType::Quilt:      return "Quilt";
        case pcl::resource::CompLoaderType::NeoForge:   return "NeoForge";
        case pcl::resource::CompLoaderType::LiteLoader: return "LiteLoader";
        default: return "";
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_help
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_help(bool /*json_output*/)
{
    std::printf("PCL-CGRE — Plain Craft Launcher GTK Rewritten Edition\n\n");
    std::printf("Usage:\n");
    std::printf("  pcl-cgre --help [--json]\n");
    std::printf("  pcl-cgre --gui                    Start graphical interface\n\n");

    std::printf("  pcl-cgre version [--json]         Print launcher version\n\n");

    std::printf("  pcl-cgre run [accountname] [instancename]\n");
    std::printf("                                    Launch Minecraft\n\n");

    std::printf("  pcl-cgre search <category> <query> [options]\n");
    std::printf("    Categories:\n");
    std::printf("      mod|modpack|respack|datapack|shader  Minecraft resources\n");
    std::printf("      minecraft [rel|devel|alpha|april]    Minecraft versions\n");
    std::printf("      minecraft latest                     Latest release & snapshot\n");
    std::printf("      plugin                               PCL-CGRE plugins\n");
    std::printf("    Options: --type t --source modrinth|curseforge --loader forge|fabric|quilt|neoforge|liteloader --limit N --json\n\n");

    std::printf("  pcl-cgre list ...                 Alias for 'pcl-cgre search'\n\n");

    std::printf("  pcl-cgre install <type> <query> [options]\n");
    std::printf("    Interactive mod/resource pack installation\n\n");

    std::printf("  pcl-cgre instance list [--json]   List local instances\n");
    std::printf("  pcl-cgre instance <name> start     Launch an instance\n");
    std::printf("  pcl-cgre instance <name> setdefault\n");
    std::printf("  pcl-cgre instance <name> account use <account>\n\n");

    std::printf("  pcl-cgre crashspy                 Deprecated — check log files\n\n");

    std::printf("  pcl-cgre account list [--json]    List accounts\n");
    std::printf("  pcl-cgre account <name> skin <set> <filename>\n");
    std::printf("  pcl-cgre account <name> download [dir]\n");
    std::printf("  pcl-cgre account <name> setdefault\n\n");

    std::printf("  pcl-cgre settings                 Deprecated — edit config file\n\n");

    std::printf("  pcl-cgre plugin list\n");
    std::printf("  pcl-cgre plugin <name> <command>\n");

    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_version  —  print launcher version
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_version(bool json_output)
{
    if (json_output)
        std::printf("{\"name\": \"PCL-CGRE\", \"version\": \"" PCL_CGRE_VERSION "\"}\n");
    else
        std::printf("PCL-CGRE version " PCL_CGRE_VERSION "\n");
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_search  —  pcl-cgre search <category> <query> [options]
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_search(int argc, char* argv[])
{
    if (argc < 3) return fail("search requires a category and query");

    const char* category = argv[2];

    /* ── search minecraft ───────────────────────────────────────────── */
    if (std::strcmp(category, "minecraft") == 0) {
        const char* filter = (argc > 3 && argv[3][0] != '-') ? argv[3] : nullptr;
        bool json_output = false;
        for (int i = 3; i < argc; i++)
            if (is_opt(argv[i], "--json")) json_output = true;

        if (filter && std::strcmp(filter, "latest") == 0) {
            /* Latest release + latest snapshot from actual version lists */
            std::promise<pcl::mc::VersionManifest> p;
            auto f = p.get_future();
            pcl::mc::fetch_version_manifest(
                [&p](pcl::mc::VersionManifest m) { p.set_value(std::move(m)); });
            auto manifest = f.get();

            if (!manifest.loaded)
                return fail(manifest.error.c_str());

            /* Merge lirpa_loof for completeness */
            {
                auto local_af = pcl::mc::load_lirpa_loof_versions();
                if (!local_af.empty()) {
                    auto& af_group = manifest.groups[3];
                    for (auto& v : local_af)
                        af_group.versions.push_back(std::move(v));
                }
            }

            /* Groups are pre-sorted by timestamp desc, so g.versions[0]
             * is the latest version in each group.  This is more reliable
             * than the API's `latest` field (which may lag behind). */
            std::string latest_rel, latest_snap;
            for (const auto& g : manifest.groups) {
                if (g.type_filter == "release" && !g.versions.empty())
                    latest_rel = g.versions[0].id;
                if (g.type_filter == "snapshot" && !g.versions.empty())
                    latest_snap = g.versions[0].id;
            }

            if (json_output) {
                std::printf("{\"latest_release\": \"%s\", \"latest_snapshot\": \"%s\"}\n",
                       latest_rel.c_str(), latest_snap.c_str());
            } else {
                std::printf("Latest release:  %s\n", latest_rel.c_str());
                std::printf("Latest snapshot: %s\n", latest_snap.c_str());
            }
            return 0;
        }

        /* Filter by type: rel, devel, alpha, april */
        std::promise<pcl::mc::VersionManifest> p;
        auto f = p.get_future();
        pcl::mc::fetch_version_manifest(
            [&p](pcl::mc::VersionManifest m) { p.set_value(std::move(m)); });
        auto manifest = f.get();

        if (!manifest.loaded)
            return fail(manifest.error.c_str());

        /* Merge local April Fool versions (API doesn't return them) */
        {
            auto local_af = pcl::mc::load_lirpa_loof_versions();
            if (!local_af.empty()) {
                auto& af_group = manifest.groups[3];
                for (auto& v : local_af)
                    af_group.versions.push_back(std::move(v));
                std::sort(af_group.versions.begin(), af_group.versions.end(),
                          [](const auto& a, const auto& b) {
                              return a.timestamp > b.timestamp;
                          });
            }
        }

        if (json_output) {
            std::printf("{\"source\": \"%s\", \"versions\": [", manifest.source_name.c_str());
            bool first = true;
            for (const auto& g : manifest.groups) {
                if (filter) {
                    if (std::strcmp(filter, "rel") == 0 && g.type_filter != "release") continue;
                    if (std::strcmp(filter, "devel") == 0 && g.type_filter != "snapshot") continue;
                    if (std::strcmp(filter, "alpha") == 0 && g.type_filter != "old_alpha") continue;
                    if (std::strcmp(filter, "april") == 0 && g.type_filter != "april_fool") continue;
                }
                for (const auto& v : g.versions) {
                    if (!first) std::printf(",");
                    std::printf("\"%s\"", v.id.c_str());
                    first = false;
                }
            }
            std::printf("]}\n");
        } else {
            for (const auto& g : manifest.groups) {
                if (filter) {
                    if (std::strcmp(filter, "rel") == 0 && g.type_filter != "release") continue;
                    if (std::strcmp(filter, "devel") == 0 && g.type_filter != "snapshot") continue;
                    if (std::strcmp(filter, "alpha") == 0 && g.type_filter != "old_alpha") continue;
                    if (std::strcmp(filter, "april") == 0 && g.type_filter != "april_fool") continue;
                }
                std::printf("%s:\n", g.label.c_str());
                for (const auto& v : g.versions)
                    std::printf("  %s\n", v.id.c_str());
            }
        }
        return 0;
    }

    /* ── search plugin ──────────────────────────────────────────────── */
    if (std::strcmp(category, "plugin") == 0) {
        std::fprintf(stderr, "Plugin search is not yet available.\n");
        std::fprintf(stderr, "Plugins installed locally can be listed with: pcl-cgre plugin list\n");
        return 1;
    }

    /* ── search mod|modpack|respack|datapack|shader ─────────────────── */
    auto type = parse_project_type(category);
    if (type == pcl::resource::ProjectType::Mod && std::strcmp(category, "mod") != 0
        && std::strcmp(category, "modpack") != 0 && std::strcmp(category, "respack") != 0
        && std::strcmp(category, "datapack") != 0 && std::strcmp(category, "shader") != 0) {
        return fail("unknown category — use mod|modpack|respack|datapack|shader|minecraft|plugin");
    }

    const char* query = (argc > 3 && argv[3][0] != '-') ? argv[3] : "";
    pcl::resource::Source source = pcl::resource::Source::All;
    pcl::resource::CompLoaderType loader = pcl::resource::CompLoaderType::Any;
    int limit = 20;
    bool json_output = false;

    for (int i = 3; i < argc; i++) {
        if (is_opt(argv[i], "--json")) {
            json_output = true;
        } else if (is_opt(argv[i], "--source") && i + 1 < argc) {
            source = parse_source(argv[++i]);
        } else if (is_opt(argv[i], "--loader") && i + 1 < argc) {
            loader = parse_loader(argv[++i]);
        } else if (is_opt(argv[i], "--limit") && i + 1 < argc) {
            limit = std::atoi(argv[++i]);
            if (limit <= 0) limit = 20;
        }
    }

    std::promise<pcl::resource::SearchResult> p;
    auto f = p.get_future();
    pcl::resource::search_resources(
        query, type, source, "", 0, limit,
        pcl::resource::SortType::Default,
        loader,
        [&p](pcl::resource::SearchResult r) {
            p.set_value(std::move(r));
        });

    auto result = f.get();

    if (!result.success && result.hits.empty())
        return fail(result.error.empty() ? "search failed" : result.error.c_str());

    if (json_output) {
        std::printf("{\"total\": %lu, \"hits\": [", (unsigned long)result.total_hits);
        for (size_t i = 0; i < result.hits.size(); i++) {
            const auto& h = result.hits[i];
            if (i) std::printf(",");
            std::printf("{\"title\": \"%s\", \"author\": \"%s\", \"downloads\": %lu, \"source\": \"%s\"}",
                   h.title.c_str(), h.author.c_str(),
                   (unsigned long)h.download_count, source_name(h.source));
        }
        std::printf("]}\n");
    } else {
        std::printf("Search: %s (type=%s, source=%s, limit=%d",
               query, project_type_name(type), source_name(source), limit);
        if (loader != pcl::resource::CompLoaderType::Any)
            std::printf(", loader=%s", loader_name(loader));
        std::printf(")\n");
        std::printf("%s\n", std::string(64, '-').c_str());
        for (size_t i = 0; i < result.hits.size(); i++) {
            const auto& h = result.hits[i];
            std::printf("%3zu. %-32s by %-16s %8s downloads [%s]\n",
                   i + 1, h.title.c_str(), h.author.c_str(),
                   pcl::resource::format_download_count(h.download_count).c_str(),
                   source_name(h.source));
        }
        if (!result.hits.empty())
            std::printf("\nTotal: %lu results\n", (unsigned long)result.total_hits);
    }
    return 0;
}

/* ── cmd_list: alias for cmd_search ───────────────────────────────────── */

static int cmd_list(int argc, char* argv[])
{
    return cmd_search(argc, argv);
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_install  —  interactive mod / resource pack installation
 *
 *  pcl-cgre install <mod|respack|datapack|shader> <query>
 *       [--instance <name>] [--prequences] [--noconfirm]
 *       [--version <ver>] [--loader <loader>] [--source modrinth|curseforge]
 *
 *  pcl-cgre install minecraft <version> [loader] [--installFAPI]
 *
 *  pcl-cgre install modpack <query> [--instance <name>] [--noconfirm]
 * ──────────────────────────────────────────────────────────────────────── */

/** Prompt for confirmation.  @a default_yes: what Enter alone means. */
static inline bool confirm(const char* prompt, bool default_yes)
{
    std::printf("%s ", prompt);
    std::fflush(stdout);
    char buf[16];
    if (!std::fgets(buf, sizeof(buf), stdin)) return false;
    if (buf[0] == '\0' || buf[0] == '\n') return default_yes;
    return (buf[0] == 'y' || buf[0] == 'Y');
}

static inline bool confirm_yes(const char* prompt) { return confirm(prompt, true); }


/* ── Check if a CompFile matches instance context ──────────────────── */
static bool version_matches(const pcl::resource::CompFile& cf,
                             const std::string& inst_ver,
                             const std::string& inst_ldr)
{
    /* MC version: exact string comparison.  Handles "1.21.1",
     * "26.2-rc-2", "1.21.11" etc. correctly. */
    bool ver_ok = inst_ver.empty() || cf.game_version == inst_ver;

    /* Loader */
    bool ldr_ok = inst_ldr.empty() || inst_ldr == "Vanilla";
    if (!ldr_ok) {
        for (auto& ml : cf.mod_loaders) {
            const char* nm = loader_name(ml);
            if (nm && strcasecmp(inst_ldr.c_str(), nm) == 0)
                { ldr_ok = true; break; }
        }
    }
    return ver_ok && ldr_ok;
}

static int install_mod_resource(int argc, char* argv[],
                                 pcl::resource::ProjectType type)
{
    if (argc < 4) return fail("install: expected a search query");

    const char* query = argv[3];
    const char* inst_name = nullptr;
    const char* version_filter = nullptr;
    bool prequences = false;
    bool no_confirm = false;
    auto loader = pcl::resource::CompLoaderType::Any;
    auto source = pcl::resource::Source::All;

    for (int i = 4; i < argc; i++) {
        if (is_opt(argv[i], "--instance") && i + 1 < argc)
            inst_name = argv[++i];
        else if (is_opt(argv[i], "--version") && i + 1 < argc)
            version_filter = argv[++i];
        else if (is_opt(argv[i], "--loader") && i + 1 < argc)
            loader = parse_loader(argv[++i]);
        else if (is_opt(argv[i], "--source") && i + 1 < argc)
            source = parse_source(argv[++i]);
        else if (is_opt(argv[i], "--prequences"))
            prequences = true;
        else if (is_opt(argv[i], "--noconfirm"))
            no_confirm = true;
    }

    /* ── Step 1: Choose instance (lock context early) ──────────────── */
    auto instances = pclcore::local::get_instance_provider().get_instances();
    if (instances.empty()) return fail("no local instances found");

    int inst_idx = 0;
    if (inst_name) {
        for (size_t i = 0; i < instances.size(); i++) {
            if (instances[i].name == inst_name) { inst_idx = (int)i; break; }
        }
        if (instances[inst_idx].name != inst_name)
            return fail("instance not found");
    } else {
        std::printf("Choose target instance:\n");
        for (size_t i = 0; i < instances.size(); i++) {
            auto& in = instances[i];
            std::printf("  %2zu. %-20s (MC %-8s, %s)\n",
                   i + 1, in.name.c_str(),
                   in.version_id.empty() ? "?" : in.version_id.c_str(),
                   in.loader.empty() ? "Vanilla" : in.loader.c_str());
        }
        std::printf("Choose (1-%zu): ", instances.size());
        std::fflush(stdout);
        char buf[16];
        if (!std::fgets(buf, sizeof(buf), stdin)) return fail("cancelled");
        inst_idx = std::atoi(buf) - 1;
        if (inst_idx < 0 || inst_idx >= (int)instances.size())
            return fail("invalid selection");
    }

    auto& inst = instances[inst_idx];
    std::string inst_ver = inst.version_id;
    std::string inst_ldr = inst.loader;
    std::printf("Using instance: %s (MC %s, %s)\n\n",
           inst.name.c_str(),
           inst_ver.empty() ? "?" : inst_ver.c_str(),
           inst_ldr.empty() ? "Vanilla" : inst_ldr.c_str());

    /* ── Step 2: Search ────────────────────────────────────────────── */
    std::printf("Searching for '%s'...\n", query);
    std::promise<pcl::resource::SearchResult> p;
    auto f = p.get_future();
    pcl::resource::search_resources(
        query, type, source, "", 0, 10,
        pcl::resource::SortType::Default, loader,
        [&p](pcl::resource::SearchResult r) { p.set_value(std::move(r)); });
    auto result = f.get();

    if (result.hits.empty()) {
        std::printf("No results found for '%s'.\n", query);
        return 1;
    }

    /* ── Step 3: Choose resource ───────────────────────────────────── */
    int chosen = 0;
    if (result.hits.size() == 1) {
        chosen = 0;
        std::printf("Found: %s by %s\n", result.hits[0].title.c_str(),
                    result.hits[0].author.c_str());
    } else {
        std::printf("Multiple results found:\n");
        for (size_t i = 0; i < result.hits.size(); i++) {
            const auto& h = result.hits[i];
            std::printf("  %2zu. %-30s by %-14s %8s dl [%s]\n",
                   i + 1, h.title.c_str(), h.author.c_str(),
                   pcl::resource::format_download_count(h.download_count).c_str(),
                   source_name(h.source));
        }
        std::printf("Choose a number (1-%zu): ", result.hits.size());
        std::fflush(stdout);
        char buf[16];
        if (!std::fgets(buf, sizeof(buf), stdin)) return fail("cancelled");
        chosen = std::atoi(buf) - 1;
        if (chosen < 0 || chosen >= (int)result.hits.size())
            return fail("invalid selection");
    }

    auto& entry = result.hits[chosen];
    bool from_cf = (entry.source == pcl::resource::Source::CurseForge);

    /* ── Step 4: Fetch versions ────────────────────────────────────── */
    std::printf("Fetching versions for %s...\n", entry.title.c_str());
    std::promise<std::vector<pcl::resource::CompFile>> p2;
    auto f2 = p2.get_future();
    pcl::resource::fetch_project_files(
        entry.project_id, from_cf,
        [&p2](std::vector<pcl::resource::CompFile> files) {
            p2.set_value(std::move(files));
        });
    auto all_files = f2.get();

    if (all_files.empty()) {
        std::printf("No downloadable files found for %s.\n", entry.title.c_str());
        return 1;
    }

    /* ── Step 5: Pre-filter compatible versions ────────────────────── */
    std::vector<int> compat;   // indices into all_files (compatible)
    std::vector<int> incompat; // indices into all_files (incompatible)
    for (int i = 0; i < (int)all_files.size(); i++) {
        if (version_matches(all_files[i], inst_ver, inst_ldr))
            compat.push_back(i);
        else
            incompat.push_back(i);
    }

    int file_idx = 0;
    bool viewing_all = false;

    if (version_filter) {
        /* --version specified: search all files */
        for (size_t i = 0; i < all_files.size(); i++) {
            if (all_files[i].file_name == version_filter ||
                all_files[i].game_version == version_filter) {
                file_idx = (int)i; break;
            }
        }
        if (file_idx == 0 && all_files[0].file_name != version_filter
            && all_files[0].game_version != version_filter) {
            for (size_t i = 0; i < all_files.size(); i++) {
                if (all_files[i].file_name.find(version_filter) != std::string::npos ||
                    all_files[i].game_version.find(version_filter) != std::string::npos) {
                    file_idx = (int)i; break;
                }
            }
        }
        if (!version_matches(all_files[file_idx], inst_ver, inst_ldr))
            viewing_all = true;
    } else if (compat.empty()) {
        /* Nothing compatible — show everything with a note */
        std::printf("No versions match your instance (MC %s, %s). "
                    "Showing all versions:\n\n",
               inst_ver.empty() ? "?" : inst_ver.c_str(),
               inst_ldr.empty() ? "Vanilla" : inst_ldr.c_str());
        viewing_all = true;
    } else {
        /* ── Paginated compatible list, toggle-able to "all" view ──── */
        constexpr int PAGE_SIZE = 10;
        int total_pages = ((int)compat.size() + PAGE_SIZE - 1) / PAGE_SIZE;
        int page = 0;

        std::printf("Compatible versions for %s on %s (MC %s):\n",
               entry.title.c_str(), inst_ldr.empty() ? "Vanilla" : inst_ldr.c_str(),
               inst_ver.empty() ? "?" : inst_ver.c_str());

        file_idx = -1;
        int all_page = 0;
        while (file_idx < 0) {
            if (viewing_all) {
                /* ── "All" view (paginated) ───────────────────────── */
                constexpr int ALL_PAGE = 10;
                int all_total = ((int)all_files.size() + ALL_PAGE - 1) / ALL_PAGE;
                int start = all_page * ALL_PAGE;
                int end   = std::min(start + ALL_PAGE, (int)all_files.size());
                bool last_page = (end >= (int)all_files.size());

                std::printf("\nAll versions (incompatibles flagged):\n");
                for (int i = start; i < end; i++) {
                    const auto& cf = all_files[i];
                    bool ok = version_matches(cf, inst_ver, inst_ldr);
                    std::printf("  %2d. %-32s MC %-10s %6s %s%s\n",
                           (i % ALL_PAGE) + 1, cf.file_name.c_str(),
                           cf.game_version.c_str(),
                           pcl::resource::format_download_count(cf.downloads).c_str(),
                           cf.date.c_str(), ok ? "" : "  ⚠");
                }
                std::printf("\n");
                if (all_page > 0)   std::printf("  [u] prev page");
                if (!last_page)     std::printf("  [d] next page");
                std::printf("  [c] Back to compatible list");
                std::printf("  Page %d/%d", all_page + 1, all_total);
                std::printf("\nChoose (number, u, d, c, Enter=1): ");
                std::fflush(stdout);
                char buf[16];
                if (!std::fgets(buf, sizeof(buf), stdin) || buf[0] == '\n') {
                    file_idx = start;
                } else if (buf[0] == 'd' && !last_page) {
                    all_page++;
                } else if (buf[0] == 'u' && all_page > 0) {
                    all_page--;
                } else if (buf[0] == 'c') {
                    viewing_all = false;
                    all_page = 0;
                } else {
                    int sel = std::atoi(buf) - 1;
                    if (sel >= 0 && sel < (end - start))
                        file_idx = start + sel;
                }
            } else {
                /* ── Compatible paginated view ────────────────────── */
                int start = page * PAGE_SIZE;
                int end   = std::min(start + PAGE_SIZE, (int)compat.size());
                bool last_page = (end >= (int)compat.size());

                for (int i = start; i < end; i++) {
                    int idx = compat[i];
                    const auto& cf = all_files[idx];
                    const char* tag = (idx == compat[0]) ? "  [Recommended]" : "";
                    std::printf("  %2d. %-32s MC %-10s %6s %s%s\n",
                           (i % PAGE_SIZE) + 1, cf.file_name.c_str(),
                           cf.game_version.c_str(),
                           pcl::resource::format_download_count(cf.downloads).c_str(),
                           cf.date.c_str(), tag);
                }
                std::printf("\n");
                if (page > 0)          std::printf("  [u] prev page");
                if (!last_page)        std::printf("  [d] next page");
                if (!incompat.empty()) std::printf("  [a] Show all (%d more)",
                                            (int)incompat.size());
                std::printf("  Page %d/%d", page + 1, total_pages);
                std::printf("\nChoose (number, u, d, a, Enter=recommended): ");
                std::fflush(stdout);

                char buf[16];
                if (!std::fgets(buf, sizeof(buf), stdin) || buf[0] == '\n') {
                    file_idx = compat[0];
                } else if (buf[0] == 'd' && !last_page) {
                    page++;
                } else if (buf[0] == 'u' && page > 0) {
                    page--;
                } else if (buf[0] == 'a' && !incompat.empty()) {
                    viewing_all = true;
                } else {
                    int sel = std::atoi(buf) - 1;
                    if (sel >= 0 && sel < (end - start))
                        file_idx = compat[start + sel];
                }
            }
        }
    }

    auto& chosen_file = all_files[file_idx];
    bool is_compat = version_matches(chosen_file, inst_ver, inst_ldr);

    /* ── Step 6: Compatibility warning (only for incompatible choices) */
    if (!is_compat) {
        const char* severity = "Warning";
        std::string reasons;
        if (!inst_ver.empty() && chosen_file.game_version != inst_ver)
            reasons += "MC Version Mismatch";
        bool ldr_match = inst_ldr.empty() || inst_ldr == "Vanilla";
        if (!ldr_match) {
            for (auto& ml : chosen_file.mod_loaders) {
                const char* nm = loader_name(ml);
                if (nm && strcasecmp(inst_ldr.c_str(), nm) == 0)
                    { ldr_match = true; break; }
            }
        }
        if (!ldr_match) {
            if (!reasons.empty()) reasons += " + ";
            reasons += "Loader Mismatch";
        }
        if (reasons.empty()) reasons = "Compatibility Issue";

        std::printf("\n\
  %s: %s!\n\
    Instance:  %-20s (MC %-8s, %s Loader)\n\
    Selected:  %-20s [For %s Loader, MC %-8s]\n\
  This mod may be incompatible with your current instance.\n\n",
               severity, reasons.c_str(),
               inst.name.c_str(),
               inst_ver.empty() ? "?" : inst_ver.c_str(),
               inst_ldr.empty() ? "Vanilla" : inst_ldr.c_str(),
               chosen_file.file_name.c_str(),
               chosen_file.mod_loaders.empty() ? "?" :
                   loader_name(chosen_file.mod_loaders[0]),
               chosen_file.game_version.empty() ? "?" :
                   chosen_file.game_version.c_str());

        if (!confirm("Proceed anyway? [y/N]", false))
            { std::printf("Cancelled.\n"); return 0; }
    }

    /* ── Step 7: Summary & Confirm ─────────────────────────────────── */
    std::printf("\nYou're all set -\n");
    std::printf("  Resource:  %s\n", entry.title.c_str());
    std::printf("  Version:   %s (MC %s)\n",
           chosen_file.file_name.c_str(), chosen_file.game_version.c_str());
    std::printf("  Instance:  %s\n", inst.name.c_str());
    if (prequences && type == pcl::resource::ProjectType::Mod)
        std::printf("  Prequences: will be installed\n");
    std::printf("  Source:    %s\n", entry.project_url.c_str());

    if (no_confirm || confirm_yes("Install? [Y/n]")) {
        std::printf("Downloading %s...\n", chosen_file.file_name.c_str());
        std::printf("  URL: %s\n", chosen_file.download_url.c_str());
        std::printf("Install into %s - download backend not yet implemented.\n",
                    inst.name.c_str());
        std::printf("Use the GUI (--gui) for full install support.\n");
    } else {
        std::printf("Cancelled.\n");
    }
    return 0;
}


static int cmd_install(int argc, char* argv[])
{
    if (argc < 3)
        return fail("install: expected category — mod|respack|datapack|shader|minecraft|modpack");

    const char* category = argv[2];

    /* ── install minecraft ──────────────────────────────────────────── */
    if (std::strcmp(category, "minecraft") == 0) {
        std::fprintf(stderr, "Minecraft version installation is not yet available via CLI.\n");
        std::fprintf(stderr, "Use the GUI (--gui) Download tab to create new instances.\n");
        return 1;
    }

    /* ── install modpack ────────────────────────────────────────────── */
    if (std::strcmp(category, "modpack") == 0) {
        std::fprintf(stderr, "Modpack installation is not yet available via CLI.\n");
        std::fprintf(stderr, "Use the GUI (--gui) for modpack browsing and installation.\n");
        return 1;
    }

    /* ── install mod|respack|datapack|shader ────────────────────────── */
    auto type = parse_project_type(category);
    if (type == pcl::resource::ProjectType::Mod && std::strcmp(category, "mod") != 0
        && std::strcmp(category, "respack") != 0 && std::strcmp(category, "datapack") != 0
        && std::strcmp(category, "shader") != 0) {
        return fail("unknown category — use mod|respack|datapack|shader|modpack|minecraft");
    }

    return install_mod_resource(argc, argv, type);
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_instance
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_instance(int argc, char* argv[])
{
    using namespace pclcore::local;

    if (argc < 3)
        return fail("instance: expected subcommand — list|<name> start|<name> setdefault|<name> account use <account>");

    /* instance list [--json] */
    if (std::strcmp(argv[2], "list") == 0) {
        bool json_output = false;
        for (int i = 3; i < argc; i++)
            if (is_opt(argv[i], "--json")) json_output = true;

        auto instances = get_instance_provider().get_instances();
        if (json_output) {
            std::printf("[");
            for (size_t i = 0; i < instances.size(); i++) {
                if (i) std::printf(",");
                std::printf("{\"name\": \"%s\", \"path\": \"%s\"}",
                       instances[i].name.c_str(), instances[i].path.c_str());
            }
            std::printf("]\n");
        } else {
            std::printf("Local Instances:\n");
            for (const auto& inst : instances)
                std::printf("  %-24s %s\n", inst.name.c_str(), inst.path.c_str());
        }
        return 0;
    }

    /* instance <name> start */
    if (argc >= 4 && std::strcmp(argv[3], "start") == 0) {
        std::fprintf(stderr, "Starting instances from CLI is not yet available.\n");
        std::fprintf(stderr, "Use the GUI (--gui) to launch instances.\n");
        return 1;
    }

    /* instance <name> setdefault */
    if (argc >= 4 && std::strcmp(argv[3], "setdefault") == 0) {
        std::fprintf(stderr, "Default instance setting is not yet implemented.\n");
        std::fprintf(stderr, "Use the GUI Settings page to set the default instance.\n");
        return 1;
    }

    /* instance <name> account use <account> */
    if (argc >= 6 && std::strcmp(argv[3], "account") == 0
        && std::strcmp(argv[4], "use") == 0) {
        std::fprintf(stderr, "Instance-level account switching is not yet implemented.\n");
        return 1;
    }

    /* instance <name> (REPL mode) */
    if (argc == 3) {
        std::printf("Instance REPL is not yet available.\n");
        std::printf("Available subcommands for instance '%s':\n", argv[2]);
        std::printf("  start              Launch the instance (not yet available)\n");
        std::printf("  setdefault         Set as default for 'pcl-cgre run'\n");
        std::printf("  account use <acc>  Switch account for this instance\n");
        std::printf("  settings           Deprecated — edit config file directly\n");
        return 0;
    }

    return fail("instance: unknown subcommand");
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_crashspy
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_crashspy()
{
    std::fprintf(stderr, "CrashSpy from CLI is deprecated.\n");
    std::fprintf(stderr, "Crash reports are located in your Minecraft instance directories.\n");
    std::fprintf(stderr, "Use the GUI CrashSpy tab (--gui) for an interactive crash analyzer.\n");
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_account
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_account(int argc, char* argv[])
{
    using namespace pclcore::local;

    if (argc < 3)
        return fail("account: expected subcommand — list|<name> skin|<name> download|<name> setdefault");

    /* account list [--json] */
    if (std::strcmp(argv[2], "list") == 0) {
        bool json_output = false;
        for (int i = 3; i < argc; i++)
            if (is_opt(argv[i], "--json")) json_output = true;

        auto accounts = get_account_provider().get_accounts();
        if (json_output) {
            std::printf("[");
            for (size_t i = 0; i < accounts.size(); i++) {
                if (i) std::printf(",");
                std::printf("{\"username\": \"%s\", \"type\": \"%s\", \"logged_in\": %s}",
                       accounts[i].username.c_str(),
                       accounts[i].account_type.c_str(),
                       accounts[i].logged_in ? "true" : "false");
            }
            std::printf("]\n");
        } else {
            std::printf("Accounts:\n");
            for (const auto& acc : accounts) {
                const char* status = acc.logged_in ? "logged in" : "offline";
                std::printf("  %-20s [%s]  %s\n", acc.username.c_str(), acc.account_type.c_str(), status);
            }
        }
        return 0;
    }

    const char* account_name = argv[2];

    if (argc < 4)
        return fail("account: expected action — skin|download|setdefault");

    /* account <name> skin <set> <filename> */
    if (std::strcmp(argv[3], "skin") == 0) {
        std::fprintf(stderr, "Skin management from CLI is not yet available.\n");
        std::fprintf(stderr, "Skins can only be managed for non-third-party accounts.\n");
        return 1;
    }

    /* account <name> download [dir] */
    if (std::strcmp(argv[3], "download") == 0) {
        std::fprintf(stderr, "Skin download from CLI is not yet available.\n");
        return 1;
    }

    /* account <name> setdefault */
    if (std::strcmp(argv[3], "setdefault") == 0) {
        std::fprintf(stderr, "Default account setting is not yet implemented.\n");
        std::fprintf(stderr, "Use the GUI to set default account.\n");
        return 1;
    }

    return fail("account: unknown action — use skin|download|setdefault");
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_settings
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_settings()
{
    std::fprintf(stderr, "Settings from CLI is deprecated.\n");
    std::fprintf(stderr, "Access the settings config file directly at:\n");
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg)
        std::fprintf(stderr, "  %s/PCL-CGRE/settings.json\n", xdg);
    else
        std::fprintf(stderr, "  ~/.config/PCL-CGRE/settings.json\n");
    std::fprintf(stderr, "Or use the GUI (--gui) for an interactive settings panel.\n");
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_plugin
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_plugin(int argc, char* argv[])
{
    if (argc < 3)
        return fail("plugin: expected subcommand — list|<name> <command>");

    /* plugin list */
    if (std::strcmp(argv[2], "list") == 0) {
        std::fprintf(stderr, "No local plugins installed.\n");
        std::fprintf(stderr, "Plugin support is under development.\n");
        return 0;
    }

    /* plugin <name> <command> */
    if (argc >= 4) {
        std::fprintf(stderr, "Plugin '%s' not found or command not supported.\n", argv[2]);
        std::fprintf(stderr, "Use 'pcl-cgre plugin list' to see installed plugins.\n");
        return 1;
    }

    return fail("plugin: expected a command for the plugin");
}

/* ──────────────────────────────────────────────────────────────────────────
 *  cmd_run
 * ──────────────────────────────────────────────────────────────────────── */

static int cmd_run(int argc, char* /*argv*/[])
{
    std::fprintf(stderr, "Direct launch from CLI is not yet available.\n");
    if (argc > 2)
        std::fprintf(stderr, "Specify account name and instance name, or set defaults.\n");
    else
        std::fprintf(stderr, "Use 'pcl-cgre instance <name> setdefault' and\n"
                             "'pcl-cgre account <name> setdefault' first.\n");
    std::fprintf(stderr, "Or use the GUI (--gui) to launch instances.\n");
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  run_cli  —  main entry point for CLI mode
 * ════════════════════════════════════════════════════════════════════════ */

int run_cli(int argc, char* argv[])
{
    if (argc < 2) {
        cmd_help(false);
        return 0;  // no args → show help (CLI default)
    }

    const char* cmd = argv[1];

    /* ── Flags (no subcommand) ─────────────────────────────────────── */
    if (cmd[0] == '-') {
        if (std::strcmp(cmd, "--help") == 0) {
            bool json_output = false;
            for (int i = 2; i < argc; i++)
                if (is_opt(argv[i], "--json")) json_output = true;
            return cmd_help(json_output);
        }
        if (std::strcmp(cmd, "--gui") == 0) {
            std::fprintf(stderr, "GUI mode must be the first and only flag.\n");
            return 1;
        }
        return fail("unknown flag — use --help or --gui");
    }

    /* ── Subcommands ───────────────────────────────────────────────── */

    if (std::strcmp(cmd, "help") == 0) {
        bool json_output = false;
        for (int i = 2; i < argc; i++)
            if (is_opt(argv[i], "--json")) json_output = true;
        return cmd_help(json_output);
    }

    if (std::strcmp(cmd, "version") == 0) {
        bool json_output = false;
        for (int i = 2; i < argc; i++)
            if (is_opt(argv[i], "--json")) json_output = true;
        return cmd_version(json_output);
    }

    if (std::strcmp(cmd, "run") == 0)
        return cmd_run(argc, argv);

    if (std::strcmp(cmd, "search") == 0)
        return cmd_search(argc, argv);

    if (std::strcmp(cmd, "list") == 0)
        return cmd_list(argc, argv);

    if (std::strcmp(cmd, "install") == 0)
        return cmd_install(argc, argv);

    if (std::strcmp(cmd, "instance") == 0)
        return cmd_instance(argc, argv);

    if (std::strcmp(cmd, "crashspy") == 0)
        return cmd_crashspy();

    if (std::strcmp(cmd, "account") == 0)
        return cmd_account(argc, argv);

    if (std::strcmp(cmd, "settings") == 0)
        return cmd_settings();

    if (std::strcmp(cmd, "plugin") == 0)
        return cmd_plugin(argc, argv);

    /* Unknown command */
    std::fprintf(stderr, "Unknown command: %s\n", cmd);
    std::fprintf(stderr, "Run 'pcl-cgre --help' for usage information.\n");
    return 1;
}
