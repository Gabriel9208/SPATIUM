# SDD: Renderer

> **Version**: 1.0
> **Status**: `READY`
> **Created**: 2026-04-17
> **Last Updated**: 2026-04-17
> **Author**: Gabriel
> **Stage**: 4 of 5

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Implement Renderer — clears the framebuffer, uploads camera
         state as frame-scope uniforms, iterates a Scene uploading
         per-mesh uniforms and issuing draw calls.
SCOPE  : core/render/Renderer.{h,cpp}
GOAL   : Renderer::draw(camera, scene, shader) renders every mesh in
         the scene with the 5-uniform contract from CONSTRAINTS.md GD-05.
         Silent-skip on missing uniforms; no shader files created or
         modified.
AVOID  : Creating or modifying any .vsh / .fsh file. Configuring VAO
         attribute pointers (that is SDD_02's job). Caching uniform
         locations in v1. Allocating on the heap in the draw path.
STATUS : Ready for implementation. No open questions.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

SDD_02 gave us geometry (`Scene` + `Mesh`). SDD_03 gave us a viewpoint
(`Camera`). We now need to tie them together through the GL pipeline:
clear the framebuffer, upload camera state to the shader as uniforms,
bind each mesh, issue the draw call.

The Renderer is deliberately thin — it's a coordinator, not a designer.
It does not own any GL resources. It has no frame state. Two `draw`
overloads + `clear` is its entire public surface. Shader authorship is
out of scope per GD-05.

- **Problem being solved**: No way to get camera + scene onto the screen.
- **Relation to other modules**: Consumes Camera (SDD_03), Scene + Mesh
  (SDD_02), and the existing `GraphicShader` (frozen). Composed into App
  in SDD_05.
- **Dependencies (upstream)**: SDD_02, SDD_03. Frozen `GraphicShader`.
- **Dependents (downstream)**: SDD_05.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| CONSTRAINTS.md | `./CONSTRAINTS.md` | **GD-05** (5-uniform contract, no shader edits); shader authorship is out of scope |
| SDD_02 | `./SDD_02_SceneGraph.md` | Provides `Mesh::bind()`, `Mesh::get_index_count()`, `Scene` iteration |
| SDD_03 | `./SDD_03_Camera.md` | Provides `Camera::get_view()`, `get_projection()`, `position()` |
| Existing `GraphicShader` | `core/gl/shader/GraphicShader.{h,cpp}` | Frozen; `use()` is non-const |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] `Renderer::clear()` clears color + depth buffers.
- [ ] `Renderer::draw(Camera, Mesh, GraphicShader)` renders a single mesh.
- [ ] `Renderer::draw(Camera, Scene, GraphicShader)` uploads frame
  uniforms once, iterates meshes, uploads per-mesh uniforms, draws.
- [ ] All five uniforms from CONSTRAINTS.md GD-05 are uploaded when
  present in the shader; missing uniforms (`glGetUniformLocation == -1`)
  are silently skipped.
- [ ] No shader files (`.vsh`, `.fsh`) are created or modified.
- [ ] No heap allocation in the draw path.

### 2.2 Input / Output Contract

**`Renderer::draw(camera, scene, shader)`**:
- **Inputs**: `const Camera&`, `const Scene&`, `const GraphicShader&`.
- **Outputs**: None.
- **Side effects**: Binds a shader program; uploads uniforms; issues
  `glDrawElements` per mesh.
