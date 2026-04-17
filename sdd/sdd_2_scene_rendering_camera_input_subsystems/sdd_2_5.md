# SDD: Stage 5 — App Integration

> **Version**: 1.0
> **Status**: `DRAFT`
> **Created**: 2026-04-17
> **Last Updated**: 2026-04-17
> **Author**: Gabriel

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Compose Stages 1–4 into a running App. Wire the EventBus, construct
         members in the correct order, implement the render loop, and plumb
         window-resize → camera-aspect updates. No new classes are introduced
         here — this stage is pure integration.
SCOPE  : app/App.h       (modify: declare members for Camera, CameraController,
                          MeshManager, Scene, Renderer)
         app/App.cpp     (modify: constructor composition + EventBus wiring +
                          render loop)
         CMakeLists.txt  (modify: final sweep — all Stage 1–5 .cpp files present
                          in CORE_SOURCES)
GOAL   : Build + run SPATIUM. With a user-authored shader that consumes at least
         uView + uProjection, a procedurally-created cube renders and responds
         to keyboard + mouse input. Window resize updates the camera aspect
         without stretching.
AVOID  : Defining new classes here. Defining new event types here. Modifying any
         frozen file (see CONSTRAINTS.md §1). Creating or editing shader files.
STATUS : Depends on completed SDD_01 + SDD_02 + SDD_03 + SDD_04.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

Stages 1–4 produce independently buildable, independently testable subsystems:

- **Stage 1** gave us input events + `Window` extensions.
- **Stage 2** gave us `Vertex`, `Mesh`, `MeshManager`, `Scene`.
- **Stage 3** gave us `Camera`, `CameraController`, `ProjectionMode`.
- **Stage 4** gave us `Renderer` with the uniform contract.

None of them *drive* the engine. `App` is the composition root — the single place where all subsystems meet, lifetimes are ordered, and the main loop lives. This stage is deliberately small in surface area: no new types, no new public APIs. But it is dense in integration correctness — member declaration order, destruction safety, event wiring, resize-propagation, and the render loop sequencing all have to be right.

Integration bugs here (a `Mesh` destroyed after `Window`, a lambda capturing a member that hasn't been constructed yet, a resize callback that fires before the camera is bound) are the failure modes this SDD is explicitly written to prevent.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| Global constraints | `./CONSTRAINTS.md` | Frozen APIs, global decisions G-D1 through G-D5 |
| Stage 1 SDD | `./SDD_01_InputEvents.md` | Event types + Window input extensions |
| Stage 2 SDD | `./SDD_02_SceneGraph.md` | `MeshManager::create_mesh` / `load_mesh` contract |
| Stage 3 SDD | `./SDD_03_Camera.md` | `Camera` + `CameraController` public API |
| Stage 4 SDD | `./SDD_04_Renderer.md` | `Renderer::draw` / `Renderer::clear` signatures |
| Existing `App` | `app/App.{h,cpp}` | Current composition: `GlfwContext` + `EventBus` + `Window`, resize wiring, `init_gl()` |
| Architecture overview | `docs/CODEMAPS/architecture.md` | System diagram, current initialization order |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] `App` composes `EventBus`, `Window`, `Camera`, `CameraController`, `MeshManager`, `Scene`, `Renderer` as private members in an order that satisfies the GL-context lifetime constraint (§3.5).
- [ ] `App::App()` wires four `EventBus` subscriptions forwarding input events to the `CameraController`.
- [ ] `App::App()` wires the window resize callback to (a) update the camera's aspect ratio and (b) emit `WindowResizeEvent` via the `EventBus`.
- [ ] `App::run()` implements the render loop: `glfwPollEvents` → `renderer_.clear()` → `renderer_.draw(camera_, scene_, shader)` → `window_.swap_buffers()`.
- [ ] At least one procedurally-created mesh (unit cube via `MeshManager::create_mesh`) is added to `scene_` in the constructor so the render loop has something to draw.
- [ ] Existing `App` behavior is preserved: `GlfwContext` base-class initialization, `Window` construction, `init_gl()` GL state setup.
- [ ] A user-provided shader (not authored by this SDD) that declares at least `uView` + `uProjection` produces a visible rendered cube.
- [ ] Build succeeds with no new warnings at `-Wall -Wextra -Wpedantic`.
- [ ] Clean exit: valgrind reports zero leaked GL handles; `Debug::enable()` reports no GL errors during a 60-second interactive session.

### 2.2 Input / Output Contract

This stage has no I/O boundary in the data-pipeline sense — it is pure composition. The relevant "contracts" are:

- **Compile-time contract**: `App`'s member types match the public signatures defined in SDD_01 through SDD_04.
- **Runtime contract**: The render loop in `App::run()` honors the sequence in G-D5 (frame uniforms uploaded once per frame, not per mesh) and the thread-model constraint in CONSTRAINTS.md §4 (all GL calls on the GL context thread).

