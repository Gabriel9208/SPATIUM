<!-- Generated: 2026-04-16 | SPATIUM C++ OpenGL Graphics Engine -->

# Architecture Codemap

**Project**: SPATIUM (3D Gaussian Splatting Viewer in C++ + CUDA)  
**Language**: C++17  
**Build System**: CMake  
**Last Updated**: 2026-04-16

## System Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                        main() Entry Point                    │
│                   (main.cpp, 6 LOC)                          │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
        ┌────────────────────────────────┐
        │  App (app/App.h, app/App.cpp)  │
        │  • Owns: EventBus, Window      │
        │  • Initializes GL context      │
        │  • Main render loop            │
        └────────┬───────────────────────┘
                 │
    ┌────────────┼──────────────┬─────────────────┐
    │            │              │                 │
    ▼            ▼              ▼                 ▼
┌────────┐  ┌───────────┐  ┌──────────┐  ┌─────────────┐
│GlfwCtx │  │ EventBus  │  │ Window   │  │ GL Context  │
│        │  │ (events/) │  │(core/win)│  │  (glad)     │
└────────┘  └───────────┘  └──────────┘  └─────────────┘
                 │              │              │
                 │ emits        │ signals      │ renders
                 │ events       │ resize       │
    ┌────────────┴──────────────┴──────────────┴──────┐
    │                                                  │
    ▼                                                  ▼
┌──────────────────────────┐          ┌──────────────────────┐
│   GL Object Layer        │          │   Shader System      │
│   (core/gl/obj/)         │          │   (core/gl/shader/)  │
│ • VAO, VBO, EBO, FBO     │          │ • ShaderProgram      │
│ • UBO, SSBO              │          │ • GraphicShader      │
│ • StorageBuffer, etc     │          │ • ComputeShader      │
└──────────────────────────┘          └──────────────────────┘
         │ Implements IGPUResource              │
         │                                      │
         └──────────────┬───────────────────────┘
                        │
                        ▼
            ┌───────────────────────────┐
            │  OpenGL API (glad)        │
            │  • GL buffer objects      │
            │  • Shader compilation     │
            │  • Render commands        │
            └───────────────────────────┘
