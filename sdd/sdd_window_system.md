# SDD: Window System

> **Version**: 3.1  
> **Status**: `READY`  
> **Created**: 2026-04-14  
> **Last Updated**: 2026-04-14    
> **Author**: Gabriel

---

## 0. Quick Reference (for Claude Code)

```
TASK   : Window management subsystem — GLFW window, OpenGL context, type-safe event bus
SCOPE  : GlfwContext, Window, EventBus, EventID, App, events.h
GOAL   : Fully functional GLFW window with OpenGL 4.6 Core Profile context, RAII lifecycle, type-safe event pub/sub, and main render loop
AVOID  : RTTI (typeid/type_index/dynamic_cast), GLEW, Compat Profile, GL render state in Window (except glViewport in on_resize), move semantics on Window
STATUS : READY — all design decisions finalized, pending implementation
```

---

## 1. Context & Background

### 1.1 Why This Task Exists

- **Problem being solved**: SPATIUM needs a foundational window and event system that manages GLFW/OpenGL lifecycle with correct initialization order, provides decoupled event dispatch, and serves as the entry point for the render loop.
- **Relation to other modules**: All future subsystems (renderer, camera, input, UI) depend on this. `App` will be the composition root; `EventBus` will carry cross-system communication.
- **Dependencies (upstream)**: GLFW, GLAD, OpenGL 4.6
- **Dependents (downstream)**: Renderer (future), Camera (future), Input system (future)

### 1.2 Reference Materials

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| GLFW Documentation | https://www.glfw.org/docs/latest/ | Window and context API reference |
| GLAD Generator | https://glad.dav1d.de/ | OpenGL loader — project uses GLAD, not GLEW |
| OpenGL 4.6 Core Spec | https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf | Core Profile reference |

---

## 2. Specification

### 2.1 Objective

**Definition of Done**:
- [ ] All 5 classes (`GlfwContext`, `EventID`, `EventBus`, `Window`, `App`) implemented per §3.3 specifications
- [ ] Initialization and destruction order matches §3.2 sequence diagrams
- [ ] Window opens, displays black background, responds to resize, and closes cleanly
- [ ] All §9 checklist items verified
- [ ] Compilation succeeds with no warnings under project build settings

### 2.2 Functional Requirements

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | GLFW init/terminate lifecycle managed via RAII `GlfwContext`, with error callback set before `glfwInit` | MUST |
| FR-02 | Window creation with configurable title, dimensions, and vsync (`bool` parameter) | MUST |
| FR-03 | OpenGL 4.6 Core Profile context created with GLAD loader (not GLEW) | MUST |
| FR-04 | Type-safe event ID generation via template static variable, without RTTI | MUST |
| FR-05 | Generic event pub/sub via `EventBus` — `subscribe<T>` and `emit<T>`, decoupled from concrete event types | MUST |
| FR-06 | `emit` with no subscribers is a no-op (no crash) | MUST |
| FR-07 | `emit` guards against re-entrancy by copying the callback vector before iteration | MUST |
| FR-08 | Window resize detected via GLFW callback → viewport updated → external callback invoked → event broadcast | MUST |
| FR-09 | Main render loop: poll events, clear buffers, (future: render), swap buffers | MUST |
| FR-10 | All event structs defined in standalone `events.h` with no coupling to any class | MUST |

### 2.3 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| OpenGL | 4.6 Core Profile (`GLFW_OPENGL_CORE_PROFILE`); Compatibility Profile is not permitted |
| GL Loader | GLAD only; `glewInit` must not appear anywhere |
| C++ Standard | C++17 minimum (`static inline` required for `EventID::counter`) |
| Code Style | Google C++ Style Guide |
| Resource Management | RAII for all resources; no manual `glfwTerminate` calls from `App` |

---

## 3. Design

### 3.1 High-Level Approach

**Chosen approach**: `App` privately inherits `GlfwContext` to guarantee GLFW initialization before any member construction. `Window` and `EventBus` are held by composition inside `App`, with `App` acting as the sole glue layer that wires the two together via callback injection. `EventBus` uses type-erased `void*` dispatch keyed by compile-time `EventID` to avoid RTTI.