Data flow is specified in §3.2.

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `App` declares private members in this exact order: `event_bus_`, `window_`, `camera_`, `cam_controller_`, `mesh_manager_`, `scene_`, `renderer_`. This order is constructor-order and reverse-destruction-order by C++ rule. | MUST |
| FR-02 | `App::App()` calls `event_bus_.subscribe<T>(...)` for all four input event types: `KeyEvent`, `MouseMoveEvent`, `MouseClickEvent`, `ScrollEvent`. Each lambda forwards to the corresponding `CameraController::on_*` method. | MUST |
| FR-03 | `App::App()` calls `cam_controller_.bind_camera(camera_)` before any EventBus subscription is wired. Subscribing first then binding would leave a window where an event could be processed against an unbound controller. | MUST |
| FR-04 | `App::App()` sets a resize callback on `window_` that (a) calls `camera_.set_aspect(window_.get_aspect_ratio())` and (b) emits `WindowResizeEvent{w, h}` on `event_bus_`. | MUST |
| FR-05 | `App::App()` adds at least one mesh to `scene_` via `mesh_manager_.create_mesh("cube", ...)` + `scene_.add_mesh(&mesh_manager_.get_by_name("cube"))`. This gives the render loop something to draw without requiring disk assets. | MUST |
| FR-06 | `App::run()` loops while `!window_.should_close()`, calling in order: `glfwPollEvents()`, `renderer_.clear()`, `renderer_.draw(camera_, scene_, shader)`, `window_.swap_buffers()`. | MUST |
| FR-07 | `App::run()` obtains a `GraphicShader` reference from somewhere (loaded in `init_gl()` or constructed as a local — see §3.3.2 decision). The shader is not authored by this SDD; the expectation is that the user has placed shader source files at paths the existing shader loader can resolve. | MUST |
| FR-08 | Existing `App` responsibilities are preserved: private inheritance from `GlfwContext`, the existing `init_gl()` call sequence (`Debug::enable()`, `glEnable(GL_DEPTH_TEST)`, `glEnable(GL_CULL_FACE)`, `glCullFace(GL_BACK)`, `glClearColor`, `glViewport`). | MUST |
| FR-09 | `App` remains non-copyable and non-movable (the existing constraint from `Window` member composition; also implicitly non-copyable via `EventBus`). | MUST |
| FR-10 | The unit-cube mesh added in FR-05 has counter-clockwise winding so back-face culling (enabled in `init_gl`) does not eliminate it. | MUST |
| FR-11 | `CMakeLists.txt` lists every `.cpp` file introduced in Stages 1–5 in `CORE_SOURCES`. | MUST |
| FR-12 | `App::run()` queries `glGetError()` after swap_buffers in debug builds and logs via `std::cerr` if non-zero. Release builds skip the check. | SHOULD |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | 60 FPS @ 1280×720 with a single unit-cube mesh on any dedicated GPU released in the last 10 years. Trivially achievable. |
| Memory | No per-frame heap allocations in `App::run()`. All resources allocated in `App::App()`. |
| Compatibility | See CONSTRAINTS.md §2. |
| Code Style | See CONSTRAINTS.md §3. |
| Thread Safety | See CONSTRAINTS.md §4. All composition and running on the main thread only. |
| Error Handling | Constructor failures (shader compile, mesh load, cube generation) throw `std::runtime_error`; `main()` catches and logs. Render loop does not throw. |

---

## 3. Design

### 3.1 High-Level Approach

**Chosen approach**: Straightforward composition root with member initialization doing the heavy lifting. All wiring (EventBus subscriptions, resize callback, scene population) happens in the constructor body *after* members are fully constructed. The render loop is a flat `while` — no state machines, no separate "update" phase.

**Rationale**:
- Member declaration order is the single most important decision. Get it right and destruction is automatically safe by the C++ rule of reverse-order destruction. Get it wrong and you get crashes in destructors that are hard to debug.
- Keeping wiring out of member initializers (using the body instead) sidesteps the problem of a lambda capturing `this` before all of `*this` is constructed. Member initializers run in declaration order; the body runs after all members exist.
- Flat render loop matches what `App::run()` already does today — preserves the cognitive overhead baseline.

**Rejected alternatives**:
- *Register subscriptions in member initializers*: Rejected — lambdas capturing `this` for not-yet-constructed later members would be undefined behavior. Explicit wiring in the constructor body is safer.
- *Split `App` into `App` + `AppInit` + `AppRunner`*: Rejected — over-engineered for a single-threaded viewer. `App` is already at the right level of granularity.
- *Move shader loading into `App::init_gl`*: Viable but not required. Recorded as D3 below.

### 3.2 Architecture / Data Flow

