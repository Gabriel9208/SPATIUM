# SDD: Scene, Rendering, Camera & Input Subsystems

> **Version**: 1.2
> **Status**: `DRAFT`
> **Created**: 2026-04-16
> **Last Updated**: 2026-04-17
> **Author**: Gabriel

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Implement the scene/render/camera/input layer on top of the existing
         App + Window + EventBus foundation, as specified by class.mmd.
SCOPE  : core/scene/{Mesh,MeshManager,Scene,Vertex}.{h,cpp}
         core/render/Renderer.{h,cpp}
         core/camera/{Camera,CameraController,ProjectionMode}.{h,cpp}
         core/event/events.h  (extend with input event payloads + enums)
         app/App.{h,cpp}       (compose new members; DO NOT rewrite existing logic)
GOAL   : Render a Scene containing one or more Meshes from a Camera's viewpoint,
         with keyboard/mouse navigation routed through EventBus.
AVOID  : Touching the already-implemented subsystems beyond additive composition:
         GlfwContext, Window, EventBus, EventID, Debug, IGPUResource, VAO, VBO,
         EBO, UBO, SSBO, FBO, StorageBuffer, ShaderProgram, GraphicShader,
         ComputeShader. Their public APIs are frozen for this task.
STATUS : Draft specification. No code written yet.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

The current SPATIUM codebase (per `backend.md`, `data.md`, `architecture.md`) establishes
the engine's plumbing: GLFW context, window, event bus, debug callbacks, and a complete GL
buffer/shader abstraction. The render loop in `App::run()` today only clears the
framebuffer — no scene is drawn, no camera exists, and no input beyond window resize is
handled.

This SDD specifies the next layer: the **scene graph**, **camera/projection**,
**render orchestration**, and **input event routing** that together turn the engine from
a window-with-a-clear-color into an interactive 3D viewer. All new components are
composed into `App`, which already privately inherits `GlfwContext` and owns `EventBus`
and `Window`.

- **Problem being solved**: The engine has no way to represent, navigate, or draw geometry.
- **Relation to other modules**:
    - Consumes `VAO`, `VBO<Vertex>`, `EBO` from `core/gl/obj/` (no changes to those).
    - Consumes `GraphicShader` from `core/gl/shader/` (no changes).
    - Consumes `EventBus` / `EventID` from `core/event/` (extends `events.h` only).
    - Composed into `App` via direct member aggregation.
- **Dependencies (upstream)**: GL buffer objects, GraphicShader, EventBus, Window, GLM.
- **Dependents (downstream)**: Future CUDA 3DGS integration will replace `Mesh`-based
  `Renderer::draw` paths with splat-based ones; `Scene` is designed to be extensible for
  that.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| Class diagram (source of truth) | `class.mmd` | Authoritative spec for shapes & relationships |
| Architecture overview | `architecture.md` | System diagram, init order, thread model |
| Backend classes (existing) | `backend.md` | **Frozen** APIs: Window, EventBus, Shaders, GL objects |
| Data / buffer layouts | `data.md` | VBO<T> template, VAO usage patterns, RAII rules |
| Dependencies | `dependencies.md` | CMake targets, GLM 1.0.3, OpenGL 4.5 core |
| Google C++ Style Guide | https://google.github.io/styleguide/cppguide.html | Trailing underscore for members, snake_case methods |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] `Mesh`, `MeshManager`, `Scene`, `Vertex` compile and cover the class diagram's public surface.
- [ ] `Camera` supports perspective ↔ orthographic switching and all move/turn operations.
- [ ] `CameraController` subscribes to `KeyEvent`, `MouseMoveEvent`, `MouseClickEvent`,
  `ScrollEvent` on the `EventBus` and mutates its bound `Camera` accordingly.
- [ ] `Renderer::draw(camera, scene, shader)` produces correct on-screen output for a
  trivial test mesh (e.g. a unit cube) with a shader that consumes `uModel`, `uView`,
  `uProjection`.
- [ ] `App` composes all new members in a well-defined order; the render loop calls
  `Renderer::clear()` → `Renderer::draw(camera_, scene_, shader)` → `window_.swap_buffers()`.
- [ ] `Window::get_aspect_ratio()` added (non-breaking addition).
- [ ] Input events are emitted from `Window` GLFW callbacks and flow through `EventBus`.
- [ ] The existing subsystems are **not modified** beyond additive extensions to
  `events.h`, `Window`, and `App`.

### 2.2 Input / Output Contract

This is a system-level module — the canonical "data flow" is described in §3.2. The only
pipeline-style contract worth calling out is `Renderer::draw`:

**Inputs** (per draw call):
- `Camera` — provides `view: glm::mat4`, `projection: glm::mat4` via getters.
- `const Scene&` or `const Mesh&` — the geometry to rasterize.
- `GraphicShader&` — must be pre-loaded; the renderer calls `.use()` and uploads uniforms.

**Outputs**:
- Draw commands issued into the currently bound framebuffer. No return value.

**Side Effects**:
- Mutates GL state (bound VAO, current program, uniform values).
- No heap allocation in the hot path.

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `Mesh` holds its `VAO`, `VBO<Vertex>`, and `EBO` as move-only value members. `Mesh` is itself non-copyable and move-only. GL handles are transferred into a `Mesh` via `std::move` from `MeshManager::create_mesh` (and, transitively, from `load_mesh`). At no point does any GL handle exist without a C++ owner, and no handle is owned by more than one object simultaneously. | MUST |
| FR-02 | `MeshManager` provides O(1) lookup by integer ID and O(1) average lookup by name string. | MUST |
| FR-03 | `Scene` holds non-owning `Mesh*` pointers; `Mesh` lifetime is owned by `MeshManager`. | MUST |
| FR-04 | `Camera` represents orientation internally as a unit `glm::quat` (not Euler angles) to eliminate gimbal lock. Rotations are applied by quaternion multiplication and the view matrix is derived from the quaternion and position. | MUST |
| FR-05 | `Camera` supports all six translation ops (left/right/up/down/forward/backward) and four rotation ops (yaw left/right, pitch up/down). Yaw uses the world up axis; pitch uses the camera's local right axis. Rotation parameters are in **radians**. | MUST |
| FR-06 | `ProjectionMode` switching recomputes the projection matrix preserving fov-equivalent framing (see §3.3.7 for the formula). | MUST |
| FR-07 | `CameraController` subscribes to the four input event types at construction-via-`bind_camera` time and updates the bound camera on each callback. | MUST |
| FR-08 | `Renderer::clear()` clears both color and depth buffers. | MUST |
| FR-09 | `Renderer::draw(camera, scene, shader)` uploads the documented uniform contract (§3.3.5), iterates meshes, and issues one `glDrawElements` per mesh. Frame-scope uniforms are uploaded once per draw; mesh-scope uniforms are uploaded once per mesh. | MUST |
| FR-10 | `Window::get_aspect_ratio()` returns `width_ / static_cast<float>(height_)` with divide-by-zero guard returning 1.0f. | MUST |
| FR-11 | `Window` emits `KeyEvent`, `MouseMoveEvent`, `MouseClickEvent`, `ScrollEvent` through its registered callbacks (wired by `App` to `EventBus::emit`). | MUST |
| FR-12 | `Camera` caches its view and projection matrices; cache is invalidated via dirty flags in mutators and recomputed lazily on getter access. | SHOULD |
| FR-13 | `MeshManager::load_mesh` supports `.obj` (Wavefront) and `.ply` (Stanford, ascii + binary little-endian). Format is dispatched by file extension. | SHOULD |
| FR-14 | `Scene` exposes a const iterator over its meshes for the renderer. | SHOULD |
| FR-15 | `Renderer` skips uniform uploads for names that the current shader does not declare (`glGetUniformLocation` returns `-1`). This lets user-authored shaders consume a subset of the uniform contract without error. | MUST |
| FR-16 | `Renderer` caches the last-used shader program ID to skip redundant `glUseProgram` calls. | NICE-TO-HAVE |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | 60 FPS @ 1280×720 with a single 100k-triangle mesh on an RTX 3000-class GPU. |
| Memory | No per-frame heap allocations in the render path. |
| Compatibility | C++17, OpenGL 4.5 core, GLM 1.0.3, GLFW 3.0+. |
| Code Style | Google C++ Style Guide: `snake_case` methods, `trailing_underscore_` members, `kUpperCamelCase` constants, `.h/.cpp` pairs, `#pragma once`. |
| Thread Safety | Single-threaded render path. No synchronization primitives. All GL calls on the context thread. |
| Error Handling | Fatal errors (file not found, shader compile failure) abort via `std::runtime_error`; recoverable conditions logged via `Debug`. |

