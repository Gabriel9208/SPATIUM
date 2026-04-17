<!-- Generated: 2026-04-16 | SPATIUM Engine Subsystems & Class Hierarchies -->

# Backend Subsystems Codemap

**Focus**: Core engine classes, GPU abstraction layer, event system, window management  
**Last Updated**: 2026-04-16

## Class Hierarchy Map

```
┌─────────────────────────────────────────────────────────────┐
│                    IGPUResource (abstract)                   │
│            core/gl/interfaces/IGPUResource.h                 │
│                                                              │
│ virtual ~IGPUResource() = default;                          │
│ virtual void bind() const = 0;                              │
│ virtual void unbind() const = 0;                            │
└────────────────────────┬────────────────────────────────────┘
                         │
        ┌────────────────┴────────────────┐
        │                                 │
        ▼                                 ▼
   ┌──────────┐                  ┌──────────────────┐
   │   VAO    │                  │ StorageBuffer<T> │
   │(VBO/EBO)◄────────┬──────────┤   (abstract)     │
   └──────────┘        │          └────────┬─────────┘
                       │                   │
                ┌──────┴─────┬──────────────┴──────────────┐
                ▼            ▼              ▼              ▼
           ┌────────┐  ┌──────┐    ┌──────────┐    ┌──────────┐
           │VBO<T>  │  │ EBO  │    │ UBO      │    │ SSBO<T>  │
           │(Vector)│  │(Index)    │(Uniform) │    │(Compute) │
           └────────┘  └──────┘    └──────────┘    └──────────┘
                             └─────────────────────────┘
                                (Future: FBO, TextureBuffer)
```

## Subsystem 1: Window Management (core/window/)

**Files**:
- `Window.h` (header)
- `Window.cpp` (implementation)

**Class Definition**:

```cpp
class Window {
private:
    GLFWwindow* handle_ = nullptr;
    std::string title_;
    int width_ = 0, height_ = 0;
    std::function<void(int, int)> resize_callback_;
    
    static void glfw_resize_callback(GLFWwindow* win, int w, int h);
    void on_resize(int w, int h);
    
public:
    explicit Window(int width, int height, std::string title, bool vsync);
    ~Window();
    
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;
    
    // Query
    bool should_close() const;
    void swap_buffers() const;
    void make_context_current() const;
    int get_width() const;
    int get_height() const;
    
    // Callbacks
    void set_resize_callback(std::function<void(int, int)> cbf);
};
```

**Key Methods**:

| Method | Signature | Purpose |
|--------|-----------|---------|
| Constructor | `Window(int w, int h, std::string title, bool vsync)` | Create GLFW window, set context current, enable vsync |
| `should_close()` | `bool` (const) | `glfwWindowShouldClose(handle_)` |
| `swap_buffers()` | `void` (const) | `glfwSwapBuffers(handle_)` |
| `make_context_current()` | `void` (const) | `glfwMakeContextCurrent(handle_)` |
| `set_resize_callback()` | Takes `std::function<void(int, int)>` | Register user callback, wires static GLFW callback |

**Ownership**: Window owns the GLFWwindow* handle; cleanup happens in destructor.

**Non-Copyable**: Deleted copy/move to prevent aliasing of GLFW handle.

---

## Subsystem 2: Event System (core/event/)

**Files**:
- `EventBus.h` (pub/sub router)
- `EventID.h` (type → ID mapping)
- `events.h` (event payload definitions)

### EventID.h (Type Registration)

```cpp
// Pseudo-code (unique ID per type)
class EventID {
    static int counter;
public:
    template<typename T>
    static int get() {
        static int id = counter++;
        return id;
    }
};
```

**Purpose**: Assign unique integer IDs to event types at runtime.

### EventBus.h (Pub/Sub Dispatcher)

```cpp
class EventBus {
    std::unordered_map<int, std::vector<std::function<void(const void*)>>> listeners_;
    
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> listener) {
        int id = EventID::get<T>();
        listeners_[id].push_back([listener](const void* e) {
            listener(*static_cast<const T*>(e));
        });
    }
    
    template<typename T>
    void emit(const T& event) {
        int id = EventID::get<T>();
        auto it = listeners_.find(id);
        if (it != listeners_.end()) {
            auto callbacks = it->second;  // Copy to guard re-entrancy
            for (auto& cb : callbacks)
                cb(&event);
        }
    }
};
```