- **Preconditions**: GL context current; shader has been loaded; scene
  contains only non-null `Mesh*` whose GL handles are still valid.

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `Renderer::clear()` calls `glClear(GL_COLOR_BUFFER_BIT \| GL_DEPTH_BUFFER_BIT)`. | MUST |
| FR-02 | `Renderer::draw(camera, mesh, shader)` uploads frame uniforms, uploads mesh uniforms, binds mesh, issues one `glDrawElements`. | MUST |
| FR-03 | `Renderer::draw(camera, scene, shader)` uploads frame uniforms ONCE, then iterates meshes and per-mesh uploads + draws. Frame uniforms are NOT re-uploaded inside the mesh loop. | MUST |
| FR-04 | `upload_frame_uniforms_` uploads `uView`, `uProjection`, `uViewPos`. | MUST |
| FR-05 | `upload_mesh_uniforms_` uploads `uModel`, `uNormalMatrix`. v1 values are `glm::mat4(1.0f)` and `glm::mat3(1.0f)` (identity). | MUST |
| FR-06 | Every uniform upload is guarded by `if (loc != -1)`. Missing uniforms are silently skipped; no GL error raised. | MUST |
| FR-07 | All three `Renderer` methods (`clear`, both `draw` overloads) take const-qualified references. | MUST |
| FR-08 | The non-const `ShaderProgram::use()` is reached via one localized `const_cast` inside the upload helpers. No other `const_cast` in the file. | MUST |
| FR-09 | Null `Mesh*` inside a `Scene` is skipped defensively (continue the loop, not crash). | SHOULD |
| FR-10 | No heap allocation in `draw`. No `std::vector`, `std::string`, `new`, or similar inside the method bodies. | SHOULD |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | 60 FPS @ 1280×720 with a single 100k-triangle mesh on an RTX 3000-class GPU. |
| Memory | Renderer itself holds zero GL state; zero per-frame allocations. |
| Compatibility | OpenGL 4.5 core, GLM 1.0.3. |
| Code Style | See CONSTRAINTS.md §3. |

---

## 3. Design

### 3.1 High-Level Approach

Two-tier uniform upload:

1. **Frame-scope** (`upload_frame_uniforms_`) — camera-derived state.
   Uploaded once per `draw()` call, before the mesh loop.
2. **Mesh-scope** (`upload_mesh_uniforms_`) — per-mesh state. For v1,
   identity values for `uModel` and `uNormalMatrix` (no `Transform`
   component on `Mesh` yet).

Every `glUniform*` call is preceded by `glGetUniformLocation`, guarded
by `!= -1`, so shaders that declare a subset of the contract work
without error.

**Rejected alternatives**:
- *Single upload helper* — rejected. A single helper forces frame
  uniforms to re-upload per mesh, wasting 3 `glUniform*` calls per mesh.
- *Uniform location caching* — deferred to future work. Adds complexity
  (per-program location map); not worth it in v1.
- *Cached `GraphicShader` pointer* — rejected. The renderer holds no
  per-frame state by design.

### 3.2 Architecture / Data Flow

```
Renderer::draw(camera, scene, shader)
      │
      ▼
upload_frame_uniforms_(camera, shader)    ── 1 call per draw()
      │     ├─ const_cast<GraphicShader&>(shader).use()
      │     ├─ glGetUniformLocation(prog, "uView")        → loc
      │     │  if (loc != -1) glUniformMatrix4fv(loc, ..., camera.get_view())
      │     ├─ glGetUniformLocation(prog, "uProjection")  → loc
      │     │  if (loc != -1) glUniformMatrix4fv(loc, ..., camera.get_projection())
      │     └─ glGetUniformLocation(prog, "uViewPos")     → loc
      │        if (loc != -1) glUniform3fv     (loc, ..., camera.position())
      ▼
for (Mesh* mesh : scene)                 ── one iteration per mesh
      │
      ├─ if (!mesh) continue;            ── defensive
      │
      ├─ upload_mesh_uniforms_(*mesh, shader)  ── 1 call per mesh
      │     ├─ glGetUniformLocation(prog, "uModel")        → loc
      │     │  if (loc != -1) glUniformMatrix4fv(loc, ..., mat4(1.0f))
      │     └─ glGetUniformLocation(prog, "uNormalMatrix") → loc
      │        if (loc != -1) glUniformMatrix3fv(loc, ..., mat3(1.0f))
      │
      ├─ mesh->bind()                    ── VAO bind (SDD_02 API)
      │
      └─ glDrawElements(GL_TRIANGLES,
                        GLsizei(mesh->get_index_count()),
                        GL_UNSIGNED_INT, nullptr)
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `Renderer`

**File**: `core/render/Renderer.h`

```cpp
#pragma once
#include "core/camera/Camera.h"
#include "core/scene/Mesh.h"
#include "core/scene/Scene.h"
#include "core/gl/shader/GraphicShader.h"