---

## 3. Design

### 3.1 High-Level Approach

**Chosen approach**: Three loosely-coupled layers composed inside `App`:

1. **Resource layer** (`MeshManager` owns `Mesh` objects by integer ID).
2. **Scene layer** (`Scene` holds non-owning pointers to meshes; trivially replaceable).
3. **Presentation layer** (`Camera` + `Renderer` + `CameraController`).

Input flows through `EventBus` — `Window` emits input events, `CameraController`
subscribes. This keeps `Camera` unaware of input and `Window` unaware of cameras.

**Rationale**:
- Separating ownership (`MeshManager`) from composition (`Scene`) lets us render multiple
  scenes sharing the same mesh asset without duplication or reference counting.
- Routing input via `EventBus` matches the existing `WindowResizeEvent` pattern and
  preserves the Google-style "no globals, no singletons" stance.
- A thin `Renderer` class (instead of a free function) is chosen so later variants
  (deferred, 3DGS) can share the same `draw(camera, scene)` call site.

**Rejected alternatives**:
- *Scene-as-owner*: Rejected — forces `shared_ptr<Mesh>` for multi-scene sharing and
  complicates the CUDA interop story later.
- *Static singleton renderer*: Rejected — violates CONSTRAINTS-style "explicit
  composition, no hidden state" and makes unit testing harder.
- *Template-based event subscription without EventBus*: Rejected — the bus is already
  built and tested; duplicating the mechanism fragments the architecture.

### 3.2 Architecture / Data Flow

```
┌─────────────────────────────────────────────────────────────────────┐
│                              App                                     │
│                                                                      │
│  GlfwContext (base)                                                  │
│      │                                                               │
│      ▼                                                               │
│  EventBus ◄──────────────────── Window (emits input + resize)       │
│      ▲       subscribe                                               │
│      │                                                               │
│  CameraController ────mutates───► Camera                            │
│                                      ▲                               │
│                                      │ reads view/proj               │
│                                      │                               │
│  MeshManager ──owns──► Mesh ◄──refs── Scene                         │
│                          │                │                          │
│                          │ has            │                          │
│                          ▼                ▼                          │
│                    VAO/VBO/EBO       std::vector<Mesh*>             │
│                                                                      │
│  Renderer ──draw(camera, scene, shader)──► OpenGL                   │
└─────────────────────────────────────────────────────────────────────┘

Per-frame flow inside App::run():

    glfwPollEvents()
        │
        ├─► Window GLFW callbacks
        │       │
        │       ▼
        │   EventBus::emit(KeyEvent | MouseMoveEvent | ...)
        │       │
        │       ▼
        │   CameraController::on_*  → mutates Camera state
        │
        ▼
    renderer_.clear()
    renderer_.draw(camera_, scene_, shader_)
    window_.swap_buffers()
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `Vertex` (struct)

**File**: `core/scene/Vertex.h`

```cpp
#pragma once
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

static_assert(sizeof(Vertex) == 2 * sizeof(glm::vec3),
              "Vertex must be tightly packed for VBO<Vertex> upload.");
```

**Attribute layout** (consumed by `Mesh` constructor when configuring the VAO):

| Location | Attribute | Size | Offset |
|----------|-----------|------|--------|
| 0 | `position` | 3 × float | `offsetof(Vertex, position)` |
| 1 | `normal`   | 3 × float | `offsetof(Vertex, normal)` |

Stride: `sizeof(Vertex)` (24 bytes).

#### 3.3.2 `Mesh`

**File**: `core/scene/Mesh.h` / `Mesh.cpp`

```cpp
class Mesh {
public:
    Mesh(int id,
         std::string name,
         VAO vao,
         VBO<Vertex> vbo,
         EBO ebo,
         std::size_t index_count);

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;
    ~Mesh() = default;

    int                get_id()          const noexcept { return id_; }
    const std::string& get_name()        const noexcept { return name_; }
    // std::size_t (not int) because a 3DGS .ply can exceed INT_MAX elements.
    // Cast to GLsizei at the glDrawElements call site in Renderer.
    std::size_t        get_index_count() const noexcept { return index_count_; }

    // Binds the VAO; caller issues the draw command. Renderer friend is avoided
    // by exposing a narrow binder — the VAO itself is never handed out.
    void bind() const { vao_.bind(); }

private:
    int           id_;
    std::string   name_;
    VAO           vao_;
    VBO<Vertex>   vbo_;
    EBO           ebo_;
    std::size_t   index_count_;
};
```

**Why `std::size_t index_count_` is added beyond the class diagram**: the diagram omits
it, but `glDrawElements` needs it and it logically belongs to `Mesh` (one-shot computed
when building the EBO). Documenting this as an extension in §7 Decision Log.

**Invariants**:
- `vao_` is configured (vertex attrib pointers set) at construction time, not in the
  renderer. This keeps the renderer hot path free of attribute setup.
- Non-copyable because VAO/VBO/EBO are non-copyable (GL handles).

#### 3.3.3 `MeshManager`

**File**: `core/scene/MeshManager.h` / `MeshManager.cpp`

```cpp
class MeshManager {
public:
    MeshManager() = default;
    ~MeshManager() = default;

    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;

    // Load from disk. Format dispatched by extension (.obj, .ply).
    // If `name` is std::nullopt (default), the registered name is derived from
    // the filename stem: e.g. "assets/models/bunny.ply" -> "bunny".
    // Returns the ID of the newly created mesh.
    // Throws std::runtime_error on load failure, unsupported extension, or
    // duplicate name.
    int load_mesh(const std::string& filename,
                  std::optional<std::string> name = std::nullopt);

    // Create from in-memory vertex/index data (procedural meshes, test assets,
    // generated geometry). MeshManager owns ALL GL-object construction —
    // callers never touch VAO/VBO/EBO directly. The attribute layout for
    // Vertex (position at loc 0, normal at loc 1) is wired here, exactly once.
    // Returns the ID of the newly created mesh.
    // Throws std::runtime_error on duplicate name or empty input.
    int create_mesh(const std::string& name,
                    const std::vector<Vertex>& vertices,
                    const std::vector<GLuint>& indices);

    // Non-const ref because downstream mutations (future: transforms) are expected.
    // Throws std::out_of_range if the ID / name is unknown.
    Mesh&       get_by_id(int id);
    const Mesh& get_by_id(int id) const;

    Mesh&       get_by_name(const std::string& name);
    const Mesh& get_by_name(const std::string& name) const;

    bool contains(int id)                  const noexcept;
    bool contains(const std::string& name) const noexcept;

private:
    std::unordered_map<int, Mesh>        meshes_;
    std::unordered_map<std::string, int> ids_;    // name -> id
    std::unordered_map<int, std::string> names_;  // id   -> name
    int next_id_ = 0;
};
```

**Rationale for three maps**: matches the class diagram, and supports bidirectional
lookup without a bimap dependency. `meshes_` is the single owner; `ids_` / `names_` are
lookup indices kept in sync by `create_mesh` (called by both `create_mesh` itself and
`load_mesh`).

**Why `load_mesh` and `create_mesh` are split**: `load_mesh` reads bytes from disk;
`create_mesh` wires GL objects from raw vertex/index data. The former is implemented
in terms of the latter — there is exactly one code path that constructs VAO/VBO/EBO,
lives inside `create_mesh`, and is called from both methods. This makes the attribute
layout (position at loc 0, normal at loc 1, stride = `sizeof(Vertex)`) a single-point
invariant and keeps disk-backed and procedural meshes indistinguishable downstream.

Callers never pass raw `VAO`/`VBO`/`EBO` into `MeshManager` — `create_mesh` takes
`std::vector<Vertex>` + `std::vector<GLuint>` instead. This is deliberate:

1. Callers should not have to know the attribute-pointer layout — that belongs inside
   `MeshManager`, expressed exactly once.
2. It keeps `Vertex`-layout details out of user code, so changing the struct (e.g.
   adding UVs later) requires updating one location, not every call site.
3. It matches the `load_mesh` path, which cannot take pre-built GL objects by
   construction — the loader produces raw vertex/index data first.
4. It keeps GL implementation details out of the caller's world, which matters if a
   future CUDA-GL interop or Vulkan path is introduced.

**Duplicate-name policy**: both `load_mesh` and `create_mesh` throw
`std::runtime_error` on duplicate names. Silent overwrite would leave `Scene`
instances holding pointers to the wrong mesh. Callers who need reload semantics must
explicitly remove the old mesh first (future API addition, not this SDD).

**File-format loader**: `load_mesh` dispatches on extension:
- `.obj` → delegates to `tinyobjloader` (vendored, single-header, MIT).
- `.ply` → delegates to `tinyply` (vendored, single-header, BSD).

Both are pulled into `external/` as single-header files; no additional `find_package`
calls are needed. Dispatch happens in an anonymous-namespace helper inside
`MeshManager.cpp`:

```cpp
// MeshManager.cpp
namespace {
    void LoadObjFromFile(const std::string& path,
                         std::vector<Vertex>& out_vertices,
                         std::vector<GLuint>& out_indices);
    void LoadPlyFromFile(const std::string& path,
                         std::vector<Vertex>& out_vertices,
                         std::vector<GLuint>& out_indices);
    std::string StemOfPath(const std::string& path);
}  // namespace