**Key Design**:
1. Type-erased storage (`void*`) allows heterogeneous events
2. Copy listeners before iterating (guards against re-entrant subscribe/emit)
3. Subscribers are functions (lambdas, std::function objects)

**Typical Flow**:
```cpp
EventBus bus;

// Subscribe (in App constructor)
bus.subscribe<WindowResizeEvent>([](const WindowResizeEvent& e) {
    std::cout << "Resized: " << e.width << "x" << e.height << "\n";
});

// Emit (in Window::on_resize)
bus.emit(WindowResizeEvent{1280, 720});
```

### events.h (Event Payload Definitions)

```cpp
struct WindowResizeEvent {
    int width;
    int height;
};

// Future events:
// struct KeyPressEvent { int key, mods; };
// struct MouseEvent { double x, y; };
```

---

## Subsystem 3: GL Utilities (core/gl/)

### IGPUResource Interface

**File**: `core/gl/interfaces/IGPUResource.h`

```cpp
class IGPUResource {
public:
    virtual ~IGPUResource() = default;
    virtual void bind() const = 0;
    virtual void unbind() const = 0;
};
```

**Role**: Common interface for all GL objects (VAO, VBO, EBO, UBO, SSBO, FBO).

**Example Implementation** (VAO):
```cpp
void VAO::bind() const { glBindVertexArray(id_); }
void VAO::unbind() const { glBindVertexArray(0); }
```

### StorageBuffer<T> Template Base

**File**: `core/gl/obj/StorageBuffer.h`

```cpp
template<class T>
class StorageBuffer : public IGPUResource {
protected:
    GLuint id_ = 0;
    std::size_t size_ = 0;
    
public:
    virtual void initialize(unsigned int _size, GLuint usageMode) = 0;
    virtual void initialize(const std::vector<T>& v, GLuint usageMode) = 0;
    virtual ~StorageBuffer() = default;
};
```

**Subclasses**: VBO, EBO, UBO, SSBO

### VAO Implementation

**File**: `core/gl/obj/VAO.h` / `VAO.cpp`

```cpp
class VAO : public IGPUResource {
private:
    GLuint id_ = 0;
public:
    VAO();
    ~VAO() override;
    
    VAO(VAO&& other) noexcept;
    VAO(const VAO&) = delete;
    
    void bind() const override;
    void unbind() const override;
};
```

**Constructor** (VAO.cpp):
```cpp
VAO::VAO() {
    glGenVertexArrays(1, &id_);
}

VAO::~VAO() {
    if (id_ != 0)
        glDeleteVertexArrays(1, &id_);
}
```

**Move** (RAII):
```cpp
VAO::VAO(VAO&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
```

### VBO<T> Template Implementation

**File**: `core/gl/obj/VBO.h`

```cpp
template<class T>
class VBO : public StorageBuffer<T> {
public:
    explicit VBO(const std::vector<T>& v, GLuint usageMode);
    
    void initialize(unsigned int _size, GLuint usageMode) override;
    void initialize(const std::vector<T>& v, GLuint usageMode) override;
    void bind() const override;
    void unbind() const override;
    void set_data(const std::vector<T>& v, GLuint usageMode);
};
```

**Example Instantiations**:
- `VBO<float>` — raw vertex data
- `VBO<glm::vec3>` — 3D positions
- `VBO<Vertex>` — custom struct (future)

---

## Subsystem 4: Shaders (core/gl/shader/)

**Files**:
- `ShaderProgram.h` / `.cpp` (abstract base)
- `GraphicShader.h` / `.cpp` (vertex + fragment)
- `ComputeShader.h` / `.cpp` (compute kernels)

### ShaderProgram (Abstract Base)

**File**: `core/gl/shader/ShaderProgram.h`

```cpp
class ShaderProgram {
protected:
    unsigned int program;
public:
    ShaderProgram() : program(0) {}
    virtual ~ShaderProgram();
    
    virtual const GLchar* read_shader(const char* filename);
    virtual GLuint load(ShaderInfo* shaders) { return 0; }
    virtual GLuint load(const char* shaderFile) { return 0; }
    virtual void use() const = 0;
    static void unuse();
    
    unsigned int get_id() const { return program; }
};
```

**Structure**:
```cpp
struct ShaderInfo {
    GLenum type;         // GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER
    const char* filename;
};
```

### GraphicShader (Vertex + Fragment)

**File**: `core/gl/shader/GraphicShader.h` / `.cpp`

