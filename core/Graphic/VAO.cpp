#include "VAO.h"
#include "../Utilty/Error.h"

#include <glad/glad.h>


VAO::VAO()
{
	GLCall(glGenVertexArrays(1, &id));
	GLCall(glBindVertexArray(id));
}

VAO::VAO(VAO&& other) noexcept : id(other.id)
{
	other.id = 0;
}

VAO::~VAO()
{
	GLCall(glDeleteVertexArrays(1, &id));
}

VAO& VAO::operator=(VAO&& other) noexcept
{
	if (this != &other)
	{
		if (id != 0)
		{
			GLCall(glDeleteVertexArrays(1, &id));
		}

		id = other.id;
		other.id = 0;
	}

	return *this;
}

void VAO::bind() const
{
	GLCall(glBindVertexArray(id));
}

void VAO::unbind() const
{
	GLCall(glBindVertexArray(0));
}
