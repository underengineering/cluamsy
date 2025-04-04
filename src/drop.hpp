#pragma once

#include "module.hpp"

class DropModule : public Module {
public:
    DropModule() {
        m_display_name = "Drop";
        m_short_name = "Drop";
    }

    virtual ~DropModule() = default;

    bool draw() override;

    void enable() override;
    void disable() override;

    void apply_config(const toml::table& config) override;

    Result process() override;

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"chance", lua_method_chance},
            {},
        };

        luaL_newmetatable(L, "Drop");

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
        auto* module = *std::bit_cast<DropModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_chance, 2);

        return rets;
    };

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
};