int MeshManager::load_mesh(const std::string& filename,
                           std::optional<std::string> name) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    // Dispatch based on extension; throws on unsupported format.
    /* ... LoadObjFromFile / LoadPlyFromFile call ... */
    std::string resolved_name = name.value_or(StemOfPath(filename));
    return create_mesh(resolved_name, vertices, indices);
}
```

**Why vendored libraries instead of hand-rolling**: a robust `.ply` parser must handle
the header grammar, three encodings (ascii / binary_little_endian / binary_big_endian),
variable element-list structures, and endianness conversion. Hand-rolling this is
~300 LOC with high bug risk. Both `tinyply` and `tinyobjloader` are zero-dependency
single-header files — see §7 Decision Log D7 for the scope change that enabled this,
and `dependencies.md` must be updated to list them.

#### 3.3.4 `Scene`

**File**: `core/scene/Scene.h` / `Scene.cpp`

```cpp
class Scene {
public:
    Scene() = default;
    explicit Scene(Mesh& mesh);
    explicit Scene(const std::vector<Mesh*>& meshes);

    void add_mesh(Mesh* mesh);  // Non-owning; mesh must outlive the scene.

    // Const iteration for the renderer.
    std::vector<Mesh*>::const_iterator begin() const noexcept { return meshes_.begin(); }
    std::vector<Mesh*>::const_iterator end()   const noexcept { return meshes_.end();   }
    std::size_t size() const noexcept { return meshes_.size(); }

private:
    std::vector<Mesh*> meshes_;
};
```

**Ownership contract**: `Scene` holds **non-owning raw pointers**. This is deliberate —
`MeshManager` is the single owner of all `Mesh` instances, and `MeshManager` outlives
`Scene` (see §3.4 for `App` member order). Documented as a hard invariant in §6.1.

Raw pointers are preferred over `std::weak_ptr` here because `MeshManager` uses value
storage (`unordered_map<int, Mesh>`), which precludes `shared_ptr` without a redesign,
and `weak_ptr` would force that redesign for no concrete safety gain in a
single-threaded engine.

#### 3.3.5 `Renderer`

**File**: `core/render/Renderer.h` / `Renderer.cpp`

```cpp
class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void clear() const;  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

    void draw(const Camera& camera,
              const Mesh& mesh,
              const GraphicShader& shader) const;

    void draw(const Camera& camera,
              const Scene& scene,
              const GraphicShader& shader) const;

private:
    // Frame-scope uniforms: camera state. Uploaded once per draw() call.
    void upload_frame_uniforms_(const Camera& camera,
                                const GraphicShader& shader) const;

    // Mesh-scope uniforms: model transform + derived normal matrix.
    // Uploaded once per mesh.
    void upload_mesh_uniforms_(const Mesh& mesh,
                               const GraphicShader& shader) const;
};
```

**Uniform contract** (authoritative — C++ side only; shader authorship is out of scope):

| Uniform | GLSL type | Scope | Uploaded by | v1 value |
|---------|-----------|-------|-------------|----------|
| `uView`          | `mat4` | per-frame | `upload_frame_uniforms_` | `camera.get_view()` |
| `uProjection`    | `mat4` | per-frame | `upload_frame_uniforms_` | `camera.get_projection()` |
| `uViewPos`       | `vec3` | per-frame | `upload_frame_uniforms_` | `camera.position()` |
| `uModel`         | `mat4` | per-mesh  | `upload_mesh_uniforms_`  | `glm::mat4(1.0f)` (identity until `Transform` exists) |
| `uNormalMatrix`  | `mat3` | per-mesh  | `upload_mesh_uniforms_`  | `glm::mat3(1.0f)` (identity; correct for identity model) |

**Silent-skip-on-missing** (FR-15): `glGetUniformLocation` returns `-1` for names the
active shader does not declare; the renderer guards every upload with an
`if (loc != -1)` check. This lets user-authored shaders consume any subset of the
contract — e.g. a debug shader using only `uView` + `uProjection` works without error.

**Shader files are explicitly out of scope for this SDD** — no `.vsh` / `.fsh` files are
created or modified here. The user authors shaders separately; the renderer's job is to
make the full uniform contract available.

**Why `const GraphicShader&` + an internal `const_cast`**: the existing
`ShaderProgram::use()` is non-const (frozen API per user instruction). The renderer's
public contract is "I do not logically modify the shader" — so the parameter is `const`.
Internally, the upload helpers do one localized cast:

```cpp
void Renderer::upload_frame_uniforms_(const Camera& camera,
                                      const GraphicShader& shader) const {
    auto& s = const_cast<GraphicShader&>(shader);
    s.use();
    const GLuint prog = s.get_id();

    if (GLint loc = glGetUniformLocation(prog, "uView"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera.get_view()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uProjection"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(camera.get_projection()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uViewPos"); loc != -1) {
        const glm::vec3 pos = camera.position();
        glUniform3fv(loc, 1, glm::value_ptr(pos));
    }
}

void Renderer::upload_mesh_uniforms_(const Mesh& /*mesh*/,
                                     const GraphicShader& shader) const {
    // v1: Mesh has no Transform yet — upload identity. When Transform arrives,
    // replace these with mesh.transform() and glm::transpose(glm::inverse(mat3(model))).
    const GLuint prog = const_cast<GraphicShader&>(shader).get_id();
    const glm::mat4 model         = glm::mat4(1.0f);
    const glm::mat3 normal_matrix = glm::mat3(1.0f);

    if (GLint loc = glGetUniformLocation(prog, "uModel"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(model));
    }
    if (GLint loc = glGetUniformLocation(prog, "uNormalMatrix"); loc != -1) {
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(normal_matrix));
    }
}
```

The cast is justified because `ShaderProgram::use()` doesn't actually modify logical
shader state — it only changes bound-program GL state. Casting here is preferable to
either (a) modifying the frozen `ShaderProgram` API, or (b) leaking non-const through
the renderer's public interface. Documented as a known wart in §6.2.

**Draw sequencing** (`draw(Scene)`):

```cpp
void Renderer::draw(const Camera& camera,
                    const Scene& scene,
                    const GraphicShader& shader) const {
    upload_frame_uniforms_(camera, shader);   // once per call
    for (const Mesh* mesh : scene) {
        if (!mesh) continue;                  // defensive; see §6.2
        upload_mesh_uniforms_(*mesh, shader); // once per mesh
        mesh->bind();
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(mesh->get_index_count()),
                       GL_UNSIGNED_INT,
                       nullptr);
    }
}
```

#### 3.3.6 `ProjectionMode` (enum class)

**File**: `core/camera/ProjectionMode.h`

```cpp
#pragma once

enum class ProjectionMode {
    kPerspective,
    kOrthogonal,
};
```

#### 3.3.7 `Camera`

**File**: `core/camera/Camera.h` / `Camera.cpp`

```cpp
class Camera {
public:
    // Constructor still takes (eye, center, up) framing — converted once to
    // internal (position, orientation) state. fov_deg is degrees by convention
    // (matches glm::perspective), but all runtime rotation methods take radians.
    Camera(glm::vec3 eye,
           glm::vec3 center,
           glm::vec3 up,
           float fov_deg,
           float aspect,
           float near_plane,
           float far_plane);

    // Translation (camera-local axes; derived from orientation_).
    void move_left(float amount);
    void move_right(float amount);
    void move_up(float amount);
    void move_down(float amount);
    void move_forward(float amount);
    void move_backward(float amount);