**Rationale**: Private inheritance of `GlfwContext` exploits C++'s guarantee that base classes initialize before members, eliminating any risk of constructing a `GLFWwindow` before `glfwInit`. Composition + callback injection keeps `Window` and `EventBus` fully decoupled — neither includes nor knows about the other.

**Rejected alternatives**:
- Singleton `GlfwContext` — introduces global state and lifetime ambiguity.
- `Window` holding a reference to `EventBus` — breaks single-responsibility; couples two independent subsystems.
- `std::type_index` for event IDs — requires RTTI (`typeid`), which is disabled in this project.

### 3.2 Architecture / Data Flow

**Ownership structure**:
```
App : private GlfwContext    (private inheritance, guarantees GLFW init order)
 ├── event_bus : EventBus    (composition, declared before window)
 └── window : Window         (composition)
      └── handle : GLFWwindow*

EventBus
 └── EventID                 (dependency, generates type-safe event IDs)
```

**Event flow**:
```
GLFW callback → Window::glfw_resize_callback (static)
             → Window::on_resize (private)
             → resize_callback (std::function, injected by App)
             → EventBus::emit<WindowResizeEvent>
             → all subscribers
```

**Initialization order** (guaranteed by C++ language rules):
```
GlfwContext()   ← base class, first; glfwInit happens here
event_bus()     ← member
Window()        ← member; GLFW is already ready
App() { }       ← constructor body; init_gl happens here
```

**Destruction order** (`~App()` is invoked first but completes last):
```
~App() body executes     ← destructor body, runs first (currently empty)
~Window()                ← member destructor; glfwDestroyWindow
~event_bus()             ← member destructor
~GlfwContext()           ← base class destructor; glfwTerminate, runs last
                            App object is fully destroyed at this point
```

**Detailed initialization sequence**:
```
main()
  └── App()
       └── GlfwContext()                    ← base class, first
            └── glfwSetErrorCallback(...)
            └── glfwInit()
       └── event_bus()                      ← member
       └── Window(1280, 720, "SPATIUM")     ← member
            └── glfwWindowHint(...)
            └── glfwCreateWindow(...)
            └── glfwMakeContextCurrent(...)
            └── glfwSwapInterval(vsync)
            └── gladLoadGLLoader(...)
            └── glfwSetWindowUserPointer(handle, this)
            └── glfwSetFramebufferSizeCallback(...)
       └── App() { }                        ← constructor body
            └── init_gl()
            └── window.set_resize_callback(...)
            └── event_bus.subscribe<...>(...)
  └── app.run()
       └── while (!window.should_close())
            └── glfwPollEvents()
            └── glClear(...)
            └── swap_buffers()
  └── ~App()
       └── ~Window() → glfwDestroyWindow()
       └── ~GlfwContext() → glfwTerminate()  ← last
```

---

### 3.3 Class Specifications

---

#### 3.3.1 `GlfwContext`

**Responsibility**: Manages the lifetime of GLFW itself (`glfwInit` / `glfwTerminate`). Acts as a private base class of `App`, leveraging C++'s guarantee that base classes are initialized before members, ensuring GLFW is ready before `Window` is constructed.

**Methods**:

| Access | Signature | Description |
|---|---|---|
| public | `GlfwContext()` | Sets the error callback, calls `glfwInit`; throws on failure |
| public | `~GlfwContext()` | Calls `glfwTerminate` |

**Implementation**:

```cpp
struct GlfwContext {
    GlfwContext() {
        glfwSetErrorCallback([](int error, const char* desc) {
            fprintf(stderr, "GLFW Error %d: %s\n", error, desc);
        });
        if (!glfwInit())
            throw std::runtime_error("Failed to initialize GLFW");
    }

    ~GlfwContext() {
        glfwTerminate();
    }
};
```

**Constraints**:
- Holds no members; purely manages GLFW lifecycle.
- `glfwTerminate` may only be called after all `GLFWwindow*` handles are destroyed — guaranteed by inheritance order (`~Window()` runs before `~GlfwContext()`).

