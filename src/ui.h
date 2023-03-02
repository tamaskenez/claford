#pragma once

#include <functional>

class UI {
   public:
    virtual void exec(std::function<void()> process_msgs_fn) = 0;
    virtual ~UI() = default;
};
