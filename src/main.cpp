#include <fmt/format.h>
#include <glog/logging.h>
#include <libfswatch/c++/monitor_factory.hpp>
#include <nowide/args.hpp>
#include <nowide/cstdlib.hpp>
#include <nowide/iostream.hpp>

#include <filesystem>
#include <string_view>

#define BE(X) (X).begin(), (X).end()

void signal_callback_handler(int signum) {
    nowide::cerr << "Caught signal " << signum << std::endl;
    std::exit(signum);
}

struct Context {
    struct Options {
        std::vector<std::string> paths;
        std::vector<std::string> extensions = {
            ".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
    } options;
};

void DisplayHelp() {
    fmt::print("claford - clang-format daemon\n");
    fmt::print("Watch directories for change and clang-format on hotkey.\n");
    fmt::print("Usage: claford [options] paths...\n\n");
    fmt::print("   -h|--help: this help\n");
    fmt::print("\n");
    fmt::print("paths... is a list of paths (directory or file) to watch\n");
}

namespace fs = std::filesystem;

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
        auto fs_path = fs::path(reinterpret_u8(path));
        auto ext = std::string(reinterpret_char(fs_path.extension().u8string()));
        bool found = false;
        for (auto& oe : ctx->options.extensions) {
            if (ext == oe) {
                found = true;
                break;
            }
        }
        if (!found) {
            continue;
        }
        std::string op, ty;
        bool cf = false;
        bool is_file = false;
        for (auto f : e.get_flags()) {
            switch (f) {
                case NoOp:
                case PlatformSpecific:
                    break;
                case Created:
                    op += "!";
                    cf = true;
                    break;
                case Updated:
                    op += "~";
                    cf = true;
                    break;
                case Removed:
                    op += "x";
                    cf = true;
                    break;
                case Renamed:
                    op += ">";
                    cf = true;
                    break;
                case OwnerModified:
                case AttributeModified:
                    break;
                case MovedFrom:
                    op += "F";
                    cf = true;
                    break;
                case MovedTo:
                    op += "T";
                    cf = true;
                    break;
                case IsFile:
                    ty += "F";
                    is_file = true;
                    break;
                case IsDir:
                    ty += "D";
                    break;
                case IsSymLink:
                    ty += "S";
                    break;
                case Link:
                    ty += "L";
                    break;
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
        if (!fs::exists(reinterpret_u8(p))) {
            continue;
        }
        int result = nowide::system(fmt::format("clang-format -i {}", p).c_str());
        if (result == EXIT_SUCCESS) {
            nowide::cout << "Formatted " << p << "\n";
        } else {
            nowide::cout << "ERROR formatting " << p << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_callback_handler);

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

    if (nowide::system("clang-format --version") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (os.paths.empty()) {
        nowide::cerr << "No path specified.\n";
        return EXIT_FAILURE;
    }

    int n_invalid_paths = 0;
    for (auto& p : os.paths) {
        if (!fs::exists(reinterpret_u8(p))) {
            ++n_invalid_paths;
            nowide::cerr << "ERROR: Path " << p << " doesn't exist\n";
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

    monitor->set_latency(0.1);
    monitor->set_recursive(true);
    monitor->start();

    return EXIT_SUCCESS;
}
