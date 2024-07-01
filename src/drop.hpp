#pragma once

#include "module.hpp"

class DropModule : public Module {
public:
    DropModule() {
        m_display_name = "Drop";
        m_short_name = "drop";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

public:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
};
