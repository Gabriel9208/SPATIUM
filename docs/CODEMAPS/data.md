<!-- Generated: 2026-04-16 | SPATIUM Data Structures & GPU Memory Layout -->

# Data Structures Codemap

**Focus**: GL buffer objects, shader uniforms, event payloads, buffer layouts  
**Last Updated**: 2026-04-16

## GL Buffer Object Hierarchy

```
IGPUResource (interface)
├── VAO (Vertex Array Object)
│   ├── Stores vertex attribute pointers
│   ├── No data ownership, only configuration
│   └── core/gl/obj/VAO.h : public IGPUResource
│       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/VAO.h
│
├── StorageBuffer<T> (template base)
│   ├── Owns size_, id_ (GL handle)
│   ├── Generic RAII wrapper for GL buffers
│   └── core/gl/obj/StorageBuffer.h
│       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/StorageBuffer.h
│
└── Concrete Buffer Types (inherit StorageBuffer<T>)
    ├── VBO<T> : public StorageBuffer<T>
    │   ├── Vertex Buffer Object
    │   ├── Stores: std::vector<T> vertex data (positions, UVs, normals, etc.)
    │   ├── GL Target: GL_ARRAY_BUFFER
    │   ├── Template params: float, glm::vec3, custom vertex struct
    │   └── core/gl/obj/VBO.h
    │       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/VBO.h
    │
    ├── EBO (Element Buffer Object)
    │   ├── Index buffer for glDrawElements
    │   ├── GL Target: GL_ELEMENT_ARRAY_BUFFER
    │   └── core/gl/obj/EBO.h / EBO.cpp
    │       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/EBO.h
    │
    ├── UBO (Uniform Buffer Object)
    │   ├── Stores uniform data (MVP matrices, light params, etc.)
    │   ├── GL Target: GL_UNIFORM_BUFFER
    │   ├── Binding point: application-defined (0–15 typical)
    │   └── core/gl/obj/UBO.h / UBO.cpp
    │       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/UBO.h
    │
    ├── SSBO (Shader Storage Buffer Object)
    │   ├── Read/write buffer accessible from compute shaders
    │   ├── GL Target: GL_SHADER_STORAGE_BUFFER
    │   ├── Binding point: 0–N (query max via glGetIntegerv)
    │   └── core/gl/obj/SSBO.h
    │       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/SSBO.h
    │
    ├── FBO (Framebuffer Object)
    │   ├── Render target (color, depth, stencil attachments)
    │   ├── GL Target: GL_FRAMEBUFFER
    │   └── core/gl/obj/FBO.h / FBO.cpp
    │       Location: /home/gabriel/Projects/SPATIUM/core/gl/obj/FBO.h
    │
    └── (Future: TextureBuffer, CubeMapBuffer for CUDA interop)
```

## Concrete Type Definitions

### VAO (Vertex Array Object)

**Location**: `/home/gabriel/Projects/SPATIUM/core/gl/obj/VAO.h`

```cpp
class VAO : public IGPUResource {
private:
    GLuint id_ = 0;
public:
    VAO();                          // glGenVertexArrays
    VAO(VAO&& other) noexcept;      // Move constructor (RAII)
    ~VAO() override;                // glDeleteVertexArrays
    void bind() const override;     // glBindVertexArray(id_)
    void unbind() const override;   // glBindVertexArray(0)
};
```

**Typical Usage**:
```cpp
VAO vao;
VBO<float> vbo(vertex_data, GL_STATIC_DRAW);
vao.bind();
  vbo.bind();
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, offset);
vao.unbind();
```

### VBO<T> (Vertex Buffer Object, Template)

**Location**: `/home/gabriel/Projects/SPATIUM/core/gl/obj/VBO.h`

```cpp
template<class T>
class VBO : public StorageBuffer<T> {
public:
    VBO();                                          // Default
    explicit VBO(const std::vector<T>& v, GLuint usageMode);
    
    void initialize(unsigned int _size, GLuint usageMode) override;
    void initialize(const std::vector<T>& v, GLuint usageMode) override;
    void bind() const override;     // glBindBuffer(GL_ARRAY_BUFFER, id_)
    void unbind() const override;   // glBindBuffer(GL_ARRAY_BUFFER, 0)
    void set_data(const std::vector<T>& v, GLuint usageMode);
};
```

**Template Instantiations** (observed in code):
- `VBO<float>` — vertex positions (3 floats per vertex)
- *(Future)*: `VBO<glm::vec3>`, `VBO<Vertex>` (struct)

**GL Usage Pattern**:
```
GL_STATIC_DRAW   → immutable, created once
GL_DYNAMIC_DRAW  → modified occasionally per frame
GL_STREAM_DRAW   → modified every frame (avoid)
```

### StorageBuffer<T> (CRTP Base)

**Location**: `/home/gabriel/Projects/SPATIUM/core/gl/obj/StorageBuffer.h`

```cpp
template<class T>
class StorageBuffer : public IGPUResource {
protected:
    GLuint id_ = 0;         // GL buffer handle
    std::size_t size_ = 0;  // Element count (NOT byte size)
public:
    virtual void initialize(unsigned int _size, GLuint usageMode) = 0;
    virtual void initialize(const std::vector<T>& v, GLuint usageMode) = 0;
    // bind/unbind implemented by subclass
};
```