---

#### 3.3.2 `EventID`

**Responsibility**: Generates a unique `int` ID for each event type using a template static variable, without relying on RTTI.

**Members**:

| Access | Name | Type | Description |
|---|---|---|---|
| private | `counter` | `static inline int` | Global counter; incremented each time a new ID is assigned |

**Methods**:

| Access | Signature | Description |
|---|---|---|
| public | `template<typename T> static int get()` | Returns the unique ID for `T`; assigned on first call and constant thereafter |

**Implementation**:

```cpp
class EventID {
public:
    template<typename T>
    static int get() {
        static int id = counter++;
        return id;
    }
private:
    static inline int counter = 0;
};
```

**Constraints**:
- Must not use `typeid`, `std::type_index`, or `dynamic_cast`.
- `counter` must be `static inline` to avoid ODR violations.
- `int` is used instead of `size_t` per Google C++ Style Guide — `counter` is an incrementing ID, not a container size.

---

#### 3.3.3 `EventBus`

**Responsibility**: Receives `emit` calls and broadcasts events to all subscribers of the corresponding type. Has no knowledge of any concrete event type or subscriber identity.

**Members**:

| Access | Name | Type | Description |
|---|---|---|---|
| private | `listeners` | `std::unordered_map<int, std::vector<std::function<void(const void*)>>>` | Key is the EventID; value is the list of listeners |

**Methods**:

| Access | Signature | Description |
|---|---|---|
| public | `template<typename T> void subscribe(std::function<void(const T&)> listener)` | Subscribes to events of type `T` |
| public | `template<typename T> void emit(const T& event)` | Broadcasts the event; invokes all matching listeners |

**Implementation**:

```cpp
class EventBus {
public:
    template<typename T>
    void subscribe(std::function<void(const T&)> listener) {
        int id = EventID::get<T>();
        listeners[id].push_back([listener](const void* e) {
            listener(*static_cast<const T*>(e));
        });
    }

    template<typename T>
    void emit(const T& event) {
        int id = EventID::get<T>();
        auto it = listeners.find(id);
        if (it != listeners.end()) {
            // Copy vector before iterating to guard against re-entrant
            // subscribe/emit calls causing iterator invalidation
            auto callbacks = it->second;
            for (auto& cb : callbacks)
                cb(&event);
        }
    }

private:
    std::unordered_map<
        int,
        std::vector<std::function<void(const void*)>>
    > listeners;
};
```

**Constraints**:
- Must not include any concrete event struct headers.
- `emit` with no subscribers must be a no-op; must not crash.
- The callback vector must be copied before iteration to prevent invalidation if a listener calls `subscribe` or `emit` internally.
- No `unsubscribe` support in the initial version.

---

#### 3.3.4 `Window`

**Responsibility**: Manages the lifetime of the GLFW window and OpenGL context. Detects window events and notifies the outside world, but has no knowledge of `EventBus`.

**Members**:

| Access | Name | Type | Default | Description |
|---|---|---|---|---|
| private | `handle` | `GLFWwindow*` | `nullptr` | GLFW window handle |
| private | `title` | `std::string` | `"SPATIUM"` | Window title |
| private | `width` | `int` | `0` | Current window width |
| private | `height` | `int` | `0` | Current window height |
| private | `resize_callback` | `std::function<void(int, int)>` | empty | Externally injected resize notification function |

**Methods**:

