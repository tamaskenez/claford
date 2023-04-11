#pragma once

#include "clang_format.h"
#include "state.h"

void AsyncClangFormat(std::unique_ptr<ClangFormat> clang_format,
					  ToAsyncClangFormatQueue* input_queue,
	   ToAppQueue* app_queue);
