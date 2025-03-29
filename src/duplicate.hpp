#pragma once

#include "module.hpp"

class DuplicateModule : public Module {
public:
    DuplicateModule() {
        m_display_name = "Duplicate";
        m_short_name = "Duplicate";
    }

    virtual bool draw();

    virtual void enable();
    virtual void disable();

    virtual std::optional<std::chrono::milliseconds> process();

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"chance", lua_method_chance},
            {"count", lua_method_count},
            {},
        };

        luaL_newmetatable(L, "Duplicate");

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
        auto* module = *std::bit_cast<DuplicateModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_chance, 2);
        if (rets == 0)
            module->m_dirty = true;

        return rets;
    };

    static int lua_method_count(lua_State* L) {
        auto* module = *std::bit_cast<DuplicateModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_count, 2);
        if (rets == 0)
            module->m_dirty = true;

        return rets;
    };

public:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
    int m_count = 1;
};
