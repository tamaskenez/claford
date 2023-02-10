#include <fmt/format.h>
#include <glog/logging.h>
#include <readerwriterqueue/readerwriterqueue.h>
#include <libfswatch/c++/monitor_factory.hpp>
#include <nowide/args.hpp>
#include <nowide/cstdlib.hpp>
#include <nowide/iostream.hpp>

#include <atomic>
#include <csignal>
#include <filesystem>
#include <functional>
#include <set>
#include <string_view>
#include <thread>
#include <unordered_map>

#define BE(X) (X).begin(), (X).end()

std::atomic<bool> g_sigint_received;

void signal_handler(int /* signum */) {
    g_sigint_received = true;
}

namespace fs = std::filesystem;

constexpr int k_formatting_loop_sleep_millisec = 10;
constexpr double k_monitor_latency_sec = 0.01;

struct Context {
    struct Options {
        std::vector<std::string> paths;
        std::vector<fs::path> extensions = {
            ".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
        bool need_extension(const fs::path& e) const {
            return std::find(BE(extensions), e) != extensions.end();
        }
    } options;
    moodycamel::ReaderWriterQueue<std::string> f2m_queue;  // Formatter to monitor queue.
};

void DisplayHelp() {
    fmt::print("claford - clang-format daemon\n");
    fmt::print("Watch directories and automatically clang-format changed files.\n");
    fmt::print("Usage: claford [options] paths...\n\n");
    fmt::print("   -h|--help: this help\n");
    fmt::print("\n");
    fmt::print("paths... is a list of directories to watch\n");
}

std::u8string_view reinterpret_u8(const std::string& s) {
    return std::u8string_view(reinterpret_cast<const char8_t*>(s.c_str()), s.size());
}

std::string_view reinterpret_char(std::u8string_view sv) {
    return std::string_view(reinterpret_cast<const char*>(sv.data()), sv.size());
}

void fsw_event_callback(const std::vector<fsw::event>& es, void* void_ctx) {
    auto* ctx = static_cast<Context*>(void_ctx);
    std::vector<std::string> paths;
    for (auto& e : es) {
        auto path = e.get_path();
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
        CHECK(ctx->f2m_queue.enqueue(std::move(p)));
    }
}

void FormattingLoop(Context& ctx) {
    std::string path;
    std::unordered_map<std::string, fs::file_time_type> paths_formatted_at;
    for (;;) {
        if (!ctx.f2m_queue.try_dequeue(path)) {
            if (g_sigint_received) {
                return;
            }
            std::this_thread::sleep_for(
                std::chrono::milliseconds(k_formatting_loop_sleep_millisec));
            continue;
        }
        // Filter by extension.
        auto fs_path = fs::path(reinterpret_u8(path));
        if (!ctx.options.need_extension(fs_path.extension())) {
            continue;
        }
        // Ignore non-existing.
        if (!fs::exists(reinterpret_u8(path))) {
            paths_formatted_at.erase(path);
            continue;
        }
        // Ignore files not changed since formatting.
        auto it = paths_formatted_at.find(path);
        if (it != paths_formatted_at.end()) {
            auto last_write_time = fs::last_write_time(path);
            if (last_write_time == it->second) {
                continue;
            }
        }
        // Format.
        int result = system(fmt::format("clang-format -i {}", path).c_str());
        if (result == EXIT_SUCCESS) {
            paths_formatted_at[path] = fs::last_write_time(path);
            nowide::cout << "Formatted " << path << "\n";
        } else {
            paths_formatted_at.erase(path);
            nowide::cout << "ERROR formatting " << path << "\n";
        }
    }
}

int main_core(int argc, char* argv[]) {
    nowide::args _(argc, argv);

    FLAGS_logtostderr = 1;
    google::InitGoogleLogging(argv[0]);

    Context ctx;
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
            os.paths.push_back(std::string(ai));
        }
    }

    if (os.paths.empty()) {
        nowide::cerr << "No path specified.\n";
        return EXIT_FAILURE;
    }

    if (nowide::system("clang-format --version") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    int n_invalid_paths = 0;
    for (auto& p : os.paths) {
        auto fs_path = fs::path(reinterpret_u8(p));
        if (!fs::exists(fs_path)) {
            ++n_invalid_paths;
            nowide::cerr << "ERROR: Path " << p << " doesn't exist.\n";
        } else if (!fs::is_directory(fs_path)) {
            ++n_invalid_paths;
            nowide::cerr << "ERROR: Path " << p << " is not a directory.\n";
        }
    }
    if (n_invalid_paths > 0) {
        return EXIT_FAILURE;
    }

    auto* monitor = fsw::monitor_factory::create_monitor(
        fsw_monitor_type::system_default_monitor_type, os.paths, fsw_event_callback, &ctx);

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
    FormattingLoop(ctx);
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
