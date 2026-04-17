# CONSTRAINTS.md — SPATIUM Global Design Constraints

> **Purpose**: Project-wide design stances that govern *all* stages of the scene / camera / render arc.
> Every stage SDD references this file in §1.2 and §6.1.
> **Do not duplicate these into stage SDDs — reference them.**
>
> **Last Updated**: 2026-04-17
> **Status**: `READY` (active for all stage SDDs)

---

## 1. Frozen APIs — DO NOT MODIFY

The public APIs of the following classes are **frozen** for the entire scene / camera / render arc (SDD_01 through SDD_05). Additive extensions to `Window`, `events.h`, and `App` are permitted and required. Anything else in this list is off-limits.

| Module | Files | Status |
|--------|-------|--------|
| GLFW wrapper | `app/GlfwContext.h` | Frozen |
| Event bus core | `core/event/EventBus.h`, `core/event/EventID.h` | Frozen |
| GL debug | `core/gl/debug/Debug.{h,cpp}` | Frozen |
| GL interface | `core/gl/interfaces/IGPUResource.h` | Frozen |
| GL buffer base | `core/gl/obj/StorageBuffer.h`, `BufferObject.h` | Frozen |
| GL buffer concrete | `core/gl/obj/{VAO,VBO,EBO,UBO,SSBO,FBO}.{h,cpp}` | Frozen |
| Shaders | `core/gl/shader/{ShaderProgram,GraphicShader,ComputeShader}.{h,cpp}` | Frozen |
| Shader source files | `shader/**/*.vsh`, `shader/**/*.fsh` | User-authored; not touched by any SDD |

**Additive permitted** (new members, methods, friend declarations): `core/window/Window.{h,cpp}`, `core/event/events.h`, `app/App.{h,cpp}`.

**If a stage SDD appears to require modifying a frozen file, stop.** Either the design is wrong, or the SDD scope has expanded — flag it as a blocker, do not silently edit.

---

## 2. Build & Platform Requirements

| Category | Requirement |
|----------|-------------|
| Language | C++17 (via `set(CMAKE_CXX_STANDARD 17)`) |
| Build | CMake 3.15+ |
| Graphics API | OpenGL 4.5 core profile |
| Linear algebra | GLM 1.0.3 (vendored in `external/glm-1.0.3/`) |
| Window / input | GLFW 3.0+ (system package or vcpkg) |
| GL loader | GLAD (vendored in `include/glad/`) |
| Warning baseline | Compiles clean at `-Wall -Wextra -Wpedantic` |

**External dependencies are capped** at the list above plus two vendored single-header libraries for mesh loading (see §5). Any additional dependency requires an SDD amendment.

---

## 3. Code Style

Follow the **Google C++ Style Guide** throughout:

- `snake_case` for methods, functions, local variables
- `trailing_underscore_` for private / protected member variables
- `kUpperCamelCase` for constants, including enumerators
- `.h` + `.cpp` pairs; `#pragma once` at the top of every header
- One class per header where practical
- No exceptions in render-loop hot paths; exceptions fine at asset-load time

---

## 4. Thread Model

The engine is **single-threaded** for the entire scene / camera / render arc.

- All OpenGL calls must occur on the thread that created the GL context (the thread that constructs `Window`).
- No `std::thread`, `std::async`, or background task introductions in SDD_01 through SDD_05.
- `EventBus` is not thread-safe; events are emitted and handled on the main thread only.
- `Mesh`, `VAO`, `VBO`, `EBO` destructors call GL functions — they must be destroyed on the GL context thread.

Future stages (CUDA interop, async asset loading) will introduce threading carefully; this is outside the current arc's scope.

---

## 5. Mesh-Loader Dependencies

Two vendored single-header libraries are approved for mesh loading:

| Library | Purpose | License | Location |
|---------|---------|---------|----------|
| `tinyobjloader` | `.obj` (Wavefront) loader | MIT | `external/tinyobjloader/tiny_obj_loader.h` |
| `tinyply` | `.ply` (Stanford) loader | BSD | `external/tinyply/tinyply.h` |

Both are header-only. No `find_package` calls needed — they are picked up by the existing `${CMAKE_SOURCE_DIR}/external` include path.

**Why vendored and not hand-rolled**: a robust `.ply` parser has to handle the header grammar, three encodings (ascii, binary LE, binary BE), variable element-list structures, and endianness conversion. That's ~300 LOC with high bug risk. Zero-dependency single-header libraries are the right trade.

`dependencies.md` in the main docs tree must list both libraries.

---

## 6. Global Design Decisions

These decisions shape the architecture across multiple stages. If you find yourself reconsidering one while implementing a single stage, stop — it's a scope-change conversation, not a unilateral edit.

### G-D1: `Scene` uses non-owning `Mesh*`, never `shared_ptr`

**Decision**: `Scene` holds raw `Mesh*` pointers. `MeshManager` is the sole owner of every `Mesh`. The invariant "MeshManager outlives every Scene that references its meshes" is a documented, caller-enforced contract.

