#include "clang_format.h"

#include "util.h"

#include <fmt/format.h>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/process/filesystem.hpp>
#include <filesystem>
#include <nowide/cstdlib.hpp>

namespace bp = boost::process;
namespace fs = std::filesystem;

struct ClangFormatImpl : public ClangFormat {
    bp::filesystem::path _path;

    explicit ClangFormatImpl(bp::filesystem::path path)
        : _path(path) {}

    virtual std::filesystem::path path() override {
        return std::filesystem::path(_path.native());
    }
};

tl::expected<std::unique_ptr<ClangFormat>, std::string> ClangFormat::make() {
    auto clang_format_path = bp::search_path("clang-format");
    if (clang_format_path.native().empty()) {
        return tl::make_unexpected("clang-format not found on PATH");
    }
    fmt::print("Found clang-format: {}\n", ToUtf8(fs::path(clang_format_path.native())));
    std::error_code ec;
    bp::ipstream pipe_err, pipe_out;
    int r = bp::system(
        clang_format_path, "--version", bp::std_out > pipe_out, bp::std_err > pipe_err, ec);
    if (ec) {
        auto msg = fmt::format("Failed to run `clang-format --version`, reason: {}", ec.message());
        if (ec.default_error_condition()
            == std::make_error_code(std::errc::executable_format_error).default_error_condition()) {
            msg = fmt::format("{}. Possible fix: add shebang to script.", msg);
        }
        return tl::make_unexpected(msg);
    }

    std::string line;
    std::vector<std::string> lines;
    while (pipe_out && std::getline(pipe_out, line) && !line.empty()) {
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }

    if (r != EXIT_SUCCESS) {
        return tl::unexpected(fmt::format("`clang-format --version` failed with exit code {}", r));
    }

    if (lines.empty()) {
        return tl::make_unexpected("`clang-format --version` returned no output.");
    }
    if (lines.size() != 1) {
        return tl::make_unexpected("`clang-format --version` returned more than 1 lines.");
    }

    fmt::print("`clang-format --version: {}\n", lines[0]);
    return std::make_unique<ClangFormatImpl>(clang_format_path);
}

namespace clang_format {

bool IsFileFormatted(const std::filesystem::path& p) {
    /*
    return bp::system(
                                      s_clang_format_path,
                                      "--dry-run","-Werror",
                                      p,
                                      bp::std_out > bp::null
                                      bp::std_err > bp::null
    ) == EXIT_SUCCESS;
    */
    (void)p;
    return false;
}

bool FormatFileInPlace(const std::filesystem::path& p) {
    return nowide::system(fmt::format("clang-format -i \"{}\"", ToUtf8(p)).c_str()) == EXIT_SUCCESS;
}
}  // namespace clang_format
