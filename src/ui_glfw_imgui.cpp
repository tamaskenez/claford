#include "ui_glfw_imgui.h"

#include "Inter-Regular.ttf.h"
#include "Karla-Regular.ttf.h"
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
const std::string kEllipsis = "...";
void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    exit(EXIT_FAILURE);
}

enum class EllipsisFormat { Intact, UseEllipsis, ReplaceWithEllipsis, Nothing };

std::string AgoText(fs::file_time_type::duration age) {
    if (age < chr::seconds(1)) {
        return fmt::format("{} ms", chr::duration_cast<chr::milliseconds>(age).count());
    } else if (age < chr::minutes(1)) {
        return fmt::format("{} sec", chr::duration_cast<chr::seconds>(age).count());
    } else if (age < chr::hours(1)) {
        return fmt::format("{} min", chr::duration_cast<chr::minutes>(age).count());
    } else if (age < chr::hours(24)) {
        return fmt::format("{} hrs", chr::duration_cast<chr::hours>(age).count());
    } else {
        return fmt::format("{} days", chr::duration_cast<chr::days>(age).count());
    }
}

std::string FormatDirWithEllipsis(const std::string& s, EllipsisFormat ef) {
    switch (ef) {
        case EllipsisFormat::Intact:
            return s;
        case EllipsisFormat::UseEllipsis:
            return kEllipsis + s;
        case EllipsisFormat::ReplaceWithEllipsis:
            return kEllipsis;
        case EllipsisFormat::Nothing:
            return {};
    }
}

std::string FormatFilenameWithEllipsis(const std::string& stem,
                                       const std::string ext,
                                       EllipsisFormat es) {
    switch (es) {
        case EllipsisFormat::Intact:
            return stem + ext;
        case EllipsisFormat::UseEllipsis:
            return stem + kEllipsis + ext;
        case EllipsisFormat::ReplaceWithEllipsis:
            return kEllipsis;
        case EllipsisFormat::Nothing:
            return {};
    }
}

std::pair<std::string, float> FitDirIntoWidth(std::string s, float max_width) {
    auto ef = EllipsisFormat::Intact;
    for (;;) {
        auto es = FormatDirWithEllipsis(s, ef);
        auto w = ImGui::CalcTextSize(es.c_str()).x;
        if (w <= max_width) {
            return std::make_pair(std::move(es), w);
        }
        switch (ef) {
            case EllipsisFormat::Intact:
            case EllipsisFormat::UseEllipsis:
                if (s.size() <= 1) {
                    ef = EllipsisFormat::ReplaceWithEllipsis;
                } else {
                    ef = EllipsisFormat::UseEllipsis;
                    s = s.substr(1);
                }
                break;
            case EllipsisFormat::ReplaceWithEllipsis:
            case EllipsisFormat::Nothing:
                return std::make_pair(std::string(), 0.0f);
        }
    }
}

std::pair<std::string, float> FitFilenameIntoWidth(std::string stem,
                                                   std::string ext,
                                                   float max_width) {
    auto ef = EllipsisFormat::Intact;
    for (;;) {
        auto es = FormatFilenameWithEllipsis(stem, ext, ef);
        auto w = ImGui::CalcTextSize(es.c_str()).x;
        if (w <= max_width) {
            return std::make_pair(std::move(es), w);
        }
        switch (ef) {
            case EllipsisFormat::Intact:
            case EllipsisFormat::UseEllipsis:
                if (!ext.empty() && ext[0] == '.') {
                    ext = ext.substr(1);
                    ef = EllipsisFormat::UseEllipsis;
                } else if (stem.size() > 1) {
                    ef = EllipsisFormat::UseEllipsis;
                    stem.pop_back();
                } else {
                    ef = EllipsisFormat::ReplaceWithEllipsis;
                }
                break;
            case EllipsisFormat::ReplaceWithEllipsis:
            case EllipsisFormat::Nothing:
                return std::make_pair(std::string(), 0.0f);
        }
    }
}