    // Rotation (RADIANS). Yaw rotates around world up; pitch around local right.
    void turn_left(float angle_rad);   // yaw +
    void turn_right(float angle_rad);  // yaw -
    void turn_up(float angle_rad);     // pitch +
    void turn_down(float angle_rad);   // pitch -

    void set_aspect(float aspect);

    // Return by const reference into the internal cache (see D9).
    // Do NOT hold across another Camera call that may mutate state.
    const glm::mat4& get_view()       const;
    const glm::mat4& get_projection() const;

    void set_projection_mode(ProjectionMode mode);
    ProjectionMode get_projection_mode() const noexcept { return mode_; }

    // Derived camera-local axes (always unit length because orientation_ is).
    glm::vec3 forward() const { return orientation_ * glm::vec3(0.0f, 0.0f, -1.0f); }
    glm::vec3 right()   const { return orientation_ * glm::vec3(1.0f, 0.0f,  0.0f); }
    glm::vec3 local_up()const { return orientation_ * glm::vec3(0.0f, 1.0f,  0.0f); }

    glm::vec3 position() const noexcept { return position_; }

private:
    void recompute_view_()       const;
    void recompute_projection_() const;

    // Projection-mode switching: preserves framing at focus_distance_.
    void pers_to_orth_();
    void orth_to_pers_();

    // -- Stored state (no Euler angles, no center/up vectors) --

    glm::vec3 position_;                               // was `eye_`
    glm::quat orientation_;                            // world->camera rotation
    float     focus_distance_ = 1.0f;                  // for ortho framing; see below

    float fov_deg_;
    float aspect_;
    float near_;
    float far_;
    ProjectionMode mode_ = ProjectionMode::kPerspective;

    // -- Lazy caches --
    mutable glm::mat4 view_       = glm::mat4(1.0f);
    mutable glm::mat4 projection_ = glm::mat4(1.0f);
    mutable bool view_dirty_       = true;
    mutable bool projection_dirty_ = true;

    // -- Canonical world up, used for yaw --
    static constexpr glm::vec3 kWorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
};
```

**Quaternion semantics**:

`orientation_` is the rotation that takes a vector in camera-local space into world
space. Its conjugate takes world→camera space, which is what the view matrix needs.
`glm::quat` is stored `(w, x, y, z)` internally; `glm::normalize` / `glm::conjugate` /
`glm::mat4_cast` / `glm::angleAxis` are the four GLM operations we need.

**Rotation implementation** (the yaw-world / pitch-local split is the key design point
for preventing horizon tilt):

```cpp
void Camera::turn_left(float angle_rad) {
    // Yaw around WORLD up. Using local up would let roll drift in over time.
    glm::quat delta = glm::angleAxis(angle_rad, kWorldUp);
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_ = true;
}

void Camera::turn_up(float angle_rad) {
    // Pitch around the CURRENT local right axis (derived from orientation_).
    glm::quat delta = glm::angleAxis(angle_rad, right());
    orientation_ = glm::normalize(delta * orientation_);
    view_dirty_ = true;
}
```

Normalization after every mutation (not lazily inside `recompute_view_`) keeps the
unit-length invariant airtight at all times, and avoids making `orientation_` a
`mutable` member — which would blur the "state vs cache" boundary the dirty-flag
pattern depends on. Cost is ~6 muls + 1 sqrt, negligible.

**Translation** (camera-local axes derived from the quaternion):

```cpp
void Camera::move_forward(float amount) {
    position_ += forward() * amount;
    view_dirty_ = true;
}
void Camera::move_right(float amount) {
    position_ += right() * amount;
    view_dirty_ = true;
}
// etc.
```

**View matrix derivation** (no `glm::lookAt` — quaternion direct):

```cpp
void Camera::recompute_view_() const {
    // View = R^-1 * T^-1; for a unit quaternion, inverse == conjugate.
    glm::mat4 rotation    = glm::mat4_cast(glm::conjugate(orientation_));
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), -position_);
    view_ = rotation * translation;
    view_dirty_ = false;
}
```

Cheaper than `glm::lookAt` (no cross products, no re-orthonormalization) and exact.

**Constructor — converting (eye, center, up) to quaternion state**:

```cpp
Camera::Camera(glm::vec3 eye, glm::vec3 center, glm::vec3 up,
               float fov_deg, float aspect, float near_plane, float far_plane)
    : position_(eye),
      focus_distance_(glm::length(center - eye)),
      fov_deg_(fov_deg), aspect_(aspect),
      near_(near_plane), far_(far_plane) {
    glm::vec3 forward_dir = glm::normalize(center - eye);
    // glm::quatLookAt gives the orientation that produces the same framing
    // as glm::lookAt(eye, center, up).
    orientation_ = glm::quatLookAt(forward_dir, up);
}
```

**Projection-switching formula** (FR-06, rewritten around `focus_distance_`):

Instead of deriving distance from `center - eye` (those vectors no longer exist as
stored state), we keep a scalar `focus_distance_`. It's set from the constructor's
`length(center - eye)` and stays constant unless the caller provides an API to change
it (not in this SDD). The orthographic box at that distance matches the perspective
frustum:

```
half_height = focus_distance_ * tan(fov_deg_ * π / 360)
half_width  = half_height * aspect_
proj_orth   = glm::ortho(-half_width, half_width, -half_height, half_height, near_, far_)
```

`orth_to_pers_` restores `glm::perspective(radians(fov_deg_), aspect_, near_, far_)` —
`fov_deg_` is always kept in state, so round-tripping is exact.

**Windows macro safety**: `<windows.h>` `#define`s bare identifiers `near` and `far`.
We use trailing-underscore (`near_`, `far_`) everywhere. No `#undef` needed as long as
we never write bare `near` or `far`.

#### 3.3.8 Input Event Payloads

**File**: `core/event/events.h` (**extend**, do not replace)

```cpp
// Already present — do not touch:
struct WindowResizeEvent { int width; int height; };

// --- new additions below ---

enum class Key {
    kW, kA, kS, kD,
    kUp, kDown, kLeft, kRight,
};

enum class KeyAction {
    kPress,
    kRelease,
    kRepeat,
};

enum class Button {
    kLeft,
    kRight,
    kMiddle,
};

enum class ClickAction {
    kPress,
    kRelease,
};

struct KeyEvent {
    Key       key;
    KeyAction action;
};

struct MouseMoveEvent {
    double dx;
    double dy;
};

struct MouseClickEvent {
    Button      button;
    ClickAction action;
    double      x;
    double      y;
};

struct ScrollEvent {
    double dy;
};
```

**GLFW key → `Key` translation**: a free function
`std::optional<Key> TranslateGlfwKey(int glfw_key)` lives in an anonymous namespace
inside `Window.cpp`. Unknown keys return `std::nullopt` and the event is not emitted —
`CameraController` only reacts to the mapped set (FR consumer side).

#### 3.3.9 `CameraController`

**File**: `core/camera/CameraController.h` / `CameraController.cpp`

```cpp
class CameraController {
public:
    CameraController() = default;
    explicit CameraController(Camera& camera);

    CameraController(const CameraController&) = delete;
    CameraController& operator=(const CameraController&) = delete;

    void bind_camera(Camera& cam);

    // Event handlers. Safe to call with no camera bound (early-return).
    void on_key(const KeyEvent& e);
    void on_mouse_move(const MouseMoveEvent& e);
    void on_click(const MouseClickEvent& e);
    void on_scroll(const ScrollEvent& e);

private:
    Camera* camera_ = nullptr;

    // Tunables. Held as members (not globals) so they can be exposed to UI later.
    // Camera's rotation API is in RADIANS — this controller converts from
    // user-friendly input units at the call site.
    float translate_speed_ = 0.1f;    // world units per key-press tick
    float rotate_speed_    = 0.003f;  // radians per pixel of mouse delta
    float zoom_speed_      = 1.0f;    // per scroll tick
    bool  rotating_        = false;   // set while RMB is held
};
```

**Subscription wiring** happens in `App` (not in `CameraController` itself) because
`EventBus` is `App`'s member — the controller takes a reference to both in `App::App()`:

