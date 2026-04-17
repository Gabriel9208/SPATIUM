<!-- Generated: 2026-04-16 | SPATIUM Codemaps Index -->

# SPATIUM Codemaps Index

**Project**: SPATIUM — Real-time 3D Gaussian Splatting Viewer (C++ + OpenGL + CUDA)  
**Version**: Pre-alpha (Core subsystems in place, rendering pipeline in progress)  
**Last Updated**: 2026-04-16  
**Location**: `/home/gabriel/Projects/SPATIUM/docs/CODEMAPS/`

---

## Quick Navigation

| Codemap | Focus | Audience | Key Sections |
|---------|-------|----------|--------------|
| **architecture.md** | System design, module boundaries, init order | Architects, new contributors | Diagram, module hierarchy, data flow, integration points |
| **backend.md** | Class hierarchies, subsystem details, APIs | Developers, API consumers | Class definitions, methods, ownership, lifetime, thread model |
| **data.md** | Buffer layouts, event payloads, memory | Graphics programmers, debuggers | GL object types, uniforms, RAII patterns, shader attributes |
| **dependencies.md** | External libraries, build config, CMake | Build engineers, maintainers | Library versions, CMake targets, platform notes, future CUDA |

---

## architecture.md

**Covers**: System-level design, initialization pipeline, module dependencies

**Start Here If You're**:
- New to the codebase
- Designing a new subsystem
- Understanding initialization order
- Tracking data flow through systems

**Key Diagrams**:
```
main() → App → {GlfwContext, EventBus, Window, GL Context}
    ↓
Event Flow: User Input → Window → EventBus → Subscribers
    ↓
Render Loop: Poll Events → Clear → Render → Swap
```

**Key Files**:
- `main.cpp` (6 LOC entry point)
- `app/App.h`, `app/App.cpp` (80 LOC, top-level lifecycle)
- `core/window/Window.h`, `.cpp` (window management)
- `core/event/` (event bus pub/sub)

**Module Map**:
| Layer | Module | Purpose |
|-------|--------|---------|
| Entry | `main()` | Bootstrap |
| Application | `App` | Lifecycle, composition |
| Context | `GlfwContext` | GLFW init/term |
| Window | `Window` | GLFW window + callbacks |
| Events | `EventBus`, `EventID` | Event routing |
| GL Debug | `Debug` | Error callbacks |
| Rendering | VAO/VBO/EBO + Shaders | *(In progress)* |

---

## backend.md

**Covers**: Class definitions, method signatures, ownership semantics, thread model

**Start Here If You're**:
- Implementing a new GL object type
- Debugging class relationships
- Understanding RAII patterns
- Adding a new subsystem

**Class Hierarchy**:
```
IGPUResource (interface)
├── VAO
├── StorageBuffer<T> (abstract)
│   ├── VBO<T>
│   ├── EBO
│   ├── UBO
│   └── SSBO<T>
└── (Future: FBO, TextureBuffer)
```

**Key Classes & Methods**:

| Class | File | Key Methods |
|-------|------|------------|
| `Window` | `core/window/Window.h` | `should_close()`, `swap_buffers()`, `set_resize_callback()` |
| `EventBus` | `core/event/EventBus.h` | `subscribe<T>()`, `emit<T>()` |
| `VAO` | `core/gl/obj/VAO.h` | `bind()`, `unbind()` |
| `VBO<T>` | `core/gl/obj/VBO.h` | `initialize()`, `set_data()`, `bind()` |
| `ShaderProgram` | `core/gl/shader/ShaderProgram.h` | `load()`, `use()` |
| `GraphicShader` | `core/gl/shader/GraphicShader.h` | Vertex + fragment pipeline |
| `App` | `app/App.h` | `App()`, `run()` |

**Ownership & Lifetime**:
- `App` owns `EventBus`, `Window` (RAII order critical)
- `Window` owns `GLFWwindow*` handle
- `VAO`, `VBO` own GL handles (cleanup in destructor)
- Non-copyable by design (deleted copy constructors)

---