fs::path RemoveBaseDirs(const std::vector<fs::path>& base_dirs, const fs::path& path) {
    if (base_dirs.empty()) {
        return path;
    }
    std::optional<fs::path> result;
    for (auto& bd : base_dirs) {
        std::error_code ec;
        auto pr = proximate(path, bd, ec);
        if (!ec && (!result || pr.native().size() < result->native().size())) {
            result = pr;
        }
    }
    if (result) {
        return std::move(*result);
    }
    return path;
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

            // ImGui::ShowDemoWindow(&show_demo_window);
            bool new_dark_mode = dark_mode;
            {
                constexpr bool use_work_area = true;
                auto* mvp = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(use_work_area ? mvp->WorkPos : mvp->Pos);
                ImGui::SetNextWindowSize(use_work_area ? mvp->WorkSize : mvp->Size);

                ImGui::Begin("claford",
                             nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoSavedSettings);

                const char* kFormatAllButtonLabel = "Format All";

                if (format_all_button_size
                    && (format_on_focus || ctx.paths_to_format_since.empty())) {
                    ImGui::InvisibleButton(kFormatAllButtonLabel, *format_all_button_size);
                } else {
                    if (ImGui::Button(kFormatAllButtonLabel)) {
                        to_app_queue.enqueue(msg::FormatAll{});
                    }
                    format_all_button_size = ImGui::GetItemRectSize();
                }
                ImGui::SameLine();
                bool prev_format_on_focus = format_on_focus;
                ImGui::Checkbox("Format On Focus", &format_on_focus);
                if (!prev_format_on_focus && format_on_focus) {
                    to_app_queue.enqueue(msg::FormatAll{});
                }
                ImGui::SameLine();
                if (ImGui::Button("Add All")) {
                    to_app_queue.enqueue(msg::AddAll{});
                }
                ImGui::SameLine();
                ImGui::Checkbox("Dark", &new_dark_mode);

                ImGui::Separator();

                struct Entry {
                    fs::path path;
                    std::string dir, stem, ext;
                    fs::file_time_type time;
                    bool formatted;

                    static Entry Make(const std::vector<fs::path>& base_dirs,
                                      const fs::path& path_in,
                                      fs::file_time_type time,
                                      bool formatted) {
                        auto path = RemoveBaseDirs(base_dirs, path_in);
                        return Entry{path_in,
                                     ToUtf8(path.parent_path()),
                                     ToUtf8(path.stem()),
                                     ToUtf8(path.extension()),
                                     time,
                                     formatted};
                    }
                };
                std::vector<Entry> entries;
                for (auto& [path, t] : ctx.paths_to_format_since) {
                    entries.push_back(Entry::Make(ctx.options.paths, path, t, false));
                }
                for (auto& [path, t] : ctx.paths_formatted_at) {
                    entries.push_back(Entry::Make(ctx.options.paths, path, t, true));
                }
                std::sort(BE(entries), [](const Entry& a, const Entry& b) {
                    return b.time < a.time;
                });

                const auto now = fs::file_time_type::clock::now();
                float max_ago_text_width =
                    std::max(ImGui::CalcTextSize("Format!").x, ImGui::CalcTextSize("Touch!").x);
                for (auto& e : entries) {
                    auto age = now - e.time;
                    auto agoText = AgoText(age);
                    max_ago_text_width =
                        std::max(max_ago_text_width, ImGui::CalcTextSize(agoText.c_str()).x);
                }
                const auto gap = ImGui::GetStyle().ItemInnerSpacing.x;
                const auto min_cursor_pos_x = ImGui::GetCursorPosX();
                const auto max_cursor_pos_x = ImGui::GetContentRegionMax().x;
                const auto content_width = max_cursor_pos_x - min_cursor_pos_x;
                const auto max_path_width = content_width - max_ago_text_width - gap;
                static int counter = 0;
                for (auto& e : entries) {
                    auto edir = e.dir;
                    if (!edir.empty()) {
                        edir += fs::path::preferred_separator;
                    }
                    auto [dir, dir_width] = FitDirIntoWidth(edir, max_path_width / 2);
                    auto [filename, filename_width] =
                        FitFilenameIntoWidth(e.stem, e.ext, max_path_width / 2);

                    auto age = now - e.time;

                    // auto& style = ImGui::GetStyle();
                    //  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y);
                    const auto mp = ImGui::GetMousePos();
                    const auto cpy = ImGui::GetCursorPos().y;
                    bool hover = min_cursor_pos_x <= mp.x && mp.x < max_cursor_pos_x && cpy <= mp.y
                              && mp.y < cpy + ImGui::GetTextLineHeightWithSpacing();

                    ImGui::SetCursorPosX(max_cursor_pos_x - max_ago_text_width - gap
                                         - max_path_width / 2 - dir_width);
                    ImGui::Selectable(fmt::format("{}##{}", dir, counter++).c_str());
                    ImGui::SameLine(max_cursor_pos_x - max_ago_text_width - gap
                                    - max_path_width / 2);

                    auto color = e.formatted
                                   ? (dark_mode ? ImVec4(0, 1, 0, 1) : ImVec4(0, 0.7, 0, 1))
                                   : (dark_mode ? ImVec4(1, 0, 0, 1) : ImVec4(0.7, 0, 0, 1));
                    ImGui::TextColored(color, "%s", filename.c_str());
                    ImGui::SameLine(max_cursor_pos_x - max_ago_text_width);
                    // bool formatOne = false;
                    //  ImGui::SetCursorPosY(ImGui::GetCursorPosY() - style.FramePadding.y);
                    ImGui::TextUnformatted(AgoText(age).c_str());
                    if (hover) {
                        ImGui::SetTooltip(e.formatted ? "Touch!" : "Format!");
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            if (e.formatted) {
                                to_app_queue.enqueue(msg::TouchOne{e.path});
                            } else {
                                to_app_queue.enqueue(msg::FormatOne{e.path});
                            }
                        }
                    }
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
    ImGui::GetStyle().FrameRounding = 4;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple
    // fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the
    // font among multiple.
    ImFont* font{};
    auto* p = ImGui::MemAlloc(Inter_Regular_ttf.size());
    std::copy(Inter_Regular_ttf.begin(), Inter_Regular_ttf.end(), static_cast<char*>(p));
    font = io.Fonts->AddFontFromMemoryTTF(p, Inter_Regular_ttf.size(), 16);
    if (font) {
        io.Fonts->Build();
    } else {
        fprintf(stderr, "Couldn't load font.\n");
    }

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
