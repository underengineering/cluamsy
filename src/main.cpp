#include <Windows.h>

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_syswm.h>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <imgui_internal.h>
#include <optional>
#include <string>

#include "common.hpp"
#include "completions.hpp"
#include "config.hpp"
#include "divert.hpp"
#include "events.hpp"
#include "lua.hpp"
#include "module.hpp"
#include "ui/file_dialog/common_file_dialog.hpp"


namespace ImGui {
bool InputText(const char* label, std::string* str,
               ImGuiInputTextFlags flags = ImGuiInputTextFlags_None,
               ImGuiInputTextCallback callback = nullptr,
               void* user_data = nullptr);
}

class Application {
private:
    Application(
        SDL_Window* window, SDL_GLContext gl_context,
        const std::unordered_map<std::string, toml::table>& config_entries)
        : m_config_entries(config_entries), m_window(window),
          m_gl_context(gl_context) {};

public:
    Application(Application&& other) noexcept
        : m_config_entries(std::move(other.m_config_entries)),
          m_lua(std::move(other.m_lua)), m_window(other.m_window),
          m_gl_context(other.m_gl_context) {
        // TODO: I hate move semantics
        m_lua.push_api(m_win_divert.modules());

        other.m_window = nullptr;
        other.m_gl_context = nullptr;
    }

    ~Application() {
        if (m_gl_context)
            SDL_GL_DeleteContext(m_gl_context);

        if (m_window)
            SDL_DestroyWindow(m_window);
    }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application& operator=(Application&&) = delete;

    static std::optional<Application>
    init(std::unordered_map<std::string, toml::table> config_entries) {
        SetProcessDPIAware();

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        const auto window =
            SDL_CreateWindow("cluamsy", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1280, 720,
                             SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                 SDL_WINDOW_ALLOW_HIGHDPI);
        if (window == nullptr) {
            LOG("Couldn't create window and renderer: %s", SDL_GetError());
            return std::nullopt;
        }

        const auto gl_context = SDL_GL_CreateContext(window);
        if (gl_context == nullptr) {
            LOG("Failed to create OpenGL context");
            return std::nullopt;
        }

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);

        // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
        const auto window_handle = wmInfo.info.win.window;
        // NOLINTEND(cppcoreguidelines-pro-type-union-access)

        if (!ChangeWindowMessageFilterEx(window_handle, WM_DROPFILES,
                                         MSGFLT_ALLOW, nullptr)) {
            LOG("Failed to allow WM_DROPFILES message: %lu", GetLastError());
        }

        if (!ChangeWindowMessageFilterEx(window_handle, WM_COPYDATA,
                                         MSGFLT_ALLOW, nullptr)) {
            LOG("Failed to allow WM_COPYDATA message: %lu", GetLastError());
        }

        const auto WM_COPYGLOBALDATA = 0x49;
        if (!ChangeWindowMessageFilterEx(window_handle, WM_COPYGLOBALDATA,
                                         MSGFLT_ALLOW, nullptr)) {
            LOG("Failed to allow WM_COPYGLOBALDATA message: %lu",
                GetLastError());
        }