```

## Module Hierarchy

| Layer | Module | Purpose | Files |
|-------|--------|---------|-------|
| **Entry** | `main()` | App bootstrap | `main.cpp` |
| **Application** | `App` | Top-level app lifecycle | `app/App.h`, `app/App.cpp` |
| **Context** | `GlfwContext` | GLFW initialization wrapper | `app/GlfwContext.h` |
| **Window** | `Window` | GLFW window management & events | `core/window/Window.h`, `.cpp` |
| **Events** | `EventBus`, `EventID` | Pub/sub event routing | `core/event/EventBus.h`, `EventID.h`, `events.h` |
| **GL Debug** | `Debug` | OpenGL error callback setup | `core/gl/debug/Debug.h`, `.cpp` |
| **GPU Buffers** | `VAO`, `VBO`, `EBO`, `UBO`, `FBO`, `SSBO` | OpenGL buffer objects | `core/gl/obj/*.h`, `.cpp` |
| **GPU Interface** | `IGPUResource` | Buffer abstraction | `core/gl/interfaces/IGPUResource.h` |
| **Shaders** | `ShaderProgram`, `GraphicShader`, `ComputeShader` | Shader compilation & usage | `core/gl/shader/*.h`, `.cpp` |
| **Rendering** | *(Future)* | Scene, mesh, camera systems | *(Planned)* |

## Data Flow

### Initialization (App::App → App::run)

```
GlfwContext::GlfwContext()
    ↓ GLFW initialized
GlfwContext + EventBus + Window()
    ↓ EventBus wired to Window resize signals
Debug::enable()
    ↓ GL debug callbacks registered
glEnable(GL_DEPTH_TEST, GL_CULL_FACE)
    ↓ GL state initialized
Ready for render loop
```

### Event Flow (Resize Example)

```
User resizes window
    ↓
GLFW calls glfw_resize_callback (static)
    ↓
Window::on_resize(w, h)
    ↓ calls user-provided callback
App's resize_callback (lambda)
    ↓
EventBus::emit(WindowResizeEvent{w, h})
    ↓
App's subscriber (if registered) processes event
    ↓
Example: update camera aspect ratio, UI layout, etc.
```

### Render Loop (App::run)

```
while (!window_.should_close())
    ├─ glfwPollEvents()              [Process GLFW events → App callbacks]
    ├─ glClear(COLOR | DEPTH)        [Clear framebuffer]
    ├─ [Future: render scene]
    ├─ [Future: compute shaders run]
    └─ window_.swap_buffers()        [Present to screen]
```

## Thread Safety & Ownership

| Component | Thread-Safe | Owner | Lifetime |
|-----------|------------|-------|----------|
| `App` | No (main thread only) | Stack (main) | Program lifetime |
| `Window` | No (GL context tied to thread) | `App` | App lifetime |
| `EventBus` | No (template magic, not sync) | `App` (declared first) | App lifetime |
| GL buffers (VAO, VBO, etc) | No (GL state not thread-safe) | App/user | Lexical scope or manual cleanup |
| `GlfwContext` | ✗ (singleton pattern in destructor) | Implicit (static) | GLFW session |

**Key**: All rendering must happen on the single GLFW context thread.

## Dependency Graph

```
main.cpp
  └─ App.h/cpp
      ├─ GlfwContext.h
      ├─ EventBus.h
      │   └─ EventID.h
      ├─ Window.h
      │   └─ GLFW/glfw3.h
      ├─ events.h
      └─ Debug.h
          └─ glad/glad.h

core/gl/obj/VAO.h
  └─ IGPUResource.h
     └─ glad/glad.h

core/gl/obj/VBO.h
  ├─ StorageBuffer.h
  │   └─ BufferObject.h
  │       └─ IGPUResource.h
  └─ glad/glad.h

core/gl/shader/GraphicShader.h
  ├─ ShaderProgram.h
  │   └─ glad/glad.h
  └─ glad/glad.h
```

## Key Integration Points

1. **App ↔ EventBus**: App owns EventBus, passes it to systems that emit events
2. **Window ↔ EventBus**: Window signals resize; App's lambda routes to EventBus
3. **GL Context ↔ Window**: Window holds GLFWwindow* handle for all GL calls
4. **Debug ↔ GL Context**: Debug callbacks fire on any GL error (when enabled)
5. **VBO/VAO ↔ App**: Managed as local variables in render loop (planned)

## External Dependencies

| Library | Version | Purpose | CMake Target |
|---------|---------|---------|--------------|
| **OpenGL** | System | GPU graphics API | `OpenGL::GL` |
| **GLFW3** | System | Window & input | `glfw` |
| **GLM** | 1.0.3 (vendored) | Linear algebra | `glm::glm` |
| **GLAD** | Vendored (`include/glad/`) | GL loader | *(linked via glad.c)* |

## Compiler Requirements

- **C++**: 17 (minimum, via `set(CMAKE_CXX_STANDARD 17)`)
- **CMake**: 3.15+
- **Toolchain**: GCC/Clang (Linux), MSVC (Windows), AppleClang (macOS)

## Build Artifacts

```
CMakeLists.txt
├─ SPATIUM (executable)
│  ├─ CORE_SOURCES
│  │  ├─ core/glad.c
│  │  ├─ core/gl/obj/*.cpp (EBO, FBO, UBO, VAO)
│  │  ├─ core/gl/shader/*.cpp (ShaderProgram, GraphicShader, ComputeShader)
│  │  ├─ core/window/Window.cpp
│  │  ├─ app/App.cpp
│  │  └─ core/gl/debug/Debug.cpp
│  └─ main.cpp
└─ target_include_directories
   ├─ ${CMAKE_SOURCE_DIR}
   ├─ ${CMAKE_SOURCE_DIR}/include (GLAD headers)
   └─ ${CMAKE_SOURCE_DIR}/external (GLM vendored)
```

## Future Expansion Points

1. **Scene Management**: Scene class to hold meshes, cameras, lights
2. **Render Pass System**: Decouple forward rendering into multiple passes
3. **CUDA Integration**: 3DGS rasterization kernels + CUDA-GL interop
4. **ImGui Integration**: UI overlay for parameter tweaking
5. **Compute Shaders**: Post-processing, tile-based blending
6. **Threading**: Async resource loading (textures, models, splat data)

---

**Note**: This project targets a single-threaded render pipeline with future CUDA integration. All GL calls must remain on the GLFW context thread.