```
               App construction
               ────────────────
                      │
   ┌──────────────────┼──────────────────────────────────┐
   │   Phase 1: Member initialization (auto, by decl order)
   │                  │
   │   GlfwContext()  │   // base class — glfwInit()
   │        │         │
   │   EventBus()     │   // default
   │        │         │
   │   Window(1280,720,"SPATIUM",true)  // GLFW window + GL context + GLAD
   │        │         │
   │   Camera(default_eye, default_center, default_up,
   │          45.0f, window_.get_aspect_ratio(), 0.1f, 100.0f)
   │        │         │
   │   CameraController()   // default; camera bound in body
   │        │         │
   │   MeshManager()  │
   │        │         │
   │   Scene()        │   // empty; populated in body
   │        │         │
   │   Renderer()     │
   │                  │
   ├──────────────────┘
   │
   │   Phase 2: Constructor body wiring
   │   ──────────────────────────────
   │
   │   cam_controller_.bind_camera(camera_);    [FR-03, before subscriptions]
   │
   │   event_bus_.subscribe<KeyEvent>        ([this](auto& e){ cam_controller_.on_key(e); });
   │   event_bus_.subscribe<MouseMoveEvent>  ([this](auto& e){ cam_controller_.on_mouse_move(e); });
   │   event_bus_.subscribe<MouseClickEvent> ([this](auto& e){ cam_controller_.on_click(e); });
   │   event_bus_.subscribe<ScrollEvent>     ([this](auto& e){ cam_controller_.on_scroll(e); });
   │
   │   window_.set_key_callback        ([this](auto& e){ event_bus_.emit(e); });
   │   window_.set_mouse_move_callback ([this](auto& e){ event_bus_.emit(e); });
   │   window_.set_click_callback      ([this](auto& e){ event_bus_.emit(e); });
   │   window_.set_scroll_callback     ([this](auto& e){ event_bus_.emit(e); });
   │
   │   window_.set_resize_callback([this](int w, int h) {
   │       camera_.set_aspect(window_.get_aspect_ratio());
   │       event_bus_.emit(WindowResizeEvent{w, h});
   │   });
   │
   │   init_gl();  // existing — Debug::enable, glEnable, glClearColor, glViewport
   │
   │   // Populate scene with a unit cube (procedural).
   │   const auto [verts, idx] = MakeUnitCube_();
   │   mesh_manager_.create_mesh("cube", verts, idx);
   │   scene_.add_mesh(&mesh_manager_.get_by_name("cube"));
   │
   └──  App constructed, ready to run.

               App::run (per-frame)
               ────────────────────
   while (!window_.should_close()) {
       glfwPollEvents();            // GLFW callbacks fire → Window callbacks
                                    // → event_bus_.emit → CameraController::on_*
                                    // → camera_ mutated
       renderer_.clear();
       renderer_.draw(camera_, scene_, *shader_);
       window_.swap_buffers();
       #ifndef NDEBUG
           CheckGlError_();
       #endif
   }

               App destruction (automatic, reverse order)
               ──────────────────────────────────────────
   ~Renderer      (trivial)
   ~Scene         (trivial; non-owning pointers)
   ~MeshManager   (destroys all Mesh values → GL handles freed via ~VAO/VBO/EBO)
   ~CameraController (trivial)
   ~Camera        (trivial)
   ~Window        (glfwDestroyWindow → GL context destroyed)
   ~EventBus      (trivial)
   ~GlfwContext   (glfwTerminate)
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `App` (modified)

**File**: `app/App.h`, `app/App.cpp`

```cpp
// app/App.h
#pragma once
#include <memory>

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

    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    void run();   // Note: no longer `const`-qualified — see D2 below.

private:
    void init_gl();

    // ─── Member declaration order is CRITICAL. See §3.5. ───
    // Construction runs top-to-bottom; destruction bottom-to-top.
    // Any GL-owning member (Mesh via MeshManager) MUST be declared after
    // Window so it is destroyed BEFORE Window destroys the GL context.
    EventBus         event_bus_;
    Window           window_;
    Camera           camera_;
    CameraController cam_controller_;
    MeshManager      mesh_manager_;
    Scene            scene_;
    Renderer         renderer_;

    // Shader — held in a unique_ptr so it can be constructed/loaded in init_gl()
    // rather than in the member initializer list (which would require loading
    // shader files before OpenGL state is fully set up).
    std::unique_ptr<GraphicShader> shader_;
};
```

```cpp
// app/App.cpp (key excerpts; existing preserved content not shown)

namespace {

// Small helper: generate a unit cube with positions + normals and 36 indices.
// 24 vertices (4 per face × 6 faces) so each face has its own normal.
// CCW winding (see FR-10).
std::pair<std::vector<Vertex>, std::vector<GLuint>> MakeUnitCube();

}  // namespace

