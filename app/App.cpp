#include "App.h"

App::App()
    : GlfwContext(),
      event_bus_(),
      window_(1280, 720, "SPATIUM")
{
    init_gl();

    // Wire Window → EventBus (App is the only place that knows both)
    window_.set_resize_callback([&](int w, int h) {
        event_bus_.emit(WindowResizeEvent{w, h});
    });

    // Register subscribers — extend here as new systems are added
    event_bus_.subscribe<WindowResizeEvent>([](const WindowResizeEvent& /*e*/) {
        // glViewport is already handled inside Window::on_resize
        // Future: camera aspect-ratio update, UI layout reflow, etc.
    });
}

void App::run() {
    while (!window_.should_close()) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // TODO: render
        window_.swap_buffers();
    }
}

void App::init_gl() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(0, 0, window_.get_width(), window_.get_height());
}