**Rationale**:
1. `shared_ptr<Mesh>` would force `MeshManager` to store `unordered_map<int, shared_ptr<Mesh>>`, adding an allocation + indirection per lookup for zero ownership benefit.
2. `shared_ptr` *lies about* the semantics — `Scene` doesn't actually co-own anything. A viewer isn't an owner.
3. Keeping a `Mesh` alive via `shared_ptr` after the GL context is destroyed would call `glDeleteVertexArrays` on an invalid context in `~Mesh()` — a crash in a destructor, the worst kind of bug.
4. Raw pointer + documented invariant makes the rule explicit. `shared_ptr` hides it. (C++ Core Guidelines F.7, R.3.)

**Consequences across stages**:
- SDD_02 (SceneGraph) implements it.
- SDD_04 (Renderer) iterates `Scene` and must null-check defensively.
- SDD_05 (AppIntegration) orders `MeshManager` **before** `Scene` in `App`'s member declaration so destruction runs in reverse (`Scene` destroyed first).

### G-D2: `CameraController` does NOT subscribe itself to `EventBus`

**Decision**: The controller has no reference to `EventBus`. Subscription is wired by `App` — `App` captures `this` in a lambda that forwards the event to the appropriate controller method.

**Rationale**:
- Keeps the controller unit-testable in isolation. You can construct a `CameraController`, call `on_key(...)` directly, and verify behavior without standing up an event bus.
- Keeps the controller's dependencies minimal — it needs `Camera`, nothing else.
- Matches the existing pattern: `Window` doesn't subscribe itself either; `App` wires it.

**Consequences across stages**:
- SDD_03 (Camera) defines `CameraController` with no `EventBus` member.
- SDD_05 (AppIntegration) is the sole place where `event_bus_.subscribe<...>(...)` is called for input events.

### G-D3: Vendored mesh loaders instead of hand-rolling

See §5 above. Recorded here for completeness.

### G-D4: `Camera` uses quaternion orientation, not Euler angles

**Decision**: `Camera` stores orientation as a unit `glm::quat`. Yaw rotations apply around the world up axis; pitch rotations apply around the live local right axis. Normalization happens after every rotation mutation.

**Rationale**:
- Gimbal lock is eliminated by construction — quaternions have no preferred axis ordering.
- Euler-angle accumulation drifts numerically; quaternions are numerically stable when renormalized.
- The **yaw-world / pitch-local split** is the standard FPS-camera convention. Pure-local axes would let roll drift in over time (tilted horizon); pure-world axes would let the pitch axis drift as you yaw.
- Normalization per-mutation (not lazy) keeps the unit-length invariant airtight at all times and avoids blurring the "state vs cache" boundary the dirty-flag pattern depends on.

**Consequences across stages**:
- SDD_03 (Camera) implements it. All other stages are unaware of the internal representation — they only see `get_view()` / `get_projection()` / `position()`.

### G-D5: Renderer uniform contract — 5 uniforms, no lighting, no shader edits

**Decision**: `Renderer` exposes exactly five uniforms to any shader it draws with:

| Uniform | GLSL type | Scope | Source |
|---------|-----------|-------|--------|
| `uView`         | `mat4` | per-frame | `camera.get_view()` |
| `uProjection`   | `mat4` | per-frame | `camera.get_projection()` |
| `uViewPos`      | `vec3` | per-frame | `camera.position()` |
| `uModel`        | `mat4` | per-mesh  | identity in v1 (future: `Transform`) |
| `uNormalMatrix` | `mat3` | per-mesh  | identity in v1 (future: from `Transform`) |

No lighting uniforms. No shader files created or modified by any SDD in this arc. Shaders are user-authored; the renderer's job is to expose capability, not to prescribe shader code.

**Silent-skip rule**: every uniform upload is guarded by `if (loc != -1)`. Shaders that declare only a subset of these uniforms work without error.

**Rationale**:
- Clean separation of responsibilities: renderer transforms data, shader decides how to visualize it.
- Lighting is an orthogonal concern with its own design space (light types, count, shadow mapping) — belongs in a future `Light` / `LightManager` SDD.
- Silent-skip lets minimal test shaders (e.g. declaring only `uView` + `uProjection`) work without GL errors, which makes rendering bring-up smoother.

**Consequences across stages**:
- SDD_04 (Renderer) implements the upload helpers.
- SDD_05 (AppIntegration) does not construct or modify shader files — that's a user activity, not an SDD deliverable.

---

## 7. Glossary

| Term | Meaning |
|------|---------|
| **Frozen API** | A class whose public interface must not be modified by any SDD in this arc |
| **Stage** | One of SDD_01 through SDD_05 — a single focused implementation session |
| **Additive extension** | New members / methods added to a file without changing existing signatures |
| **G-Dn** | Global decision number `n`, defined in this file |
| **Silent-skip** | Uniform upload guard pattern: `if (glGetUniformLocation(...) != -1) { upload; }` |

---

## 8. Change Policy

This file changes only via explicit amendment discussion, never silently. Any stage SDD that needs a new global constraint or decision must add a row here as part of its PR, not as a side effect of implementation.

---

*End of CONSTRAINTS.md*