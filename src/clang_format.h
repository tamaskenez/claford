#pragma once

#include <filesystem>
#include <tl/expected.hpp>

class ClangFormat {
   public:
    static tl::expected<std::unique_ptr<ClangFormat>, std::string> make();
	
    virtual ~ClangFormat() = default;
    
	virtual bool is_file_formatted(const std::filesystem::path& f) = 0;
    virtual bool format_file_in_place(const std::filesystem::path& f) = 0;
};
