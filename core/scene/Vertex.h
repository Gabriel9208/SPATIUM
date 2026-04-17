#pragma once

#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

static_assert(sizeof(Vertex) == 2 * sizeof(glm::vec3),
              "Vertex must be tightly packed for VBO<Vertex> upload.");
