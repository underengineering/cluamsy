#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <SDL.h>
#include <SDL_opengl.h>
#include <cmath>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>

#include "common.hpp"
#include "divert.hpp"
#include "module.hpp"

namespace ImGui {
bool InputText(const char* label, std::string* str,
               ImGuiInputTextFlags flags = ImGuiInputTextFlags_None,
               ImGuiInputTextCallback callback = nullptr,
               void* user_data = nullptr);
}

// #include "common.hpp"

// ! the order decides which module get processed first
// Module *modules[MODULE_CNT] = {
//     &lagModule, &dropModule,   &throttleModule, &dupModule,
//     &oodModule, &tamperModule, &resetModule,    &bandwidthModule,
// };

// serializing config files using a stupid custom format
#define CONFIG_FILE "config.txt"
#define CONFIG_MAX_RECORDS 64
#define CONFIG_BUF_SIZE 4096
typedef struct {
    char* filterName;
    char* filterValue;
} filterRecord;
filterRecord filters[CONFIG_MAX_RECORDS] = {0};
char configBuf[CONFIG_BUF_SIZE + 2]; // add some padding to write \n
// BOOL parameterized =
//     0; // parameterized flag, means reading args from command line

// loading up filters and fill in
void loadConfig() {
    /* char path[MSG_BUFSIZE];
    char *p;
    FILE *f;
    GetModuleFileName(NULL, path, MSG_BUFSIZE);
    LOG("Executable path: %s", path);
    p = strrchr(path, '\\');
    if (p == NULL)
        p = strrchr(path, '/'); // holy shit
    strcpy(p + 1, CONFIG_FILE);
    LOG("Config path: %s", path);
    f = fopen(path, "r");
    if (f) {
        size_t len;
        char *current, *last;
        len = fread(configBuf, sizeof(char), CONFIG_BUF_SIZE, f);
        if (len == CONFIG_BUF_SIZE) {
            LOG("Config file is larger than %d bytes, get truncated.",
                CONFIG_BUF_SIZE);
        }
        // always patch in a newline at the end to ease parsing
        configBuf[len] = '\n';
        configBuf[len + 1] = '\0';

        // parse out the kv pairs. isn't quite safe
        filtersSize = 0;
        last = current = configBuf;
        do {
            // eat up empty lines
        EAT_SPACE:
            while (isspace(*current)) {
                ++current;
            }
            if (*current == '#') {
                current = strchr(current, '\n');
                if (!current)
                    break;
                current = current + 1;
                goto EAT_SPACE;
            }

            // now we can start
            last = current;
            current = strchr(last, ':');
            if (!current)
                break;
            *current = '\0';
            filters[filtersSize].filterName = last;
            current += 1;
            while (isspace(*current)) {
                ++current;
            } // eat potential space after :
            last = current;
            current = strchr(last, '\n');
            if (!current)
                break;
            filters[filtersSize].filterValue = last;
            *current = '\0';
            if (*(current - 1) == '\r')
                *(current - 1) = 0;
            last = current = current + 1;
            ++filtersSize;
        } while (last && last - configBuf < CONFIG_BUF_SIZE);
        LOG("Loaded %u records.", filtersSize);
    }

    if (!f || filtersSize == 0) {
        LOG("Failed to load from config. Fill in a simple one.");
        // config is missing or ill-formed. fill in some simple ones
        filters[filtersSize].filterName = "loopback packets";
        filters[filtersSize].filterValue =
            "outbound and ip.DstAddr >= 127.0.0.1 "
            "and ip.DstAddr <= 127.255.255.255";
        filtersSize = 1;
    } */
}

class Application {
private:
    Application() = default;

public:
    Application(Application&& other) {
        m_window = other.m_window;
        m_glContext = other.m_glContext;

        other.m_window = nullptr;
        other.m_glContext = nullptr;
    }

    ~Application() {
        if (m_glContext)
            SDL_GL_DeleteContext(m_glContext);

        if (m_window)
            SDL_DestroyWindow(m_window);
    }

    static std::optional<Application> init() {
        SetProcessDPIAware();

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        Application app;
        if ((app.m_window =
                 SDL_CreateWindow("cluamsy", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, 1280, 720,
                                  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                      SDL_WINDOW_ALLOW_HIGHDPI)) == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Couldn't create window and renderer: %s",
                         SDL_GetError());
            return std::nullopt;
        }