```cpp
// Inside App::App() — after camera_controller_ and event_bus_ are constructed:
event_bus_.subscribe<KeyEvent>(
    [this](const KeyEvent& e)        { cam_controller_.on_key(e); });
event_bus_.subscribe<MouseMoveEvent>(
    [this](const MouseMoveEvent& e)  { cam_controller_.on_mouse_move(e); });
event_bus_.subscribe<MouseClickEvent>(
    [this](const MouseClickEvent& e) { cam_controller_.on_click(e); });
event_bus_.subscribe<ScrollEvent>(
    [this](const ScrollEvent& e)     { cam_controller_.on_scroll(e); });
```

#### 3.3.10 `Window` Additions (non-breaking)

**File**: `core/window/Window.h` / `Window.cpp`

Additions only — the existing methods/constructor remain as specified in `backend.md`.

```cpp
// NEW public:
float get_aspect_ratio() const;

void set_key_callback        (std::function<void(const KeyEvent&)>        cb);
void set_mouse_move_callback (std::function<void(const MouseMoveEvent&)>  cb);
void set_click_callback      (std::function<void(const MouseClickEvent&)> cb);
void set_scroll_callback     (std::function<void(const ScrollEvent&)>     cb);

// NEW private:
std::function<void(const KeyEvent&)>        key_callback_;
std::function<void(const MouseMoveEvent&)>  mouse_move_callback_;
std::function<void(const MouseClickEvent&)> click_callback_;
std::function<void(const ScrollEvent&)>     scroll_callback_;

double last_mouse_x_ = 0.0;
double last_mouse_y_ = 0.0;
bool   has_last_mouse_ = false;

static void glfw_key_callback   (GLFWwindow*, int, int, int, int);
static void glfw_cursor_callback(GLFWwindow*, double, double);
static void glfw_mouse_callback (GLFWwindow*, int, int, int);
static void glfw_scroll_callback(GLFWwindow*, double, double);

void on_key   (int glfw_key, int action);
void on_mouse (double x, double y);
void on_click (int glfw_button, int action, double x, double y);
void on_scroll(double dy);
```

`get_aspect_ratio()` implementation:

```cpp
float Window::get_aspect_ratio() const {
    return (height_ > 0)
        ? static_cast<float>(width_) / static_cast<float>(height_)
        : 1.0f;
}
```

**Class-diagram reconciliation**: the diagram shows `Window(width, height, title)` with
three parameters, while the current implementation has four (`vsync` included). The
existing constructor is **not** modified. The diagram is treated as *logical* shape —
`vsync` remains as a fourth parameter with a default of `true`. Recorded in §7 Decision Log.

#### 3.3.11 `App` Composition

**File**: `app/App.h` / `app/App.cpp` (**extend**, do not rewrite existing plumbing)

```cpp
class App : private GlfwContext {
public:
    App();
    void run() const;

private:
    void init_gl();

    // Member declaration order == construction order. Critical.
    EventBus         event_bus_;
    Window           window_;
    Camera           camera_;
    CameraController cam_controller_;
    MeshManager      mesh_manager_;
    Scene            scene_;
    Renderer         renderer_;
};
```

**Why this order**: see §3.4 "GL State Ownership & Initialization Order".

### 3.4 File Structure Changes

```
SPATIUM/
├── app/
│   ├── App.h                        ← MODIFY: add members (Camera, ..., Renderer)
│   ├── App.cpp                      ← MODIFY: compose, wire EventBus subscriptions, draw loop
│   └── GlfwContext.h                ← UNCHANGED
├── core/
│   ├── camera/                      ← NEW DIRECTORY
│   │   ├── ProjectionMode.h         ← ADD: enum class
│   │   ├── Camera.h                 ← ADD
│   │   ├── Camera.cpp               ← ADD
│   │   ├── CameraController.h       ← ADD
│   │   └── CameraController.cpp     ← ADD
│   ├── scene/                       ← NEW DIRECTORY
│   │   ├── Vertex.h                 ← ADD
│   │   ├── Mesh.h                   ← ADD
│   │   ├── Mesh.cpp                 ← ADD
│   │   ├── MeshManager.h            ← ADD
│   │   ├── MeshManager.cpp          ← ADD
│   │   ├── Scene.h                  ← ADD
│   │   └── Scene.cpp                ← ADD
│   ├── render/                      ← NEW DIRECTORY
│   │   ├── Renderer.h               ← ADD
│   │   └── Renderer.cpp             ← ADD
│   ├── event/
│   │   ├── events.h                 ← MODIFY: add input event structs + enums
│   │   ├── EventBus.h               ← UNCHANGED
│   │   └── EventID.h                ← UNCHANGED
│   ├── window/
│   │   ├── Window.h                 ← MODIFY: add input callbacks + get_aspect_ratio
│   │   └── Window.cpp               ← MODIFY: implement GLFW input bridging
│   └── gl/                          ← UNCHANGED (all subdirectories)
├── external/
│   ├── glm-1.0.3/                   ← UNCHANGED
│   ├── tinyobjloader/               ← NEW: single-header .obj loader (vendored)
│   │   └── tiny_obj_loader.h
│   └── tinyply/                     ← NEW: single-header .ply loader (vendored)
│       └── tinyply.h
├── shader/                          ← UNCHANGED: shader authoring is out of scope;
│                                    ← user maintains these files independently.
│                                    ← Renderer silently skips uniforms the shader
│                                    ← does not declare (FR-15).
└── CMakeLists.txt                   ← MODIFY: append new .cpp files to CORE_SOURCES
```

### 3.5 GL State Ownership & Initialization Order

This section is specific to C++/GL systems programming and replaces what a data-pipeline
project would put under "protocol design".

**Member construction order inside `App`** (must match declaration order):

```
1. GlfwContext    (base-class subobject)   — glfwInit()
2. event_bus_                                — trivial
3. window_                                   — glfwCreateWindow, GL context current,
                                               GLAD load (load must happen here, not
                                               before — context required for GetProcAddress)
4. camera_                                   — depends on window_.get_aspect_ratio()
5. cam_controller_                            — binds to camera_
6. mesh_manager_                              — trivial; load_mesh calls happen later
7. scene_                                     — may reference mesh_manager_ meshes
8. renderer_                                  — trivial
```

**Destruction order is reverse** — Renderer → Scene → MeshManager → CameraController →
Camera → Window (destroys GL handles) → EventBus → GlfwContext (`glfwTerminate`).

**Critical**: all GL-owning objects (`Mesh`, and by extension `MeshManager`) must be
destroyed **before** `Window`, because `Window`'s destructor destroys the GL context.
Member declaration order above guarantees this.

**GLAD initialization**: stays inside `Window`'s constructor, immediately after
`glfwMakeContextCurrent` — unchanged from the current implementation. Do not move it.

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Extend `core/event/events.h` with enums + input event structs — `core/event/events.h`
    - Expected result: Header compiles standalone; no existing code broken.
    - Completion signal: `grep -r WindowResizeEvent` still yields the original struct intact.

- [ ] **Step 2**: Add `Window` input callbacks + `get_aspect_ratio()` — `core/window/Window.h`, `core/window/Window.cpp`
    - Expected result: Window emits all four input events through user-provided callbacks.
    - Completion signal: Manual test — print events in `App::App()`-registered lambdas, confirm WASD and mouse produce output.

- [ ] **Step 3**: Vendor `tinyobjloader` and `tinyply` under `external/`; implement `Vertex`, `Mesh`, `MeshManager` with extension-dispatched loader — `core/scene/*`, `external/tinyobjloader/`, `external/tinyply/`
    - Expected result: `MeshManager::load_mesh("test.obj")` and `MeshManager::load_mesh("test.ply")` both populate maps and configure VAO attrs.
    - Completion signal: `get_by_id(0).get_index_count() > 0` for both a small `.obj` and a small `.ply` test asset.

- [ ] **Step 4**: Implement `Scene` — `core/scene/Scene.{h,cpp}`
    - Expected result: Can be constructed empty, from one mesh, or from a vector.
    - Completion signal: Range-based for works over a `Scene`.

- [ ] **Step 5**: Implement `ProjectionMode`, `Camera` (quaternion orientation) — `core/camera/*`
    - Expected result: `get_view()` / `get_projection()` match reference matrices (T-05), gimbal-lock regression (T-08) passes.
    - Completion signal: Unit tests T-05, T-06, T-07, T-08 pass.

- [ ] **Step 6**: Implement `CameraController` — `core/camera/CameraController.{h,cpp}`
    - Expected result: Mouse move → camera yaws (world-up axis); WASD → camera translates along local axes.
    - Completion signal: Manual visual test in the render loop.

