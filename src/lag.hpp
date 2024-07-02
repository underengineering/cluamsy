#pragma once

#include "lua_util.hpp"
#include "module.hpp"
#include "packet.hpp"

using namespace std::chrono_literals;

class LagModule : public Module {
private:
    // Threshold for how many packet to throttle at most
    static inline size_t MAX_PACKETS = 1024;

public:
    LagModule() {
        m_display_name = "Lag";
        m_short_name = "Lag";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"chance", lua_method_chance},
            {"lag_time", lua_method_lag_time},
            {},
        };

        luaL_newmetatable(L, "Lag");

        // Inherit from `Module`
        luaL_getmetatable(L, "Module");
        lua_setmetatable(L, -2);

        luaL_setfuncs(L, methods, 0);

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pop(L, 1);
    }

private:
    static int lua_method_chance(lua_State* L) {
        LagModule* module = std::bit_cast<LagModule*>(lua_touserdata(L, 1));
        return lua_getset(L, module->m_chance, 2);
    };

    static int lua_method_lag_time(lua_State* L) {
        LagModule* module = std::bit_cast<LagModule*>(lua_touserdata(L, 1));

        int lag_time = module->m_lag_time.count();
        const auto rets = lua_getset(L, lag_time, 2);
        if (rets == 1)
            module->m_lag_time = std::chrono::milliseconds(lag_time);

        return rets;
    };

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;

    std::chrono::milliseconds m_lag_time = 200ms;
    std::list<PacketNode> m_lagged_packets;
};