App::App()
    : GlfwContext(),
      event_bus_(),
      window_(1280, 720, "SPATIUM", /*vsync=*/true),
      camera_(/*eye*/    glm::vec3(0.0f, 0.0f, 3.0f),
              /*center*/ glm::vec3(0.0f, 0.0f, 0.0f),
              /*up*/     glm::vec3(0.0f, 1.0f, 0.0f),
              /*fov_deg*/45.0f,
              /*aspect*/ window_.get_aspect_ratio(),
              /*near*/   0.1f,
              /*far*/    100.0f),
      cam_controller_(),
      mesh_manager_(),
      scene_(),
      renderer_() {

    // ─── Phase 2: wiring (after all members exist) ───

    // Bind controller to camera BEFORE subscribing (FR-03).
    cam_controller_.bind_camera(camera_);

    // EventBus subscriptions — controller receives events.
    event_bus_.subscribe<KeyEvent>(
        [this](const KeyEvent& e) { cam_controller_.on_key(e); });
    event_bus_.subscribe<MouseMoveEvent>(
        [this](const MouseMoveEvent& e) { cam_controller_.on_mouse_move(e); });
    event_bus_.subscribe<MouseClickEvent>(
        [this](const MouseClickEvent& e) { cam_controller_.on_click(e); });
    event_bus_.subscribe<ScrollEvent>(
        [this](const ScrollEvent& e) { cam_controller_.on_scroll(e); });

    // Window callbacks → EventBus::emit.
    window_.set_key_callback(
        [this](const KeyEvent& e) { event_bus_.emit(e); });
    window_.set_mouse_move_callback(
        [this](const MouseMoveEvent& e) { event_bus_.emit(e); });
    window_.set_click_callback(
        [this](const MouseClickEvent& e) { event_bus_.emit(e); });
    window_.set_scroll_callback(
        [this](const ScrollEvent& e) { event_bus_.emit(e); });

    // Window resize — update camera aspect AND emit event (FR-04).
    window_.set_resize_callback([this](int w, int h) {
        camera_.set_aspect(window_.get_aspect_ratio());
        event_bus_.emit(WindowResizeEvent{w, h});
    });

    // GL state + debug.
    init_gl();

    // Shader — loaded AFTER init_gl so GL state is ready.
    ShaderInfo shaders[] = {
        { GL_VERTEX_SHADER,   "shader/vertex/basic.vsh" },
        { GL_FRAGMENT_SHADER, "shader/fragment/basic.fsh" },
        { GL_NONE, nullptr }
    };
    shader_ = std::make_unique<GraphicShader>();
    shader_->load(shaders);

    // Seed the scene with a unit cube so run() has something to draw (FR-05).
    const auto [verts, idx] = MakeUnitCube();
    mesh_manager_.create_mesh("cube", verts, idx);
    scene_.add_mesh(&mesh_manager_.get_by_name("cube"));
}

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
```

#### 3.3.2 Shader Ownership

A small integration decision: where does the `GraphicShader` object live? Three options:

1. As a local variable in `App::run()`. Rejected — must be loaded only once, not every call.
2. As an `App` member, constructed in the initializer list. Rejected — requires passing shader filenames through the constructor, which couples `App` to hardcoded paths at the wrong layer.
3. As an `App` member `std::unique_ptr<GraphicShader>`, default-null, populated in the constructor body after `init_gl()`. **Chosen**.

Option 3 lets shader loading happen at the right point in the initialization sequence (after GL state is set up) without forcing filenames into the member initializer list. The `unique_ptr` indirection is trivial — it avoids needing `GraphicShader` to be default-constructible if it isn't.

#### 3.3.3 `MakeUnitCube` — Procedural Geometry Helper

**File**: `app/App.cpp` (anonymous namespace; not exported)

```cpp
// Returns (24 vertices, 36 indices). Each of the 6 faces has 4 unique
// vertices with the face's normal — face-shared vertices are deliberately
// NOT merged so that normals stay face-accurate (a shared-vertex cube
// with averaged normals produces smooth-shading artifacts).
// Indices wind counter-clockwise when viewed from outside the cube (FR-10).
std::pair<std::vector<Vertex>, std::vector<GLuint>> MakeUnitCube() {
    std::vector<Vertex> v;
    std::vector<GLuint> i;
    v.reserve(24);
    i.reserve(36);

    // +X face, -X face, +Y, -Y, +Z, -Z.
    // Each face: 4 vertices (TL, TR, BR, BL when looking at the face
    // from outside), then 2 triangles: (0, 1, 2) and (0, 2, 3).
    // Implementation is ~60 lines of straight-line vertex/index emission.
    /* ... detailed face-by-face emission elided for SDD clarity ... */

    return {std::move(v), std::move(i)};
}
```

**Side-length**: 1.0 centered at origin. Default camera at `(0, 0, 3)` looking at origin ensures the cube fills a reasonable portion of the screen with a 45° FoV.

### 3.4 File Structure Changes

```
SPATIUM/
├── app/
│   ├── App.h                  ← MODIFY: add members Camera/CameraController/
│   │                                    MeshManager/Scene/Renderer/shader_
│   ├── App.cpp                ← MODIFY: constructor composition, wiring,
│   │                                    init_gl preserved, run() loop,
│   │                                    anonymous-namespace MakeUnitCube
│   └── GlfwContext.h          ← UNCHANGED (frozen)
├── core/                      ← UNCHANGED at this stage (all subsystems from
│                                 Stages 1–4 are consumed as-is)
├── main.cpp                   ← UNCHANGED (single `App app; app.run();` line
│                                 wrapped in try/catch for std::runtime_error)
└── CMakeLists.txt             ← MODIFY: final sweep — confirm every .cpp from
                                         Stages 1–5 is in CORE_SOURCES.
```

### 3.5 GL State Ownership & Initialization Order

**Member declaration order inside `App`** (FR-01, critical):

```
Base class subobject:
  1. GlfwContext                    glfwInit()

Members in declaration order:
  2. event_bus_                     trivial; no dependencies
  3. window_                        glfwCreateWindow + glfwMakeContextCurrent + GLAD load
                                    [must come BEFORE any Camera/Mesh/Renderer
                                     construction because they assume a live GL ctx]
  4. camera_                        depends on window_.get_aspect_ratio()
  5. cam_controller_                default-constructed; camera bound in body
  6. mesh_manager_                  trivial; meshes added in body (need GL ctx)
  7. scene_                         trivial; populated in body
  8. renderer_                      trivial

Then constructor body does wiring + init_gl + shader load + scene population.
```

**Destruction is reverse order** by the C++ rule, guaranteed by declaration order:

```
~Renderer         (trivial)
~Scene            (trivial — holds only non-owning pointers; G-D1)
~MeshManager      (destroys all Mesh instances → ~Mesh → ~VAO, ~VBO, ~EBO
                   → glDeleteVertexArrays / glDeleteBuffers)
                   ←── ★ GL handles destroyed here
