#include <glad/glad.h>
#include "core/window/Window.h"

#include <stdexcept>

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
}

Window::~Window() {
    if (handle_) glfwDestroyWindow(handle_);
}

void Window::set_resize_callback(std::function<void(int, int)> cbf) {
    resize_callback_ = std::move(cbf);
}

void Window::glfw_resize_callback(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    self->on_resize(w, h);
}

void Window::on_resize(int w, int h) {
    width_  = w;
    height_ = h;
    glViewport(0, 0, w, h);
    if (resize_callback_) resize_callback_(w, h);
}
