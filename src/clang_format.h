#pragma once

#include <filesystem>
#include <tl/expected.hpp>

class ClangFormat {
   public:
    static tl::expected<std::unique_ptr<ClangFormat>, std::string> make();
    virtual ~ClangFormat() = default;
    virtual std::filesystem::path path() = 0;
};

namespace clang_format {
bool IsFileFormatted(const std::filesystem::path& p);
bool FormatFileInPlace(const std::filesystem::path& p);
}  // namespace clang_format