class Renderer {
public:
    Renderer() = default;
    ~Renderer() = default;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void clear() const;

    void draw(const Camera& camera,
              const Mesh& mesh,
              const GraphicShader& shader) const;

    void draw(const Camera& camera,
              const Scene& scene,
              const GraphicShader& shader) const;

private:
    // Frame-scope: uploaded once per draw() call.
    void upload_frame_uniforms_(const Camera& camera,
                                const GraphicShader& shader) const;

    // Mesh-scope: uploaded once per mesh.
    void upload_mesh_uniforms_(const Mesh& mesh,
                               const GraphicShader& shader) const;
};
```

#### 3.3.2 Implementation

**File**: `core/render/Renderer.cpp`

```cpp
#include "Renderer.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

void Renderer::clear() const {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::upload_frame_uniforms_(const Camera& camera,
                                      const GraphicShader& shader) const {
    // Localized const_cast: ShaderProgram::use() is non-const (frozen API).
    // Renderer logically does not modify the shader — only GL program-bind
    // state — so the parameter is const. See SDD §6.1.
    auto& s = const_cast<GraphicShader&>(shader);
    s.use();
    const GLuint prog = s.get_id();

    if (GLint loc = glGetUniformLocation(prog, "uView"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE,
                           glm::value_ptr(camera.get_view()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uProjection"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE,
                           glm::value_ptr(camera.get_projection()));
    }
    if (GLint loc = glGetUniformLocation(prog, "uViewPos"); loc != -1) {
        const glm::vec3 pos = camera.position();
        glUniform3fv(loc, 1, glm::value_ptr(pos));
    }
}

void Renderer::upload_mesh_uniforms_(const Mesh& /*mesh*/,
                                     const GraphicShader& shader) const {
    // v1: Mesh has no Transform yet — upload identity.
    // When Transform is added (follow-up SDD), replace these with
    // mesh.transform() and transpose(inverse(mat3(model))).
    const GLuint prog = const_cast<GraphicShader&>(shader).get_id();

    static const glm::mat4 kIdentityMat4 = glm::mat4(1.0f);
    static const glm::mat3 kIdentityMat3 = glm::mat3(1.0f);

    if (GLint loc = glGetUniformLocation(prog, "uModel"); loc != -1) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(kIdentityMat4));
    }
    if (GLint loc = glGetUniformLocation(prog, "uNormalMatrix"); loc != -1) {
        glUniformMatrix3fv(loc, 1, GL_FALSE, glm::value_ptr(kIdentityMat3));
    }
}

void Renderer::draw(const Camera& camera,
                    const Mesh& mesh,
                    const GraphicShader& shader) const {
    upload_frame_uniforms_(camera, shader);
    upload_mesh_uniforms_(mesh, shader);
    mesh.bind();
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(mesh.get_index_count()),
                   GL_UNSIGNED_INT, nullptr);
}

