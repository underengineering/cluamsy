#pragma once

#include <chrono>
#include <lua.hpp>

#include "lua_util.hpp"

class Module {
    friend class WinDivert;

public:
    virtual bool draw() = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual std::optional<std::chrono::milliseconds> process() = 0;

    static void lua_setup(lua_State* L) {
        luaL_newmetatable(L, "Module");

        lua_pushcfunction(L, lua_method_enabled);
        lua_setfield(L, -2, "enabled");

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pop(L, 1);
    };

private:
    static int lua_method_enabled(lua_State* L) {
        Module* module = std::bit_cast<Module*>(lua_touserdata(L, 1));
        return lua_getset(L, module->m_enabled, 2);
    };

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
