#pragma once

#include <nlohmann/json.hpp>
#include <mutex>
#include <string>

namespace pcl {

/**
 * 配置管理器 — 树形配置的加载、读取、写入、持久化
 *
 * 单例, 线程安全。配置文件位于:
 *   ~/.config/pcl-cgre/global.json
 *
 * 所有 set() / set_json() 调用会自动触发 3s 延迟保存。
 */
class ConfigManager {
public:
    static ConfigManager& instance();

    void init();

    template<typename T>
    T get(const std::string& path) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const nlohmann::json* node = traverse(path);
        if (!node)
            throw std::runtime_error("ConfigManager::get: path not found: " + path);
        return node->get<T>();
    }

    template<typename T>
    T get_or(const std::string& path, const T& default_val) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const nlohmann::json* node = traverse(path);
        if (!node)
            return default_val;
        try {
            return node->get<T>();
        } catch (...) {
            return default_val;
        }
    }

    template<typename T>
    void set(const std::string& path, const T& value) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            nlohmann::json* node = traverse(path, true);
            if (node) {
                nlohmann::json jv(value);
                if (*node == jv) return;  // 值未变, 跳过
                if (!m_backup_saved) {
                    m_backup = m_config;
                    m_backup_saved = true;
                }
                *node = std::move(jv);
                m_dirty = true;
            }
        }
        schedule_save();
    }

    void set_json(const std::string& path, const nlohmann::json& value);
    nlohmann::json get_json(const std::string& path) const;
    void save();

    bool is_dirty() const { return m_dirty; }

    nlohmann::json& raw() { return m_config; }
    const nlohmann::json& raw() const { return m_config; }
    std::string config_path() const;

    /** 页面切换时调用 — 静默保存，不弹 Toast */
    void save_silent();

    /** 回滚到上次保存前的状态 */
    void rollback();

    /** 重置备份并取消待保存定时器 (回滚/页面重建后调用) */
    void clear_backup();

private:
    ConfigManager() = default;
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    const nlohmann::json* traverse(const std::string& path) const;
    nlohmann::json* traverse(const std::string& path, bool create);
    static nlohmann::json make_default_config();

    void schedule_save();

    nlohmann::json m_config;
    nlohmann::json m_backup;
    mutable std::mutex m_mutex;
    bool m_dirty = false;
    bool m_backup_saved = false;
    unsigned int m_save_timer = 0;
};

}  // namespace pcl