~CameraController (trivial)
~Camera           (trivial)
~Window           (glfwDestroyWindow → GL context destroyed)
                   ←── ★ GL context destroyed here
~EventBus         (trivial)
~GlfwContext      (glfwTerminate)
```

**The critical constraint** is the ★ line: `MeshManager` (and thus all owned `Mesh`/VAO/VBO/EBO) must be destroyed **before** `Window`, because `Window`'s destructor destroys the GL context. Deleting a VAO after the context is gone is undefined behavior — the handle is invalid and `glDeleteVertexArrays` may crash or corrupt driver state.

Because `scene_` holds non-owning pointers (G-D1), its destruction is trivial and cannot cause this problem. But if someone "fixes" G-D1 by switching to `shared_ptr<Mesh>` and sharing lifetime via `Scene`, a `Scene` outliving `MeshManager` could keep a `Mesh` alive past context teardown — exactly the scenario G-D1 exists to prevent.

### 3.6 Event Plumbing — The Full Chain

For traceability, here is the complete path of a single `W` key press:

```
User presses W
    ↓
GLFW callback fires (registered in Window::Window)
    ↓
Window::glfw_key_callback (static, retrieves Window* via glfwGetWindowUserPointer)
    ↓
Window::on_key(glfw_key=GLFW_KEY_W, action=GLFW_PRESS)
    ↓
TranslateGlfwKey(GLFW_KEY_W) → Key::kW
    ↓
key_callback_(KeyEvent{Key::kW, KeyAction::kPress})   [user-provided]
    ↓
App's lambda: event_bus_.emit(KeyEvent{...})
    ↓
EventBus dispatches to all subscribers of KeyEvent
    ↓
App's subscription lambda: cam_controller_.on_key(e)
    ↓
CameraController::on_key checks key + action, calls camera_->move_forward(translate_speed_)
    ↓
Camera marks view_dirty_ = true
    ↓
Next Renderer::draw → upload_frame_uniforms_ → camera_.get_view() → recompute_view_() → fresh matrix uploaded
```

Eight hops. Worth it for the decoupling — `Camera` knows nothing about keys, `Window` knows nothing about cameras, `CameraController` knows nothing about `EventBus`.

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Add members to `App.h` in the declaration order from §3.5 — `app/App.h`
    - Expected result: Compiles once all Stage 1–4 headers exist.
    - Completion signal: `#include` verification: App.h pulls in exactly the headers it needs.

- [ ] **Step 2**: Write the member initializer list in `App.cpp` constructor — `app/App.cpp`
    - Expected result: All members constructed with correct initial values; compiles.
    - Completion signal: Compile succeeds.

- [ ] **Step 3**: Write the constructor body — bind camera, EventBus subscriptions, Window input callbacks, resize callback, `init_gl()`, shader load.
    - Expected result: All wiring in place; binding precedes subscriptions (FR-03).
    - Completion signal: Constructor body compiles; lambda captures resolve.

- [ ] **Step 4**: Implement `MakeUnitCube` helper in anonymous namespace — `app/App.cpp`
    - Expected result: 24 vertices + 36 indices, CCW winding, correct per-face normals.
    - Completion signal: Visually render a rotating cube and confirm all 6 faces visible (no culled faces = correct winding).

- [ ] **Step 5**: Wire scene population in constructor body (after shader load).
    - Expected result: `scene_.size() == 1` after constructor returns.
    - Completion signal: Debug assert `scene_.size() > 0` before entering run().

- [ ] **Step 6**: Rewrite `App::run()` body — render loop calling poll, clear, draw, swap; debug-only `glGetError` probe.
    - Expected result: Loop runs until window close requested.
    - Completion signal: Manual — launch app, ESC or close button exits cleanly.

- [ ] **Step 7**: Update `CMakeLists.txt` — confirm every new `.cpp` from Stages 1–5 is in `CORE_SOURCES`.
    - Command: `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build`
    - Expected output: Compile succeeds, all new sources built.

- [ ] **Step 8**: End-to-end test
    - Command: `./build/SPATIUM`
    - Expected output: Window opens, cube visible, WASD moves camera, mouse drag rotates, window resize rescales correctly.

### 4.2 Configuration / Constants

