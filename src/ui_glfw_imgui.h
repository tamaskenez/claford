#pragma once

#include "state.h"
#include "ui.h"

#include <memory>

std::unique_ptr<UI> make_ui_glfw_imgui(const State& ctx, ToAppQueue& to_app_queue);
