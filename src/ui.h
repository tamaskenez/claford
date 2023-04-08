#pragma once

#include <functional>

enum class ProcessMsgsResult { QueueWasEmpty, QueueWasNotEmpty, ShouldExit };

class UI {
   public:
    virtual void exec(std::function<ProcessMsgsResult()> process_msgs_fn) = 0;
    virtual ~UI() = default;
};
