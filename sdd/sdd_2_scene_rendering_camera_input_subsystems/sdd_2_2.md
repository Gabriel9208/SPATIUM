# SDD: Scene Graph — Vertex, Mesh, MeshManager, Scene

> **Version**: 1.0
> **Status**: `READY`
> **Created**: 2026-04-17
> **Last Updated**: 2026-04-17
> **Author**: Gabriel
> **Stage**: 2 of 5

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Implement the scene graph: Vertex struct, Mesh (owns VAO/VBO/EBO),
         MeshManager (owns all Mesh instances, loads .obj/.ply and creates
         procedural), Scene (non-owning view of a subset of meshes).
SCOPE  : core/scene/Vertex.h
         core/scene/Mesh.{h,cpp}
         core/scene/MeshManager.{h,cpp}
         core/scene/Scene.{h,cpp}
         external/tinyobjloader/tiny_obj_loader.h  (vendored)
         external/tinyply/tinyply.h                (vendored)
GOAL   : Given a file path or raw vertex+index data, produce a Mesh with
         correctly configured VAO attribute pointers; index/name lookup
         works O(1); Scene can reference a subset of meshes non-destructively.
AVOID  : Letting callers construct VAO/VBO/EBO themselves, storing Mesh
         inside Scene by value or shared_ptr, duplicate-name silent
         overwrite, hand-rolling .ply parsing.
STATUS : Ready for implementation. No open questions.
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

The engine has no representation of geometry. This SDD introduces the
asset layer: `Vertex` describes one vertex's data, `Mesh` bundles a VAO
with its backing VBO+EBO, `MeshManager` owns all `Mesh` instances and
provides file-loading + procedural construction, and `Scene` holds a
non-owning list of `Mesh*` for the renderer (SDD_04) to iterate.

This layer is deliberately decoupled from Camera (SDD_03) and Renderer
(SDD_04). It can be implemented and tested standalone.

- **Problem being solved**: No geometry representation exists.
- **Relation to other modules**: Consumes GL objects from `core/gl/obj/`
  (frozen). Consumed by `Renderer` (SDD_04) and composed into `App` (SDD_05).
- **Dependencies (upstream)**: `VAO`, `VBO<T>`, `EBO`, `IGPUResource`
  (all frozen); GLM (existing).
- **Dependents (downstream)**: SDD_04, SDD_05.

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| CONSTRAINTS.md | `./CONSTRAINTS.md` | **GD-01** (Scene non-owning), **GD-03** (vendored loaders) |
| Frozen GL objects | `core/gl/obj/`, `core/gl/interfaces/IGPUResource.h` | Interfaces we consume |
| tinyobjloader | https://github.com/tinyobjloader/tinyobjloader | Single-header MIT |
| tinyply | https://github.com/ddiakopoulos/tinyply | Single-header BSD |
| Existing data.md | `docs/CODEMAPS/data.md` | VBO<T> / VAO usage patterns |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] `Vertex` struct is tightly packed, 24 bytes (2 × `glm::vec3`).
- [ ] `Mesh` is move-only, holds `VAO`, `VBO<Vertex>`, `EBO` by value,
  tracks index count as `std::size_t`.
- [ ] `MeshManager::load_mesh` loads `.obj` and `.ply` (binary_little_endian + ascii).
- [ ] `MeshManager::create_mesh` constructs a mesh from raw `vector<Vertex>`
  + `vector<GLuint>`; this is the sole code path for VAO attribute
  pointer configuration.
- [ ] `MeshManager` lookup by ID and by name is O(1) average.
- [ ] Duplicate names throw `std::runtime_error`.
- [ ] `Scene` is constructible empty, from one mesh, or from a vector
  of `Mesh*`; holds non-owning pointers only.

### 2.2 Input / Output Contract

**`MeshManager::load_mesh`**:
- **Inputs**: `filename` (path), `optional<string> name` (default: derived from filename stem).
- **Outputs**: `int` — the ID of the newly registered mesh.
- **Side effects**: Reads file from disk; constructs GL objects; inserts into three internal maps.
- **Throws**: `std::runtime_error` on load failure, unsupported extension, or duplicate name.

