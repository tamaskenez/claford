#pragma once

#include <filesystem>
#include <optional>

class ClangFormat {
   public:
    static std::optional<std::unique_ptr<ClangFormat>> make();

    virtual ~ClangFormat() = default;

    virtual bool is_file_formatted(const std::filesystem::path& f) = 0;
    virtual bool format_file_in_place(const std::filesystem::path& f) = 0;
};
