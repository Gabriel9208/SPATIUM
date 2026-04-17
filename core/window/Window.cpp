#include <glad/glad.h>
#include "core/window/Window.h"

#include <optional>
#include <stdexcept>

// ---------------------------------------------------------------------------
// GLFW code → SPATIUM enum translators (anonymous namespace, file-local)
// ---------------------------------------------------------------------------
namespace {

std::optional<Key> TranslateGlfwKey(int glfw_key) {
    switch (glfw_key) {
        case GLFW_KEY_W:     return Key::kW;
        case GLFW_KEY_A:     return Key::kA;
        case GLFW_KEY_S:     return Key::kS;
        case GLFW_KEY_D:     return Key::kD;
        case GLFW_KEY_UP:    return Key::kUp;
        case GLFW_KEY_DOWN:  return Key::kDown;
        case GLFW_KEY_LEFT:  return Key::kLeft;
        case GLFW_KEY_RIGHT: return Key::kRight;
        default:             return std::nullopt;
    }
}

KeyAction TranslateGlfwAction(int glfw_action) {
    switch (glfw_action) {
        case GLFW_PRESS:   return KeyAction::kPress;
        case GLFW_RELEASE: return KeyAction::kRelease;
        case GLFW_REPEAT:  return KeyAction::kRepeat;
        default:           return KeyAction::kRelease;  // defensive fallback
    }
}

std::optional<Button> TranslateGlfwButton(int glfw_button) {
    switch (glfw_button) {
        case GLFW_MOUSE_BUTTON_LEFT:   return Button::kLeft;
        case GLFW_MOUSE_BUTTON_RIGHT:  return Button::kRight;
        case GLFW_MOUSE_BUTTON_MIDDLE: return Button::kMiddle;
        default:                       return std::nullopt;
    }
}

ClickAction TranslateGlfwClickAction(int glfw_action) {
    return (glfw_action == GLFW_PRESS) ? ClickAction::kPress : ClickAction::kRelease;
}

}  // namespace

Window::Window(int width, int height, std::string title, bool vsync)
    : title_(std::move(title)), width_(width), height_(height)
{
    // Window hints must be set before glfwCreateWindow
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    handle_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!handle_)
        throw std::runtime_error("Failed to create GLFW window");

    glfwMakeContextCurrent(handle_);
    glfwSwapInterval(vsync ? 1 : 0);

    // GLAD must be loaded after context is current; on_resize calls glViewport
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to initialize GLAD");

    // Register callbacks — user pointer must be set first
    glfwSetWindowUserPointer(handle_, this);
    glfwSetFramebufferSizeCallback(handle_, Window::glfw_resize_callback);
    glfwSetKeyCallback        (handle_, Window::glfw_key_callback);
    glfwSetCursorPosCallback  (handle_, Window::glfw_cursor_callback);
    glfwSetMouseButtonCallback(handle_, Window::glfw_mouse_callback);
    glfwSetScrollCallback     (handle_, Window::glfw_scroll_callback);
}

Window::~Window() {
    if (handle_) glfwDestroyWindow(handle_);
}

float Window::get_aspect_ratio() const {
    return (height_ > 0)
        ? static_cast<float>(width_) / static_cast<float>(height_)
        : 1.0f;
}

void Window::set_resize_callback(std::function<void(int, int)> cbf) {
    resize_callback_ = std::move(cbf);
}

void Window::set_key_callback(std::function<void(const KeyEvent&)> cb) {
    key_callback_ = std::move(cb);
}

void Window::set_mouse_move_callback(std::function<void(const MouseMoveEvent&)> cb) {
    mouse_move_callback_ = std::move(cb);
}

void Window::set_click_callback(std::function<void(const MouseClickEvent&)> cb) {
    click_callback_ = std::move(cb);
}

void Window::set_scroll_callback(std::function<void(const ScrollEvent&)> cb) {
    scroll_callback_ = std::move(cb);
}

// ---------------------------------------------------------------------------
// Static GLFW callbacks — retrieve Window* and forward to instance handlers
// ---------------------------------------------------------------------------

void Window::glfw_resize_callback(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->on_resize(w, h);
}

void Window::glfw_key_callback(GLFWwindow* win, int key, int /*scancode*/,
                                int action, int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->on_key(key, action);
}

void Window::glfw_cursor_callback(GLFWwindow* win, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->on_mouse(x, y);
}

void Window::glfw_mouse_callback(GLFWwindow* win, int button, int action,
                                  int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(win, &x, &y);
    self->on_click(button, action, x, y);
}

void Window::glfw_scroll_callback(GLFWwindow* win, double /*x_off*/, double y_off) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->on_scroll(y_off);
}

// ---------------------------------------------------------------------------
// Instance-side handlers
// ---------------------------------------------------------------------------

void Window::on_resize(int w, int h) {
    width_  = w;
    height_ = h;
    glViewport(0, 0, w, h);
    if (resize_callback_) resize_callback_(w, h);
}

void Window::on_key(int glfw_key, int glfw_action) {
    auto key = TranslateGlfwKey(glfw_key);
    if (!key) return;
    if (!key_callback_) return;
    key_callback_(KeyEvent{*key, TranslateGlfwAction(glfw_action)});
}

void Window::on_mouse(double x, double y) {
    if (!has_last_mouse_) {
        last_mouse_x_   = x;
        last_mouse_y_   = y;
        has_last_mouse_ = true;
        return;  // suppress first callback — delta would be x - 0
    }
    const double dx = x - last_mouse_x_;
    const double dy = y - last_mouse_y_;
    last_mouse_x_ = x;
    last_mouse_y_ = y;
    if (mouse_move_callback_) {
        mouse_move_callback_(MouseMoveEvent{dx, dy});
    }
}

void Window::on_click(int glfw_button, int glfw_action, double x, double y) {
    auto btn = TranslateGlfwButton(glfw_button);
    if (!btn) return;
    if (!click_callback_) return;
    click_callback_(MouseClickEvent{*btn, TranslateGlfwClickAction(glfw_action), x, y});
}

void Window::on_scroll(double dy) {
    if (scroll_callback_) {
        scroll_callback_(ScrollEvent{dy});
    }
}
