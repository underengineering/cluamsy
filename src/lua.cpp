#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

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

    LOG("Initializing modules api");
    Module::lua_setup(L);
    LagModule::lua_setup(L);
    DropModule::lua_setup(L);
    ThrottleModule::lua_setup(L);

    LOG("Creating modules");
    lua_newtable(L); // cluamsy

    lua_newtable(L); // modules
    for (const auto& module : modules) {
        LOG("Setting up '%s'", module->m_display_name);

        // Setup metatable
        module->lua_setup(L);

        // Create userdata
        void* ud = lua_newuserdata(L, sizeof(void*));
        *std::bit_cast<Module**>(ud) = module.get();

        // Set userdata metatable
        luaL_getmetatable(L, module->m_short_name);
        assert(lua_type(L, -1) != LUA_TNIL);
        lua_setmetatable(L, -2);

        lua_setfield(L, -2, module->m_short_name);
    }
    lua_setfield(L, -2, "modules");

    lua_setglobal(L, "cluamsy");
}

Lua::~Lua() {
    if (m_lua_state != nullptr) {
        LOG("Destroying ua state");
        lua_close(m_lua_state);
    }
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
