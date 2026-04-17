//
// Created by gabriel on 4/16/26.
//

#ifndef SPATIUM_DEBUG_H
#define SPATIUM_DEBUG_H

#include <string_view>

#include <glad/glad.h>

class Debug {
private:
    static std::string_view glDebugSourceStr(GLenum source);
    static std::string_view glDebugTypeStr(GLenum type);
    static std::string_view glDebugSeverityStr(GLenum severity);
    static void MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                       GLsizei length, const GLchar* message, const void*   userParam );

public:
    Debug() = delete;


    static void enable();
};



#endif //SPATIUM_DEBUG_H
