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
    bp::filesystem::path path;

    explicit ClangFormatImpl(bp::filesystem::path path)
        : path(path) {}

    bool is_file_formatted(const fs::path& f) override {
        return bp::system(path,
                          "--dry-run",
                          "-Werror",
                          f.string(),
                          bp::std_out > bp::null,
                          bp::std_err > bp::null)
            == EXIT_SUCCESS;
    }
    bool format_file_in_place(const std::filesystem::path& f) override {
        return bp::system(path, "-i", f.string(), bp::std_out > bp::null, bp::std_err > bp::null)
            == EXIT_SUCCESS;
    }
};

std::vector<std::string> read_pipe_to_strings(bp::ipstream& pipe) {
    std::string line;
    std::vector<std::string> lines;
    while (pipe && std::getline(pipe, line) && !line.empty()) {
        line = trim(line);
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

void print_error_report(const std::string& msg,
                        const std::vector<std::string>& out,
                        const std::vector<std::string>& err) {
    fmt::print(stderr, "{}\n", msg);
    if (out.empty() && err.empty()) {
        fmt::print("Both stdout and stderr were empty.\n");
        return;
    }
    if (out.empty()) {
        fmt::print(stderr, "stdout was empty, stderr:\n");
    } else {
        if (err.empty()) {
            fmt::print(stderr, "stderr was empty, ");
        }
        fmt::print(stderr, "stdout:\n");
        for (auto& x : out) {
            fmt::print(stderr, "> {}\n", x);
        }
        if (!err.empty()) {
            fmt::print(stderr, "stderr:\n");
        }
    }
    if (!err.empty()) {
        for (auto& x : err) {
            fmt::print(stderr, "> {}\n", x);
        }
    }
}

std::optional<std::unique_ptr<ClangFormat>> ClangFormat::make() {
    auto clang_format_path = bp::search_path("clang-format");
    if (clang_format_path.native().empty()) {
        fmt::print(stderr, "clang-format not found on PATH, which is:\n");
        for (auto& p : ::boost::this_process::path()) {
            fmt::print(stderr, "- {}\n", ToUtf8(fs::path(p.native())));
        }
        return std::nullopt;
    }
    fmt::print("Found clang-format: {}\n", ToUtf8(fs::path(clang_format_path.native())));
    std::error_code ec;
    bp::ipstream pipe_err, pipe_out;
    int r = bp::system(
        clang_format_path, "--version", bp::std_out > pipe_out, bp::std_err > pipe_err, ec);

    auto out_lines = read_pipe_to_strings(pipe_out);
    auto err_lines = read_pipe_to_strings(pipe_err);

    if (ec) {
        auto msg = fmt::format("Failed to run `clang-format --version`, reason: {}", ec.message());
        if (ec.default_error_condition()
            == std::make_error_code(std::errc::executable_format_error).default_error_condition()) {
            msg = fmt::format("{}. Possible fix: add shebang to script.", msg);
        }
        print_error_report(msg, out_lines, err_lines);
        return std::nullopt;
    }

    if (r != EXIT_SUCCESS) {
        print_error_report(fmt::format("`clang-format --version` failed with exit code {}", r),
                           out_lines,
                           err_lines);
        return std::nullopt;
    }

    if (out_lines.empty()) {
        print_error_report("`clang-format --version` returned no output.", out_lines, err_lines);
        return std::nullopt;
    }
    if (out_lines.size() != 1) {
        print_error_report(
            "`clang-format --version` returned more than 1 lines.", out_lines, err_lines);
        return std::nullopt;
    }

    fmt::print("`clang-format --version: {}\n", out_lines[0]);
    return std::make_unique<ClangFormatImpl>(clang_format_path);
}