```cpp
class GraphicShader : public ShaderProgram {
public:
    GraphicShader();
    
    GLuint load(ShaderInfo* shaders) override;
    void use() const override;
};
```

**Typical Usage** (from App::run, commented):
```cpp
ShaderInfo shaders[] = {
    { GL_VERTEX_SHADER,   "../shader/vertex/basic.vsh" },
    { GL_FRAGMENT_SHADER, "../shader/fragment/basic.fsh" },
    { GL_NONE, nullptr }
};
GraphicShader gs;
gs.load(shaders);
gs.use();
glDrawArrays(GL_TRIANGLES, 0, vertex_count);
```

### ComputeShader

**File**: `core/gl/shader/ComputeShader.h` / `.cpp`

```cpp
class ComputeShader : public ShaderProgram {
public:
    ComputeShader();
    
    GLuint load(const char* shaderFile) override;
    void use() const override;
};
```

**Planned Use**: 3DGS tile-based blending, depth sorting, etc.

---

## Subsystem 5: Debug Utilities (core/gl/debug/)

**File**: `core/gl/debug/Debug.h` / `.cpp`

```cpp
class Debug {
private:
    static std::string_view glDebugSourceStr(GLenum source);
    static std::string_view glDebugTypeStr(GLenum type);
    static std::string_view glDebugSeverityStr(GLenum severity);
    static void MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar* message, const void* userParam);
public:
    Debug() = delete;
    static void enable();
};
```

**Purpose**: Register GL debug output callback via `glDebugMessageCallback`.

**Usage** (in App::init_gl):
```cpp
Debug::enable();  // Logs all GL errors to console
```

---

## Subsystem 6: Application (app/)

**Files**:
- `GlfwContext.h` (GLFW singleton wrapper)
- `App.h` / `App.cpp` (main engine class)

### GlfwContext

**File**: `app/GlfwContext.h`

```cpp
class GlfwContext {
public:
    GlfwContext();
    ~GlfwContext();
    GlfwContext(const GlfwContext&) = delete;
};
```

**Purpose**: RAII wrapper for `glfwInit()` and `glfwTerminate()`.

**Constructor**: `glfwInit()` → call parent setup  
**Destructor**: `glfwTerminate()` → cleanup

### App

**File**: `app/App.h` / `app/App.cpp`

```cpp
class App : private GlfwContext {
private:
    EventBus event_bus_;
    Window window_;
    
    void init_gl();
public:
    App();
    void run() const;
};
```

**Constructor** (App::App):
1. Call `GlfwContext::GlfwContext()` — initializes GLFW
2. Construct `EventBus` — must be before Window (used in callback)
3. Construct `Window(1280, 720, "SPATIUM")`
4. Wire Window resize callback to EventBus
5. Call `init_gl()` — OpenGL state initialization

**init_gl**:
```cpp
void App::init_gl() {
    Debug::enable();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glViewport(0, 0, window_.get_width(), window_.get_height());
}
```

**run**:
```cpp
while (!window_.should_close()) {
    glfwPollEvents();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Future: render scene, compute shaders, etc.
    window_.swap_buffers();
}
```

---

## Lifetime & Initialization Order

**App Construction**:
```
main()
  ↓
App app;
  ├─ GlfwContext::GlfwContext() [glfwInit()]
  ├─ EventBus event_bus_ [constructed]
  ├─ Window window_(1280, 720, "SPATIUM") [glfwCreateWindow, make_context_current]
  ├─ window_.set_resize_callback(lambda) [wire to EventBus]
  ├─ init_gl() [Debug::enable, glEnable, glClearColor, glViewport]
  └─ [Ready to run]
```

**App Destruction**:
```
App::~App()
  ├─ ~Window() [glfwDestroyWindow]
  ├─ ~EventBus()
  └─ ~GlfwContext() [glfwTerminate()]
```

---

## Thread Model

| Component | Thread-Safe | Notes |
|-----------|------------|-------|
| Window | ❌ | GL context tied to single thread |
| EventBus | ❌ | No synchronization primitives |
| VAO, VBO, EBO, etc. | ❌ | GL state is thread-local |
| App | ❌ | Runs on main thread only |

**Rule**: All rendering and GL calls must happen on the same thread that created the context.

---

**See Also**:
- `architecture.md` — System diagram, initialization flow
- `data.md` — Buffer layouts, event payloads, uniforms
- `dependencies.md` — External libraries (OpenGL, GLFW, GLM)
