#pragma once

#include <filesystem>

namespace clang_format {
bool IsFileFormatted(const std::filesystem::path& p);
bool FormatFileInPlace(const std::filesystem::path& p);
}  // namespace clang_format
