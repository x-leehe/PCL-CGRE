#include "core/ConfigManager.hpp"
#include "core/ConfigEvent.hpp"
#include "core/Log.hpp"

#include <fstream>
#include <cstdio>
#include <sys/stat.h>
#include <glib.h>

namespace pcl {

/* ═══════════════════════════════════════════════════════════════════════
 * 默认配置 — 与 docs/settings-tree.md §配置文件映射 保持一致
 * ═══════════════════════════════════════════════════════════════════════ */

static const char* DEFAULT_CONFIG_JSON = R"JSON({
  "launcher": {
    "ui": {
      "opacity": 1.0,
      "theme": "follow_system",
      "light_accent": "blue",
      "dark_accent": "blue",
      "show_logo": true,
      "lock_window": false,
      "show_trivia": true,
      "blur_enabled": false,
      "blur_radius": 20,
      "blur_sampling": 50,
      "blur_method": "gaussian",
      "font_family": "",
      "motd_font_family": "",
      "background": {
        "fit": "smart",
        "opacity": 1.0,
        "blur": 0,
        "pause_blur": true,
        "color_overlay": false
      },
      "logo": {
        "type": "default",
        "text": "",
        "image_path": ""
      },
      "homepage": {
        "type": "preset",
        "preset": "Trivia",
        "local_path": "",
        "remote_url": ""
      },
      "hidden": {
        "pages": [],
        "setup": [],
        "tools": [],
        "instance": [],
        "functions": []
      }
    },
    "locale": {
      "ui_lang": "zh_CN",
      "format_culture": "zh-CN"
    },
    "system": {
      "announcements": "all",
      "max_fps": 60,
      "max_log_lines": 10000,
      "disable_hw_accel": false
    },
    "debug": {
      "anim_speed": 1.0,
      "skip_copy": false,
      "mode": false,
      "delay": false
    }
  },

  "game": {
    "window": {
      "title": "",
      "visibility": "keep",
      "size_mode": "default",
      "custom_width": 854,
      "custom_height": 480
    },
    "launch": {
      "instance_isolation": "modded_only",
      "custom_info": "PCL",
      "priority": "normal",
      "ip_stack": "ipv4",
      "pre_launch_cmd": "",
      "pre_launch_wait": false
    },
    "java": {
      "memory": {
        "mode": "auto",
        "max_mb": 2048
      },
      "renderer": "auto",
      "jvm_args": "",
      "game_args": "",
      "disable_jlw": false,
      "disable_lf": false,
      "prefer_dedicated_gpu": false,
      "disable_lwjgl_unsafe": false
    },
    "download": {
      "source": "prefer_official",
      "version_source": "prefer_official",
      "threads": 64,
      "speed_limit_kbps": 0,
      "auto_select_instance": true,
      "fix_authlib": true
    },
    "community": {
      "mod_source": "prefer_curseforge",
      "filename_format": "${filename}",
      "mod_list_style": "translation_first",
      "hide_quilt": false,
      "auto_install_deps": true
    },
    "accessibility": {
      "release_notice": true,
      "snapshot_notice": false,
      "auto_lang": true,
      "read_clipboard": true
    }
  },

  "java": {
    "default_runtime": 0,
    "runtimes": [
      { "name": "Java 21", "path": "/usr/lib/jvm/java-21-openjdk" },
      { "name": "Java 17", "path": "/usr/lib/jvm/java-17-openjdk" }
    ]
  },

  "network": {
    "doh_enabled": false,
    "http_proxy": {
      "type": "none",
      "url": ""
    }
  },

  "account": {
    "microsoft": {
      "auth_method": "web"
    }
  }
})JSON";

/* ═══════════════════════════════════════════════════════════════════════
 * 遍历 — 按点分路径在 JSON 树中查找/创建节点
 * ═══════════════════════════════════════════════════════════════════════ */

const nlohmann::json* ConfigManager::traverse(const std::string& path) const
{
    const nlohmann::json* cur = &m_config;
    size_t start = 0;

    while (start < path.size()) {
        auto dot = path.find('.', start);
        std::string key = (dot == std::string::npos)
            ? path.substr(start)
            : path.substr(start, dot - start);

        if (!cur->is_object() || !cur->contains(key))
            return nullptr;

        cur = &(*cur)[key];
        start = (dot == std::string::npos) ? path.size() : dot + 1;
    }
    return cur;
}

nlohmann::json* ConfigManager::traverse(const std::string& path, bool create)
{
    nlohmann::json* cur = &m_config;
    size_t start = 0;

    while (start < path.size()) {
        auto dot = path.find('.', start);
        std::string key = (dot == std::string::npos)
            ? path.substr(start)
            : path.substr(start, dot - start);

        if (!cur->is_object()) {
            if (create) *cur = nlohmann::json::object();
            else return nullptr;
        }

        if (!cur->contains(key)) {
            if (create) (*cur)[key] = nlohmann::json::object();
            else return nullptr;
        }

        cur = &(*cur)[key];
        start = (dot == std::string::npos) ? path.size() : dot + 1;
    }
    return cur;
}