**Ownership**: Each StorageBuffer owns its GL handle; destructor must call `glDeleteBuffers`.

### Event Payloads

**Location**: `/home/gabriel/Projects/SPATIUM/core/event/events.h`

```cpp
struct WindowResizeEvent {
    int width;
    int height;
};

// (Future events to be defined)
// struct KeyPressEvent { int key, mods; };
// struct MouseMoveEvent { double x, y; };
// struct ScrollEvent { double xOffset, yOffset; };
```

**Flow**: `Window::on_resize()` → `EventBus::emit(WindowResizeEvent)` → Registered subscribers

### Window Class

**Location**: `/home/gabriel/Projects/SPATIUM/core/window/Window.h`

```cpp
class Window {
private:
    GLFWwindow* handle_;
    std::string title_;
    int width_, height_;
    std::function<void(int, int)> resize_callback_;
    
    static void glfw_resize_callback(GLFWwindow* win, int w, int h);
    void on_resize(int w, int h);
public:
    explicit Window(int width, int height, std::string title, bool vsync);
    ~Window();
    
    bool should_close() const;
    void swap_buffers() const;
    void make_context_current() const;
    int get_width() const;
    int get_height() const;
    void set_resize_callback(std::function<void(int, int)> cbf);
};
```

**GLFW Callback Bridge**:
```
GLFW static callback (glfw_resize_callback)
    ↓ retrieves Window* from glfwGetWindowUserPointer
    ↓
Window::on_resize (instance method)
    ↓ calls user callback
    ↓
Custom lambda in App::App
    ↓
EventBus::emit(WindowResizeEvent)
```

## Shader Uniforms & Attributes

### Vertex Attribute Layout

**File**: `shader/vertex/basic.vsh` (OpenGL 4.5 core)

```glsl
#version 450 core

layout(location = 0) in vec4 position;  // Attribute index 0
void main() {
    gl_Position = position;
}
```

**Expected VBO data** (if using vec4):
```cpp
std::vector<float> vertex_data = {
    -0.5f, -0.5f, 0.0f, 1.0f,  // vertex 0
     0.5f, -0.5f, 0.0f, 1.0f,  // vertex 1
     0.5f,  0.5f, 0.0f, 1.0f   // vertex 2
};
```

**Or as interleaved position + texcoord** (from App::run comment):
```cpp
std::vector<float> v = {
    // x,    y,    z,  u,    v
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f, 0.5f, 0.0f,
     0.5f,  0.5f, 0.0f, 0.5f, 0.5f,
    // ... more vertices
};
// Stride = 5 * sizeof(float) = 20 bytes
// glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void*)0);
```

### Fragment Shader

**File**: `shader/fragment/basic.fsh`

```glsl
#version 450 core
void main() {
    // (commented out or minimal implementation)
}
```

*(No output defined; likely placeholder for future texturing/lighting)*

## Memory Layout Examples

### Interleaved Vertex Data (Common in Game Engines)

```
Float array in memory:
┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
│ x0  │ y0  │ z0  │ u0  │ v0  │ x1  │ y1  │ z1  │ u1  │ v1  │
└─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
 offset 0   offset 20   offset 40   offset 60

Stride = 20 bytes
Position offset = 0
TexCoord offset = 12

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, (void*)0);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, (void*)12);
```

### GL Object Binding Points

| Target | Example | Max Binding Points | Used For |
|--------|---------|-------------------|----------|
| `GL_ARRAY_BUFFER` | VBO | ∞ (one active at a time) | Vertex data |
| `GL_ELEMENT_ARRAY_BUFFER` | EBO | 1 per VAO | Index data |
| `GL_UNIFORM_BUFFER` | UBO | 16–128+ (GL_MAX_UNIFORM_BUFFER_BINDINGS) | Uniform matrices |
| `GL_SHADER_STORAGE_BUFFER` | SSBO | 128–1024+ (GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS) | GPU read/write data |
| `GL_FRAMEBUFFER` | FBO | 1 active | Render targets |

## Event Bus Data Flow

**Location**: `/home/gabriel/Projects/SPATIUM/core/event/EventBus.h`

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

**Type Dispatch** (EventID):
- Uses a static counter to assign unique IDs to each event type
- Allows heterogeneous event storage with type-safe retrieval

## RAII & Resource Ownership

All GL objects follow **RAII**:

```cpp
// Constructor — allocates GL handle
VAO::VAO()  { glGenVertexArrays(1, &id_); }

// Destructor — deallocates
VAO::~VAO() { glDeleteVertexArrays(1, &id_); }

// Move semantics for efficiency
VAO::VAO(VAO&& o) noexcept : id_(std::exchange(o.id_, 0)) {}

// No copy (GL handles are non-copyable)
VAO(const VAO&) = delete;
```

**Ownership Guarantee**: If a VAO goes out of scope, its GL handle is automatically freed.

---

**Next**: See `backend.md` for class hierarchies and `dependencies.md` for external libs.