void Renderer::draw(const Camera& camera,
                    const Scene& scene,
                    const GraphicShader& shader) const {
    upload_frame_uniforms_(camera, shader);
    for (const Mesh* mesh : scene) {
        if (!mesh) continue;
        upload_mesh_uniforms_(*mesh, shader);
        mesh->bind();
        glDrawElements(GL_TRIANGLES,
                       static_cast<GLsizei>(mesh->get_index_count()),
                       GL_UNSIGNED_INT, nullptr);
    }
}
```

### 3.4 File Structure Changes

```
SPATIUM/
├── core/
│   └── render/                      ← NEW DIRECTORY
│       ├── Renderer.h               ← NEW
│       └── Renderer.cpp             ← NEW
└── CMakeLists.txt                   ← MODIFY: append core/render/Renderer.cpp
```

No shader files touched. No other files modified.

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Create `core/render/Renderer.h` with full class declaration.
    - Completion: Header compiles standalone.

- [ ] **Step 2**: Implement `clear()` in `Renderer.cpp`.
    - Completion: Trivial — calls `glClear` with both bit flags.

- [ ] **Step 3**: Implement `upload_frame_uniforms_`.
    - Completion: All three uniforms guarded by `-1` check.

- [ ] **Step 4**: Implement `upload_mesh_uniforms_` with identity values
  and `static const` mat4/mat3 to avoid stack construction per call.
    - Completion: Both uniforms guarded by `-1` check.

- [ ] **Step 5**: Implement `draw(Camera, Mesh, GraphicShader)`.
    - Completion: Single-mesh draw path functional.

- [ ] **Step 6**: Implement `draw(Camera, Scene, GraphicShader)` with
  null-mesh skip.
    - Completion: Scene iteration + draws functional.

- [ ] **Step 7**: Update `CMakeLists.txt` — append `core/render/Renderer.cpp`.

- [ ] **Step 8**: Write unit tests T-01 through T-06.
    - Completion: All pass.

- [ ] **Step 9**: End-to-end smoke test — requires SDD_02 (procedural
  cube in `MeshManager`) and SDD_03 (Camera). User provides a minimal
  shader declaring at least `uView` + `uProjection`.
    - Completion: Cube visible on screen; T-07 passes.

- [ ] **Step 10**: Build + verify zero warnings.
    - Command: `cmake --build build`

### 4.2 Configuration / Constants

- `kIdentityMat4 = glm::mat4(1.0f)` and `kIdentityMat3 = glm::mat3(1.0f)`
  — static const at file scope inside `upload_mesh_uniforms_` to avoid
  per-call construction.

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | `clear` clears both buffers | Call `clear()` after setting color to blue and a depth value | Color buffer is background color; depth buffer is `1.0` | Read back with `glReadPixels` / depth texture |
| T-02 | `draw(Mesh)` with all 5 uniforms present | Shader declares all 5; single cube mesh | `glGetError()` == `GL_NO_ERROR` after call; one `glDrawElements` invoked | Instrument the call or verify via apitrace |
| T-03 | `draw(Scene)` with 3 meshes | Scene containing 3 cubes | One frame-uniforms upload + 3 mesh-uniforms uploads + 3 `glDrawElements` | Counter-based verification |
| T-04 | **Uniform silent-skip** | Shader declares only `uView` + `uProjection`; all 5 uniforms attempted | No GL error on the 3 missing uniforms | `glGetError()` == `GL_NO_ERROR` throughout |
| T-05 | Empty scene | Empty `Scene` passed to `draw` | Frame uniforms uploaded; zero `glDrawElements` calls; no error | Counter-based verification |
| T-06 | Null mesh in scene | Scene containing `{valid, nullptr, valid}` | Two `glDrawElements` calls; middle skipped; no crash | Defensive skip verified |
| T-07 | End-to-end visual | Procedural cube + valid shader + valid camera | Cube visible in framebuffer | Manual / pixel-diff test |
| T-08 | No heap allocation in draw | Instrument with a custom `operator new` in test harness | Zero allocations observed during `draw` | Allocation counter == 0 |
| T-09 | `clear` is `const` | `const Renderer r; r.clear();` | Compiles | Template check |

### 5.2 Validation Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

# Smoke test (requires SDD_02 + SDD_03 built):
./build/SPATIUM

# GL trace:
apitrace trace ./build/SPATIUM
apitrace dump *.trace | grep -E "glDrawElements|glUniform"
```