| Access | Signature | Description |
|---|---|---|
| public | `Window(int width, int height, std::string title = "SPATIUM", bool vsync = true)` | Creates and initializes the GLFW window; RAII |
| public | `~Window()` | Calls `glfwDestroyWindow`; releases resources |
| public (deleted) | `Window(const Window&)` | Copy prohibited |
| public (deleted) | `Window& operator=(const Window&)` | Copy assignment prohibited |
| public (deleted) | `Window(Window&&)` | Move prohibited |
| public (deleted) | `Window& operator=(Window&&)` | Move assignment prohibited |
| public | `bool should_close() const` | Wraps `glfwWindowShouldClose` |
| public | `void swap_buffers()` | Wraps `glfwSwapBuffers` |
| public | `void make_context_current()` | Wraps `glfwMakeContextCurrent`; reserved for future multi-context use |
| public | `void set_resize_callback(std::function<void(int, int)> cbf)` | Injects the resize notification function |
| public | `int get_width() const` | Returns the current width |
| public | `int get_height() const` | Returns the current height |
| private static | `void glfw_resize_callback(GLFWwindow*, int w, int h)` | GLFW C callback; forwards via UserPointer |
| private | `void on_resize(int w, int h)` | Updates width/height, updates viewport, invokes resize_callback |

**Implementation**:

```cpp
class Window {
public:
    Window(int width, int height, std::string title = "SPATIUM", bool vsync = true)
        : title(std::move(title)), width(width), height(height)
    {
        // GLFW context hints — must be set before glfwCreateWindow
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        handle = glfwCreateWindow(width, height, this->title.c_str(), nullptr, nullptr);
        if (!handle)
            throw std::runtime_error("Failed to create GLFW window");

        glfwMakeContextCurrent(handle);
        glfwSwapInterval(vsync ? 1 : 0);

        // GLAD is part of context initialization; glViewport inside on_resize
        // requires GLAD to be loaded first
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
            throw std::runtime_error("Failed to initialize GLAD");

        // Register GLFW callbacks
        glfwSetWindowUserPointer(handle, this);
        glfwSetFramebufferSizeCallback(handle, Window::glfw_resize_callback);

        // GL render state initialization is NOT done here;
        // that is the responsibility of App::init_gl()
    }

    ~Window() {
        if (handle) glfwDestroyWindow(handle);
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    bool should_close() const        { return glfwWindowShouldClose(handle); }
    void swap_buffers()              { glfwSwapBuffers(handle); }
    void make_context_current()      { glfwMakeContextCurrent(handle); }
    int  get_width()  const          { return width; }
    int  get_height() const          { return height; }

    void set_resize_callback(std::function<void(int, int)> cbf) {
        resize_callback = std::move(cbf);
    }

private:
    GLFWwindow* handle = nullptr;
    std::string title  = "SPATIUM";
    int width  = 0;
    int height = 0;
    std::function<void(int, int)> resize_callback;

    static void glfw_resize_callback(GLFWwindow* win, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        self->on_resize(w, h);
    }

    void on_resize(int w, int h) {
        width = w; height = h;
        glViewport(0, 0, w, h);  // viewport tracks window size — Window's responsibility
        if (resize_callback) resize_callback(w, h);
        // resize_callback guard is retained: Window may be instantiated directly
        // (e.g. in unit tests) without App injecting a callback
    }
};
```

**Constraints**:
- Constructor responsibilities: GLFW window creation, context creation, GLAD initialization, callback registration.
- GLAD lives in `Window` because `on_resize` calls `glViewport` — which requires GL functions to be loaded. The two are inseparable; moving GLAD to `App::init_gl()` would require moving `glViewport` out of `on_resize`, breaking the natural ownership of viewport-follows-window.
- `glfwSwapInterval` is controlled by the `vsync` constructor parameter; must not be hardcoded.
- `glViewport` is called from `on_resize`; GLFW guarantees this callback fires on the main thread during `glfwPollEvents`. If multi-threading is introduced in the future, this assumption must be revisited.
- GL render state (depth test, face culling, etc.) does **not** belong here; it is handled by `App::init_gl()`.
- `on_resize` must be private; must not be called directly from outside.
- `Window` must not include or hold a reference to `EventBus`.
- GLFW profile must be `GLFW_OPENGL_CORE_PROFILE`; `GLFW_OPENGL_COMPAT_PROFILE` is not permitted.
- Both copy and move are `= delete`: move is unsafe because lambdas in `resize_callback` may capture the old `this` (injected by `App`), and reassigning `glfwSetWindowUserPointer` alone is insufficient to resolve all dangling pointer issues.
- `make_context_current()` is retained as public for future multi-context scenarios; it has no use in the current single-window design.

