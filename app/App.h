#pragma once

#include <memory>

#include <glad/glad.h>      // must be first — before any header that pulls in GLFW/gl.h
#include "app/GlfwContext.h"
#include "core/event/EventBus.h"
#include "core/window/Window.h"
#include "core/camera/Camera.h"
#include "core/camera/CameraController.h"
#include "core/scene/MeshManager.h"
#include "core/scene/Scene.h"
#include "core/render/Renderer.h"
#include "core/gl/shader/GraphicShader.h"

class App : private GlfwContext {
public:
    App();
    ~App() = default;

    App(const App&)            = delete;
    App& operator=(const App&) = delete;
    App(App&&)                 = delete;
    App& operator=(App&&)      = delete;

    void run();

private:
    void init_gl();

    // ── Member declaration order is CRITICAL (FR-01, §3.5) ──────────────────
    // Construction: top-to-bottom.  Destruction: bottom-to-top (C++ rule).
    // MeshManager (GL-owning) MUST be declared after Window so it is
    // destroyed BEFORE Window destroys the GL context.
    EventBus         event_bus_;
    Window           window_;
    Camera           camera_;
    CameraController cam_controller_;
    MeshManager      mesh_manager_;
    Scene            scene_;
    Renderer         renderer_;

    // Shader loaded in constructor body after init_gl(); kept as unique_ptr
    // so it can be default-null and populated after GL state is ready (D3).
    // TODO: refactor
    std::unique_ptr<GraphicShader> shader_;
};