/* ═══════════════════════════════════════════════════════════════════════
 * set_json / get_json — JSON 值接口 (数组、对象等复杂类型)
 * ═══════════════════════════════════════════════════════════════════════ */

void ConfigManager::set_json(const std::string& path, const nlohmann::json& value)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json* node = traverse(path, true);
        if (node) {
            if (*node == value) return;  // 值未变, 跳过
            if (!m_backup_saved) {
                m_backup = m_config;
                m_backup_saved = true;
            }
            *node = value;
            m_dirty = true;
        }
    }
    schedule_save();
}

nlohmann::json ConfigManager::get_json(const std::string& path) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const nlohmann::json* node = traverse(path);
    if (!node)
        throw std::runtime_error("ConfigManager::get_json: path not found: " + path);
    return *node;
}

/* ═══════════════════════════════════════════════════════════════════════
 * init — 创建目录 / 加载或生成默认配置
 * ═══════════════════════════════════════════════════════════════════════ */

void ConfigManager::init()
{
    std::string dir = config_path();
    // 去掉文件名部分，获取目录路径
    auto slash = dir.rfind('/');
    std::string config_dir = dir.substr(0, slash);

    // 递归创建目录 (类似 mkdir -p)
    std::string mkdir_cmd = "mkdir -p " + config_dir;
    std::system(mkdir_cmd.c_str());

    std::ifstream ifs(dir);
    if (ifs.good()) {
        try {
            m_config = nlohmann::json::parse(ifs);
            ifs.close();
            return;
        } catch (const nlohmann::json::parse_error& e) {
            LOG_WARN("ConfigManager: corrupt global.json (%s), regenerating defaults", e.what());
            // 备份损坏文件
            std::string backup = dir + ".corrupt";
            std::rename(dir.c_str(), backup.c_str());
        }
    }

    // 不存在或损坏: 生成默认
    m_config = make_default_config();
    m_dirty = true;
    save();
}

/* ═══════════════════════════════════════════════════════════════════════
 * save — 原子写入
 * ═══════════════════════════════════════════════════════════════════════ */

void ConfigManager::save()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_dirty) return;

    std::string tmp = config_path() + ".tmp";
    {
        std::ofstream ofs(tmp);
        if (!ofs) {
            LOG_ERR("ConfigManager: cannot write %s", tmp.c_str());
            return;
        }
        ofs << m_config.dump(2) << std::endl;
    }

    if (std::rename(tmp.c_str(), config_path().c_str()) != 0) {
        LOG_ERR("ConfigManager: rename %s → %s failed", tmp.c_str(), config_path().c_str());
        return;
    }

    m_dirty = false;
}

/* ═══════════════════════════════════════════════════════════════════════
 * 辅助方法
 * ═══════════════════════════════════════════════════════════════════════ */

std::string ConfigManager::config_path() const
{
    const char* home = g_get_home_dir();
    return std::string(home) + "/.config/pcl-cgre/global.json";
}

nlohmann::json ConfigManager::make_default_config()
{
    return nlohmann::json::parse(DEFAULT_CONFIG_JSON);
}

ConfigManager& ConfigManager::instance()
{
    static ConfigManager mgr;
    return mgr;
}

ConfigManager::~ConfigManager()
{
    if (m_save_timer)
        g_source_remove(m_save_timer);
    // 最后保存一次
    if (m_dirty) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string tmp = config_path() + ".tmp";
        {
            std::ofstream ofs(tmp);
            if (ofs) ofs << m_config.dump(2) << std::endl;
        }
        std::rename(tmp.c_str(), config_path().c_str());
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * 延迟保存
 * ═══════════════════════════════════════════════════════════════════════ */

void ConfigManager::schedule_save()
{
    if (m_save_timer)
        g_source_remove(m_save_timer);

    auto on_timeout = +[](gpointer data) -> gboolean {
        auto* mgr = static_cast<ConfigManager*>(data);
        mgr->m_save_timer = 0;
        mgr->save();
        ConfigEvent::instance().emit_saved(true);  // show toast
        return G_SOURCE_REMOVE;
    };
    m_save_timer = g_timeout_add(1600, on_timeout, this);
}

void ConfigManager::save_silent()
{
    if (m_save_timer) {
        g_source_remove(m_save_timer);
        m_save_timer = 0;
    }
    save();
    ConfigEvent::instance().emit_saved(false);  // silent, no toast
}

void ConfigManager::clear_backup()
{
    m_backup_saved = false;
    m_backup = nlohmann::json{};
    if (m_save_timer) {
        g_source_remove(m_save_timer);
        m_save_timer = 0;
    }
    m_dirty = false;
}

void ConfigManager::rollback()
{
    bool need_save = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_backup_saved) {
            m_config = m_backup;
            m_backup_saved = false;
            m_dirty = true;
            m_backup = nlohmann::json();
            need_save = true;
        }
    }
    if (need_save) {
        save();
        ConfigEvent::instance().emit_rolled_back();
    }
}

}  // namespace pcl