---

#### 3.3.5 `App`

**Responsibility**: Owns and initializes `Window` and `EventBus`, acting as the sole glue layer between them — wiring callbacks to subscribers and driving the main render loop. Privately inherits `GlfwContext` to guarantee GLFW initialization order. `main.cpp` only needs to construct `App` and call `run()`.

**Members**:

| Access | Name | Type | Description |
|---|---|---|---|
| private | `event_bus` | `EventBus` | Composition; declaration must precede `window` |
| private | `window` | `Window` | Composition |

**Methods**:

| Access | Signature | Description |
|---|---|---|
| public | `App()` | Creates `Window`, calls `init_gl`, wires all callbacks and subscriptions |
| public | `void run()` | Runs the render loop until `window.should_close()` |
| private | `void init_gl()` | Initializes GL render state; called after `Window` is constructed |

**Implementation**:

```cpp
class App : private GlfwContext {
public:
    App() : GlfwContext(),                      // 1. glfwInit
            event_bus(),                        // 2. EventBus (explicit for clarity)
            window(1280, 720, "SPATIUM") {      // 3. Window; context, GLAD, vsync ready
        init_gl();                              // 4. GL state initialization

        // Wire Window → EventBus (App is the only place that knows both)
        window.set_resize_callback([&](int w, int h) {
            event_bus.emit(WindowResizeEvent{w, h});
        });

        // Register all subscribers in one place
        event_bus.subscribe<WindowResizeEvent>([](const WindowResizeEvent& e) {
            // glViewport is already handled inside Window::on_resize
            // Place other systems' resize reactions here (e.g., camera updates)
        });
    }

    void run() {
        while (!window.should_close()) {
            glfwPollEvents();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            // TODO: render
            window.swap_buffers();
        }
    }
    // ~App() → ~Window() → ~GlfwContext() (glfwTerminate) handled automatically

private:
    EventBus event_bus;  // must be declared before window
    Window   window;

    void init_gl() {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glViewport(0, 0, window.get_width(), window.get_height());
    }
};
```

**Constraints**:
- `GlfwContext` is inherited `private`; the inheritance relationship is not exposed externally.
- `event_bus` must be declared **before** `window` (C++ initializes members in declaration order).
- `event_bus` is explicitly listed in the initializer list for readability, even though default construction would occur automatically.
- `init_gl()` must be called at the start of the constructor body (requires an active context).
- `init_gl()` contains only `gl*` calls (render pipeline configuration); no GLFW functions.
- `App` is the only translation unit that includes both `Window` and `EventBus`.
- Manual `glfwTerminate` is not needed; `~GlfwContext()` handles it automatically.

---

### 3.4 Event Structs

Defined in a standalone `events.h` that any class may include:

```cpp
// events.h
#pragma once

struct WindowResizeEvent {
    int width;
    int height;
};

struct WindowCloseEvent {};

struct KeyPressEvent {
    int key;
    int scancode;
    int action;
    int mods;
};

struct MouseButtonEvent {
    int button;
    int action;
    int mods;
};

struct MouseScrollEvent {
    double xoffset;
    double yoffset;
};

struct CursorMoveEvent {
    double x;
    double y;
};
```

---

### 3.5 GL State Ownership

GL state configuration is part of render pipeline setup, not window management. Although GL state is bound to the context — whose lifetime matches the window — the *content* of that state is a rendering-layer decision and is therefore placed in `App::init_gl()` rather than `Window`. When a `Renderer` class is introduced later, `init_gl()` should be migrated there.

#### Required Settings

| Call | Description | If Omitted |
|---|---|---|
| `glEnable(GL_DEPTH_TEST)` | Enables depth testing; closer fragments occlude farther ones | Objects rendered in wrong depth order |
| `glEnable(GL_CULL_FACE)` | Enables back-face culling | Both faces rendered; wasted GPU cycles |
| `glCullFace(GL_BACK)` | Specifies back-face culling | Default is already `GL_BACK`, but explicit is safer |
| `glClearColor(0,0,0,1)` | Sets background color (black) | Background color is undefined after clear |
| `glViewport(0, 0, w, h)` | Sets the rendering area | Output may only cover part of the window |

