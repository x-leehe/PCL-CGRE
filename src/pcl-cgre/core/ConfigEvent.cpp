#include "core/ConfigEvent.hpp"

namespace pcl {

ConfigEvent& ConfigEvent::instance()
{
    static ConfigEvent ev;
    return ev;
}

void ConfigEvent::connect_save(std::function<void(bool)> cb)
{
    m_save_cb = std::move(cb);
}

void ConfigEvent::connect_rollback(std::function<void()> cb)
{
    m_rollback_cb = std::move(cb);
}

void ConfigEvent::emit_saved(bool show_toast)
{
    if (m_save_cb)
        m_save_cb(show_toast);
}

void ConfigEvent::emit_rolled_back()
{
    if (m_rollback_cb)
        m_rollback_cb();
}

}  // namespace pcl