**`MeshManager::create_mesh`**:
- **Inputs**: `name`, `const vector<Vertex>&`, `const vector<GLuint>&`.
- **Outputs**: `int` — the ID of the newly registered mesh.
- **Side effects**: Constructs GL objects on the current GL context; inserts into maps.
- **Throws**: `std::runtime_error` on empty inputs or duplicate name.

### 2.3 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | `Vertex` is `{glm::vec3 position; glm::vec3 normal;}`. `static_assert(sizeof(Vertex) == 2*sizeof(glm::vec3))`. | MUST |
| FR-02 | `Mesh` holds `VAO`, `VBO<Vertex>`, `EBO` as move-only value members; `Mesh` is non-copyable and move-only (default move). GL handles transfer into `Mesh` via `std::move` from `MeshManager::create_mesh`. | MUST |
| FR-03 | `Mesh` exposes `get_id()`, `get_name()`, `get_index_count()` (returns `std::size_t`) and `bind()` which binds the VAO. `Mesh` does NOT expose the VAO/VBO/EBO themselves. | MUST |
| FR-04 | `MeshManager` provides `load_mesh(filename, optional<name> = nullopt)` → returns int ID. Default name is the filename stem (no directory, no extension). | MUST |
| FR-05 | `MeshManager::create_mesh(name, vertices, indices)` takes raw data; configures `VAO` attribute pointers for position (loc 0) and normal (loc 1) exactly once per mesh. | MUST |
| FR-06 | `MeshManager` stores owned meshes in `unordered_map<int, Mesh>`; also maintains `unordered_map<string,int>` and `unordered_map<int,string>` for bidirectional lookup. | MUST |
| FR-07 | Duplicate name in `load_mesh` or `create_mesh` throws `std::runtime_error` (no silent overwrite). | MUST |
| FR-08 | `load_mesh` dispatches by extension: `.obj` → tinyobjloader, `.ply` → tinyply. Unknown extension throws. | MUST |
| FR-09 | `Scene` holds `std::vector<Mesh*>`; three constructors: default, single-mesh (`Mesh&` → stores `&mesh`), vector (`const vector<Mesh*>&`). `add_mesh(Mesh*)` asserts non-null in debug. | MUST |
| FR-10 | `Scene` exposes const `begin()` / `end()` / `size()` for range-based iteration. | MUST |
| FR-11 | `MeshManager` and `Scene` are non-copyable (copy-deleted). | MUST |
| FR-12 | `get_by_id` / `get_by_name` throw `std::out_of_range` on miss. | MUST |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | `load_mesh` dominated by disk I/O and parsing. No per-frame heap allocation triggered by this layer. |
| Memory | Single `Mesh` value per mesh in `meshes_`; two small string/int indices. |
| Compatibility | C++17, OpenGL 4.5 core, GLM 1.0.3. |
| Code Style | See CONSTRAINTS.md §3. |

---

## 3. Design

### 3.1 High-Level Approach

Three-tier ownership model:

1. **`Vertex`** is data.
2. **`Mesh`** owns its GL objects via value-member RAII.
3. **`MeshManager`** owns all meshes by value in a map.
4. **`Scene`** is a view — non-owning pointers.

`load_mesh` is implemented *in terms of* `create_mesh`: file parsing
produces `vector<Vertex>` + `vector<GLuint>`, which is then fed into the
single GL-object-construction path inside `create_mesh`. This is the
single most important factoring in this SDD: the `Vertex` attribute
layout is expressed exactly once.

**Rejected alternatives**:
- *Scene owns meshes* — rejected per GD-01 (CONSTRAINTS.md).
- *Hand-rolled `.ply` parser* — rejected per GD-03.
- *`create_mesh` takes pre-built `VAO/VBO/EBO`* — rejected because it
  leaks the attribute layout to every call site; see §3.3.3.