```cpp
// app/App.cpp — grouped at top of anonymous namespace
namespace {
    constexpr int          kWindowWidth   = 1280;
    constexpr int          kWindowHeight  = 720;
    constexpr char         kWindowTitle[] = "SPATIUM";
    constexpr bool         kVsync         = true;

    constexpr glm::vec3    kDefaultEye    = {0.0f, 0.0f, 3.0f};
    constexpr glm::vec3    kDefaultCenter = {0.0f, 0.0f, 0.0f};
    constexpr glm::vec3    kDefaultUp     = {0.0f, 1.0f, 0.0f};
    constexpr float        kDefaultFovDeg = 45.0f;
    constexpr float        kDefaultNear   = 0.1f;
    constexpr float        kDefaultFar    = 100.0f;

    constexpr const char*  kVertexShaderPath   = "shader/vertex/basic.vsh";
    constexpr const char*  kFragmentShaderPath = "shader/fragment/basic.fsh";
}
```

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | Construction succeeds | default constructor | `App` fully constructed, no exceptions | No throw, scene has 1 mesh |
| T-02 | Destruction is GL-safe | `App` goes out of scope | No GL errors, no leaked handles | valgrind reports no leaks; `glGetError` returns `GL_NO_ERROR` during dtor sequence (tested via instrumented build) |
| T-03 | Member declaration order | static assert on member layout | `offsetof` shows event_bus_ < window_ < ... < renderer_ | Compile-time check via `static_assert(offsetof(App, event_bus_) < offsetof(App, window_))` (sanity — not strictly required but cheap) |
| T-04 | W key moves camera forward | Press W | `camera_.position()` changes in the forward direction | Integration test: observe position before and after GLFW event injection |
| T-05 | Window resize updates camera aspect | Resize window 1280×720 → 1920×1080 | `camera_.get_projection()[0][0]` changes consistently with new aspect | Query projection matrix after resize callback fires |
| T-06 | Render loop terminates on close | `glfwSetWindowShouldClose(window, true)` from callback | `run()` returns | Manual — close window, app exits with code 0 |
| T-07 | Unit cube renders visibly | Run with user-authored basic shader | Cube appears, all 6 faces visible | Manual visual test — if any face missing, CCW winding is wrong |
| T-08 | Minimal shader works | Shader declares only `uView` + `uProjection` | No GL errors; cube still renders (flat silhouette) | `glGetError` clean after first frame (validates G-D5 silent-skip) |
| T-09 | Construction throws on shader-load failure | Point `kVertexShaderPath` at a non-existent file | `std::runtime_error` propagates out of constructor | `main()`'s try/catch observes the exception |
| T-10 | Destruction order when exception thrown mid-construction | Force shader load to throw | Already-constructed members destroyed in reverse order; no GL leak | valgrind clean even on error path |

### 5.2 Validation Commands

```bash
# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Run
./build/SPATIUM

# Leak check (exit after ~5 seconds for a clean trace)
valgrind --leak-check=full --show-leak-kinds=all ./build/SPATIUM

# Static analysis (optional, if clang-tidy configured)
clang-tidy app/App.cpp -- -std=c++17
```

### 5.3 Acceptance Criteria

