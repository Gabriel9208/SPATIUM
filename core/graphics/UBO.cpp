#include "UBO.h"

UBO::UBO(const unsigned int _size)
{
	initialize(_size);
}

UBO& UBO::operator=(UBO&& other) noexcept
{

	if (this != &other)
	{
		std::exchange(other.size_, 0);
		BufferObject::operator=(std::move(other));
	}
	return *this;
}

void UBO::associate(unsigned int program, const char* uniformBlockName, unsigned int bindingPoint) const
{
	int UBOsize = 0;
	int idx = glGetUniformBlockIndex(program, uniformBlockName);

	glGetActiveUniformBlockiv(program, idx, GL_UNIFORM_BLOCK_DATA_SIZE, &UBOsize);

	//bind UBO to its binding point
	glBindBufferRange(GL_UNIFORM_BUFFER, bindingPoint, id_, 0, UBOsize);

	// get the uniform with index idx from the binding point bindingPoint
	glUniformBlockBinding(program, idx, bindingPoint);
}

void UBO::fill_data(GLintptr offset, GLintptr size, const void* data) const
{
	bind();
	glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
	unbind();
}


void UBO::initialize(unsigned int _size)
{
	glGenBuffers(1, &id_);
	glBindBuffer(GL_UNIFORM_BUFFER, id_);
	glBufferData(GL_UNIFORM_BUFFER, _size, 0, GL_DYNAMIC_DRAW);
	size_ = _size;
}

void UBO::bind() const
{
	glBindBuffer(GL_UNIFORM_BUFFER, id_);
}

void UBO::unbind() const
{
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