## data.md

**Covers**: Buffer object types, memory layouts, shader uniforms, event payloads

**Start Here If You're**:
- Writing vertex data for rendering
- Designing a shader interface
- Debugging buffer mismatches
- Understanding GPU memory layout

**GL Buffer Types**:

| Type | GL Target | Purpose | Ownership |
|------|-----------|---------|-----------|
| `VAO` | GL_VERTEX_ARRAY | Vertex attribute configuration | Binds VBO/EBO |
| `VBO<T>` | GL_ARRAY_BUFFER | Vertex data (positions, UVs, normals) | T-templated |
| `EBO` | GL_ELEMENT_ARRAY_BUFFER | Index data for `glDrawElements` | GLuint[] |
| `UBO` | GL_UNIFORM_BUFFER | Uniform matrices, light params | Binding point 0–15 |
| `SSBO<T>` | GL_SHADER_STORAGE_BUFFER | GPU read/write data | Compute shaders |

**Vertex Attribute Layout**:
```glsl
#version 450 core
layout(location = 0) in vec4 position;  // Shader expects vec4 at location 0

// C++ side:
std::vector<float> vertices = { -0.5f, -0.5f, 0.0f, 1.0f, ... };
glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, (void*)0);
```

**Memory Layout Example** (Interleaved):
```
Float[x0, y0, z0, u0, v0,  x1, y1, z1, u1, v1,  ...]
Stride = 20 bytes
Position @ offset 0, size 12 bytes
TexCoord @ offset 12, size 8 bytes
```

**Event Payloads**:
```cpp
struct WindowResizeEvent {
    int width;
    int height;
};
// More events to be added (KeyPress, MouseMove, Scroll, etc.)
```

---

## dependencies.md

**Covers**: External libraries, build configuration, CMake targets, platform-specific setup

**Start Here If You're**:
- Setting up the build environment
- Adding a new dependency
- Cross-compiling to a new platform
- Integrating CUDA

**Direct Dependencies**:

| Library | CMake Target | Version | Purpose |
|---------|--------------|---------|---------|
| OpenGL | `OpenGL::GL` | 4.5 core | GPU API, rendering |
| GLFW | `glfw` | 3.0+ | Window, input, GL context |
| GLM | `glm::glm` | 1.0.3 (vendored) | Linear algebra, matrices |
| GLAD | *(compiled)* | Latest | GL function loader |

**CMake Target Graph**:
```
SPATIUM (executable)
├─ OpenGL::GL           [System OpenGL library]
├─ glfw                 [GLFW3 window library]
├─ glm::glm             [GLM header-only]
└─ core/glad.c          [GL loader, compiled]
```

**Build Commands**:
```bash
# Debug
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
./build/SPATIUM

# Release
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/SPATIUM
```

**Platform Setup**:
- **Linux**: `apt install libglfw3-dev libglm-dev`
- **Windows**: `vcpkg install glfw3:x64-windows glm:x64-windows`
- **macOS**: `brew install glfw3 glm`

**Future CUDA Integration**:
```cmake
find_package(CUDA REQUIRED)
target_link_libraries(SPATIUM PRIVATE CUDA::cudart)
```

---

## Cross-References

### By Task

**I want to...**

| Task | Start With | Then Read |
|------|-----------|-----------|
| Build the project | `dependencies.md` | Platform section |
| Add a new GL buffer type | `backend.md` | Class hierarchy section |
| Implement a shader | `data.md` | Vertex attribute layout |
| Debug a resize event | `architecture.md` | Event flow diagram |
| Integrate CUDA | `dependencies.md` | Future CUDA section |
| Understand class ownership | `backend.md` | Lifetime & initialization order |
| Design a new event | `data.md` | Event payloads section |

### By File

**Need docs for a specific file?**