### 3.2 Architecture / Data Flow

```
              load_mesh(filename, name?)
                       │
                       ▼
            ┌─────────────────────────┐
            │  Dispatch by extension  │
            └────────┬────────────────┘
                     │
           ┌─────────┴─────────┐
           ▼                   ▼
    LoadObjFromFile      LoadPlyFromFile
    (tinyobjloader)         (tinyply)
           │                   │
           └─────────┬─────────┘
                     ▼
              vector<Vertex>
              vector<GLuint>
                     │
                     ▼
            create_mesh(name, verts, idx)  ← SINGLE CONSTRUCTION PATH
                     │
                     ▼
      ┌──────────────────────────────┐
      │ 1. VAO vao;                  │ (glGenVertexArrays)
      │ 2. VBO<Vertex> vbo(verts);   │ (glGenBuffers + upload)
      │ 3. EBO ebo(idx);             │ (glGenBuffers + upload)
      │ 4. vao.bind();               │
      │      vbo.bind();             │
      │      glVertexAttribPointer(0, 3, FLOAT, ..., offsetof(position)); │
      │      glVertexAttribPointer(1, 3, FLOAT, ..., offsetof(normal));   │
      │      ebo.bind();             │
      │    vao.unbind();             │
      │ 5. std::move everything into Mesh │
      │ 6. meshes_.emplace(id, Mesh{...}) │
      │ 7. ids_[name]=id; names_[id]=name │
      └──────────────────────────────┘
                     │
                     ▼
               Return int id

Scene uses: non-owning Mesh*, never touches GL, never destructs meshes.
```

### 3.3 Key Interfaces / Implementations

#### 3.3.1 `Vertex`

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

Attribute layout used by `MeshManager::create_mesh`:

| Location | Attribute | Size | Offset |
|----------|-----------|------|--------|
| 0 | `position` | 3 × GL_FLOAT | `offsetof(Vertex, position)` |
| 1 | `normal`   | 3 × GL_FLOAT | `offsetof(Vertex, normal)` |

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

    // std::size_t (not int) — a 3DGS .ply can exceed INT_MAX elements.
    // Cast to GLsizei at the glDrawElements call site in the Renderer.
    std::size_t        get_index_count() const noexcept { return index_count_; }

    // Binds the VAO. Renderer uses this; VAO itself is never handed out.
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
    // If `name` is nullopt (default), derived from filename stem.
    // Returns the ID of the newly created mesh.
    // Throws std::runtime_error on load failure, unsupported extension,
    // or duplicate name.
    int load_mesh(const std::string& filename,
                  std::optional<std::string> name = std::nullopt);

    // Create from in-memory vertex/index data. MeshManager owns ALL
    // GL-object construction — callers never touch VAO/VBO/EBO.
    // Returns the ID of the newly created mesh.
    // Throws std::runtime_error on duplicate name or empty input.
    int create_mesh(const std::string& name,
                    const std::vector<Vertex>& vertices,
                    const std::vector<GLuint>& indices);

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

**`create_mesh` implementation** (the one place VAO setup lives):

```cpp
int MeshManager::create_mesh(const std::string& name,
                             const std::vector<Vertex>& vertices,
                             const std::vector<GLuint>& indices) {
    if (vertices.empty()) {
        throw std::runtime_error("create_mesh: vertices empty");
    }
    if (ids_.count(name) != 0) {
        throw std::runtime_error("create_mesh: duplicate name '" + name + "'");
    }

    VAO vao;
    VBO<Vertex> vbo(vertices, GL_STATIC_DRAW);
    EBO ebo(indices, GL_STATIC_DRAW);

    vao.bind();
      vbo.bind();
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                            sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, position)));
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                            sizeof(Vertex),
                            reinterpret_cast<void*>(offsetof(Vertex, normal)));
      ebo.bind();
    vao.unbind();

    const int id = next_id_++;
    meshes_.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(id),
        std::forward_as_tuple(id, name,
                              std::move(vao),
                              std::move(vbo),
                              std::move(ebo),
                              indices.size()));
    ids_[name] = id;
    names_[id] = name;
    return id;
}
```

