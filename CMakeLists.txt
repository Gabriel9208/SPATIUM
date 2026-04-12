cmake_minimum_required(VERSION 3.15)
project(SPATIUM)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(glm CONFIG REQUIRED)

set(CORE_SOURCES
    core/glad.c
        core/graphics/EBO.cpp
        core/graphics/FBO.cpp
        core/graphics/UBO.cpp
        core/graphics/VAO.cpp
        core/graphics/shader_program/ShaderProgram.cpp
        core/graphics/shader_program/GraphicShader.cpp
        core/graphics/shader_program/ComputeShader.cpp
        core/graphics/BufferObject.h
        core/graphics/interfaces/IGPUResource.h
        core/graphics/VBO.h
        core/graphics/SSBO.h
)

add_executable(SPATIUM
    main.cpp
    ${CORE_SOURCES}
)

target_include_directories(SPATIUM PRIVATE
        ${CMAKE_SOURCE_DIR}          # lets Core files include each other as "Core/..."
        ${CMAKE_SOURCE_DIR}/include   # vendored GLAD headers (glad/glad.h, KHR/khrplatform.h)
        ${CMAKE_SOURCE_DIR}/external SYSTEM  # vendored GLAD headers (glad/glad.h, KHR/khrplatform.h)
)

target_link_libraries(SPATIUM PRIVATE
    glfw
    OpenGL::GL
    glm::glm
)
