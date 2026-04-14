#pragma once

#include "GlfwContext.h"
#include "../core/event/EventBus.h"
#include "core/window/Window.h"
#include "../core/event/events.h"

class App : private GlfwContext {
public:
    App();
    void run();

private:
    EventBus event_bus_;  // must be declared before window
    Window   window_;

    void init_gl();
};