### 5.3 Acceptance Criteria

- [ ] All tests T-01 through T-09 pass.
- [ ] FR-01 through FR-10 verified.
- [ ] No files created or modified under `shader/`.
- [ ] No `find_package` or dependency changes.
- [ ] `dependencies.md` not touched.
- [ ] Clean build at `-Wall -Wextra -Wpedantic`.
- [ ] §9 checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ Global constraints from **CONSTRAINTS.md** apply in full. Especially:
    - **GD-05** — 5-uniform contract; shader authorship out of scope.
- ❌ Do NOT create or modify any `.vsh` / `.fsh` file. The user owns
  shader authoring independently.
- ❌ Do NOT configure VAO attribute pointers anywhere in `Renderer`.
  That is SDD_02's single-point responsibility, done inside
  `MeshManager::create_mesh`.
- ❌ Do NOT add state members to `Renderer`. The class has no per-frame
  or per-call state.
- ❌ Do NOT use `const_cast` anywhere except inside the two upload
  helpers to call `ShaderProgram::use()`. The cast is localized and
  justified by the frozen-API constraint.
- ❌ Do NOT add uniform-location caching in v1. It's listed as a future
  optimization; doing it here bloats the spec.
- ❌ Do NOT allocate on the heap inside `draw`. No `std::vector`,
  no `std::string`, no `new`.
- ❌ Do NOT add a `cached_program_id_` or any shader-identity tracking
  member. Shader binding is whatever `use()` does; the renderer is stateless.
- ❌ Do NOT throw exceptions from any `draw` or `clear` method. Render
  loop must not throw per CONSTRAINTS.md §5.

### 6.2 Known Pitfalls & Limitations

- ⚠️ **`glGetUniformLocation` is a hash lookup inside the driver**. Called
  5 times per draw in v1, ~once per mesh. Measurable at hundreds of meshes;
  trivial in v1. Do NOT optimize prematurely.
- ⚠️ **`glUniform*` with `loc == -1` is spec-defined as a silent no-op**.
  The guard is redundant for correctness but we keep it for clarity and
  to avoid spurious `GL_INVALID_OPERATION` on some debug-output settings.
- ⚠️ **Uploading frame uniforms before `use()`** produces GL errors — the
  `use()` call binds the program; uniforms target the currently-bound
  program. Order: `use()` first, then `glUniform*`.
- ⚠️ **`const_cast` hazard**: only use the cast to reach `use()` and
  `get_id()`. Casting to then mutate shader state would be a contract
  violation.
- ⚠️ **`static const glm::mat4`** construction order: placed inside the
  function body, initialized on first call; thread-safe in C++11+ under
  single-threaded assumption (CONSTRAINTS.md §4).
- ⚠️ **Back-face culling + depth test are enabled by App's `init_gl`**:
  meshes with reversed winding will vanish. This is an asset issue, not
  a renderer bug — document so SDD_02's loader test cube has correct
  winding.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| Identity `uModel` | No per-mesh transforms in v1. | Medium | Add `Transform` component to `Mesh`; replace identity in `upload_mesh_uniforms_`. Follow-up SDD. |
| No uniform-location caching | Re-query every draw. | Low | Cache per-program in a sidecar map. |
| No shader binding deduplication | `use()` called every draw even if program didn't change. | Low | Add `GLuint last_bound_program_id_` field; skip `use()` if same. Nice-to-have. |
| No frustum culling | Every mesh drawn unconditionally. | Low | Add AABB to Mesh + frustum test when scenes grow. |
| No lighting uniforms | Shaders must hardcode lighting. | Medium | Future `Light` / `LightManager` SDD. |

---

## 7. Open Questions & Decision Log

### Open Questions

