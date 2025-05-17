#pragma once

#include <SDL.h>
#include <filesystem>
#include <list>
#include <lua.hpp>
#include <memory>
#include <string>
#include <vector>

#include "module.hpp"

class RegistryKey {
public:
    [[nodiscard]] std::string get() const {
        return std::format("cluamsy{:d}", m_counter);
    };

private:
    static inline size_t s_counter = 0;
    size_t m_counter = s_counter++;
};

class Lua {
private:
    struct Script {
        Lua* lua;
        std::string name;
        std::filesystem::path file_path;
        std::optional<RegistryKey> unload_function_key;
    };

    struct TimerData {
        Lua* lua;
        SDL_TimerID timer_id;
    };

public:
    Lua() noexcept;

    Lua(const Lua&) = delete;
    Lua& operator=(const Lua&) = delete;

    Lua(Lua&& other) noexcept;
    Lua& operator=(Lua&& other) noexcept;

    ~Lua();

    void push_api(const std::vector<std::shared_ptr<Module>>& modules);

    [[nodiscard]] lua_State* state() const noexcept { return m_lua_state; };
    bool load_script(const std::filesystem::path& file_path);
    bool unload_script(const std::list<Script>::const_iterator it);

    [[nodiscard]] const std::list<Script>& scripts() const noexcept {
        return m_scripts;
    }

private:
    void set_registry_ref();

    static uint32_t timer_callback_trampoline(uint32_t interval, void* param);
    static int timer_create(lua_State* L);
    static int timer_remove(lua_State* L);
    void push_timer_api();
    void push_module_api(const Module& module);

private:
    lua_State* m_lua_state;

    std::list<Script> m_scripts;
    std::list<TimerData> m_timer_data;
};