**`load_mesh` implementation** (thin wrapper around `create_mesh`):

```cpp
int MeshManager::load_mesh(const std::string& filename,
                           std::optional<std::string> name) {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;

    const std::string ext = ExtensionOfPath(filename);  // anonymous ns helper
    if (ext == ".obj") {
        LoadObjFromFile(filename, vertices, indices);
    } else if (ext == ".ply") {
        LoadPlyFromFile(filename, vertices, indices);
    } else {
        throw std::runtime_error("load_mesh: unsupported extension '" + ext + "'");
    }

    std::string resolved = name.value_or(StemOfPath(filename));
    return create_mesh(resolved, vertices, indices);
}
```

Anonymous-namespace loader helpers sit in `MeshManager.cpp`:

```cpp
namespace {
    void LoadObjFromFile(const std::string& path,
                         std::vector<Vertex>& out_vertices,
                         std::vector<GLuint>& out_indices);
    void LoadPlyFromFile(const std::string& path,
                         std::vector<Vertex>& out_vertices,
                         std::vector<GLuint>& out_indices);
    std::string ExtensionOfPath(const std::string& path);
    std::string StemOfPath(const std::string& path);
}
```

Implementation hints:
- Use `std::filesystem::path(path).extension().string()` for
  `ExtensionOfPath`, then lowercase.
- Use `std::filesystem::path(path).stem().string()` for `StemOfPath`.
- For `tinyobjloader`: enable triangulation, extract positions + normals;
  if the `.obj` has no normals, compute per-vertex averaged normals as a
  fallback (document limitation).
- For `tinyply`: request `vertex[x,y,z]` and `vertex[nx,ny,nz]` properties,
  plus `face[vertex_indices]` or `face[vertex_index]`. Support both ascii
  and binary_little_endian.

#### 3.3.4 `Scene`

**File**: `core/scene/Scene.h` / `Scene.cpp`

```cpp
class Scene {
public:
    Scene() = default;
    explicit Scene(Mesh& mesh);
    explicit Scene(const std::vector<Mesh*>& meshes);

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Non-owning. Mesh must outlive the Scene (invariant: MeshManager
    // outlives Scene — see CONSTRAINTS.md GD-01).
    void add_mesh(Mesh* mesh);

    std::vector<Mesh*>::const_iterator begin() const noexcept { return meshes_.begin(); }
    std::vector<Mesh*>::const_iterator end()   const noexcept { return meshes_.end();   }
    std::size_t size() const noexcept { return meshes_.size(); }

private:
    std::vector<Mesh*> meshes_;
};
```

`add_mesh` body:

```cpp
void Scene::add_mesh(Mesh* mesh) {
    assert(mesh != nullptr && "Scene::add_mesh does not accept nullptr");
    meshes_.push_back(mesh);
}
```

### 3.4 File Structure Changes

```
SPATIUM/
├── core/
│   └── scene/                       ← NEW DIRECTORY
│       ├── Vertex.h                 ← NEW
│       ├── Mesh.h                   ← NEW
│       ├── Mesh.cpp                 ← NEW
│       ├── MeshManager.h            ← NEW
│       ├── MeshManager.cpp          ← NEW
│       ├── Scene.h                  ← NEW
│       └── Scene.cpp                ← NEW
├── external/
│   ├── tinyobjloader/
│   │   └── tiny_obj_loader.h        ← NEW (vendored)
│   └── tinyply/
│       └── tinyply.h                ← NEW (vendored)
├── CMakeLists.txt                   ← MODIFY: append new .cpp files to CORE_SOURCES
└── dependencies.md                  ← MODIFY: document tinyply + tinyobjloader
```

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Create `Vertex.h` with struct + `static_assert`.
    - Completion: `#include "core/scene/Vertex.h"` compiles standalone.

