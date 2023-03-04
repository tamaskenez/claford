#include "ui_glfw_imgui.h"

#include "state.h"
#include "util.h"

#include <GLFW/glfw3.h>  // Will drag system OpenGL headers
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace fs = std::filesystem;
namespace chr = std::chrono;

namespace {
void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    exit(EXIT_FAILURE);
}
}  // namespace

struct UI_GLFW_ImGui : public UI {
    GLFWwindow* window;
    const State& ctx;
    ToAppQueue& to_app_queue;
    bool format_on_focus = false;
    std::optional<ImVec2> format_all_button_size;
    int window_has_focus = 1;
    bool dark_mode = false;
    UI_GLFW_ImGui(GLFWwindow* window, const State& ctx, ToAppQueue& to_app_queue)
        : window(window)
        , ctx(ctx)
        , to_app_queue(to_app_queue) {}
    ~UI_GLFW_ImGui() {
        // Cleanup
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void ApplyDarkMode() {
        dark_mode ? ImGui::StyleColorsDark() : ImGui::StyleColorsLight();
    }

    void exec(std::function<void()> process_msgs_fn) override {
        bool show_demo_window = false;
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        ApplyDarkMode();
        while (!glfwWindowShouldClose(window)) {
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear
            // imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main
            // application, or clear/overwrite your copy of the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your
            // main application, or clear/overwrite your copy of the keyboard data. Generally you
            // may always pass all inputs to dear imgui, and hide them from your application based
            // on those two flags.
            glfwPollEvents();

            {
                int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
                if (focused && !window_has_focus && format_on_focus) {
                    to_app_queue.enqueue(msg::FormatAll{});
                }
                window_has_focus = focused;
            }

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()!
            // You can browse its code to learn more about Dear ImGui!).
            if (show_demo_window) {
                ImGui::ShowDemoWindow(&show_demo_window);
            }
            bool new_dark_mode;
            {
                auto* mvp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(mvp->WorkPos);
                ImGui::SetNextWindowSize(mvp->WorkSize);

                ImGui::Begin("A", nullptr, ImGuiWindowFlags_NoTitleBar);

                const char* kFormatAllButtonLabel = "Format All";
                if (format_all_button_size && ctx.paths_to_format_since.empty()) {
                    ImGui::InvisibleButton(kFormatAllButtonLabel, *format_all_button_size);
                } else {
                    if (ImGui::Button(kFormatAllButtonLabel)) {
                        to_app_queue.enqueue(msg::FormatAll{});
                    }
                    format_all_button_size = ImGui::GetItemRectSize();
                }
                ImGui::SameLine();
                ImGui::Checkbox("Format On Focus", &format_on_focus);
                ImGui::SameLine();
                ImGui::Checkbox("Demo", &show_demo_window);
                ImGui::SameLine();
                ImGui::Checkbox("Dark", &new_dark_mode);

                using TimeAndPath = std::pair<fs::file_time_type, std::string>;
                std::vector<TimeAndPath> paths;
                for (auto& [path, t] : ctx.paths_to_format_since) {
                    paths.emplace_back(t, path);
                }
                std::sort(BE(paths));
                std::reverse(BE(paths));

                auto now = fs::file_time_type::clock::now();
                for (auto& [t, p] : paths) {
                    auto age = now - t;
                    std::string s;
                    if (age < chr::seconds(1)) {
                        s = fmt::format("{} ms",
                                        chr::duration_cast<chr::milliseconds>(age).count());
                    } else if (age < chr::minutes(1)) {
                        s = fmt::format("{} s", chr::duration_cast<chr::seconds>(age).count());
                    } else if (age < chr::hours(1)) {
                        s = fmt::format("{} s", chr::duration_cast<chr::minutes>(age).count());
                    } else if (age < chr::hours(24)) {
                        s = fmt::format("{} s", chr::duration_cast<chr::hours>(age).count());
                    } else {
                        s = fmt::format("{} s", chr::duration_cast<chr::days>(age).count());
                    }
                    ImGui::Text("%s", p.c_str());
                    ImGui::SameLine();
                    ImGui::Text("%s", s.c_str());
                }
                ImGui::End();
            }

            // Rendering
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(clear_color.x * clear_color.w,
                         clear_color.y * clear_color.w,
                         clear_color.z * clear_color.w,
                         clear_color.w);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            process_msgs_fn();
            glfwSwapBuffers(window);
            std::this_thread::sleep_for(chr::milliseconds(window_has_focus ? 1 / 60 : 1 / 20));

            if (new_dark_mode != dark_mode) {
                dark_mode = new_dark_mode;
                ApplyDarkMode();
            }
        }
    }
};

std::unique_ptr<UI> make_ui_glfw_imgui(const State& ctx, ToAppQueue& to_app_queue) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit failed.\n");
        return nullptr;
    }

// Decide GL+GLSL versions
#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac

    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
// glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
// glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    int xpos, ypos, width, height;
    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &xpos, &ypos, &width, &height);
    GLFWwindow* window = glfwCreateWindow(height / 2, height / 2, "claford", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "glfwCreateWindow failed.\n");
        return nullptr;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple
    // fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
    // font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in
    // your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture
    // when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below
    // will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher
    // quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to
    // write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the
    // "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL,
    // io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

    return std::make_unique<UI_GLFW_ImGui>(window, ctx, to_app_queue);
}
