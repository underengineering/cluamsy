#pragma once

#include "module.hpp"

using namespace std::chrono_literals;

class TamperModule : public Module {
private:
    // Threshold for how many packet to throttle at most
    static inline size_t MAX_PACKETS = 1024;

public:
    TamperModule() {
        m_display_name = "Tamper";
        m_short_name = "Tamper";
    }

    virtual ~TamperModule() = default;

    bool draw() override;

    void enable() override;
    void disable() override;

    void apply_config(const toml::table& config) override;

    Result process() override;

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {},
        };

        luaL_newmetatable(L, "Tamper");

        // Inherit from `Module`
        luaL_getmetatable(L, "Module");
        lua_setmetatable(L, -2);

        luaL_setfuncs(L, methods, 0);

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pop(L, 1);
    }

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
    int m_max_bit_flips = 1;
};
