# Project Constraints & Architecture Decisions

## Project Overview

## Architecture Decisions

- [ADR-001] `App` privately inherits `GlfwContext` → guarantees GLFW init before `Window` member construction
- [ADR-002] GLAD initialized inside `Window` constructor → `on_resize` calls `glViewport`, which requires loaded GL functions; the two are inseparable
- [ADR-003] `Window` move `= delete` → `resize_callback` lambda captures `this`; reassigning `glfwSetWindowUserPointer` alone does not fix dangling capture
- [ADR-004] `EventID` uses `template static int` variable, not `typeid`/`std::type_index` → RTTI is avoided
- [ADR-005] `Window` emits input via `std::function` callbacks, not directly into `EventBus` → wiring is `App`'s sole responsibility (GD-02); keeps `Window` testable without a bus
- [ADR-006] First mouse-move callback suppressed via `has_last_mouse_` flag → prevents camera snap-to-cursor (delta of hundreds of pixels) at startup
- [ADR-007] `VAO::VAO()` calls `glBindVertexArray` in constructor → `EBO` bind in its constructor is automatically recorded in the active VAO; `create_mesh` relies on this ordering
- [ADR-008] `MeshManager::create_mesh` is the sole `glVertexAttribPointer` site for the `Vertex` layout → position(loc=0) / normal(loc=1) expressed exactly once; `load_mesh` is implemented in terms of `create_mesh`
- [ADR-009] `MeshManager` declared before `Scene` in `App` members → destruction in reverse order ensures `Scene` (non-owning `Mesh*`) is destroyed before `MeshManager` (owning); enforces GD-01
- [ADR-010] tinyply used via `TINYPLY_IMPLEMENTATION` in `MeshManager.cpp` → single-header mode; `external/tinyply/tinyply.cpp` exists but is NOT in `CORE_SOURCES`
- [ADR-011] `glm::quatLookAt` lives in `<glm/gtc/quaternion.hpp>` (non-experimental) → no `GLM_ENABLE_EXPERIMENTAL` required; `glm::angleAxis` and `glm::conjugate` are available from the same header
- [ADR-012] `static constexpr glm::vec3 kWorldUp` compiles cleanly in GLM 1.0.3 + C++17 → used as the world-up axis for yaw rotations in `Camera`
- [ADR-013] `CameraController` holds a raw `Camera*` (not a reference) → allows default construction and late binding via `bind_camera()`; no EventBus dependency (GD-02); App wires subscriptions
- [ADR-014] `GraphicShader::use()` is already `const` in the frozen API → `Renderer::upload_frame_uniforms_` calls `shader.use()` directly on the const reference; the SDD's proposed `const_cast` is unnecessary
- [ADR-015] `Renderer` is fully stateless (no member variables) → `upload_mesh_uniforms_` uses `static const` identity mat4/mat3 at function scope to avoid per-call construction; initialized once on first call
- [ADR-016] `Renderer::draw(Camera, Scene, shader)` uploads frame uniforms exactly once before the mesh loop → per-mesh re-upload would waste 3 `glUniform*` calls per mesh at no benefit
- [ADR-017] `App.h` includes `<glad/glad.h>` as its first include → `App.h` transitively pulls in both GLFW (via `GlfwContext.h`) and glad (via `MeshManager.h`); GLAD must precede GLFW to avoid `#error OpenGL header already included`
- [ADR-018] EventBus wiring + Window callbacks + scene population all happen in `App::App()` constructor body, not member initializers → `[this]`-capturing lambdas are only safe after all members are fully constructed
- [ADR-019] `cam_controller_.bind_camera(camera_)` is called before any EventBus subscription → eliminates a window where an event could be dispatched against an unbound controller
- [ADR-020] `MakeUnitCube()` uses 24 vertices (4 per face) not 8 → shared-vertex cube averages normals at corners, producing smooth-shading artifacts on a hard-edged object; face-local vertices avoid this

## Hard Constraints

## Environment
