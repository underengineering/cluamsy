#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <SDL.h>
#include <cassert>

#include "common.hpp"
#include "drop.hpp"
#include "lag.hpp"
#include "lua.hpp"
#include "throttle.hpp"

Lua::Lua(const std::vector<std::shared_ptr<Module>>& modules) {
    LOG("Creating lua state");

    lua_State* L = m_lua_state = luaL_newstate();
    luaL_openlibs(L);

    // Store ourselves in registry
    set_registry_ref();
}

Lua::Lua(Lua&& other)
    : m_lua_state(other.m_lua_state),
      m_timer_data(std::move(other.m_timer_data)) {
    other.m_lua_state = nullptr;

    // Update registry pointer
    set_registry_ref();
}

Lua::~Lua() {
    for (const auto& timer_data : m_timer_data)
        SDL_RemoveTimer(timer_data.timer_id);

    if (m_lua_state != nullptr) {
        LOG("Destroying lua state");
        lua_close(m_lua_state);
    }
}

void Lua::push_api(const std::vector<std::shared_ptr<Module>>& modules) {
    const auto L = m_lua_state;

    LOG("Initializing modules api");
    Module::lua_setup(L);
    LagModule::lua_setup(L);
    DropModule::lua_setup(L);
    ThrottleModule::lua_setup(L);

    LOG("Creating modules");
    lua_newtable(L); // cluamsy

    lua_newtable(L); // Timer
    push_timer_api();
    lua_setfield(L, -2, "Timer");

    lua_newtable(L); // modules
    for (const auto& module : modules)
        push_module_api(*module);
    lua_setfield(L, -2, "modules");

    lua_setglobal(L, "cluamsy");
}

bool Lua::run_file(const std::string& file_name) const {
    LOG("Running file '%s'", file_name.c_str());

    const auto status = luaL_loadfile(m_lua_state, file_name.c_str());
    if (status != 0) {
        LOG("luaL_loadfile fail: %i", status);
        return false;
    }

    if (lua_pcall(m_lua_state, 0, 0, 0) != LUA_OK) {
        LOG("lua_pcall fail: %s", lua_tostring(m_lua_state, -1));
        return false;
    }

    return true;
}

void Lua::set_registry_ref() {
    auto* L = m_lua_state;

    lua_pushliteral(L, "Lua");
    const auto* this_ud = lua_newuserdata(L, sizeof(void*));
    *std::bit_cast<Lua**>(this_ud) = this;
    lua_settable(L, LUA_REGISTRYINDEX);
}

uint32_t Lua::timer_callback_trampoline(uint32_t interval, void* param) {
    auto* timer_data = std::bit_cast<TimerData*>(param);
    const auto L = timer_data->lua->m_lua_state;

    // Get callback from the registry
    lua_pushlightuserdata(L, timer_data);
    lua_gettable(L, LUA_REGISTRYINDEX);

    lua_pushnumber(L, interval);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        LOG("timer callback fail: %s", lua_tostring(L, -1));
        return 0;
    }

    // Keep firing by default
    auto result = interval;
    if (int type = lua_type(L, -1); type != LUA_TNONE && type != LUA_TNIL)
        result = static_cast<uint32_t>(lua_tonumber(L, -1));

    return result;
}

int Lua::timer_create(lua_State* L) {
    const auto interval = static_cast<uint32_t>(lua_tonumber(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushliteral(L, "Lua");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* lua = *std::bit_cast<Lua**>(lua_touserdata(L, -1));

    // Allocate a timer data
    auto& timer_data = lua->m_timer_data.emplace_back(lua);

    const auto timer_id =
        SDL_AddTimer(interval, timer_callback_trampoline, &timer_data);
    if (timer_id == 0) {
        LOG("Timer creation failed");
        lua->m_timer_data.pop_back();
        return 0;
    }

    timer_data.timer_id = timer_id;

    // Save function to the registry
    lua_pushlightuserdata(L, &timer_data);
    lua_pushvalue(L, -3);
    lua_settable(L, LUA_REGISTRYINDEX);

    // Create timer userdata
    const auto ud = lua_newuserdata(L, sizeof(TimerData*));
    *std::bit_cast<TimerData**>(ud) = &timer_data;

    luaL_getmetatable(L, "Timer");
    lua_setmetatable(L, -2);

    return 1;
}

int Lua::timer_remove(lua_State* L) {
    const auto* timer_data = *std::bit_cast<TimerData**>(lua_touserdata(L, -1));

    // Remove callback from the registry
    lua_pushlightuserdata(L, &timer_data);
    lua_pushnil(L);
    lua_settable(L, LUA_REGISTRYINDEX);

    SDL_RemoveTimer(timer_data->timer_id);

    // Remove timer data
    auto* lua = timer_data->lua;
    for (auto it = lua->m_timer_data.begin(); it != lua->m_timer_data.end();
         ++it) {
        if (&*it == timer_data) {
            lua->m_timer_data.erase(it);
            break;
        }
    }

    return 0;
}

void Lua::push_timer_api() {
    const auto L = m_lua_state;

    luaL_Reg methods[] = {
        {"remove", timer_remove},
        {},
    };

    luaL_newmetatable(L, "Timer");

    luaL_setfuncs(L, methods, 0);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pop(L, 1);

    lua_pushcfunction(L, Lua::timer_create);
    lua_setfield(L, -2, "create");
}

void Lua::push_module_api(const Module& module) {
    const auto L = m_lua_state;

    // Setup metatable
    module.lua_setup(L);

    // Create userdata
    void* ud = lua_newuserdata(L, sizeof(void*));
    *std::bit_cast<Module**>(ud) = const_cast<Module*>(&module);

    // Set userdata metatable
    luaL_getmetatable(L, module.m_short_name);
    assert(lua_type(L, -1) != LUA_TNIL);
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, module.m_short_name);
}
