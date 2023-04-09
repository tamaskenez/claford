#include "clang_format.h"
#include "state.h"
#include "ui_glfw_imgui.h"
#include "util.h"

#include <fmt/format.h>
#include <glog/logging.h>
#include <libfswatch/c++/monitor_factory.hpp>
#include <nowide/args.hpp>
#include <nowide/iostream.hpp>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <functional>
#include <list>
#include <set>
#include <string_view>
#include <thread>
#include <unordered_map>

std::atomic<bool> g_sigint_received;

void signal_handler(int /* signum */) {
    g_sigint_received = true;
}

namespace fs = std::filesystem;

constexpr double k_monitor_latency_sec = 0.02;

void DisplayHelp() {
    fmt::print("claford - clang-format daemon\n");
    fmt::print("Watch directories and automatically clang-format changed files.\n");
    fmt::print("Usage: claford [options] paths...\n\n");
    fmt::print("   -h|--help: this help\n");
    fmt::print("\n");
    fmt::print("paths... is a list of directories to watch\n");
}

void fsw_event_callback(const std::vector<fsw::event>& es, void* void_ctx) {
    auto* ctx = static_cast<State*>(void_ctx);
    std::vector<fs::path> paths;
    for (auto& e : es) {
        auto path = PathFromUtf8(e.get_path());
        bool cf = false;
        bool is_file = false;
        for (auto f : e.get_flags()) {
            switch (f) {
                case NoOp:
                case PlatformSpecific:
                    break;
                case Created:
                case Updated:
                case Removed:
                case Renamed:
                    cf = true;
                    break;
                case OwnerModified:
                case AttributeModified:
                    break;
                case MovedFrom:
                case MovedTo:
                    cf = true;
                    break;
                case IsFile:
                    is_file = true;
                    break;
                case IsDir:
                case IsSymLink:
                case Link:
                case Overflow:
                    break;
            }
        }
        if (is_file && cf) {
            paths.push_back(std::move(path));
        }
    }
    std::sort(BE(paths));
    paths.erase(std::unique(BE(paths)), paths.end());
    for (auto& p : paths) {
        CHECK(ctx->to_app_queue.enqueue(msg::FileChanged{std::move(p)}));
    }
}

void FileChanged(const fs::path& path, State& ctx) {
    // Filter by extension.
    if (!ctx.options.extensions.contains(path.extension())) {
        return;
    }
    // Ignore non-existing.
    if (!fs_exists_noexcept(path)) {
        ctx.paths_formatted_at.erase(path);
        ctx.paths_to_format_since.erase(path);
        return;
    }
    // Ignore files not changed since formatting.
    auto last_write_time = fs_last_write_time_noexcept(path);
    if (!last_write_time) {
        ctx.paths_formatted_at.erase(path);
        ctx.paths_to_format_since.erase(path);
        return;
    }
    auto it = ctx.paths_formatted_at.find(path);
    if (it != ctx.paths_formatted_at.end()) {
        if (*last_write_time == it->second) {
            return;
        }
    }
    if (ctx.clang_format->is_file_formatted(path)) {
        // Already formatted.
        ctx.paths_formatted_at[path] = *last_write_time;
        ctx.paths_to_format_since.erase(path);
    } else {
        // Needs formatting.
        ctx.paths_formatted_at.erase(path);
        ctx.paths_to_format_since[path] = *last_write_time;
    }
}

