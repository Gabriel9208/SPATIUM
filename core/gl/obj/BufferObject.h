#pragma once

#include <utility>

#include <glad/glad.h>

#include "core/gl/interfaces/IGPUResource.h"

class BufferObject : public IGPUResource
{
protected:
    GLuint id_{0};

    explicit BufferObject() = default;

    BufferObject(BufferObject&& other) noexcept :
        id_(std::exchange(other.id_, 0))
        {}

public:
    BufferObject(const BufferObject& other) = delete;
    ~BufferObject() override
    {
        if (id_ != 0)
        {
            glDeleteBuffers(1, &id_);
        }
    }

    BufferObject& operator=(BufferObject&& other) noexcept
    {
        if (this != &other)
        {
            if (id_ != 0)
            {
                glDeleteBuffers(1, &id_);
            }
            std::exchange(other.id_, 0);
        }

        return *this;
    }

    BufferObject& operator=(const BufferObject& other) = delete;
};
