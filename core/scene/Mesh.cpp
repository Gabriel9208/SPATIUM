#include "core/scene/Mesh.h"

Mesh::Mesh(int id,
           std::string name,
           VAO vao,
           VBO<Vertex> vbo,
           EBO ebo,
           std::size_t index_count)
    : id_(id)
    , name_(std::move(name))
    , vao_(std::move(vao))
    , vbo_(std::move(vbo))
    , ebo_(std::move(ebo))
    , index_count_(index_count)
{}