- [ ] **Step 2**: Create `Mesh.h` + `Mesh.cpp`.
    - Completion: `Mesh` compiles; move-only enforced.

- [ ] **Step 3**: Vendor `tinyobjloader/tiny_obj_loader.h` and
  `tinyply/tinyply.h` under `external/`.
    - Completion: Headers in place; no build changes needed (already
      covered by existing `external/` include directory).

- [ ] **Step 4**: Create `MeshManager.h` with full class declaration.

- [ ] **Step 5**: Implement `create_mesh` in `MeshManager.cpp`. This is
  the single-point VAO setup.
    - Completion: A test call
      `create_mesh("cube", {...8 vertices...}, {...36 indices...})`
      returns id=0 and `get_by_name("cube").get_index_count() == 36`.

- [ ] **Step 6**: Implement anonymous-namespace helpers `ExtensionOfPath`,
  `StemOfPath`, `LoadObjFromFile`, `LoadPlyFromFile`.
    - Completion: Each helper tested in isolation with a small test file.

- [ ] **Step 7**: Implement `load_mesh` as a thin dispatch wrapper around
  `create_mesh`.
    - Completion: Loading `test.obj` and `test.ply` both succeed.

- [ ] **Step 8**: Implement `get_by_id`, `get_by_name`, `contains`.
    - Completion: Lookup tests pass; miss throws `std::out_of_range`.

- [ ] **Step 9**: Create `Scene.h` + `Scene.cpp`.

- [ ] **Step 10**: Update `CMakeLists.txt` — append
  `core/scene/Mesh.cpp`, `core/scene/MeshManager.cpp`,
  `core/scene/Scene.cpp` to `CORE_SOURCES`.

- [ ] **Step 11**: Update `dependencies.md` to list the two vendored
  libraries and their licenses.

- [ ] **Step 12**: Build + run verification tests.
    - Command: `cmake --build build`
    - Expected: Zero warnings at `-Wall -Wextra -Wpedantic`.

### 4.2 Configuration / Constants

No user-facing constants. Internal: `GL_STATIC_DRAW` is used for all
buffer uploads (meshes are assumed immutable after load for v1).

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | Procedural cube | `create_mesh("cube", 8_verts, 36_idx)` | id = 0, registered | `get_by_name("cube").get_index_count() == 36` |
| T-02 | Duplicate name | Two `create_mesh("x", ...)` calls | Second throws | `std::runtime_error` caught |
| T-03 | Empty vertices | `create_mesh("x", {}, {})` | Throws | `std::runtime_error` caught |
| T-04 | `.obj` load | `load_mesh("test.obj")` | id ≥ 0; name = "test" | `get_by_name("test").get_index_count() > 0` |
| T-05 | `.ply` ascii load | `load_mesh("cube.ply")` (ascii) | id ≥ 0 | loads without error |
| T-06 | `.ply` binary_little_endian load | `load_mesh("bunny.ply")` (binary) | id ≥ 0 | loads without error |
| T-07 | Unsupported extension | `load_mesh("foo.stl")` | Throws | `std::runtime_error` caught |
| T-08 | Explicit name param | `load_mesh("bunny.ply", "hero")` | name = "hero" (not "bunny") | `contains("hero") && !contains("bunny")` |
| T-09 | Bidirectional lookup | After load | id↔name consistent | `get_by_name(get_by_id(id).get_name()).get_id() == id` |
| T-10 | Missing lookup | `get_by_id(999)` | Throws | `std::out_of_range` caught |
| T-11 | `Mesh` move-only | `Mesh m2 = m1;` attempt | Compile error | static_assert via concept or compile-fail test |
| T-12 | `Scene` iteration | Scene with 3 meshes | Range-based for yields 3 | `for (Mesh* m : scene) count++;` → 3 |
| T-13 | `Vertex` packing | `sizeof(Vertex)` | 24 bytes | static_assert already in Vertex.h |