#### Responsibility Assignment

| Call | Location | Rationale |
|---|---|---|
| `glEnable`, `glCullFace`, `glClearColor` | `App::init_gl()` | Render pipeline configuration; unrelated to window management |
| `glViewport` (initial) | `App::init_gl()` | Requires window dimensions; `App` has access via `get_width/height` |
| `glViewport` (on resize) | `Window::on_resize()` | Window size changed; naturally Window's responsibility |
| `glfwSwapInterval` | `Window` constructor | GLFW function, not GL; controlled by `vsync` parameter |

---

### 3.6 File Structure

```
spatium/
├── src/
│   ├── glfw_context.h      ← ADD: GlfwContext struct
│   ├── event_id.h          ← ADD: EventID class
│   ├── event_bus.h         ← ADD: EventBus class
│   ├── events.h            ← ADD: All event structs
│   ├── window.h            ← ADD: Window class
│   ├── window.cpp          ← ADD: Window implementation
│   ├── app.h               ← ADD: App class
│   ├── app.cpp             ← ADD: App implementation
│   └── main.cpp            ← ADD: Entry point
└── CMakeLists.txt          ← MODIFY: Add new sources, link GLFW/GLAD/OpenGL
```

---

## 4. Implementation Plan

### 4.1 Task Breakdown

- [ ] **Step 1**: Create `glfw_context.h`
  - Expected result: `GlfwContext` compiles; error callback + init/terminate lifecycle works
  - Completion signal: Can instantiate `GlfwContext` without crash

- [ ] **Step 2**: Create `event_id.h` and `event_bus.h`
  - Expected result: `EventBus` can `subscribe` and `emit` arbitrary struct types
  - Completion signal: Subscribe to a test event, emit it, listener receives correct data

- [ ] **Step 3**: Create `events.h`
  - Expected result: All event structs defined; no dependencies on any class header
  - Completion signal: File compiles standalone

- [ ] **Step 4**: Create `window.h` / `window.cpp`
  - Expected result: GLFW window opens with OpenGL 4.6 Core Profile; GLAD loaded; resize callback functional
  - Completion signal: Window opens, resize triggers `on_resize`, `glViewport` updates

- [ ] **Step 5**: Create `app.h` / `app.cpp`
  - Expected result: `App` composes all subsystems; render loop runs; resize event flows end-to-end
  - Completion signal: Black window opens, responds to resize, closes cleanly

- [ ] **Step 6**: Create `main.cpp`
  - Expected result: Entry point constructs `App` and calls `run()`
  - Completion signal: Full application runs end-to-end

- [ ] **Step 7**: Walk through §9 Implementation Verification Checklist

### 4.2 Configuration / Constants

```cpp
constexpr int  DEFAULT_WIDTH  = 1280;
constexpr int  DEFAULT_HEIGHT = 720;
constexpr char DEFAULT_TITLE[] = "SPATIUM";
constexpr bool DEFAULT_VSYNC  = true;
constexpr int  GL_VERSION_MAJOR = 4;
constexpr int  GL_VERSION_MINOR = 6;
```

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | Window creation | Default parameters | Window opens, GLAD loads, GL 4.6 context active | No exceptions; `glGetString(GL_VERSION)` reports 4.6 |
| T-02 | Window resize | Drag window border | `on_resize` called; `glViewport` updated; `WindowResizeEvent` emitted | Subscriber receives correct width/height |
| T-03 | EventBus no subscribers | `emit<WindowResizeEvent>(...)` with no subscribers | No crash | Returns normally |
| T-04 | EventBus type isolation | Subscribe to `KeyPressEvent`, emit `WindowResizeEvent` | `KeyPressEvent` listener not called | Listener invocation count remains 0 |
| T-05 | Clean shutdown | Close window | Destruction order: `~Window()` → `~GlfwContext()` | No segfault, no leak |
| T-06 | VSync parameter | `Window(..., vsync=false)` | `glfwSwapInterval(0)` called | Frame rate uncapped |