ProcessMsgsResult ProcessMsgs(State& ctx) {
    std::any msg;
    for (;;) {
        if (g_sigint_received) {
            return ProcessMsgsResult::ShouldExit;
        }
        if (!ctx.to_app_queue.try_dequeue(msg)) {
            return ProcessMsgsResult::QueueWasEmpty;
        }
        if (auto* c = std::any_cast<msg::FileChanged>(&msg)) {
            FileChanged(c->path, ctx);
        } else if (std::any_cast<msg::AddAll>(&msg)) {
            std::vector<fs::path> all_files;
            for (auto& path : ctx.options.paths) {
                for (auto const& dir_entry : fs::recursive_directory_iterator(path)) {
                    std::error_code ec;
                    auto canonical_path = fs::canonical(dir_entry.path(), ec);
                    if (ec) {
                        fprintf(stderr,
                                "Can't convert %s to canonical path, reason: %s\n",
                                dir_entry.path().string().c_str(),
                                ec.message().c_str());
                        continue;
                    }
                    all_files.push_back(canonical_path.c_str());
                }
            }
            std::sort(all_files.begin(), all_files.end());
            all_files.erase(std::unique(all_files.begin(), all_files.end()), all_files.end());
            for (auto& f : all_files) {
                FileChanged(f, ctx);
            }
        } else if (std::any_cast<msg::FormatAll>(&msg)) {
            std::vector<fs::path> formatted_paths;
            for (auto it = ctx.paths_to_format_since.begin(); it != ctx.paths_to_format_since.end();
                 /* no inc */) {
                if (ctx.clang_format->format_file_in_place(it->first)) {
                    // Use "now" if failed to query last write time (silently ignoring this rare
                    // error).
                    ctx.paths_formatted_at[it->first] =
                        fs_last_write_time_noexcept(it->first).value_or(
                            fs::file_time_type::clock::now());
                    nowide::cout << "Formatted " << it->first << "\n";
                    it = ctx.paths_to_format_since.erase(it);
                } else {
                    nowide::cout << "ERROR formatting " << it->first << "\n";
                    ++it;
                }
            }
        } else if (auto* fo = std::any_cast<msg::FormatOne>(&msg)) {
            if (ctx.clang_format->format_file_in_place(fo->path)) {
                // Use "now" if failed to query last write time (silently ignoring this rare
                // error).
                ctx.paths_formatted_at[fo->path] = fs_last_write_time_noexcept(fo->path).value_or(
                    fs::file_time_type::clock::now());
                nowide::cout << "Formatted " << fo->path << "\n";
                ctx.paths_to_format_since.erase(fo->path);
            } else {
                nowide::cout << "ERROR formatting " << fo->path << "\n";
            }
        } else if (auto* to = std::any_cast<msg::TouchOne>(&msg)) {
            std::error_code ec;
            const auto now = fs::file_time_type::clock::now();
            fs::last_write_time(to->path, now, ec);
            if (!ec) {
                // Assume touch is for formatted files.
                auto it = ctx.paths_formatted_at.find(to->path);
                if (it != ctx.paths_formatted_at.end()) {
                    it->second = fs_last_write_time_noexcept(to->path).value_or(now);
                } else {
                    assert(false);
                }
            }
        } else {
            fprintf(stderr, "Invalid message\n");
            assert(false);
        }
        return ProcessMsgsResult::QueueWasNotEmpty;
    }
}

int main_core(int argc, char* argv[]) {
    nowide::args _(argc, argv);

    FLAGS_logtostderr = 1;
    google::InitGoogleLogging(argv[0]);

    State ctx;
    auto& os = ctx.options;

    for (int i = 1; i < argc; ++i) {
        auto ai = std::string_view(argv[i]);
        if (ai.starts_with("-")) {
            if (ai == "-h" || ai == "--help") {
                DisplayHelp();
            } else {
                nowide::cerr << "Invalid option: " << ai << "\n";
                return EXIT_FAILURE;
            }
        } else {
            std::error_code ec;
            auto abs_path = fs::absolute(PathFromUtf8(ai), ec);
            if (ec) {
                std::cerr << "Can't convert path to absolute: " << argv[i] << "\n";
                return EXIT_FAILURE;
            }
            fmt::print("{} -> abs -> {}\n", ai, abs_path.string());
            os.paths.push_back(abs_path.string());
        }
    }

    if (os.paths.empty()) {
        nowide::cerr << "No path specified.\n";
        return EXIT_FAILURE;
    }

    auto clang_format = ClangFormat::make();
    if (!clang_format) {
        fmt::print(stderr, "Error: {}\n", clang_format.error());
        return EXIT_FAILURE;
    }
    ctx.clang_format = std::move(*clang_format);

    int n_invalid_paths = 0;
    for (auto& p : os.paths) {
        if (!fs_exists_noexcept(p)) {
            ++n_invalid_paths;
            nowide::cerr << "ERROR: Path " << ToUtf8(p) << " doesn't exist.\n";
        } else if (!fs_is_directory_noexcept(p)) {
            ++n_invalid_paths;
            nowide::cerr << "ERROR: Path " << ToUtf8(p) << " is not a directory.\n";
        }
    }
    if (n_invalid_paths > 0) {
        return EXIT_FAILURE;
    }

    auto ui = make_ui_glfw_imgui(ctx, ctx.to_app_queue);
    if (!ui) {
        return EXIT_FAILURE;
    }

    std::vector<std::string> paths;
    paths.reserve(paths.size());
    for (auto& p : os.paths) {
        paths.push_back(ToUtf8(p));
    }
    auto* monitor = fsw::monitor_factory::create_monitor(
        fsw_monitor_type::system_default_monitor_type, paths, fsw_event_callback, &ctx);

    if (!monitor) {
        std::cerr << "ERROR: couldn't create system default filesystem monitor\n";
        return EXIT_FAILURE;
    }

    monitor->set_latency(k_monitor_latency_sec);
    monitor->set_recursive(true);

    std::signal(SIGINT, signal_handler);

    auto monitor_thread = std::thread([monitor]() {
        monitor->start();  // Enters event loop, returns when stopped.
    });

    fmt::print("claford is running, CTRL-C to exit...\n");
    ui->exec([&ctx]() {
        return ProcessMsgs(ctx);
    });
    monitor->stop();

    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    try {
        return main_core(argc, argv);
    } catch (std::exception& e) {
        nowide::cerr << "Terminating with exception: " << e.what() << "\n";
    } catch (...) {
        nowide::cerr << "Terminating with unknown exception.\n";
    }
    return EXIT_FAILURE;
}
