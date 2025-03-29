#pragma once

#include <SDL.h>
#include <list>
#include <lua.hpp>
#include <memory>
#include <string>
#include <vector>

#include "module.hpp"

class Lua {
private:
    struct Script {
        Lua* lua;
        std::string file_name;
    };

public:
    Lua() = delete;

    Lua(Lua&& other);
    Lua& operator=(Lua&& other);

    Lua(const std::vector<std::shared_ptr<Module>>& modules);
    ~Lua();

    void push_api(const std::vector<std::shared_ptr<Module>>& modules);

    lua_State* state() const { return m_lua_state; };
    bool load_script(const std::string& file_name);
    bool unload_script(const std::string& file_name);

    const std::list<Script>& scripts() const { return m_scripts; }

private:
    void set_registry_ref();

    static uint32_t timer_callback_trampoline(uint32_t interval, void* param);
    static int timer_create(lua_State* L);
    static int timer_remove(lua_State* L);
    void push_timer_api();
    void push_module_api(const Module& module);

private:
    lua_State* m_lua_state;

    struct TimerData {
        Lua* lua;
        SDL_TimerID timer_id;
    };

    std::list<Script> m_scripts;
    std::list<TimerData> m_timer_data;
};