- [ ] **Step 7**: Implement `Renderer` with 5-uniform contract + split upload helpers — `core/render/Renderer.{h,cpp}`
    - Expected result: `draw(camera, scene, shader)` uploads frame uniforms once + mesh uniforms per mesh. Silent-skip on missing uniforms works.
    - Completion signal: T-12, T-13, T-14 pass. **Do not create or modify any `.vsh` / `.fsh` files** — shader authoring is out of scope.

- [ ] **Step 8**: Compose into `App` + wire `EventBus` subscriptions — `app/App.cpp`
    - Expected result: Member declaration order matches §3.5; lambda subscriptions route events.
    - Completion signal: No resource leaks on exit (valgrind clean or similar). Procedural unit cube visible (`create_mesh("cube", ...)` in `App::App`) if the user has authored a minimal shader.

- [ ] **Step 9**: Update `CMakeLists.txt` — append new `.cpp` files to `CORE_SOURCES`.
    - Command: `cmake --build build` — must succeed with no warnings at `-Wall -Wextra`.

- [ ] **Step 10**: End-to-end validation
    - Command: `./build/SPATIUM`
    - Expected output: If the user provides a shader consuming at least `uView` + `uProjection`, a procedurally-generated cube renders; keyboard/mouse navigation functional; window resize updates aspect correctly; no GL errors over a 60-second session.

### 4.2 Configuration / Constants

Collected here for easy tuning; in code, these live as `constexpr` members in their owning class, not as a global config dict.

```cpp
// Camera default construction (in App::App via member initializer):
constexpr float kDefaultFovDeg     = 45.0f;
constexpr float kDefaultNear       = 0.1f;
constexpr float kDefaultFar        = 100.0f;
constexpr glm::vec3 kDefaultEye    = {0.0f, 0.0f, 3.0f};
constexpr glm::vec3 kDefaultCenter = {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 kDefaultUp     = {0.0f, 1.0f, 0.0f};

// CameraController tunables (see class definition in §3.3.9).
```

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | MeshManager load + lookup | Valid `.obj` or `.ply` via `load_mesh` | Mesh retrievable by both id and name (name = filename stem) | `get_by_id(0)` and `get_by_name("bunny")` return same `Mesh&` |
| T-02 | MeshManager duplicate name | Call `create_mesh` twice with same name | Second call throws `std::runtime_error` | No silent overwrite, exception caught in test |
| T-03 | MeshManager procedural creation | `create_mesh("cube", cube_verts, cube_idx)` | Mesh registered with id = 0 (first mesh), name = "cube" | `contains("cube")` true; `get_by_name("cube").get_index_count() == 36` |
| T-04 | Scene non-ownership | Scene holding Mesh*, MeshManager destroyed | Scene access is UB (documented) | Unit test commented `// ownership: MeshManager outlives Scene` |
| T-05 | Camera initial matrices | eye=(0,0,3), center=(0,0,0), up=(0,1,0), fov=45°, aspect=1, near=0.1, far=100 | View matrix equivalent (within float eps) to `glm::lookAt(eye,center,up)`; projection equals `glm::perspective(radians(45), 1, 0.1, 100)` | Frobenius norm of diff < 1e-5 |
| T-06 | Camera projection mode switch | Perspective → Orthogonal → Perspective | Final perspective matrix equals original | `memcmp` on matrix data after round-trip |
| T-07 | Camera quaternion unit-length invariant | 10,000 random rotation mutations | `\|\|orientation_\|\| - 1.0` remains < 1e-5 | Normalization keeps quaternion unit-length |
| T-08 | **Gimbal-lock regression** | Pitch up 89° → yaw 90° → pitch down 89° | No roll introduced; `local_up()` remains parallel to `kWorldUp` to within 1° | `dot(local_up(), kWorldUp) > 0.9998` |
| T-09 | Aspect update | `Window::get_aspect_ratio` after resize callback | Camera's next `get_projection()` reflects new aspect | Projection matrix `[0][0]` entry updates correctly |
| T-10 | CameraController no-camera-bound | `on_key` called with `camera_ == nullptr` | No crash, no effect | Unit test passes |
| T-11 | EventBus → Controller integration | `App` emits `KeyEvent{kW, kPress}` | Camera moves forward along its current forward direction | `camera.position()` changes by `translate_speed_ * forward()` |
| T-12 | Renderer draw empty scene | Empty `Scene` | No GL errors, no draws issued, but frame uniforms still uploaded | `glGetError()` == `GL_NO_ERROR`, no `glDrawElements` calls |
| T-13 | Renderer draw with valid scene | Scene with one procedurally-created cube mesh | One `upload_frame_uniforms_` + one `upload_mesh_uniforms_` + one `glDrawElements` call | Pixel output non-trivial; depth buffer populated |
| T-14 | **Uniform silent-skip** | Shader that declares only `uView` + `uProjection`; renderer tries to upload all 5 uniforms | No GL error on the three missing uniforms | `glGetError()` == `GL_NO_ERROR` after full draw |

### 5.2 Validation Commands

```bash
# Build (Debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Run
./build/SPATIUM

# Static analysis (if clang-tidy configured)
clang-tidy core/scene/*.cpp core/camera/*.cpp core/render/*.cpp -- -std=c++17

# Leak check
valgrind --leak-check=full ./build/SPATIUM   # Exit after one frame for a clean trace
```

### 5.3 Acceptance Criteria

