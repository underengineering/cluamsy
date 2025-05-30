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

    TamperModule(const TamperModule&) = default;
    TamperModule(TamperModule&&) = delete;
    TamperModule& operator=(const TamperModule&) = default;
    TamperModule& operator=(TamperModule&&) = delete;

    virtual ~TamperModule() = default;

    bool draw() override;

    void enable() override;
    void disable(std::list<PacketNode>& packets) override;

    void apply_config(const toml::table& config) override;

    Result process(std::list<PacketNode>& packets) override;

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"chance", lua_method_chance},
            {"max_bit_flips", lua_method_max_bit_flips},
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
    static int lua_method_chance(lua_State* L) {
        auto* module = *std::bit_cast<TamperModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_chance, 2);
        if (rets == 0)
            events::queue_redraw();

        return rets;
    };

    static int lua_method_max_bit_flips(lua_State* L) {
        auto* module = *std::bit_cast<TamperModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_max_bit_flips, 2);
        if (rets == 0)
            events::queue_redraw();

        return rets;
    };

private:
    bool m_inbound = true;
    bool m_outbound = true;
    float m_chance = 10.f;
    int m_max_bit_flips = 1;
};
