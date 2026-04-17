<!-- Generated: 2026-04-16 | SPATIUM External Dependencies & Build Configuration -->

# Dependencies Codemap

**Focus**: External libraries, build configuration, CMake targets, version pinning  
**Last Updated**: 2026-04-16

## Direct Dependencies

### 1. OpenGL (Graphics API)

**CMake Target**: `OpenGL::GL`

| Property | Value |
|----------|-------|
| Type | System library (provides GPU rendering) |
| Minimum Version | GL 4.5 core (implied by shaders) |
| Platform Support | Linux (libGL), Windows (native), macOS (via Metal fallback) |
| Header | `#include <glad/glad.h>` (GLAD loader) |
| Usage | All GPU calls, buffer objects, shaders |

**CMake Configuration**:
```cmake
find_package(OpenGL REQUIRED)
target_link_libraries(SPATIUM PRIVATE OpenGL::GL)
```

**Key Classes Using OpenGL**:
- `VAO`, `VBO`, `EBO`, `UBO`, `SSBO`, `FBO` — buffer object calls
- `ShaderProgram`, `GraphicShader`, `ComputeShader` — compilation, binding, uniforms
- `Debug` — error callback setup
- `App::init_gl()` — state initialization
- `App::run()` — render loop calls (glClear, glDrawArrays, etc.)

**Shader Version**: GL 4.5 core (from `#version 450 core` in `.vsh` / `.fsh` files)

---

### 2. GLFW3 (Window & Input)

**CMake Target**: `glfw`

| Property | Value |
|----------|-------|
| Type | Window/input library |
| Minimum Version | 3.0+ |
| Platform Support | Linux, Windows, macOS |
| Header | `#include <GLFW/glfw3.h>` |
| Usage | Window creation, event polling, buffer swapping |

**CMake Configuration**:
```cmake
find_package(glfw3 REQUIRED)
target_link_libraries(SPATIUM PRIVATE glfw)
```

**Key Classes Using GLFW**:
- `GlfwContext` — wraps `glfwInit()` / `glfwTerminate()`
- `Window` — wraps `glfwCreateWindow`, callbacks, polling
- `App::run()` — calls `glfwPollEvents()`, `glfwWindowShouldClose()`

**GLFW Callback Flow**:
```
glfwSetWindowSizeCallback(handle, glfw_resize_callback)
    ↓ static function, retrieves Window* via glfwGetWindowUserPointer
    ↓
Window::on_resize(w, h)
    ↓ calls user callback (set via Window::set_resize_callback)
    ↓
App's resize_callback (lambda)
    ↓
EventBus::emit(WindowResizeEvent)
```

---

### 3. GLM (Linear Algebra)

**CMake Target**: `glm::glm`

| Property | Value |
|----------|-------|
| Type | Header-only math library |
| Version | 1.0.3 (vendored in `external/glm-1.0.3/`) |
| Platform Support | All |
| Header | `#include <glm/glm.hpp>` (and submodules) |
| Usage | Matrix/vector operations, transformations |

**CMake Configuration**:
```cmake
find_package(glm CONFIG REQUIRED)
find_package(glm REQUIRED)
target_link_libraries(SPATIUM PRIVATE glm::glm)
```

**Vendored Location**: `/home/gabriel/Projects/SPATIUM/external/glm-1.0.3/`

**Key Types Used** (future):
- `glm::vec3` — 3D positions
- `glm::mat4` — transformation matrices
- `glm::mat3` — normal matrices
- `glm::quat` — rotations (alternative to matrices)

**Current Usage**: Minimal (not yet fully integrated, but framework ready)

**Example** (future shader uniforms):
```cpp
glm::mat4 model = glm::translate(glm::mat4(1.0f), {0.5f, 0.0f, 0.0f});
glm::mat4 view = glm::lookAt(eye, center, up);
glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
// Pass to UBO via glBindBuffer + glBufferSubData
```

---

### 4. GLAD (OpenGL Loader)

**CMake Target**: Linked via `core/glad.c` (compiled source)

| Property | Value |
|----------|-------|
| Type | OpenGL function loader (code-gen library) |
| Source | `include/glad/` (pre-generated headers) |
| Header | `#include <glad/glad.h>` |
| Implementation | `core/glad.c` (vendored) |
| Usage | Loads GL function pointers at runtime |

**CMake Integration**:
```cmake
set(CORE_SOURCES
    core/glad.c  # Compiled source
    # ... other sources
)

target_include_directories(SPATIUM PRIVATE
    ${CMAKE_SOURCE_DIR}/include  # glad headers
    # ...
)
```

**Headers**:
- `/home/gabriel/Projects/SPATIUM/include/glad/glad.h` — GL function prototypes
- `/home/gabriel/Projects/SPATIUM/include/KHR/khrplatform.h` — platform types

**Initialization** (implicit in GlfwContext):
```cpp
// glfwCreateWindow sets GL context current
// glad is loaded via #include <glad/glad.h>
// All GL functions become available
```

---

## External Library Dependency Tree

```
SPATIUM (executable)
│
├─ OpenGL::GL
│   ├─ System OpenGL library
│   ├─ GL 4.5 core (from shader versions)
│   └─ [Link: -lGL (Linux), OpenGL.framework (macOS), opengl32 (Windows)]
│
├─ glfw
│   ├─ GLFW 3.0+
│   ├─ [Provides: window, input, GL context]
│   └─ [Link: -lglfw (or libglfw.a if vendored)]
│
├─ glm::glm
│   ├─ GLM 1.0.3 (vendored in external/)
│   ├─ Header-only
│   └─ [No linking required, includes only]
│
└─ glad (via core/glad.c)
    ├─ Pre-generated GL loader
    ├─ [Compiled from: include/glad/glad.h + KHR/khrplatform.h]
    └─ [No find_package; directly compiled]
```

