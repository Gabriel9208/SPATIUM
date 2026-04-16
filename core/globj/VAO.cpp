#include "VAO.h"

#include <glad/glad.h>

VAO::VAO()
{
	glGenVertexArrays(1, &id_);
	glBindVertexArray(id_);
}

VAO::~VAO()
{
	if (id_ != 0)
	{
		glDeleteVertexArrays(1, &id_);
	}
}

VAO& VAO::operator=(VAO&& other) noexcept
{
	if (this != &other)
	{
		if (id_ != 0)
		{
			glDeleteVertexArrays(1, &id_);
		}

		std::exchange(other.id_, 0);
	}

	return *this;
}

void VAO::bind() const
{
	glBindVertexArray(id_);
}

void VAO::unbind() const
{
	glBindVertexArray(0);
}