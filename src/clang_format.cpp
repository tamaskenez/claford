#include "clang_format.h"

#include "util.h"

#include <fmt/format.h>
#include <nowide/cstdlib.hpp>

namespace clang_format {
bool IsFileFormatted(const std::filesystem::path& p) {
    return nowide::system(fmt::format("clang-format --dry-run -Werror \"{}\"", ToUtf8(p)).c_str())
        == EXIT_SUCCESS;
}
bool FormatFileInPlace(const std::filesystem::path& p) {
    return nowide::system(fmt::format("clang-format -i \"{}\"", ToUtf8(p)).c_str()) == EXIT_SUCCESS;
}
}  // namespace clang_format