### 5.2 Validation Commands

```bash
cmake --build build/
./build/spatium
valgrind --leak-check=full ./build/spatium
```

### 5.3 Acceptance Criteria

- [ ] T-01 through T-06 pass
- [ ] FR-01 through FR-10 verified
- [ ] Compilation produces no warnings
- [ ] Valgrind reports no leaks
- [ ] §9 checklist fully checked

---

## 6. Constraints & Anti-Patterns

### 6.1 Hard Constraints (MUST NOT)

- ❌ Do NOT use `GLFW_OPENGL_COMPAT_PROFILE` — Core Profile only
- ❌ Do NOT use `glewInit` or any GLEW headers — this project uses GLAD
- ❌ Do NOT use `typeid`, `std::type_index`, or `dynamic_cast` in the event system
- ❌ Do NOT put GL render state calls (`glEnable`, `glCullFace`, `glClearColor`) inside `Window`
- ❌ Do NOT include `EventBus` from `Window`
- ❌ Do NOT provide move constructor/assignment for `Window` — `resize_callback` lambda capture makes it unsafe
- ❌ Do NOT call `glfwTerminate` manually from `App`
- ❌ Do NOT put GLFW functions inside `init_gl()` — it contains only `gl*` calls

### 6.2 Known Pitfalls & Limitations

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| No unsubscribe | Listeners cannot be removed | Low | Return a token; add `unsubscribe(token)` |
| Listener dangling | If a lambda's captured object is destroyed first, UB occurs | Medium | `weak_ptr` + validity check before dispatch |
| Single resize callback | `Window` only supports one external callback | Low | Upgrade to `std::vector` |
| `EventID::counter` not atomic | Concurrent first calls to `get<T>()` at startup are a data race | Low (single-threaded; all first calls happen at startup) | Switch to `std::atomic<int>` |
| Re-entrancy cost in `emit` | `emit` copies the vector; minor allocation overhead per call | Low | `emitting_depth` counter with pending queue |
| `glViewport` thread assumption | `on_resize` assumes it is called from the main thread via `glfwPollEvents` | Low (GLFW guarantees this currently) | Revisit if multi-threading is introduced |

---

## 7. Open Questions & Decision Log

### Open Questions

| # | Question | Owner | Deadline |
|---|----------|-------|----------|
| — | No open questions | — | — |

### Decision Log

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | GLAD lives in `Window`, not `App::init_gl()` | `on_resize` calls `glViewport`, which requires GL functions loaded. Moving GLAD out would require moving `glViewport` out of `on_resize`, breaking viewport-follows-window ownership. The two decisions are bound together. |
| D2 | `glfwSwapInterval` is a `Window` constructor parameter | `glfwSwapInterval` is a GLFW API, not `gl*`. It controls buffer-swap timing — a different layer from render state. Placing it in `init_gl()` would mix two distinct API layers. |
| D3 | `Window` disallows move (`= delete`) | `glfwSetWindowUserPointer` reassignment only fixes the GLFW side; `resize_callback` lambdas may capture the old `this`, causing dangling pointers. `App` holds `Window` by value and never moves it. |
| D4 | `EventID` uses `int` instead of `size_t` | Google C++ Style Guide discourages unsigned types for non-size purposes. `EventID::counter` is an incrementing ID, not a container size. |
| D5 | `emit` copies callback vector before iteration | Guards against re-entrancy. Accepted trade-off: minor allocation overhead per `emit` call. |
| D6 | `event_bus` explicitly listed in App initializer list | Default construction would occur automatically, but explicit listing makes initialization order visible to readers. |
| D7 | `make_context_current()` retained as public | Reserved for future multi-context scenarios. Has no use in the current single-window design. |

---

## 8. Progress Tracker

| Session | Date | Work Completed | Remaining |
|---------|------|----------------|-----------|
| 1 | 2025-07-XX | SDD finalized (v3.1) | Pending implementation |

---