---

## CMakeLists.txt Breakdown

**Location**: `/home/gabriel/Projects/SPATIUM/CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.15)
project(SPATIUM)

set(CMAKE_CXX_STANDARD 17)

# Find external packages
find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(glm CONFIG REQUIRED)  # Config version (glm-config.cmake)

# Define sources
set(CORE_SOURCES
    core/glad.c
    core/gl/obj/EBO.cpp
    core/gl/obj/FBO.cpp
    core/gl/obj/UBO.cpp
    core/gl/obj/VAO.cpp
    core/gl/shader/ShaderProgram.cpp
    core/gl/shader/GraphicShader.cpp
    core/gl/shader/ComputeShader.cpp
    core/gl/obj/BufferObject.h      # Header-only
    core/gl/interfaces/IGPUResource.h
    core/gl/obj/VBO.h
    core/gl/obj/SSBO.h
    core/window/Window.h
    core/window/Window.cpp
    app/GlfwContext.h
    core/event/EventID.h
    core/event/EventBus.h
    core/event/events.h
    app/App.h
    app/App.cpp
    core/gl/debug/Debug.cpp
    core/gl/debug/Debug.h
)

# Create executable
add_executable(SPATIUM
    main.cpp
    ${CORE_SOURCES}
)

# Include paths
target_include_directories(SPATIUM PRIVATE
    ${CMAKE_SOURCE_DIR}              # Access "Core/..." includes
    ${CMAKE_SOURCE_DIR}/include      # glad headers
    ${CMAKE_SOURCE_DIR}/external     # GLM vendored
)

# Link libraries
target_link_libraries(SPATIUM PRIVATE
    glfw
    OpenGL::GL
    glm::glm
)
```

**Key Points**:
1. `CMAKE_CXX_STANDARD 17` enforces C++17
2. `find_package` locations depend on system install or vcpkg
3. `target_include_directories` with `PRIVATE` scope (not exposed to dependents)
4. `target_link_libraries` with `PRIVATE` (library internal only)

---

## Build Output Targets

**Executable**: `SPATIUM`

**Source Files Compiled** (14 `.cpp` files + `glad.c`):
```
core/glad.c                         [GL loader]
core/gl/obj/EBO.cpp
core/gl/obj/FBO.cpp
core/gl/obj/UBO.cpp
core/gl/obj/VAO.cpp
core/gl/shader/ShaderProgram.cpp
core/gl/shader/GraphicShader.cpp
core/gl/shader/ComputeShader.cpp
core/window/Window.cpp
app/App.cpp
core/gl/debug/Debug.cpp
main.cpp
```

**Header-Only (no .cpp needed)**:
```
core/gl/obj/BufferObject.h          [Base template]
core/gl/interfaces/IGPUResource.h
core/gl/obj/VBO.h                   [Template implementation]
core/gl/obj/SSBO.h                  [Template implementation]
core/window/Window.h
app/GlfwContext.h
core/event/EventID.h
core/event/EventBus.h
core/event/events.h
app/App.h
core/gl/debug/Debug.h
```

---

## Platform-Specific Notes

### Linux

**Required System Packages**:
```bash
sudo apt install libglfw3-dev libglm-dev
```

**OpenGL**:
- Default system OpenGL via libGL.so

**GL Header Paths**:
```bash
# GLAD is vendored; system GL/gl.h not needed
# -I/home/gabriel/Projects/SPATIUM/include finds glad/glad.h
```

### Windows (MSVC)

**vcpkg** (recommended):
```bash
vcpkg install glfw3:x64-windows glm:x64-windows
```

**CMake**:
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### macOS

**Homebrew**:
```bash
brew install glfw3 glm
```

**OpenGL**:
- Uses `OpenGL.framework` (native)
- GLX not available; use GLFW for context

---

## Build Configuration Variants

### Debug Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/SPATIUM
```

**Behavior**:
- Full debug symbols
- No optimizations
- GL debug callbacks enabled (via Debug::enable())

### Release Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Behavior**:
- Optimized (-O3 / -Ofast)
- Minimal symbols
- GL debug overhead removed

---

## Future Dependencies (CUDA 3DGS Rasterizer)

**Planned Additions**:

```cmake
find_package(CUDA REQUIRED)
find_package(CUB REQUIRED)  # NVIDIA Thrust

target_link_libraries(SPATIUM PRIVATE
    CUDA::cudart
    CUDA::cuda_driver
    cub
)

target_compile_options(SPATIUM PRIVATE
    $<$<COMPILE_LANGUAGE:CUDA>:--gpu-architecture=sm_75>
)
```

**Integration Points**:
1. CUDA kernels for 3DGS rasterization (tile-based rendering)
2. CUDA-OpenGL interop (cudaGraphicsGLRegisterImage)
3. UBO/SSBO for gaussian data and depth sorting

---

## Dependency Version Matrix

| Dependency | Version | Min | Max | Status |
|-----------|---------|-----|-----|--------|
| C++ | 17 | ✓ | — | Required |
| CMake | 3.15+ | ✓ | — | Required |
| OpenGL | 4.5 core | ✓ | — | Required |
| GLFW | 3.0+ | ✓ | — | Required |
| GLM | 1.0.3 | 1.0.0 | — | Vendored |
| GLAD | Latest | ✓ | — | Vendored |
| *(CUDA)* | 12.0+ | — | — | *(Future)* |

---

**See Also**:
- `architecture.md` — System design and module boundaries
- `backend.md` — Class hierarchies and subsystem details
- `data.md` — Buffer layouts and data structures
