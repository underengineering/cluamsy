#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <SDL.h>
#include <cassert>

#include "bandwidth.hpp"
#include "drop.hpp"
#include "duplicate.hpp"
#include "lag.hpp"
#include "tamper.hpp"
#include "throttle.hpp"

#include "common.hpp"
#include "lua.hpp"

Lua::Lua() noexcept {
    LOG("Creating lua state");

    lua_State* L = m_lua_state = luaL_newstate();
    luaL_openlibs(L);

    // Store ourselves in registry
    set_registry_ref();
}

Lua::Lua(Lua&& other) noexcept
    : m_lua_state(other.m_lua_state), m_scripts(std::move(other.m_scripts)),
      m_timer_data(std::move(other.m_timer_data)) {
    other.m_lua_state = nullptr;

    // Update registry pointer
    set_registry_ref();
}

Lua& Lua::operator=(Lua&& other) noexcept {
    m_lua_state = other.m_lua_state;
    m_timer_data = std::move(other.m_timer_data);
    m_scripts = std::move(other.m_scripts);
    other.m_lua_state = nullptr;

    // Update registry pointer;
    set_registry_ref();

    return *this;
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
    BandwidthModule::lua_setup(L);
    DuplicateModule::lua_setup(L);
    TamperModule::lua_setup(L);

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

bool Lua::load_script(const std::filesystem::path& file_path) {
    const auto file_path_str = file_path.string();
    LOG("Running file '%ls'", file_path.c_str());

    const auto status = luaL_loadfile(m_lua_state, file_path_str.c_str());
    if (status != 0) {
        const auto* err = lua_tostring(m_lua_state, -1);
        LOG("luaL_loadfile fail (%i): %s", status, err);
        return false;
    }

    if (lua_pcall(m_lua_state, 0, 1, 0) != LUA_OK) {
        LOG("lua_pcall fail: %s", lua_tostring(m_lua_state, -1));
        return false;
    }

    std::optional<RegistryKey> key;

    // Returned value is the unload function
    if (!lua_isnil(m_lua_state, -1) && !lua_isfunction(m_lua_state, -1)) {
        LOG("returned value must be a function, ignoring");
        lua_pop(m_lua_state, 1);
    } else if (lua_isfunction(m_lua_state, -1)) {
        // Save to registry
        key = RegistryKey();
        lua_setfield(m_lua_state, LUA_REGISTRYINDEX, key->get().c_str());
    }

    // TODO
    const auto name = file_path.filename().string();

    m_scripts.emplace_back(Script{
        .lua = this,
        .name = name,
        .file_path = file_path,
        .unload_function_key = key,
    });

    return true;
}

bool Lua::unload_script(const std::list<Script>::const_iterator it) {
    const auto registry_key = it->unload_function_key;
    if (registry_key) {
        LOG("Unloading script %ls", it->file_path.c_str());

        const auto& key = registry_key->get();
        lua_getfield(m_lua_state, LUA_REGISTRYINDEX, key.c_str());
        if (lua_isfunction(m_lua_state, -1)) {
            // Call unload function
            if (lua_pcall(m_lua_state, 0, 0, 0) != LUA_OK) {
                LOG("lua_pcall fail: %s", lua_tostring(m_lua_state, -1));
                return false;
            }
        } else {
            lua_pop(m_lua_state, 1);
        }

        // Remove the key
        lua_pushnil(m_lua_state);
        lua_setfield(m_lua_state, LUA_REGISTRYINDEX, key.c_str());
    }

    m_scripts.erase(it);

    return true;
}

void Lua::set_registry_ref() {
    auto* const L = m_lua_state;

    lua_pushliteral(L, "Lua");
    const auto* const this_ud = lua_newuserdata(L, sizeof(void*));
    *std::bit_cast<Lua**>(this_ud) = this;
    lua_settable(L, LUA_REGISTRYINDEX);
}

uint32_t Lua::timer_callback_trampoline(uint32_t interval, void* param) {
    auto* const timer_data = std::bit_cast<TimerData*>(param);
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
    if (auto type = lua_type(L, -1); type != LUA_TNONE && type != LUA_TNIL)
        result = static_cast<uint32_t>(lua_tonumber(L, -1));

    return result;
}

int Lua::timer_create(lua_State* L) {
    const auto interval = static_cast<uint32_t>(lua_tonumber(L, 1));
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushliteral(L, "Lua");
    lua_gettable(L, LUA_REGISTRYINDEX);
    auto* const lua = *std::bit_cast<Lua**>(lua_touserdata(L, -1));

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
    auto* const lua = timer_data->lua;
    for (auto it = lua->m_timer_data.cbegin(); it != lua->m_timer_data.cend();
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

    lua_pushcfunction(L, Lua::timer_remove);
    lua_setfield(L, -2, "remove");
}

void Lua::push_module_api(const Module& module) {
    const auto L = m_lua_state;

    // Setup metatable
    module.lua_setup(L);

    // Create userdata
    void* const ud = lua_newuserdata(L, sizeof(void*));
    *std::bit_cast<Module**>(ud) = const_cast<Module*>(&module);

    // Set userdata metatable
    luaL_getmetatable(L, module.m_short_name);
    assert(lua_type(L, -1) != LUA_TNIL);
    lua_setmetatable(L, -2);

    lua_setfield(L, -2, module.m_short_name);
}
