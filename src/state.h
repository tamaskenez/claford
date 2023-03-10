#pragma once

#include <moodycamel/concurrentqueue.h>

#include <any>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using ToAppQueue = moodycamel::ConcurrentQueue<std::any>;

struct State {
    struct Options {
        std::vector<std::string> paths;
        std::set<std::filesystem::path> extensions = {
            ".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
    } options;
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_formatted_at;
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_to_format_since;
    ToAppQueue to_app_queue;
};

namespace msg {
struct Idle {};
struct FormatAll {};
struct AddAll {};
struct FileChanged {
    std::string path;
};
struct FormatOne {
    std::string path;
};
struct TouchOne {
    std::string path;
};
}  // namespace msg
