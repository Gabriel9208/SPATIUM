#pragma once

#include <functional>
#include <string>

#include <GLFW/glfw3.h>

#include "core/event/events.h"

class Window {
public:
    explicit Window(int width, int height, std::string title = "SPATIUM", bool vsync = true);
    ~Window();

    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&)                 = delete;
    Window& operator=(Window&&)      = delete;

    [[nodiscard]] bool should_close()         const { return glfwWindowShouldClose(handle_); }
    void               swap_buffers()         const { glfwSwapBuffers(handle_); }
    void               make_context_current() const { glfwMakeContextCurrent(handle_); }

    [[nodiscard]] int   get_width()        const { return width_; }
    [[nodiscard]] int   get_height()       const { return height_; }
    [[nodiscard]] float get_aspect_ratio() const;

    void set_resize_callback    (std::function<void(int, int)>               cbf);
    void set_key_callback       (std::function<void(const KeyEvent&)>        cb);
    void set_mouse_move_callback(std::function<void(const MouseMoveEvent&)>  cb);
    void set_click_callback     (std::function<void(const MouseClickEvent&)> cb);
    void set_scroll_callback    (std::function<void(const ScrollEvent&)>     cb);

private:
    GLFWwindow*                    handle_         = nullptr;
    std::string                    title_;
    int                            width_          = 0;
    int                            height_         = 0;
    std::function<void(int, int)>  resize_callback_;

    std::function<void(const KeyEvent&)>        key_callback_;
    std::function<void(const MouseMoveEvent&)>  mouse_move_callback_;
    std::function<void(const MouseClickEvent&)> click_callback_;
    std::function<void(const ScrollEvent&)>     scroll_callback_;

    double last_mouse_x_   = 0.0;
    double last_mouse_y_   = 0.0;
    bool   has_last_mouse_ = false;

    static void glfw_resize_callback(GLFWwindow* win, int w, int h);
    static void glfw_key_callback   (GLFWwindow* win, int key, int scancode, int action, int mods);
    static void glfw_cursor_callback(GLFWwindow* win, double x, double y);
    static void glfw_mouse_callback (GLFWwindow* win, int button, int action, int mods);
    static void glfw_scroll_callback(GLFWwindow* win, double x_off, double y_off);

    void on_resize(int w, int h);
    void on_key   (int glfw_key, int glfw_action);
    void on_mouse (double x, double y);
    void on_click (int glfw_button, int glfw_action, double x, double y);
    void on_scroll(double dy);
};