*(None — SDD ready for implementation.)*

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | `Renderer::draw` takes `const GraphicShader&`; a localized `const_cast` reaches the non-const `use()`. | User specified all `draw` args should be const references. `ShaderProgram::use()` is non-const in the frozen API. A cast at the call site is the minimum-damage workaround, preferable to leaking non-const through the renderer's public interface. | 2026-04-17 |
| D2 | Uniform uploads are split into frame-scope (`upload_frame_uniforms_`) and mesh-scope (`upload_mesh_uniforms_`) helpers. | Frame uniforms should not be re-uploaded per mesh. The split makes this structural, not a discipline problem. `draw(Mesh)` and `draw(Scene)` share the factoring. | 2026-04-17 |
| D3 | Silent-skip on `glGetUniformLocation == -1`. | The GD-05 uniform contract defines what the renderer *offers*. Which of those a given shader *consumes* is the shader's choice. Silent-skip lets user-authored shaders consume any subset without error. The explicit guard is defensive against debug-output configurations that warn on `glUniform*(-1, ...)`. | 2026-04-17 |
| D4 | `Renderer` is stateless (no per-frame members, no cached program ID). | The draw path must be heap-allocation-free and thread-contention-free. Adding state is justified only when measured; in v1, the stateless form is simpler and fast enough. | 2026-04-17 |
| D5 | `static const glm::mat4` identity matrices are placed inside `upload_mesh_uniforms_`, not at class scope. | They're implementation details of that helper. Keeping them local avoids polluting the header and matches where they're used. | 2026-04-17 |
| D6 | Null `Mesh*` inside a `Scene` is skipped with `continue`, not asserted. | `Scene::add_mesh` already debug-asserts against nullptr (SDD_02). The renderer is the last line of defense in release builds; skipping is safer than crashing. | 2026-04-17 |

---

## 8. Progress Tracker

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | 2026-04-17 | SDD drafted, marked READY | All implementation steps pending |

### Current Blockers

*(None — depends on SDD_02 and SDD_03 being implemented first.)*

---

## 9. Implementation Verification Checklist

### `Renderer.h`
- [ ] Non-copyable.
- [ ] All three public methods (`clear`, two `draw` overloads) are `const`-qualified and take const references.
- [ ] Two private helpers: `upload_frame_uniforms_` and `upload_mesh_uniforms_`.
- [ ] No state members.
- [ ] Includes only what's needed: `Camera.h`, `Mesh.h`, `Scene.h`, `GraphicShader.h`.

### `Renderer.cpp` — `clear`
- [ ] Calls `glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)`.
- [ ] Nothing else.

### `Renderer.cpp` — `upload_frame_uniforms_`
- [ ] First statement: `const_cast<GraphicShader&>(shader).use();`.
- [ ] Three uniform uploads: `uView`, `uProjection`, `uViewPos`.
- [ ] Each upload guarded by `if (GLint loc = ...; loc != -1)`.
- [ ] Uses `glm::value_ptr` for the data pointer.
- [ ] `uView` / `uProjection` use `glUniformMatrix4fv`; `uViewPos` uses `glUniform3fv`.
- [ ] Comment pointing to §6.1 justifying the const_cast.

### `Renderer.cpp` — `upload_mesh_uniforms_`
- [ ] `static const` identity matrices at function scope (not class scope, not per-call).
- [ ] Two uniform uploads: `uModel` (mat4, identity), `uNormalMatrix` (mat3, identity).
- [ ] Each guarded by `-1` check.
- [ ] Mesh parameter currently unused; comment indicates where `Transform` hook will go.

### `Renderer.cpp` — `draw(Camera, Mesh, GraphicShader)`
- [ ] Single-mesh path: frame upload → mesh upload → mesh.bind() → `glDrawElements`.
- [ ] `static_cast<GLsizei>(mesh.get_index_count())` for the count parameter.
- [ ] `GL_UNSIGNED_INT` for index type (matches `EBO` contract).

