#include "UBO.h"

#include "../Utilty/Error.h"

UBO::UBO(unsigned int _size)
{
	initialize(_size);
}

UBO::UBO(UBO&& other) noexcept:
	id(other.id),
	size(other.size)
{
	other.id = 0;
}

UBO::~UBO()
{
	GLCall(glDeleteBuffers(1, &id));
}

UBO& UBO::operator=(UBO&& other) noexcept
{
	if (this != &other)
	{
		if (id != 0)
		{
			GLCall(glDeleteBuffers(1, &id));
		}
		size = other.size;
		id = other.id;
		other.id = 0;
	}
	return *this;
}

void UBO::associateWithShaderBlock(unsigned int program, const char* uniformBlockName, unsigned int bindingPoint)
{
	int UBOsize = 0;
	GLCall(int idx = glGetUniformBlockIndex(program, uniformBlockName));

	GLCall(glGetActiveUniformBlockiv(program, idx, GL_UNIFORM_BLOCK_DATA_SIZE, &UBOsize));
	
	//bind UBO to its binding point
	GLCall(glBindBufferRange(GL_UNIFORM_BUFFER, bindingPoint, id, 0, UBOsize));
	
	// get the uniform with index idx from the binding point bindingPoint
	GLCall(glUniformBlockBinding(program, idx, bindingPoint));
}

void UBO::fillInData(GLintptr offset, GLintptr size, const void* data)
{
	bind();
	GLCall(glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data));
	unbind();
}


void UBO::initialize(unsigned int _size)
{
	GLCall(glGenBuffers(1, &id));
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, id));
	GLCall(glBufferData(GL_UNIFORM_BUFFER, _size, 0, GL_DYNAMIC_DRAW));
	size = _size;
}

void UBO::bind() const
{
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, id));
}

void UBO::unbind() const
{
	GLCall(glBindBuffer(GL_UNIFORM_BUFFER, 0));
}
