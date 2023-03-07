#pragma once

#include <filesystem>
#include <optional>
#include <string>

#define BE(X) (X).begin(), (X).end()

std::filesystem::path PathFromUtf8(const std::string s);
std::string ToUtf8(const std::filesystem::path& path);
bool fs_exists_noexcept(const std::filesystem::path& path);
bool fs_is_directory_noexcept(const std::filesystem::path& path);
std::optional<std::filesystem::file_time_type> fs_last_write_time_noexcept(
    const std::filesystem::path& path);
