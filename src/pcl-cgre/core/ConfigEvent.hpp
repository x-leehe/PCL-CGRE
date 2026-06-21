#pragma once

#include <functional>

namespace pcl {

/**
 * 配置事件总线 — ConfigManager 与 UI 的解耦层
 *
 * 隔离原则: ConfigManager 不了解 UI 的存在。
 * 保存/回滚事件通过此类传播, UI 层订阅。
 *
 * 失败保护: 未调用 connect_*() 时, emit_*() 静默无操作。
 * 配置正常保存, 仅缺失 Toast / 回滚 UI 反馈。
 */
class ConfigEvent {
public:
    static ConfigEvent& instance();

    void connect_save(std::function<void(bool show_toast)> cb);
    void connect_rollback(std::function<void()> cb);
    void emit_saved(bool show_toast);
    void emit_rolled_back();

private:
    ConfigEvent() = default;
    ConfigEvent(const ConfigEvent&) = delete;
    ConfigEvent& operator=(const ConfigEvent&) = delete;

    std::function<void(bool)> m_save_cb;
    std::function<void()> m_rollback_cb;
};

}  // namespace pcl
