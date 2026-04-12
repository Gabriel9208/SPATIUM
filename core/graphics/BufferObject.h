#pragma once

#include <utility>

#include <glad/glad.h>

#include "core/graphics/interfaces/IGPUResource.h"

class BufferObject : public IGPUResource
{
protected:
    GLuint id{0};

    explicit BufferObject() = default;

    BufferObject(BufferObject&& other) noexcept :
        id(std::exchange(other.id, 0))
        {}

public:
    BufferObject(const BufferObject& other) = delete;
    ~BufferObject() override
    {
        if (id != 0)
        {
            glDeleteBuffers(1, &id);
        }
    }

    BufferObject& operator=(BufferObject&& other) noexcept
    {
        if (this != &other)
        {
            if (id != 0)
            {
                glDeleteBuffers(1, &id);
            }
            std::exchange(other.id, 0);
        }

        return *this;
    }

    BufferObject& operator=(const BufferObject& other) = delete;
};