        if ((app.m_glContext = SDL_GL_CreateContext(app.m_window)) == nullptr) {
            return std::nullopt;
        }

        return app;
    }

    void run() {
        SDL_GL_MakeCurrent(m_window, m_glContext);
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
        ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext);
        ImGui_ImplOpenGL3_Init();

        if (true) {
            // Query default monitor resolution
            float ddpi, hdpi, vdpi;
            if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) != 0) {
                fprintf(stderr,
                        "Failed to obtain DPI information for display 0: %s\n",
                        SDL_GetError());
                exit(1);
            }
            float dpi_scaling = ddpi / 96.f;

            ImGui::GetStyle().ScaleAllSizes(dpi_scaling);
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf",
                                         std::round(12.0f * dpi_scaling));
        }

        // Run the main loop
        while (1) {
            SDL_Event event;
            if (SDL_PollEvent(&event) <= 0) {
                SDL_Delay(10);

                // Check if modules need to be rerendered
                bool modules_dirty = false;
                for (auto& module : g_modules) {
                    modules_dirty |= module->m_dirty;
                    module->m_dirty = false;
                }

                if (!m_dirty && !modules_dirty)
                    continue;

                m_dirty = false;
            }

            if (event.type == SDL_QUIT)
                break;

            ImGui_ImplSDL2_ProcessEvent(&event);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            draw();

            ImGui::Render();

            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            SDL_GL_SwapWindow(m_window);
        }
    }

    void draw() {
        const auto& io = ImGui::GetIO();

        ImGui::Begin("cluamsy", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
        ImGui::SetWindowPos({0, 0});
        ImGui::SetWindowSize(io.DisplaySize);

        if (ImGui::Button(m_enabled ? "Stop" : "Start")) {
            toggle_win_divert();
            m_dirty = true;
        }
        ImGui::SameLine();
        ImGui::InputText("Filter", &m_filter);

        for (const auto& module : g_modules) {
            m_dirty |= module->draw();
        }

        ImGui::End();
    }

private:
    void toggle_win_divert() {
        bool was_enabled = m_enabled;
        if (!was_enabled) {
            printf("start '%s'\n", m_filter.c_str());

            char buf[MSG_BUFSIZE];
            if (!divertStart(m_filter.c_str(), buf)) {
                printf("FAIL: %s\n", buf);
            }
            m_enabled = true;
        } else {
            printf("stop\n");
            divertStop();
            m_enabled = false;
        }
    }

private:
    std::string m_filter;
    // Is windivert enabled
    bool m_enabled = false;
    // Should rerender
    bool m_dirty = false;

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
};

static bool check_is_running() {
    // It will be closed and destroyed when programm terminates (according to
    // MSDN).
    HANDLE hStartEvent = CreateEventW(NULL, FALSE, FALSE,
                                      L"Global\\CLUMSY_IS_RUNNING_EVENT_NAME");

    if (hStartEvent == NULL)
        return TRUE;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hStartEvent);
        hStartEvent = NULL;
        return TRUE;
    }

    return FALSE;
}

int main(int argc, char* argv[]) {
    // LOG("Is Run As Admin: %d", IsRunAsAdmin());
    // LOG("Is Elevated: %d", IsElevated());

    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    printf("test\n");

    // fill in config
    // loadConfig();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) <
        0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Couldn't initialize SDL: %s", SDL_GetError());
        exit(-1);
    }

    auto app = Application::init();
    if (!app) {
        exit(-1);
    }

    // lua_state_init();

    // parse arguments and set globals *before* setting up UI.
    // arguments can be read and set after callbacks are setup
    // LOG("argc: %d", argc);
    if (argc > 1) {
        /* if (!parseArgs(argc, argv)) {
            fprintf(stderr,
                    "invalid argument count. ensure you're using options as "
                    "\"--drop on\"");
            exit(-1); // fail fast.
        }
        parameterized = 1; */
    }

    app->run();

    printf("oh\n");
    SDL_Quit();

    // endTimePeriod(); // try close if not closing

    // lua_state_close();

    return 0;
}