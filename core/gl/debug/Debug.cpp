//
// Created by gabriel on 4/16/26.
//

#include "Debug.h"

#include <iostream>

std::string_view Debug::glDebugSourceStr(GLenum source)
{
    switch (source) {
    case GL_DEBUG_SOURCE_API:             return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "Window System";
    case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:     return "Third Party";
    case GL_DEBUG_SOURCE_APPLICATION:     return "Application";
    case GL_DEBUG_SOURCE_OTHER:           return "Other";
    default:                              return "Unknown";
    }
}

std::string_view Debug::glDebugTypeStr(GLenum type)
{
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:               return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "Undefined Behavior";
    case GL_DEBUG_TYPE_PERFORMANCE:         return "Performance";
    case GL_DEBUG_TYPE_PORTABILITY:         return "Portability";
    case GL_DEBUG_TYPE_MARKER:              return "Marker";
    case GL_DEBUG_TYPE_PUSH_GROUP:          return "Push Group";
    case GL_DEBUG_TYPE_POP_GROUP:           return "Pop Group";
    case GL_DEBUG_TYPE_OTHER:               return "Other";
    default:                                return "Unknown";
    }
}

std::string_view Debug::glDebugSeverityStr(GLenum severity)
{
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:         return "HIGH";
    case GL_DEBUG_SEVERITY_MEDIUM:       return "MEDIUM";
    case GL_DEBUG_SEVERITY_LOW:          return "LOW";
    case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
    default:                             return "Unknown";
    }
}


void Debug::MessageCallback(
    GLenum        source,
    GLenum        type,
    GLuint        id,
    GLenum        severity,
    GLsizei       length,
    const GLchar* message,
    const void*   userParam
) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

    std::cerr << "[GL Debug]\n"
              << "  Source:   " << glDebugSourceStr(source)   << "\n"
              << "  Type:     " << glDebugTypeStr(type)       << "\n"
              << "  Severity: " << glDebugSeverityStr(severity) << "\n"
              << "  ID:       " << id                         << "\n"
              << "  Message:  " << message                    << "\n";

    if (severity == GL_DEBUG_SEVERITY_HIGH) {
        __builtin_trap();
    }
}

void Debug::enable()
{
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(MessageCallback, 0);
}