- [ ] All test cases T-01 through T-08 pass (T-09 and T-10 are defensive / optional).
- [ ] All MUST-priority FRs (FR-01 through FR-11) verified.
- [ ] No modifications to any file in CONSTRAINTS.md §1's Frozen list.
- [ ] No shader files created or modified (shader authoring is the user's domain).
- [ ] `Debug::enable()` reports no GL errors during a 60-second interactive session.
- [ ] Clean build at `-Wall -Wextra -Wpedantic` — zero warnings introduced by this stage.
- [ ] §9 verification checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

All global frozen-file constraints from `CONSTRAINTS.md §1` apply to this stage. In addition:

- ❌ **Do NOT define any new classes in this stage.** Every class used has been specified in Stages 1–4. If you find yourself writing `class Foo { ... };` in `App.h` or `App.cpp` (outside the anonymous namespace for local helpers), stop.
- ❌ **Do NOT create or modify shader files** (`.vsh`, `.fsh`). Shader authoring is explicitly out of scope per G-D5 / CONSTRAINTS.md §1.
- ❌ **Do NOT reorder the `App` members from FR-01.** The order is load-bearing for destruction safety (§3.5).
- ❌ **Do NOT wire EventBus subscriptions in the member initializer list.** Use the constructor body — members must exist before `this`-capturing lambdas can reference them safely.
- ❌ **Do NOT add per-frame heap allocations in `run()`.** The scene is built once in the constructor.
- ❌ **Do NOT add `std::thread` or async anywhere in this stage.** CONSTRAINTS.md §4.
- ❌ **Do NOT let exceptions escape `run()`.** The render loop must be exception-free at all call sites. Constructor exceptions are fine (they propagate to `main`).

### 6.2 Known Pitfalls & Limitations

- ⚠️ **Lambda capture lifetime**: `[this]` lambdas in subscriptions remain valid as long as `App` does. If someone later splits the EventBus lifetime from `App`, these lambdas become dangling. Fine for this stage; flagged for future refactors.
- ⚠️ **Resize callback fires during `glfwPollEvents`, not immediately**: if you resize the window, the resize callback runs inside `poll_events`, which means `camera_.set_aspect` runs mid-frame but before `renderer_.draw`. That's actually the ideal ordering — the first draw after resize uses the updated aspect.
- ⚠️ **Shader hot-reload is NOT supported**: `shader_` is constructed once. Restart the app to pick up shader edits. Flagged for a future SDD.
- ⚠️ **Procedural cube's normal accuracy depends on NOT merging shared vertices**: a naive cube with 8 vertices + 12 triangles averages normals at the corners, giving you weird smooth shading on what should be a hard-edged cube. `MakeUnitCube` uses 24 vertices (4 per face) deliberately.
- ⚠️ **Culling + winding interaction**: `init_gl` enables back-face culling with `glCullFace(GL_BACK)`. The CCW-front-face assumption is OpenGL's default but can be changed elsewhere — if any earlier code calls `glFrontFace(GL_CW)`, the cube will be invisible. Check this first if the cube doesn't render.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| Hardcoded default camera pose | `(0, 0, 3)` looking at origin. No way to reset or reposition without recompiling. | Low | Add a `reset_camera()` method on `App` or bind to a key in `CameraController`. |
| Single scene, hardcoded content | Only one cube. No way to load other meshes without recompiling. | Medium | Add a command-line arg or a config file in a follow-up SDD. |
| Shader paths hardcoded | `"shader/vertex/basic.vsh"` is baked in. Breaks if working directory changes. | Low | Pass paths via command-line arg or resolve relative to executable path. |
| No multi-scene support | `App` owns one `Scene`. Switching scenes requires destruction + reconstruction. | Low | Add `scene_slot_` vector and scene-switching API in a follow-up SDD. |
| No ImGui / UI layer | No way to tweak camera speed, light params, etc. at runtime. | Medium | Dedicated ImGui-integration SDD later. |

---

## 7. Open Questions & Decision Log

### Open Questions

| # | Question | Owner | Deadline |
|---|----------|-------|----------|
| Q1 | Should `main()` own the `try`/`catch` or should `App` have a `run_safe()` that catches internally? | Author | Before Step 6 |

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | Member declaration order is `EventBus → Window → Camera → CameraController → MeshManager → Scene → Renderer`. | Satisfies construction-order dependencies (Window needed for Camera's aspect; nothing else has ordering constraints) AND reverse-order destruction safety (MeshManager destroyed before Window, so GL handles cleaned up before context). | 2026-04-17 |
| D2 | `App::run()` is NOT `const`-qualified. | The render loop mutates `camera_` (indirectly via event dispatch → CameraController), which is a non-const member method. Marking `run()` const would force a cascade of `mutable` / `const_cast` workarounds for no semantic benefit. | 2026-04-17 |
| D3 | Shader held as `std::unique_ptr<GraphicShader>` member, loaded in constructor body after `init_gl()`. | See §3.3.2. Keeps shader filenames out of member initializer list; defers shader load until GL state is ready. | 2026-04-17 |
| D4 | EventBus wiring lives in `App::App()` constructor body, not member initializers. | Lambdas capture `this`; all members must exist before `this`-capturing lambdas run. Member initializers run in declaration order — a lambda capturing a later-declared member would be UB. Constructor body runs after all members exist, so it is the safe place for wiring. | 2026-04-17 |
| D5 | `cam_controller_.bind_camera(camera_)` runs BEFORE any EventBus subscription. | Defense in depth — subscriptions shouldn't fire before run() is called (poll_events isn't happening yet), but binding-before-subscribing removes the window entirely. Cost: one line ordering. | 2026-04-17 |
| D6 | Procedural unit cube lives in `app/App.cpp` anonymous namespace, not in a general-purpose utility. | Keeps the scope of this SDD focused. If a second procedural primitive is needed later (sphere, plane, grid), promote to a `Primitives` utility in a follow-up SDD. | 2026-04-17 |
| D7 | Hardcoded shader paths are acceptable for v1. | Making them configurable is a UX/config concern orthogonal to rendering correctness. Revisit when the viewer gets a proper config system. | 2026-04-17 |

---

## 8. Progress Tracker

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | 2026-04-17 | SDD drafted | Implementation pending Stages 1–4 completion |

### Current Blockers

- 🔴 This stage cannot start until SDD_01 through SDD_04 are all `DONE`.
- 🟡 User must author at least a minimal `.vsh` / `.fsh` pair consuming `uView` + `uProjection` for visual validation (T-07). Hints: vertex shader multiplies `uProjection * uView * vec4(position, 1.0)`; fragment shader outputs a fixed color like `vec4(0.6, 0.7, 0.9, 1.0)`.

---

## 9. Implementation Verification Checklist

### `App.h`
- [ ] Member declaration order exactly matches §3.5 (FR-01).
- [ ] `App` is non-copyable and non-movable (explicitly `= delete`).
- [ ] Private inheritance from `GlfwContext` preserved.
- [ ] `run()` is **not** `const`-qualified (D2).
- [ ] `shader_` is `std::unique_ptr<GraphicShader>`, not a by-value `GraphicShader` member.
- [ ] Every `#include` in `App.h` is actually used.

### `App::App()` constructor body
- [ ] `cam_controller_.bind_camera(camera_)` is the FIRST wiring call (FR-03, D5).
- [ ] All four input-event subscriptions are registered before `window_.set_key_callback` / etc. are wired. (Ordering here doesn't matter for correctness — but keeping subscriptions-first, emissions-second is a readable convention.)
- [ ] Resize callback updates `camera_.set_aspect(window_.get_aspect_ratio())` AND emits `WindowResizeEvent` (FR-04).
- [ ] `init_gl()` called before shader load (shader load needs GL state ready).
- [ ] Shader load happens after `init_gl()`.
- [ ] Scene populated AFTER shader load (cube creation needs GL context, which has been live since Window construction — order is safe but keeping scene creation last keeps the constructor linearly readable).
- [ ] No `this`-capturing lambda references any member not yet constructed.

### `App::run()`
- [ ] Loop condition is `!window_.should_close()`.
- [ ] Sequence inside loop: `glfwPollEvents` → `renderer_.clear` → `renderer_.draw` → `window_.swap_buffers`.
- [ ] `#ifndef NDEBUG` guards the `glGetError` probe.
- [ ] No heap allocations in the loop body.
- [ ] No exceptions thrown from the loop (defensive — `Renderer::draw` already does not throw).

### `MakeUnitCube`
- [ ] 24 vertices (not 8 — distinct normals per face require face-local duplication).
- [ ] 36 indices (6 faces × 2 triangles × 3 vertices).
- [ ] CCW winding when viewed from outside the cube (FR-10).
- [ ] Per-face normals are axis-aligned unit vectors.
- [ ] Positions are in [-0.5, +0.5] on each axis (unit cube centered at origin).

### `CMakeLists.txt`
- [ ] Every `.cpp` introduced across Stages 1–5 is in `CORE_SOURCES`:
    - Stage 1: `core/window/Window.cpp` (pre-existing, may need re-check if unchanged)
    - Stage 2: `core/scene/Mesh.cpp`, `core/scene/MeshManager.cpp`, `core/scene/Scene.cpp`
    - Stage 3: `core/camera/Camera.cpp`, `core/camera/CameraController.cpp`
    - Stage 4: `core/render/Renderer.cpp`
    - Stage 5: `app/App.cpp` (modifying, not adding)
- [ ] Single build at `-Wall -Wextra -Wpedantic` produces no warnings.
- [ ] `dependencies.md` updated (if not already) to list `tinyobjloader` and `tinyply` per CONSTRAINTS.md §5.

### Integration
- [ ] Launch app with a valid shader → cube visible, all 6 faces rendered.
- [ ] WASD moves camera; visible change in view.
- [ ] Mouse drag rotates camera; no horizon tilt after 10+ mixed yaw/pitch operations (validates SDD_03's quaternion design end-to-end).
- [ ] Window resize → cube is not stretched; aspect ratio preserved.
- [ ] Close window → app exits with code 0, no GL errors logged.
- [ ] Shader file missing → `std::runtime_error` propagates cleanly, app exits with non-zero code and a sensible error message.

---

## 10. Appendix

### 10.1 Related SDDs

- [CONSTRAINTS.md](./CONSTRAINTS.md) — global constraints and decisions G-D1 through G-D5
- [SDD_01_InputEvents.md](./SDD_01_InputEvents.md) — event types and Window extensions (dependency)
- [SDD_02_SceneGraph.md](./SDD_02_SceneGraph.md) — `MeshManager`, `Mesh`, `Scene` (dependency)
- [SDD_03_Camera.md](./SDD_03_Camera.md) — `Camera`, `CameraController` (dependency)
- [SDD_04_Renderer.md](./SDD_04_Renderer.md) — `Renderer` with uniform contract (dependency)

### 10.2 Scratch Pad / Notes

- Future: when per-mesh transforms exist, `App` will need a way to set a `Transform` on the cube (e.g. to rotate it for visual effect). The cleanest place for this to live is a small update step in `run()` — `scene_.get_mesh("cube").transform().rotate(...)` or similar. Out of scope for this SDD.
- Future: `main()` could take command-line args for window size, mesh file to load, etc. For now, hardcoded — `main.cpp` stays one line: `App app; app.run();` inside a `try`/`catch(std::runtime_error&)`.
- Future: graceful shutdown on SIGINT — currently the window must be closed via its close button or `glfwSetWindowShouldClose`. A signal handler that flips the close flag would be polite but is not required.

---

*End of SDD*

---

## Outcome & Decisions

### Final Technical Choices

- Member declaration order implemented exactly as specified in FR-01: `event_bus_` → `window_` → `camera_` → `cam_controller_` → `mesh_manager_` → `scene_` → `renderer_` → `shader_`. Destruction is guaranteed to be safe reverse-order by the C++ rule.
- `cam_controller_.bind_camera(camera_)` is the very first wiring call in the constructor body, before any EventBus subscription.
- `shader_` held as `std::unique_ptr<GraphicShader>`, null-initialized in the member list, populated in the constructor body after `init_gl()`.
- `MakeUnitCube()` in anonymous namespace: 24 vertices (4 per face), 36 indices, CCW winding verified by cross product for all 6 faces, positions in `[-0.5, +0.5]`, per-face axis-aligned normals.
- `main.cpp` updated with `try/catch(std::runtime_error&)` for clean error reporting.
- `init_gl()` body preserved unchanged from existing implementation.
- No shader files created or modified — user must update `shader/vertex/basic.vsh` to consume `uView`/`uProjection` for camera-relative rendering.

### Rejected Alternatives

- Wiring in member initializers: rejected (D4) — lambdas capturing `this` are only safe after all members exist.
- `GraphicShader` as by-value member in initializer list: rejected (D3) — shader files must not be opened before `init_gl()` sets up GL state.

### Build Note

`App.h` required `#include <glad/glad.h>` as its first include. `App.h` transitively pulls in both `GlfwContext.h` (→ GLFW → system OpenGL headers) and `MeshManager.h` (→ glad). GLAD must load before GLFW headers to avoid the `#error OpenGL header already included` guard in `glad.h`. Adding it explicitly to `App.h` guarantees correct order for all translation units that include `App.h` (including `main.cpp`).

### Notes for Future Work

- User must update `shader/vertex/basic.vsh` to apply `uProjection * uView * vec4(aPosition, 1.0)` — until then the renderer silently skips the missing uniforms (ADR-016) and the cube renders un-transformed.
- Per-mesh transforms will go into `upload_mesh_uniforms_` in `Renderer.cpp` (noted in SDD_04 Outcome).
- Shader paths (`shader/vertex/basic.vsh`, `shader/fragment/basic.fsh`) are hardcoded; a config-file SDD can lift this constraint later.