### 5.2 Validation Commands

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
# Unit tests (once a test harness exists):
./build/tests/test_scene_graph
```

### 5.3 Acceptance Criteria

- [ ] All tests T-01 through T-13 pass.
- [ ] FR-01 through FR-12 verified.
- [ ] `create_mesh` is the ONLY function in the project that calls
  `glVertexAttribPointer` for the `Vertex` layout.
- [ ] Clean build at `-Wall -Wextra -Wpedantic`.
- [ ] `dependencies.md` updated.
- [ ] §9 checklist walked through.

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ Global constraints from **CONSTRAINTS.md** apply in full. Especially:
    - **GD-01** — `Scene` MUST use non-owning `Mesh*`.
    - **GD-03** — only `tinyobjloader` and `tinyply` permitted for mesh loading.
- ❌ Do NOT expose the VAO/VBO/EBO from `Mesh`. Callers get `bind()` and
  `get_index_count()`; that's it.
- ❌ Do NOT call `glVertexAttribPointer` anywhere except inside
  `MeshManager::create_mesh`.
- ❌ Do NOT store `Mesh` inside `Scene` by value or as `shared_ptr`.
- ❌ Do NOT silently overwrite on duplicate name — throw.
- ❌ Do NOT derive the default name from the full filename (e.g.
  `"assets/bunny.ply"`); use the stem (`"bunny"`).
- ❌ Do NOT accept `Mesh(const Mesh&)` or `Mesh& operator=(const Mesh&)`
  — copy is deleted; move is default.

### 6.2 Known Pitfalls & Limitations

- ⚠️ **`create_mesh` must run on the GL context thread** — it calls
  `glGenVertexArrays` etc. If a future SDD introduces async asset loading,
  the file-parsing step can run on a worker thread but GL-object
  construction must be marshaled back to the main thread.
- ⚠️ **`offsetof` on `glm::vec3`** — `glm::vec3` is a standard-layout type,
  so `offsetof` is well-defined. `reinterpret_cast<void*>` is the idiomatic
  `glVertexAttribPointer` argument form; `-Wpedantic` sometimes flags this,
  use `(void*)offsetof(...)` as the standard C cast if pedantic warnings
  appear.
- ⚠️ **`.obj` files without normals** — many `.obj` files have no normals.
  The loader must either compute per-face flat normals or per-vertex
  averaged normals. For v1, compute per-vertex averaged (document as
  a v1 simplification).
- ⚠️ **`.ply` quad faces** — tinyply may return faces of variable size.
  For v1, only triangulated `.ply` is supported; reject quads explicitly.
- ⚠️ **GL error on construction** — `GL_INVALID_OPERATION` after
  `glVertexAttribPointer` usually means no VAO was bound. Verify
  `vao.bind()` happens first.
- ⚠️ **Map iterator invalidation** — `unordered_map::emplace` can
  invalidate iterators but not references. The current code takes no
  iterator across a mutation, so this is fine; keep it that way.

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| Triangle-only `.ply` | Quad/NGon faces rejected. | Low | Add a triangulation helper when needed. |
| Positions + normals only | No UVs, colors, materials. | Low | Extend `Vertex` and loader helpers when texturing arrives. |
| `.ply` big-endian unsupported | Only ascii + binary_little_endian. | Low | Extend helper when a big-endian asset is encountered. |
| No mesh unload | Once loaded, a mesh lives until `MeshManager` destruction. | Medium | Add `remove(int id)` / `remove(const string& name)` when asset hot-reload is needed. |
| `load_mesh` blocks on I/O | Synchronous disk read. | Low | Async loading belongs to a future SDD with its own threading plan. |

---

## 7. Open Questions & Decision Log

### Open Questions

*(None — SDD ready for implementation.)*

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | `Mesh::get_index_count()` returns `std::size_t`, not `int`. | 3DGS `.ply` can exceed `INT_MAX` elements. `std::size_t` is the idiomatic container size type and casts cleanly to `GLsizei` at the call site. | 2026-04-17 |
| D2 | `MeshManager` exposes two construction paths: `load_mesh` and `create_mesh`. `create_mesh` takes raw `vector<Vertex>` + `vector<GLuint>`, NOT pre-built `VAO/VBO/EBO`. | Keeps the `Vertex` attribute layout as a single-point invariant inside `create_mesh`. `load_mesh` is implemented in terms of `create_mesh`, so the VAO setup code exists in exactly one place. Procedural construction is needed for rendering bring-up (unit cube test in SDD_04) — depending on a disk-backed asset would couple two stages' bring-up. | 2026-04-17 |
| D3 | Default name for `load_mesh` = filename stem (not full path, not basename with extension). | Matches user mental model ("load bunny.ply" → "bunny"). `std::filesystem::path::stem()` provides it in one line. | 2026-04-17 |
| D4 | Duplicate name throws, does not overwrite. | Silent overwrite would leave `Scene` instances pointing at the wrong mesh. Explicit remove-then-reload semantics are safer and can be added later if needed. | 2026-04-17 |
| D5 | `Mesh::bind()` is the only entry point for VAO binding from outside `MeshManager`. | Hides GL details from consumers. Renderer binds via `mesh.bind()`, never touches the underlying VAO. | 2026-04-17 |

---

## 8. Progress Tracker

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | 2026-04-17 | SDD drafted, marked READY | All implementation steps pending |

### Current Blockers

*(None)*

---

## 9. Implementation Verification Checklist

### `Vertex`
- [ ] `struct Vertex { glm::vec3 position; glm::vec3 normal; };`
- [ ] `static_assert(sizeof(Vertex) == 2*sizeof(glm::vec3))` present.
- [ ] No alignment attributes (default layout is tight for this struct).

### `Mesh`
- [ ] Copy constructor and copy assignment are deleted.
- [ ] Move constructor and move assignment are `= default` (noexcept).
- [ ] `VAO`, `VBO<Vertex>`, `EBO` are value members (not pointers, not references).
- [ ] `index_count_` is `std::size_t`, not `int`.
- [ ] `bind()` is the only public VAO-touching method.
- [ ] Destructor is defaulted (RAII cleanup via member destructors).

### `MeshManager`
- [ ] Non-copyable.
- [ ] `create_mesh` is the ONLY function calling `glVertexAttribPointer` for the Vertex layout.
- [ ] `create_mesh` throws on empty `vertices`.
- [ ] `create_mesh` throws on duplicate name (checks `ids_.count(name) != 0`).
- [ ] `load_mesh` default name = stem of filename (not full path).
- [ ] `load_mesh` dispatches by extension; unsupported extension throws.
- [ ] `meshes_`, `ids_`, `names_` updated atomically inside `create_mesh` (three map operations, no early return between them after the first insert).
- [ ] `get_by_id` / `get_by_name` throw `std::out_of_range` on miss.
- [ ] `contains` does NOT throw.
- [ ] No iterator held across `emplace` or `insert`.

### Loaders (anonymous namespace in `MeshManager.cpp`)
- [ ] `LoadObjFromFile` uses tinyobjloader's triangulation flag.
- [ ] `LoadObjFromFile` computes per-vertex averaged normals when the `.obj` has none.
- [ ] `LoadPlyFromFile` handles ascii AND binary_little_endian.
- [ ] `LoadPlyFromFile` rejects (throws on) quad or NGon faces.
- [ ] `ExtensionOfPath` lowercases before comparison.

### `Scene`
- [ ] Non-copyable.
- [ ] Holds `std::vector<Mesh*>` (raw non-owning).
- [ ] `add_mesh(nullptr)` triggers debug assert.
- [ ] `begin()` / `end()` / `size()` are `const noexcept`.
- [ ] Three constructors: default, single-mesh (`Mesh&`), vector (`const vector<Mesh*>&`).

### Build
- [ ] Three new `.cpp` files in `CORE_SOURCES`.
- [ ] `external/tinyobjloader/` and `external/tinyply/` present; picked up by existing `${CMAKE_SOURCE_DIR}/external` include dir.
- [ ] `dependencies.md` updated with both libraries + licenses.
- [ ] `-Wall -Wextra -Wpedantic` clean.

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- [CONSTRAINTS.md](./CONSTRAINTS.md) — GD-01, GD-03 apply.
- [SDD_04_Renderer.md](./SDD_04_Renderer.md) — consumer of `Mesh` / `Scene`.
- [SDD_05_AppIntegration.md](./SDD_05_AppIntegration.md) — composes
  `MeshManager` and `Scene` into `App`.

### 10.2 Scratch Pad / Notes

- Unit-cube vertex/index data for T-01 should be kept in a test helper
  so both SDD_02 tests and SDD_04's T-13 (visual render test) can share it.
- If mesh transforms are added in a follow-up SDD, `Mesh` gains a
  `glm::mat4 transform_` member + `get_transform()` getter; `Renderer`
  reads via the getter inside `upload_mesh_uniforms_`. No other change.

---

## Outcome & Decisions

### Final Technical Choices

| Choice | Detail |
|--------|--------|
| `VAO` constructor binds immediately | `VAO::VAO()` calls `glGenVertexArrays` + `glBindVertexArray`. `EBO` constructor's `glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ...)` therefore fires while VAO is active, recording the EBO in the VAO automatically. The explicit `ebo.bind()` inside the setup block is redundant but kept for clarity. |
| `EBO` constructor signature | `explicit EBO(const std::vector<unsigned int>& v)` — no `usageMode` parameter; `GL_STATIC_DRAW` is hardcoded internally. `std::vector<GLuint>` is accepted because `GLuint == unsigned int` on this platform. |
| Non-deduplicated OBJ vertices | One `Vertex` per face-corner, sequential indices. Avoids hash-map complexity; correct for GL rendering. Deduplication can be added later if memory becomes a concern. |
| `glm/gtx/norm.hpp` avoided | That header requires `GLM_ENABLE_EXPERIMENTAL`. Used `glm/geometric.hpp` (core) for `glm::length` and `glm::cross` instead. |
| tinyply single-header mode | `TINYPLY_IMPLEMENTATION` defined in `MeshManager.cpp` before including `tinyply/tinyply.h`. `external/tinyply/tinyply.cpp` is present but NOT added to the build — it is just the CMake library shim. |
| PLY face buffer format | tinyply two-pass mode (`list_size_hint=0`): buffer contains concatenated index values without per-face count bytes. Triangle check: `buffer.size_bytes() / idx_stride / faces->count == 3`. |
| `(void*)offsetof(...)` cast | Used C-style cast for `glVertexAttribPointer` offset arguments — avoids `-Wpedantic` warning from `reinterpret_cast<void*>` on a non-null integer. |

### Rejected Alternatives

- *EBO with usageMode param*: EBO's frozen API has no such parameter. `GL_STATIC_DRAW` is fine for v1.
- *Deduplicated vertex map in OBJ loader*: adds hash-map complexity for no measurable quality benefit at this stage.
- *Storing VAO bind state manually before setup block*: unnecessary because `VAO::VAO()` always binds on construction.

### Notes for Future SDDs

- **SDD_04 (Renderer)**: call `mesh.bind()` to bind the VAO, then `glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.get_index_count()), GL_UNSIGNED_INT, nullptr)`. The `GLsizei` cast is required because `get_index_count()` returns `std::size_t`.
- **SDD_05 (AppIntegration)**: declare `MeshManager mesh_manager_` **before** `Scene scene_` in `App`'s member list — destruction runs in reverse declaration order, ensuring `Scene` (non-owning) is destroyed before `MeshManager` (owning). This is GD-01.
- **`App.cpp` line 59** already has a `glVertexAttribPointer` call for its own hardcoded 5-float vertex layout. This is unrelated to `Vertex` and pre-dates this SDD. It remains in place.

*End of SDD_02*