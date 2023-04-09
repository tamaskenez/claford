#include "util.h"

#include <string_view>

namespace fs = std::filesystem;

namespace {
std::u8string_view reinterpret_u8(std::string_view s) {
    return std::u8string_view(reinterpret_cast<const char8_t*>(s.data()), s.size());
}

std::string_view reinterpret_char(std::u8string_view sv) {
    return std::string_view(reinterpret_cast<const char*>(sv.data()), sv.size());
}
}  // namespace

std::filesystem::path PathFromUtf8(std::string_view s) {
    return fs::path(reinterpret_u8(s));
}

std::string ToUtf8(const std::filesystem::path& path) {
    return std::string(reinterpret_char(path.u8string()));
}

bool fs_exists_noexcept(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool fs_is_directory_noexcept(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

std::optional<std::filesystem::file_time_type> fs_last_write_time_noexcept(
    const std::filesystem::path& path) {
    std::error_code ec;
    auto r = fs::last_write_time(path, ec);
    if (ec) {
        return std::nullopt;
    }
    return r;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && isspace(s[0])) {
        s.remove_prefix(1);
    }
    while (!s.empty() && isspace(s.back())) {
        s.remove_suffix(1);
    }
    return s;
}
