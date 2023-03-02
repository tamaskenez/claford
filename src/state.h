#pragma once

#include <moodycamel/concurrentqueue.h>

#include <any>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct State {
    struct Options {
        std::vector<std::string> paths;
        std::set<std::filesystem::path> extensions = {
            ".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
    } options;
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_formatted_at;
    std::unordered_map<std::string, std::filesystem::file_time_type> paths_to_format_since;
    moodycamel::ConcurrentQueue<std::any> to_app_queue;
};

namespace msg {
struct Idle {};
struct FormatAll {};
struct FileChanged {
    std::string path;
};
}  // namespace msg
