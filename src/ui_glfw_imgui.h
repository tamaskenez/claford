#pragma once

#include "ui.h"

#include <memory>

struct State;
std::unique_ptr<UI> make_ui_glfw_imgui(const State& ctx);
