#pragma once

#include <cassert>
#include <chrono>

#include "lua_util.hpp"
#include "module.hpp"

using namespace std::chrono_literals;

namespace detail {

class RateStats {
public:
    RateStats() = delete;
    RateStats(size_t max_tokens) : m_max_tokens(max_tokens) {}

    void set_max_tokens(size_t max_tokens) noexcept {
        m_max_tokens = max_tokens;
    }

    void reset() noexcept { m_tokens = m_max_tokens; }

    void replenish(std::chrono::steady_clock::time_point now_ts,
                   size_t replenish_rate) noexcept {
        const auto elapsed_seconds =
            std::chrono::duration<double>(now_ts - m_last_ts).count();

        // Replenish tokens based on elapsed time
        m_tokens += static_cast<size_t>(elapsed_seconds *
                                        static_cast<float>(replenish_rate));
        if (m_tokens > m_max_tokens)
            m_tokens = m_max_tokens;
    }

    bool update(std::chrono::steady_clock::time_point now_ts,
                size_t size) noexcept {
        if (m_tokens >= size) {
            m_tokens -= size;
            m_last_ts = now_ts;
            return true;
        }

        return false;
    }

private:
    size_t m_max_tokens;
    size_t m_tokens = 0;
    std::chrono::steady_clock::time_point m_last_ts;
};

} // namespace detail

class BandwidthModule : public Module {
public:
    BandwidthModule() {
        m_display_name = "Bandwidth";
        m_short_name = "Bandwidth";
    }

    bool draw() override;

    void enable() override;
    void disable() override;

    void apply_config(const toml::table& config) override;

    std::optional<std::chrono::milliseconds> process() override;

    static void lua_setup(lua_State* L) {
        luaL_Reg methods[] = {
            {"limit", lua_method_limit},
            {},
        };

        luaL_newmetatable(L, "Bandwidth");

        // Inherit from `Module`
        luaL_getmetatable(L, "Module");
        lua_setmetatable(L, -2);

        luaL_setfuncs(L, methods, 0);

        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        lua_pop(L, 1);
    }

private:
    static int lua_method_limit(lua_State* L) {
        auto* module = *std::bit_cast<BandwidthModule**>(lua_touserdata(L, 1));
        const auto rets = lua_getset(L, module->m_limit, 2);
        if (rets == 0)
            module->m_dirty = true;

        return rets;
    };

private:
    bool m_inbound = true;
    bool m_outbound = true;
    int m_limit = 10;

    detail::RateStats m_rate_stats{0};
};
