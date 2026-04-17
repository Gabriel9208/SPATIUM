#include <glad/glad.h>

#include "app/App.h"

#include <iostream>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "core/gl/debug/Debug.h"
#include "core/scene/Vertex.h"

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers
// ---------------------------------------------------------------------------

namespace {

constexpr int         kWindowWidth   = 1280;
constexpr int         kWindowHeight  = 720;
constexpr char        kWindowTitle[] = "SPATIUM";
constexpr bool        kVsync         = true;

constexpr glm::vec3   kDefaultEye    = {0.0f, 0.0f, 3.0f};
constexpr glm::vec3   kDefaultCenter = {0.0f, 0.0f, 0.0f};
constexpr glm::vec3   kDefaultUp     = {0.0f, 1.0f, 0.0f};
constexpr float       kDefaultFovDeg = 45.0f;
constexpr float       kDefaultNear   = 0.1f;
constexpr float       kDefaultFar    = 100.0f;

constexpr const char* kVertexShaderPath   = "../shader/vertex/basic.vsh";
constexpr const char* kFragmentShaderPath = "../shader/fragment/basic.fsh";

// Returns 24 vertices and 36 indices for a unit cube centered at the origin.
// Each face has 4 unique vertices with a face-accurate (axis-aligned) normal
// so shading is hard-edged.  All quads use CCW winding when viewed from
// outside the cube (FR-10).
//
// Index pattern per quad (base = face_index * 4):
//   triangle 0: base+0, base+1, base+2
//   triangle 1: base+0, base+2, base+3
std::pair<std::vector<Vertex>, std::vector<GLuint>> MakeUnitCube() {
    std::vector<Vertex>  verts;
    std::vector<GLuint>  idx;
    verts.reserve(24);
    idx.reserve(36);

    // Helper: push 4 face vertices + 2 triangles.
    auto push_face = [&](std::initializer_list<glm::vec3> positions,
                         glm::vec3 normal) {
        const GLuint base = static_cast<GLuint>(verts.size());
        for (const auto& p : positions)
            verts.push_back({p, normal});
        idx.insert(idx.end(), {
            base+0, base+1, base+2,
            base+0, base+2, base+3
        });
    };

    // +X face  (normal = +1,0,0)  — CCW looking from +X toward -X
    push_face({
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
    }, { 1.0f, 0.0f, 0.0f});

    // -X face  (normal = -1,0,0)  — CCW looking from -X toward +X
    push_face({
        {-0.5f, -0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
    }, {-1.0f, 0.0f, 0.0f});

    // +Y face  (normal = 0,+1,0)  — CCW looking from +Y toward -Y
    push_face({
        {-0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        { 0.5f,  0.5f, -0.5f},
    }, { 0.0f, 1.0f, 0.0f});

    // -Y face  (normal = 0,-1,0)  — CCW looking from -Y toward +Y
    push_face({
        {-0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f,  0.5f},
    }, { 0.0f,-1.0f, 0.0f});

    // +Z face  (normal = 0,0,+1)  — CCW looking from +Z toward -Z
    push_face({
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f,  0.5f},
    }, { 0.0f, 0.0f, 1.0f});

    // -Z face  (normal = 0,0,-1)  — CCW looking from -Z toward +Z
    push_face({
        {-0.5f, -0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
    }, { 0.0f, 0.0f,-1.0f});

    return {std::move(verts), std::move(idx)};
}

}  // namespace

// ---------------------------------------------------------------------------
// App constructor
// ---------------------------------------------------------------------------

App::App()
    : GlfwContext(),
      event_bus_(),
      window_(kWindowWidth, kWindowHeight, kWindowTitle, kVsync),
      camera_(kDefaultEye, kDefaultCenter, kDefaultUp,
              kDefaultFovDeg,
              window_.get_aspect_ratio(),
              kDefaultNear,
              kDefaultFar),
      cam_controller_(),
      mesh_manager_(),
      scene_(),
      renderer_()
{
    // ── Phase 2: wiring (all members exist at this point) ───────────────────

    // Bind camera BEFORE any EventBus subscription (FR-03, D5).
    cam_controller_.bind_camera(camera_);

    // EventBus subscriptions — controller receives all four input events.
    event_bus_.subscribe<KeyEvent>(
        [this](const KeyEvent& e) { cam_controller_.on_key(e); });
    event_bus_.subscribe<MouseMoveEvent>(
        [this](const MouseMoveEvent& e) { cam_controller_.on_mouse_move(e); });
    event_bus_.subscribe<MouseClickEvent>(
        [this](const MouseClickEvent& e) { cam_controller_.on_click(e); });
    event_bus_.subscribe<ScrollEvent>(
        [this](const ScrollEvent& e) { cam_controller_.on_scroll(e); });

    // Window callbacks → EventBus::emit (App owns both, so it wires them).
    window_.set_key_callback(
        [this](const KeyEvent& e) { event_bus_.emit(e); });
    window_.set_mouse_move_callback(
        [this](const MouseMoveEvent& e) { event_bus_.emit(e); });
    window_.set_click_callback(
        [this](const MouseClickEvent& e) { event_bus_.emit(e); });
    window_.set_scroll_callback(
        [this](const ScrollEvent& e) { event_bus_.emit(e); });

    // Resize: update camera aspect AND forward to EventBus (FR-04).
    window_.set_resize_callback([this](int w, int h) {
        camera_.set_aspect(window_.get_aspect_ratio());
        event_bus_.emit(WindowResizeEvent{w, h});
    });

    // GL state — must come before shader load.
    init_gl();

    // Shader — loaded after init_gl so GL state is ready (D3).
    ShaderInfo shaders[] = {
        { GL_VERTEX_SHADER,   kVertexShaderPath   },
        { GL_FRAGMENT_SHADER, kFragmentShaderPath  },
        { GL_NONE, nullptr }
    };
    shader_ = std::make_unique<GraphicShader>();
    if (shader_->load(shaders) == 0) {
        throw std::runtime_error("Failed to load shader program");
    }

    // Seed scene with a procedural unit cube (FR-05).
    auto [verts, idx] = MakeUnitCube();
    mesh_manager_.create_mesh("cube", verts, idx);
    scene_.add_mesh(&mesh_manager_.get_by_name("cube"));
}

// ---------------------------------------------------------------------------
// App::run — render loop
// ---------------------------------------------------------------------------

void App::run() {
    while (!window_.should_close()) {
        glfwPollEvents();
        renderer_.clear();
        renderer_.draw(camera_, scene_, *shader_);
        window_.swap_buffers();

#ifndef NDEBUG
        if (GLenum err = glGetError(); err != GL_NO_ERROR) {
            std::cerr << "[App::run] GL error: 0x"
                      << std::hex << err << std::dec << "\n";
        }
#endif
    }
}

// ---------------------------------------------------------------------------
// App::init_gl — one-time GL state (preserved from existing impl)
// ---------------------------------------------------------------------------

void App::init_gl() {
    Debug::enable();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(0, 0, window_.get_width(), window_.get_height());
}