## 9. Implementation Verification Checklist

> **Warning**: Verify every item after implementation. Do not skip or assume. Each item reflects a real pitfall.

### GlfwContext
- [ ] `glfwSetErrorCallback` is set before `glfwInit()`
- [ ] Return value of `glfwInit()` is checked; throws on failure
- [ ] `glfwTerminate()` is in the destructor, not called manually from `App`

### EventID
- [ ] Does not use `typeid` or `std::type_index`
- [ ] `counter` is `static inline int`

### EventBus
- [ ] `emit` copies the callback vector before iterating (re-entrancy guard)
- [ ] `emit` does not crash when listener list is empty
- [ ] `EventBus.h` does not include any concrete event struct

### Window
- [ ] `glfwWindowHint` called before `glfwCreateWindow`
- [ ] OpenGL version set to 4.6 Core Profile (not Compatibility)
- [ ] `glfwCreateWindow` return value null-checked; throws on failure
- [ ] `glfwMakeContextCurrent` called before `gladLoadGLLoader`
- [ ] `gladLoadGLLoader` return value checked; throws on failure
- [ ] `glewInit` is not used
- [ ] `glfwSwapInterval` controlled by `vsync` parameter; not hardcoded
- [ ] `glfwSetWindowUserPointer(handle, this)` called before any callback registration
- [ ] `glfw_resize_callback` is a `static` member function
- [ ] `on_resize` is `private`
- [ ] `glViewport` updated inside `on_resize`
- [ ] `~Window()` null-checks `handle` before `glfwDestroyWindow`
- [ ] Copy constructor and copy assignment are `= delete`
- [ ] Move constructor and move assignment are `= delete`
- [ ] `Window` does not include `EventBus`

### App
- [ ] Inheritance is `private GlfwContext`
- [ ] Initializer list order: `GlfwContext`, `event_bus`, `window`
- [ ] `event_bus` declared before `window` in member list
- [ ] `init_gl()` called at start of constructor body
- [ ] `init_gl()` contains only `gl*` calls; no GLFW functions
- [ ] `glEnable(GL_DEPTH_TEST)` called
- [ ] `glEnable(GL_CULL_FACE)` called
- [ ] `glCullFace(GL_BACK)` called
- [ ] `glClearColor` set
- [ ] `glViewport` set to initial value in `init_gl()`
- [ ] `set_resize_callback` injection completed in constructor
- [ ] `glfwPollEvents()` called every frame
- [ ] `glClear` called at start of every frame

---

## 10. Appendix

### 10.1 Related SDDs

- (None yet — future SDDs for Renderer, Camera, Input will reference this document)

*End of SDD v3.1*

---

## Outcome & Decisions

**Date completed:** 2026-04-14  
**Final status:** DONE

### Final Technical Choices

| Item | Choice |
|------|--------|
| File locations | `core/window/` for all window/event headers; `core/App.h/.cpp` for App |
| OpenGL version | 4.6 Core Profile as specified |
| GLAD loader | `reinterpret_cast<GLADloadproc>(glfwGetProcAddress)` — C-style cast avoided |
| Trailing underscore | Private members use `name_` convention (matches project's existing style) |
| `App` member naming | `event_bus` and `window` (snake_case per Google C++ Style Guide) |

### Decisions Carried Forward from SDD

All decisions D1–D7 in §7 were upheld without modification:
- D1: GLAD in `Window` (not `App`) — `on_resize` calls `glViewport`
- D2: `vsync` is a `Window` constructor param, not in `init_gl()`
- D3: `Window` move `= delete` — lambda capture safety
- D4–D7: as documented in §7

### Notes for Future SDDs

- `App::init_gl()` should migrate to a `Renderer` class once the rendering layer is introduced; it is a temporary home.
- `EventBus` has no `unsubscribe` — implement token-based removal before any subscriber needs to be destroyed before `App`.
- `EventID::counter` is not atomic — safe currently (single-threaded startup), but revisit if threading is added.
- `Window` only supports one external resize callback; upgrade to `std::vector` if multiple systems need direct notification.