### `Renderer.cpp` — `draw(Camera, Scene, GraphicShader)`
- [ ] Frame upload happens EXACTLY ONCE, before the loop.
- [ ] Loop uses range-based for over the `Scene`.
- [ ] `if (!mesh) continue;` guard before any use of `mesh`.
- [ ] Per-mesh: mesh upload → mesh.bind() → `glDrawElements`.
- [ ] No allocation inside the loop.

### Absence checks
- [ ] No `.vsh` / `.fsh` files created or modified anywhere.
- [ ] No VAO attribute pointer calls (`glVertexAttribPointer`, `glEnableVertexAttribArray`) anywhere in `Renderer.cpp`.
- [ ] No `const_cast` outside the two upload helpers.
- [ ] No mutable state on the class.
- [ ] No exceptions thrown from any method.
- [ ] No `new` / `malloc` / `std::vector` / `std::string` local in `draw` body.

### Build
- [ ] `core/render/Renderer.cpp` added to `CORE_SOURCES`.
- [ ] Clean build at `-Wall -Wextra -Wpedantic`.

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- [CONSTRAINTS.md](./CONSTRAINTS.md) — GD-05 uniform contract is the authority.
- [SDD_02_SceneGraph.md](./SDD_02_SceneGraph.md) — provides `Mesh::bind()`, `Mesh::get_index_count()`, `Scene` iteration.
- [SDD_03_Camera.md](./SDD_03_Camera.md) — provides `Camera::get_view()`, `get_projection()`, `position()`.
- [SDD_05_AppIntegration.md](./SDD_05_AppIntegration.md) — composes Renderer into App; owns the render loop that calls `clear` + `draw`.

### 10.2 Scratch Pad / Notes

- When `Transform` is added to `Mesh`, `upload_mesh_uniforms_` becomes:
  ```cpp
  const glm::mat4 model        = mesh.transform();
  const glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(model)));
  ```
  plus the two existing `glUniform*` calls. Nothing else changes.
- When `Light` is added, `upload_frame_uniforms_` gains `uLightDir`,
  `uLightColor`, `uAmbient` (or a UBO binding). No shader edits forced
  because of the silent-skip contract.
- Uniform-location caching: a `std::unordered_map<GLuint, LocationCache>`
  on the Renderer, or preferably on `GraphicShader` if that class is ever
  unfrozen.

---

*End of SDD_04*

---

## Outcome & Decisions

### Final Technical Choices

- `Renderer` is fully stateless: no member variables, no cached GL state.
- `GraphicShader::use()` is already `const` in the frozen API — the localized `const_cast` described in SDD D1 was **not needed**; `shader.use()` and `shader.get_id()` both called directly on the const reference.
- `static const glm::mat4/mat3` identity matrices placed at function scope in `upload_mesh_uniforms_` — initialized once on first call (C++11 thread-safe static initialization under the single-threaded constraint).
- `glGetUniformLocation` result captured with `if (GLint loc = ...; loc != -1)` — C++17 init-if syntax for compact, scoped guard.
- Frame uniforms uploaded exactly once before the mesh loop in `draw(Camera, Scene, GraphicShader)`.
- Null `Mesh*` entries inside a `Scene` are skipped with `continue` (defensive, not asserted).

### Rejected Alternatives

- `const_cast` workaround: not needed; `use()` is const.
- Uniform-location caching: deferred as per the SDD; adds complexity not justified in v1.
- State member for last-bound program: deferred; renderer stays stateless.

### Notes for SDD_05

- `App` must call `renderer_.clear()` at the top of each frame, before `renderer_.draw(camera_, scene_, shader_)`.
- `App` owns `Renderer renderer_`, `Camera camera_`, `Scene scene_`, `GraphicShader shader_` as value members.
- Depth test and back-face culling must be enabled in App's `init_gl()` (GL_DEPTH_TEST, GL_CULL_FACE) before the render loop starts.
- `glClearColor` should be set once in `init_gl()`; `clear()` itself only calls `glClear`.