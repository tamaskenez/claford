#include "async_clang_format.h"
#include "state.h"

void AsyncClangFormat(std::unique_ptr<ClangFormat> clang_format,
                      ToAsyncClangFormatQueue* input_queue,
                      ToAppQueue* app_queue) {
    ACFMsg msg;
    for (;;) {
        input_queue->wait_dequeue(msg);
        switch (msg.command) {
            case ACFMsg::Command::Exit:
                return;
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
