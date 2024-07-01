#pragma once

#include <array>
#include <chrono>
#include <memory>

class Module {
    friend class WinDivert;

public:
    virtual bool draw() = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual std::optional<std::chrono::milliseconds> process() = 0;

public:
    // Static module data
    const char* m_display_name; // display name shown in ui
    const char* m_short_name;   // single word name
    bool m_enabled = false;
    float m_indicator = 0.f;
    // Should rerender
    bool m_dirty = false;

private:
    // Checked in `WinDivert`
    bool m_was_enabled = false;
};

extern std::array<std::shared_ptr<Module>, 2> g_modules;
