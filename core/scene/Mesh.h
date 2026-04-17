#pragma once

#include <string>
#include <cstddef>

#include "core/gl/obj/VAO.h"
#include "core/gl/obj/VBO.h"
#include "core/gl/obj/EBO.h"
#include "core/scene/Vertex.h"

class Mesh {
public:
    Mesh(int id,
         std::string name,
         VAO vao,
         VBO<Vertex> vbo,
         EBO ebo,
         std::size_t index_count);

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept        = default;
    Mesh& operator=(Mesh&&) noexcept = default;
    ~Mesh()                      = default;

    [[nodiscard]] int                get_id()          const noexcept { return id_; }
    [[nodiscard]] const std::string& get_name()        const noexcept { return name_; }

    // std::size_t so 3DGS-scale meshes (> INT_MAX indices) are representable.
    // Cast to GLsizei at the glDrawElements call site in Renderer.
    [[nodiscard]] std::size_t        get_index_count() const noexcept { return index_count_; }

    // Binds the VAO. This is the only public entry point for VAO binding.
    void bind() const { vao_.bind(); }

private:
    int           id_;
    std::string   name_;
    VAO           vao_;
    VBO<Vertex>   vbo_;
    EBO           ebo_;
    std::size_t   index_count_;
};