- [ ] All test cases T-01 through T-14 pass.
- [ ] FR-01 through FR-11 and FR-15 (all MUSTs) verified individually.
- [ ] No modifications to any file in §6.1's protected list.
- [ ] No shader files created or modified — the `shader/` directory is unchanged by this SDD.
- [ ] `Debug::enable()` produces no GL errors during a 60-second interactive session.
- [ ] Clean build at `-Wall -Wextra -Wpedantic` with no warnings.
- [ ] `§9` verification checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ **Do NOT modify the public API of any of these existing classes** (per the user's explicit instruction): `GlfwContext`, `EventBus`, `EventID`, `Debug`, `IGPUResource`, `StorageBuffer`, `VAO`, `VBO`, `EBO`, `UBO`, `SSBO`, `FBO`, `ShaderProgram`, `GraphicShader`, `ComputeShader`. *Additive* changes to `Window`, `events.h`, and `App` are permitted and required; they must not break existing behavior.
- ❌ **Do NOT make `Scene` own its meshes** (no `unique_ptr<Mesh>` inside `Scene`). `MeshManager` is the sole owner.
- ❌ **Do NOT introduce new external dependencies beyond the two approved additions**: `tinyobjloader` (for `.obj`) and `tinyply` (for `.ply`), both vendored as single-header files under `external/`. Any dependency beyond these requires a new SDD or a documented amendment here.
- ❌ **Do NOT call GL functions from any thread other than the one that created the context.** No `std::thread`, no `std::async` in this task.
- ❌ **Do NOT use `#include <bits/stdc++.h>`** or any non-portable headers.
- ❌ **Do NOT put VAO attribute-pointer configuration inside `Renderer`.** It belongs in the `Mesh` constructor — the renderer hot path only binds and draws.
- ❌ **Do NOT introduce global state** — no singletons, no `static` mutable data (aside from function-local `static` counters for ID assignment, which is fine).
- ❌ **Do NOT use exceptions across the ABI of deep hot-path code.** Exceptions are fine at load time (`MeshManager::load_mesh`) but the render loop must not throw.

### 6.2 Known Pitfalls & Limitations

- ⚠️ **Windows macro pollution**: `<windows.h>` defines `near`/`far`. `Camera.h` uses `near_`/`far_` everywhere — never write bare `near` or `far`.
- ⚠️ **Quaternion drift**: repeated `glm::quat` multiplication accumulates floating-point error; unit length must be restored via `glm::normalize` after every rotation mutation. Without this, the view matrix slowly skews over long sessions (invisible at first, obvious after thousands of rotations).
- ⚠️ **Yaw axis must be world up, not local up**: a subtle mistake with visible consequences. If both yaw and pitch use local axes, roll drifts in over time and the horizon tilts. The yaw-world / pitch-local split (D11) is the correct convention.
- ⚠️ **GLAD must be loaded after GL context is current**: already handled inside `Window`'s constructor. Do not reorder.
- ⚠️ **Uninitialized mouse-delta on first frame**: `Window::on_mouse` must track a `has_last_mouse_` flag and emit `dx=dy=0` (or suppress) on the first callback, otherwise the camera snaps wildly.
- ⚠️ **`glDrawElements` index type**: `EBO` indexes are `GLuint` — ensure the loaders and the draw call agree (`GL_UNSIGNED_INT`).
- ⚠️ **`Scene::add_mesh(Mesh*)` with nullptr**: add a debug assert; silently ignoring is worse than crashing in debug.
- ⚠️ **Depth test + back-face culling are enabled in `App::init_gl()`**: meshes must be wound counter-clockwise (OpenGL default) or faces will vanish.
- ⚠️ **Uniform upload with `glGetUniformLocation(prog, "foo")` returning `-1`**: per the GL spec, `glUniform*` is a defined no-op on `-1` — but we still guard explicitly (FR-15) for clarity and to avoid spurious `GL_INVALID_OPERATION` warnings from some debug-output configurations.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| Single model matrix (identity) | `Renderer::upload_mesh_uniforms_` uploads identity for `uModel` and `uNormalMatrix`. No per-mesh transforms. | Medium | Add `Transform` component to `Mesh` or introduce a separate `SceneNode` type in a follow-up SDD. One-line change in the upload helper. |
| Mesh loader is triangle-only | `tinyobjloader`/`tinyply` paths assume triangulated geometry with positions + normals. Quads, NGons, vertex colors, UVs, and materials are ignored. | Low | Extend the loader helper when texturing / PBR arrives. |
| 3DGS `.ply` is not supported by this code path | A Gaussian-splat `.ply` has splat-specific properties (opacity, scale, rotation, SH coeffs) that `Mesh`/`VBO<Vertex>` cannot represent. | Medium | Introduce a separate `SplatCloud` type with its own VBO layout — out of scope for this SDD. |
| No lighting uniforms in the renderer | The renderer does not upload any light-related state. Shaders that need lighting must hardcode values (directional light dir, color, ambient) internally. | Medium | Add a `Light` / `LightManager` pair in a follow-up SDD; extend the uniform contract then. |
| No frustum culling | `Renderer` draws every mesh in the scene unconditionally. | Low | Add AABB to `Mesh` and frustum test in `Renderer::draw` when scene size warrants it. |
| `focus_distance_` does not auto-update | If the camera moves, the orthographic framing stays sized to the original focus distance. | Low | Expose `set_focus_distance()` or auto-update on navigation in a follow-up. |
| No uniform-location caching | Each draw re-queries uniform locations via `glGetUniformLocation` (5 calls per draw for `frame` + per mesh for `mesh`). | Low | Cache locations by program ID in a `ShaderProgram` extension or in `Renderer`'s internals. |
| `const_cast` inside `Renderer` upload helpers | Needed to call the non-const `ShaderProgram::use()` from a `const GraphicShader&` parameter. | Low | Would resolve if `ShaderProgram::use()` became `const`, but that class is frozen per user instruction. |

---

## 7. Open Questions & Decision Log

### Open Questions

*(All draft open questions resolved — see Decision Log D11–D13 for Q1 and for review-round decisions.)*

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | `Scene` uses non-owning `Mesh*`, **not** `shared_ptr<Mesh>`. | `shared_ptr` would force `MeshManager` to store `unordered_map<int, shared_ptr<Mesh>>` (extra allocation + indirection) and would *lie about* ownership — `Scene` doesn't actually co-own. Worse, keeping a `Mesh` alive via `shared_ptr` after `MeshManager` / `Window` (and thus the GL context) are destroyed would invoke `glDeleteVertexArrays` on an invalid context at `~Mesh()`. Raw pointer + documented invariant "MeshManager outlives Scene" makes the rule explicit and checkable (C++ Core Guidelines F.7, R.3). | 2026-04-16 |
| D2 | `Mesh::get_index_count()` added; return type is `std::size_t`, **not** `int`. | (a) It's a natural property of the EBO and `glDrawElements` needs it. (b) `int` is too small — a 3DGS `.ply` can exceed `INT_MAX` elements. `std::size_t` matches the C++ container size convention and casts cleanly to `GLsizei` at the GL call site. | 2026-04-16 |
| D3 | Kept `vsync` as a 4th `Window` constructor param (diagram shows 3). | Breaking the existing `Window` API is out of scope per user instruction. Diagram treated as logical shape. | 2026-04-16 |
| D4 | `CameraController` does **not** subscribe itself to `EventBus`. | Controller doesn't know about the bus; `App` wires subscriptions. Preserves the controller's testability in isolation. | 2026-04-16 |
| D5 | Input enum classes use `k`-prefix style (`kPress`, `kW`, ...). | Google C++ Style Guide convention. The practical win is macro-collision avoidance: `<windows.h>` and other system headers `#define` bare identifiers like `Press`, `Left`, `Right` — `Button::kLeft` is safe against this, `Button::Left` is not. | 2026-04-16 |
| D6 | `Renderer::draw` takes **`const GraphicShader&`**; a localized `const_cast` inside the upload helpers calls `ShaderProgram::use()`. | User requested all `draw` args be `const` references. The existing `ShaderProgram::use()` is non-const (frozen API), so a cast is the minimum-damage workaround. Isolated to one helper function, preferable to leaking non-const through the public interface. | 2026-04-16 |
| D7 | Allow two vendored single-header libraries: `tinyobjloader` (for `.obj`) and `tinyply` (for `.ply`). | User's requirement expanded from `.obj` only to `.obj` + `.ply`. A robust `.ply` parser (header grammar, three encodings, endianness) is ~300 LOC with high bug risk — vendoring zero-dependency single-header libraries is the clear trade. `dependencies.md` must be updated. | 2026-04-16 |
| D8 | `Camera` uses lazy recompute with `mutable` dirty flags. | Avoids recomputing matrices on every mutation. If input produces N state changes per frame (plausible for interactive input), eager recompute does N × matrix derivation while lazy does 1. `mutable` is the language feature for this exact pattern: "logically const, physically caches." | 2026-04-16 |
| D9 | `Camera::get_view` / `get_projection` return **`const glm::mat4&`**. | (a) Avoids copying a 64-byte matrix per getter call. (b) Safe: the reference points into the Camera's own cache member, whose lifetime is the Camera's. Caller must not hold the reference across a Camera mutation — documented in the header. (Resolves draft Q2.) | 2026-04-16 |
| D10 | *(Superseded by D12.)* The renderer uploads `uModel` + `uNormalMatrix` uniforms from day one; `Mesh` has no transform in v1 so identity values are sent. | Future-proofs the shader-facing contract. Adding a `Transform` component to `Mesh` later becomes a one-line change inside `upload_mesh_uniforms_` instead of a shader-rewrite + renderer-rewrite in lockstep. (Resolves draft Q3.) | 2026-04-16 |
| D11 | `Camera` uses **quaternion orientation** (`glm::quat`) instead of Euler-angle or explicit `(eye, center, up)` state. Yaw uses the world up axis; pitch uses the local right axis. Normalization happens after every mutation. | Gimbal lock is eliminated by construction. Repeated Euler-angle rotations also accumulate drift; quaternions are numerically stable when renormalized. The yaw-world / pitch-local split is the standard FPS-camera convention: it prevents roll drift (which pure-local axes would introduce) and prevents pitch-axis drift (which pure-world axes would introduce). Normalization-per-mutation (rather than lazy normalization inside `recompute_view_`) keeps the unit-length invariant airtight at all times and avoids blurring the state/cache boundary. | 2026-04-17 |
| D12 | Renderer uploads a **5-uniform contract**: `uView`, `uProjection`, `uViewPos`, `uModel`, `uNormalMatrix`. No lighting uniforms. Shader files are **not created or modified** by this SDD. | User will author shaders independently. The renderer's responsibility is to expose capability via uniforms; which uniforms the shader *consumes* is the shader's concern. FR-15 (silent skip on missing uniforms) lets partial-consumption shaders work without error. Lighting belongs in a future `Light` / `LightManager` SDD; keeping it out of `Renderer` preserves a clean "transform data only" responsibility. | 2026-04-17 |
| D13 | `MeshManager` exposes **two construction methods**: `load_mesh(filename, optional name)` (disk-backed) and `create_mesh(name, vertices, indices)` (in-memory). `create_mesh` takes raw `std::vector<Vertex>` + `std::vector<GLuint>`, not pre-built `VAO`/`VBO`/`EBO`. | Raw data parameters keep the `Vertex` attribute layout as a single-point invariant inside `MeshManager`, not leaked to every call site. The split mirrors real asset systems (disk load vs. procedural) and lets `load_mesh` be implemented in terms of `create_mesh`. `std::optional<std::string>` name lets callers either let the filename stem be the name (default) or assign an explicit name for decoupled identity. Procedural construction is required for T-13 (unit-cube render test) — depending on a disk-backed cube would couple rendering bring-up to loader bring-up. (Resolves draft Q1.) | 2026-04-17 |

---

## 8. Progress Tracker

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | 2026-04-16 | SDD v1.0 drafted | All implementation steps pending |
| #2 | 2026-04-17 | SDD v1.1: added `.ply` support (tinyply), `get_view`/`get_projection` return `const glm::mat4&`, `Renderer::draw` args all `const` (localized `const_cast` documented), `std::size_t` for index count, resolved Q2/Q3 | Q1 still open |
| #3 | 2026-04-17 | SDD v1.2: resolved Q1 via `load_mesh` / `create_mesh` split (D13); FR-01 rewritten to drop "RAII handoff" jargon; `Camera` switched to quaternion orientation with yaw-world / pitch-local convention (D11); renderer uniform contract formalized to 5 uniforms with split `upload_frame_uniforms_` / `upload_mesh_uniforms_` helpers and silent-skip semantics (D12, FR-15); shader authoring explicitly scoped out; new tests T-07 (quaternion unit-length), T-08 (gimbal-lock regression), T-14 (uniform silent-skip) | No open questions — ready for implementation |

### Current Blockers

- 🟡 Q1/Q2/Q3 in §7 should be resolved before implementation starts; none are blocking design review.

---

## 9. Implementation Verification Checklist

### `Vertex` / `Mesh`
- [ ] `Vertex` is tightly packed; `static_assert` in place.
- [ ] `Mesh` is non-copyable, move-constructible, move-assignable.
- [ ] VAO attribute pointers for `position` (loc 0) and `normal` (loc 1) set in `Mesh` constructor.
- [ ] `index_count_` is `std::size_t`, matches the number of indices uploaded to the EBO, cast to `GLsizei` at the `glDrawElements` call site.
- [ ] Destructor order: VAO → EBO → VBO is fine because none reference each other after construction.

### `MeshManager`
- [ ] `meshes_`, `ids_`, `names_` stay in sync on every `create_mesh` call.
- [ ] `load_mesh` is implemented as dispatch-plus-call into `create_mesh` — there is exactly one code path that constructs VAO/VBO/EBO.
- [ ] `load_mesh` with `name == std::nullopt` derives the name from the filename stem (e.g. `"assets/bunny.ply"` → `"bunny"`).
- [ ] Duplicate-name inserts throw `std::runtime_error` — no silent overwrite.
- [ ] `create_mesh` with empty `vertices` throws.
- [ ] `create_mesh` wires the `Vertex` attribute pointers (position at loc 0, normal at loc 1) — this is the only place in the codebase that does so.
- [ ] `get_by_id` / `get_by_name` throw on miss, do not silently insert.
- [ ] Unsupported file extensions in `load_mesh` throw (not silently fail).
- [ ] Non-copyable.

### `Scene`
- [ ] Never takes ownership — `Mesh*` only, never `Mesh` by value or `unique_ptr`.
- [ ] `add_mesh(nullptr)` triggers a debug assert.
- [ ] Iteration is const-correct.

### `Camera`
- [ ] Internal state is `position_` + `orientation_` (quaternion); no stored `center_` / `up_`.
- [ ] `near_` / `far_` (not `near` / `far`) — Windows macro-safe.
- [ ] Rotation API parameters are in **radians** (header doc-comment says so).
- [ ] Yaw (`turn_left` / `turn_right`) rotates around `kWorldUp` — not local up.
- [ ] Pitch (`turn_up` / `turn_down`) rotates around `right()` — the live local right axis.
- [ ] `glm::normalize` is called on `orientation_` after every rotation mutator.
- [ ] Dirty flags set in every mutator that affects view or projection.
- [ ] `set_aspect` marks projection dirty only (not view).
- [ ] `pers_to_orth_` / `orth_to_pers_` preserve `fov_deg_` across round-trips.
- [ ] `pers_to_orth_` uses `focus_distance_` (not a recomputed `length(center-eye)`).
- [ ] `get_view` / `get_projection` return `const glm::mat4&`, call recompute helpers when dirty.
- [ ] Constructor converts `(eye, center, up)` to `(position_, orientation_)` via `glm::quatLookAt` and stores `focus_distance_ = length(center - eye)`.
- [ ] `forward()`, `right()`, `local_up()` return unit vectors (by construction, because `orientation_` is kept unit-length).

### `CameraController`
- [ ] Safe when `camera_ == nullptr` (all handlers early-return).
- [ ] Does not subscribe to `EventBus` itself — subscription happens in `App`.
- [ ] Mouse delta handling ignores the first event after `has_last_mouse_ == false`.

### `Window` (additions only)
- [ ] Existing methods untouched; diff is purely additive.
- [ ] GLFW input callbacks retrieve `Window*` via `glfwGetWindowUserPointer`, same pattern as the existing resize callback.
- [ ] `get_aspect_ratio` guards against `height_ == 0`.
- [ ] Unknown GLFW keys do not produce `KeyEvent`s (translator returns `nullopt`).

### `Renderer`
- [ ] All three `draw` overloads and `clear` take const-qualified references everywhere.
- [ ] The only `const_cast` in the file lives inside `upload_frame_uniforms_` / `upload_mesh_uniforms_` and is justified by a comment pointing at §6.2.
- [ ] `clear` clears both color and depth.
- [ ] `draw(Scene)` iterates without heap allocation.
- [ ] `upload_frame_uniforms_` is called **once** per `draw()` call, never inside the mesh loop.
- [ ] `upload_mesh_uniforms_` is called **once per mesh**.
- [ ] Every uniform upload is guarded by `if (loc != -1)` — shaders that don't declare a uniform must not produce GL errors.
- [ ] `glGetError()` returns `GL_NO_ERROR` after a successful draw on a valid scene, even with a minimal shader that declares only `uView` + `uProjection`.
- [ ] No VAO / attribute pointer manipulation inside the renderer.
- [ ] No shader files (`.vsh`, `.fsh`) are created or modified by the implementation of this SDD.

### `App`
- [ ] Member declaration order matches §3.5.
- [ ] `EventBus` subscriptions use `this`-capturing lambdas; `CameraController` is bound to `camera_` before any subscription fires.
- [ ] `window_.set_resize_callback` updates `camera_.set_aspect(window_.get_aspect_ratio())` in addition to emitting `WindowResizeEvent`.
- [ ] Existing `init_gl`, `GlfwContext` inheritance, and `run()` loop structure are preserved; new code is only additive inside `run()` (clear → draw → swap).

### `events.h`
- [ ] `WindowResizeEvent` is untouched.
- [ ] All new enums use `enum class` (not plain `enum`).
- [ ] All new event types are trivially copyable POD-like structs.

### Build System
- [ ] New `.cpp` files appended to `CORE_SOURCES` in `CMakeLists.txt`.
- [ ] `tinyobjloader` and `tinyply` single-header files present under `external/` and picked up by the existing `${CMAKE_SOURCE_DIR}/external` include directory.
- [ ] No new `find_package` calls (both loaders are header-only vendored sources).
- [ ] Compiles at `-Wall -Wextra -Wpedantic` with zero warnings. Suppress diagnostics inside the single translation unit that includes each tiny-loader if needed (wrap with `#pragma GCC diagnostic push/pop`).
- [ ] `dependencies.md` updated to list the two new vendored libraries.

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- *(None yet — this is the first SDD in the `scene/camera/render` arc.)*

### 10.2 Scratch Pad / Notes

- Future: a `Transform` component on `Mesh` to support per-mesh model matrices, replacing the identity `uModel` currently hardcoded in the renderer.
- Future: `SceneNode` hierarchy with parent/child transforms (will require separating "asset" from "instance" — `Mesh` stays an asset in `MeshManager`, `SceneNode` becomes the instance).
- Future: CUDA 3DGS path will plug in as an alternate `Renderer::draw` overload — no changes to `Scene`/`Camera`/`CameraController` required; this is the payoff of the current layering.
- Consider adding a `KeyState` map in `CameraController` so that continuous press → continuous movement per frame (rather than per-key-event impulse). Currently deferred to keep v1 simple.

---

*End of SDD*