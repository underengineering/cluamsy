#pragma once

#include <chrono>
#include <lua.hpp>
#include <toml.hpp>

#include "lua_util.hpp"

class Module {
    friend class WinDivert;

public:
    struct Result {
        std::optional<std::chrono::milliseconds> schedule_after = std::nullopt;
        bool dirty = false;
    };

public:
    Module() = default;

    Module(const Module&) = default;
    Module(Module&&) = delete;
    Module& operator=(const Module&) = default;
    Module& operator=(Module&&) = delete;

    virtual ~Module() = default;

    virtual bool draw() = 0;

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual void apply_config(const toml::table& config) {
        m_enabled = config["enabled"].value_or(false);
    };

    virtual Result process() = 0;

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
        auto* module = *std::bit_cast<Module**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_enabled, 2);

        return rets;
    };

public:
    // Static module data
    const char* m_display_name = nullptr; // display name shown in ui
    const char* m_short_name = nullptr;   // single word name
    bool m_enabled = false;
    float m_indicator = 0.f;

private:
    // Checked in `WinDivert`
    bool m_was_enabled = false;
};