| File | Primary Codemap | Section |
|------|-----------------|---------|
| `main.cpp` | `architecture.md` | Quick navigation table |
| `app/App.h` | `backend.md` | Subsystem 6: Application |
| `core/window/Window.h` | `backend.md` | Subsystem 1: Window |
| `core/event/EventBus.h` | `backend.md` | Subsystem 2: Event System |
| `core/gl/obj/VAO.h` | `data.md` | VAO type definition |
| `core/gl/obj/VBO.h` | `data.md` | VBO<T> template |
| `core/gl/shader/ShaderProgram.h` | `backend.md` | Subsystem 4: Shaders |
| `CMakeLists.txt` | `dependencies.md` | CMakeLists.txt breakdown |

---

## Key Concepts

### RAII (Resource Acquisition Is Initialization)

All GL objects (VAO, VBO, etc.) follow RAII:
- Constructor: `glGenBuffers`, `glGenVertexArrays`, etc.
- Destructor: `glDeleteBuffers`, `glDeleteVertexArrays`, etc.
- No manual cleanup needed; scope exit = automatic cleanup

**See**: `backend.md` → Class definitions, or `data.md` → RAII & Resource Ownership

### Event-Driven Architecture

Window events flow through a pub/sub event bus:
```
Window Input → EventBus → Subscribers (cameras, UI, etc.)
```

**See**: `architecture.md` → Event Flow diagram, or `backend.md` → Subsystem 2

### Template-Based GL Abstraction

`VBO<T>`, `SSBO<T>` use templates to support any vertex format:
```cpp
VBO<float> positions;
VBO<glm::vec3> normals;
VBO<MyVertex> data;
```

**See**: `data.md` → VBO<T> Template, or `backend.md` → Concrete Types

### GL Context Thread Affinity

All GL calls must happen on the thread that created the context (tied to GLFW window).

**See**: `architecture.md` → Thread Safety & Ownership, or `backend.md` → Thread Model

---

## Metrics

| Metric | Value | Status |
|--------|-------|--------|
| Core source files | 14 `.cpp` + `glad.c` | ✓ Compiled |
| Header-only files | 6 `.h` | ✓ Included |
| Lines of code (core) | ~800 | Early stage |
| Classes defined | 15+ | Core subsystems |
| Test coverage | 0% | *(Planned)* |
| Documentation | 4 codemaps | ✓ Generated |

---

## Glossary

| Term | Definition |
|------|-----------|
| **GLFW** | Graphics Library Framework — window creation, input handling |
| **GL** | OpenGL — GPU graphics API |
| **VAO** | Vertex Array Object — GL state container for vertex attributes |
| **VBO** | Vertex Buffer Object — GPU memory for vertex data |
| **EBO** | Element Buffer Object — GPU memory for indices (glDrawElements) |
| **UBO** | Uniform Buffer Object — GPU memory for shader uniforms |
| **SSBO** | Shader Storage Buffer Object — GPU read/write buffer |
| **FBO** | Framebuffer Object — render target (color, depth, stencil) |
| **EventBus** | Pub/sub message dispatcher (observer pattern) |
| **RAII** | Resource Acquisition Is Initialization — scope-based resource management |
| **CUDA** | Compute Unified Device Architecture — GPU compute framework |
| **3DGS** | 3D Gaussian Splatting — rendering technique |
| **Interop** | CUDA-OpenGL interoperability — shared GPU memory |

---

## Files in This Directory

```
docs/CODEMAPS/
├── INDEX.md           ← You are here
├── architecture.md    [System design, module boundaries, data flow]
├── backend.md         [Class hierarchies, subsystem details]
├── data.md            [Buffer layouts, shader uniforms, memory]
└── dependencies.md    [External libs, CMake, build config]
```

---

## Next Steps

1. **New contributor?** Start with `architecture.md` for the big picture.
2. **Implementing a feature?** Check `backend.md` for class signatures and `data.md` for memory layouts.
3. **Setting up the build?** Go to `dependencies.md` for your platform.
4. **Debugging?** Use cross-references above to find the relevant codemap section.

---

**Generated**: 2026-04-16  
**Codebase**: SPATIUM C++ OpenGL Graphics Engine  
**Status**: Early-stage — core subsystems documented, rendering pipeline in progress
