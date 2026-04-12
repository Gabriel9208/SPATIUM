#include "VAO.h"

#include <glad/glad.h>

VAO::VAO()
{
	glGenVertexArrays(1, &id);
	glBindVertexArray(id);
}

VAO::~VAO()
{
	if (id != 0)
	{
		glDeleteVertexArrays(1, &id);
	}
}

VAO& VAO::operator=(VAO&& other) noexcept
{
	if (this != &other)
	{
		if (id != 0)
		{
			glDeleteVertexArrays(1, &id);
		}

		std::exchange(other.id, 0);
	}

	return *this;
}

void VAO::bind() const
{
	glBindVertexArray(id);
}

void VAO::unbind() const
{
	glBindVertexArray(0);
}