        return Application(window, gl_context, config_entries);
    }

    bool run() {
        SDL_GL_MakeCurrent(m_window, m_gl_context);
        SDL_GL_SetSwapInterval(1);

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.ConfigFlags |=
            ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |=
            ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplSDL2_InitForOpenGL(m_window, m_gl_context);
        ImGui_ImplOpenGL3_Init();

        {
            // Query default monitor resolution
            float ddpi = 0.f, hdpi = 0.f, vdpi = 0.f;
            if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) != 0) {
                LOG("Failed to obtain DPI information for display 0: %s",
                    SDL_GetError());
                return false;
            }

            float dpi_scaling = ddpi / 96.f;
            ImGui::GetStyle().ScaleAllSizes(dpi_scaling);
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf",
                                         std::round(12.0f * dpi_scaling));
        }

        auto first_frame = true;

        // Run the main loop
        while (true) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT)
                    break;

                if (event.type == SDL_DROPFILE) {
                    m_lua.load_script(event.drop.file);
                    SDL_free(event.drop.file);
                    continue;
                }

                ImGui_ImplSDL2_ProcessEvent(&event);
            }

            if (event.type == SDL_QUIT)
                break;

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (draw()) {
                // Redraw on the next frame if state has changed
                SDL_Event event{events::REDRAW};
                SDL_PushEvent(&event);
            }

            ImGui::Render();

            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            SDL_GL_SwapWindow(m_window);

            // https://github.com/ocornut/imgui/issues/2949
            if (first_frame) {
                first_frame = false;
                continue;
            }

            SDL_WaitEvent(nullptr);
        }

        return true;
    }

    bool draw() {
        const auto& io = ImGui::GetIO();
        auto dirty = false;

        ImGui::Begin("cluamsy", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_MenuBar);
        ImGui::SetWindowPos({0, 0});
        ImGui::SetWindowSize(io.DisplaySize);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::Button("Load script")) {
                    std::filesystem::path path;
                    if (SUCCEEDED(open_file_dialog(path))) {
                        m_lua.load_script(path);
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (ImGui::Button(m_enabled ? "Stop" : "Start")) {
            toggle_win_divert();
            dirty = true;
        }
        ImGui::SameLine();

        ImGui::SetNextItemWidth(32.f * ImGui::GetFontSize());
        if (ImGui::InputText("Filter", &m_filter)) {
            m_selected_config_entry = std::nullopt;
            dirty = true;
        }

        if (ImGui::IsItemFocused()) {
            const auto completions = get_filter_completions(m_filter);
            if (completions) {
                const auto input_rect_min = ImGui::GetItemRectMin();
                const auto input_rect_max = ImGui::GetItemRectMax();

                ImGui::SetNextWindowPos({input_rect_min.x, input_rect_max.y});
                if (ImGui::Begin("filter_completions", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                     ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNavFocus)) {
                    for (const auto& completion : *completions)
                        ImGui::Text("%s", completion.data());
                    ImGui::End();
                }
            }
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(8.f * ImGui::GetFontSize());
        if (ImGui::BeginCombo(
                "Config", m_selected_config_entry.value_or("None").data())) {
            ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            for (const auto& [name, config] : m_config_entries) {
                if (ImGui::Selectable(name.c_str(),
                                      m_selected_config_entry == name)) {
                    m_filter = config["filter"].value_or("");
                    m_selected_config_entry = name;

                    for (auto& module : m_win_divert.modules()) {
                        const auto* module_entry =
                            config[module->m_short_name].as_table();
                        if (module_entry) {
                            module->apply_config(*module_entry);
                        }
                    }

                    dirty = true;
                }
            }
            ImGui::EndCombo();

            // FIXME:
            dirty = true;
        }

        if (!m_error_message.empty()) {
            ImGui::TextColored({1.f, 0.f, 0.f, 1.f}, "Failed: %s",
                               m_error_message.c_str());
        }

        ImGui::BeginChild("modules", {0.f, 0.f}, ImGuiChildFlags_ResizeY);
        for (const auto& module : m_win_divert.modules()) {
            dirty |= module->draw();
        }
        ImGui::EndChild();

        if (ImGui::BeginTable(
                "Scripts", 2,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Full path");
            ImGui::TableHeadersRow();

            int row = 1;
            for (auto it = m_lua.scripts().cbegin();
                 it != m_lua.scripts().cend(); ++it, row++) {
                ImGui::PushID(&it->name);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", it->name.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%ls", it->file_path.c_str());

                if (ImGui::TableGetHoveredRow() == row &&
                    ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("scripts_popup");
                    dirty = true;
                }

                if (ImGui::BeginPopup("scripts_popup")) {
                    if (ImGui::Button("Unload")) {
                        assert(m_lua.unload_script(it));
                        ImGui::CloseCurrentPopup();
                        dirty = true;
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        // ImGui::GetCurrentWindow()->GetID("Logs");
        // ImGui::Dummy({0.f, });
        //
        //
        // // Show logs at the bottom (TODO: docking)
        // if (ImGui::BeginChild("Logs", {0.f, 0.f},
        //                       ImGuiChildFlags_Borders |
        //                           ImGuiChildFlags_ResizeY)) {
        //     ImGui::LabelText("#lol", "fuck you");
        // }
        // ImGui::EndChild();

        ImGui::End();

        return dirty;
    }

private:
    void toggle_win_divert() {
        auto was_enabled = m_enabled;
        if (!was_enabled) {
            const auto err = m_win_divert.start(m_filter);
            if (err) {
                m_error_message = *err;
            } else {
                m_error_message = "";
                m_enabled = true;
            }
        } else {
            m_win_divert.stop();
            m_enabled = false;
        }
    }

private:
    std::unordered_map<std::string, toml::table> m_config_entries;
    std::optional<std::string_view> m_selected_config_entry;

    WinDivert m_win_divert;
    Lua m_lua;

    std::string m_filter;
    std::string m_error_message;
    // Is windivert enabled
    bool m_enabled = false;

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_gl_context = nullptr;
};

static bool check_is_running() {
    // It will be closed and destroyed when programm terminates (according to
    // MSDN).
    const auto event_handle = CreateEventW(
        nullptr, false, false, L"Global\\CLUMSY_IS_RUNNING_EVENT_NAME");

    if (event_handle == nullptr)
        return true;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(event_handle);
        return true;
    }

    return false;
}

int main(int argc, char* argv[]) {
    // LOG("Is Run As Admin: %d", IsRunAsAdmin());
    // LOG("Is Elevated: %d", IsElevated());

    if (check_is_running()) {
        MessageBoxA(NULL, "There's already an instance of clumsy running.",
                    "Aborting", MB_OK);
        return -1;
    }

    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) <
        0) {
        LOG("Couldn't initialize SDL: %s", SDL_GetError());
        return -1;
    }

    auto config_entries = parse_config();
    auto app = Application::init(config_entries.value_or(
        std::unordered_map<std::string, toml::table>()));
    if (!app) {
        LOG("App initialization failed, exiting");
        return -1;
    }

    app->run();

    SDL_Quit();

    return 0;
}
