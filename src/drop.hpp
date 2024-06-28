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
    virtual bool process();
    virtual void disable();

public:
    bool dropInbound = true;
    bool dropOutbound = true;
    short chance = 1000;
};
