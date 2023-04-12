#include "async_clang_format.h"

#include "state.h"
#include "util.h"

#include <fmt/format.h>

void AsyncClangFormat(std::unique_ptr<ClangFormat> clang_format,
                      ToAsyncClangFormatQueue* input_queue,
                      ToAppQueue* app_queue,
                      std::atomic<bool>* exit_flag) {
    ACFMsg msg;
    for (;;) {
        constexpr int64_t k_one_second_in_usec = 1000000;
        bool got_msg = input_queue->wait_dequeue_timed(msg, k_one_second_in_usec);
        if (*exit_flag) {
            break;
        }
        if (!got_msg) {
            continue;
        }
        switch (msg.command) {
            case ACFMsg::Command::CheckFormat: {
                bool result = clang_format->is_file_formatted(msg.path);
                app_queue->enqueue(msg::AsyncClangFormatResult{
                    .completion = [path = std::move(msg.path),
                                   completion = std::move(msg.completion),
                                   result]() {
                        completion(path, result);
                    }});
            } break;
            case ACFMsg::Command::Format: {
                bool result = clang_format->format_file_in_place(msg.path);
                app_queue->enqueue(msg::AsyncClangFormatResult{
                    .completion = [path = std::move(msg.path),
                                   completion = std::move(msg.completion),
                                   result]() {
                        completion(path, result);
                    }});
            } break;
        }
    }
}
