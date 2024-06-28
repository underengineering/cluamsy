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
    virtual void process();
    virtual void disable();

public:
    bool m_drop_inbound = true;
    bool m_drop_outbound = true;
    float m_chance = 10.f;
};
