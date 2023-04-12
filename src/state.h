#pragma once

#include "clang_format.h"

#include <moodycamel/concurrentqueue.h>
#include <readerwriterqueue/readerwriterqueue.h>

#include <any>
#include <filesystem>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

struct ACFMsg {
    enum class Command { CheckFormat, Format };

    Command command;
    std::filesystem::path path;
    std::function<void(std::filesystem::path, bool)> completion;
};

using ToAppQueue = moodycamel::ConcurrentQueue<std::any>;
using ToAsyncClangFormatQueue = moodycamel::BlockingReaderWriterQueue<ACFMsg>;

template<>
struct std::hash<std::filesystem::path> {
    size_t operator()(const std::filesystem::path& x) const noexcept {
        const std::filesystem::path::string_type& s = x.native();
        return std::hash<std::filesystem::path::string_type>()(s);
    }
};

struct State {
    struct Options {
        std::vector<std::filesystem::path> paths;
        std::set<std::filesystem::path> extensions = {
            ".cpp", ".cxx", ".c", ".m", ".mm", ".h", ".hpp", ".hxx"};
    } options;
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> paths_formatted_at;
    std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>
        paths_to_format_since;
    ToAppQueue to_app_queue;
    ToAsyncClangFormatQueue to_async_clang_format_queue;
    std::thread async_clang_format;
    std::atomic<bool> exit_flag;
};

namespace msg {
struct Idle {};
struct FormatAll {};
struct AddAll {};
struct FileChanged {
    std::filesystem::path path;
};
struct FormatOne {
    std::filesystem::path path;
};
struct TouchOne {
    std::filesystem::path path;
};
struct AsyncClangFormatResult {
    std::function<void()> completion;
};
}  // namespace msg
