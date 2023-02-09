#include <fmt/format.h>
#include <glog/logging.h>
#include <libfswatch/c++/monitor_factory.hpp>
#include <nowide/args.hpp>
#include <nowide/iostream.hpp>

#include <filesystem>
#include <string_view>

// std::unordered_set<std::string, Entry> entries

struct Options {
    std::vector<std::string> paths;
    std::vector<std::string> extensions = {".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
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

void fsw_event_callback(const std::vector<fsw::event>& es, void*) {
    for (auto& e : es) {
        std::string op, ty;
        for (auto f : e.get_flags()) {
            switch (f) {
                case NoOp:
                case PlatformSpecific:
                    break;
                case Created:
                    op += "!";
                    break;
                case Updated:
                    op += "~";
                    break;
                case Removed:
                    op += "x";
                    break;
                case Renamed:
                    op += ">";
                    break;
                case OwnerModified:
                case AttributeModified:
                    break;
                case MovedFrom:
                    op += "F";
                    break;
                case MovedTo:
                    op += "T";
                    break;
                case IsFile:
                    ty += "F";
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
        nowide::cout << fmt::format("[{}] {}/{}: {}\n", e.get_time(), ty, op, e.get_path());
    }
}

int main(int argc, char* argv[]) {
    nowide::args _(argc, argv);

    FLAGS_logtostderr = 1;
    google::InitGoogleLogging(argv[0]);

    Options os;

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
        fsw_monitor_type::system_default_monitor_type, os.paths, fsw_event_callback);

    if (!monitor) {
        std::cerr << "ERROR: couldn't create system default filesystem monitor\n";
        return EXIT_FAILURE;
    }

    monitor->start();

    return EXIT_SUCCESS;
}
