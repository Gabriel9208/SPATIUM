cmake_minimum_required(VERSION 3.15)
project(SPATIUM)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(SPATIUM main.cpp)
target_link_libraries(SPATIUM PRIVATE glfw OpenGL::GL)