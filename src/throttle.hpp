#pragma once

#include <chrono>
#include <list>

#include "module.hpp"
#include "packet.hpp"

using namespace std::chrono_literals;

class ThrottleModule : public Module {
private:
    // Threshold for how many packet to throttle at most
    static inline size_t MAX_PACKETS = 1024;

public:
    ThrottleModule() {
        m_display_name = "Throttle";
        m_short_name = "Throttle";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

private:
    void flush();

public:
    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"chance", lua_method_chance},
            {"timeframe", lua_method_timeframe},
            {},
        };

        luaL_newmetatable(L, "Throttle");

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
        ThrottleModule* module =
            std::bit_cast<ThrottleModule*>(lua_touserdata(L, 1));
        return lua_getset(L, module->m_chance, 2);
    };

    static int lua_method_timeframe(lua_State* L) {
        ThrottleModule* module =
            std::bit_cast<ThrottleModule*>(lua_touserdata(L, 1));

        int lag_time = module->m_timeframe_ms.count();
        const auto rets = lua_getset(L, lag_time, 2);
        if (rets == 1)
            module->m_timeframe_ms = std::chrono::milliseconds(lag_time);

        return rets;
    };

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
    std::chrono::milliseconds m_timeframe_ms = 200ms;
    bool m_drop_throttled = false;

    bool m_throttling = false;
    std::chrono::steady_clock::time_point m_start_point;
    std::list<PacketNode> m_throttle_list;
};
