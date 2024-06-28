#pragma once

#include <array>
#include <memory>

class Module {
public:
    virtual bool draw() = 0;

    virtual void enable() = 0;
    virtual bool process() = 0;
    virtual void disable() = 0;

public:
    // Static module data
    const char* m_display_name; // display name shown in ui
    const char* m_short_name;   // single word name
    bool m_enabled;
    bool m_processed;
};

extern std::array<std::shared_ptr<Module>, 1> g_modules;
