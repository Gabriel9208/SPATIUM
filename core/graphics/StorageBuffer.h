#pragma once

#include <vector>
#include <glad/glad.h>

#include "BufferObject.h"

template<class T>
class StorageBuffer: public BufferObject
{
protected:
	unsigned int size_ = 0;

public:
	StorageBuffer() : BufferObject() {}
	StorageBuffer(StorageBuffer&& other) noexcept: BufferObject(std::move(other)) {}
	~StorageBuffer() override = default;

	StorageBuffer& operator=(StorageBuffer&& other) noexcept
	{
		if (this != &other)
		{
			std::exchange(other.size, 0);
			BufferObject::operator=(std::move(other));
		}

		return *this;
	}

	virtual void initialize(unsigned int _size, GLuint usageMode = GL_STATIC_DRAW) = 0;
	virtual void initialize(const std::vector<T>& v, GLuint usageMode = GL_STATIC_DRAW) = 0;

	void invalid() const;

	static void copy_and_write(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size);

	[[nodiscard]] inline unsigned int getSize() const { return size_; }
	[[nodiscard]] inline unsigned int getId() const { return id_; }
};

template< typename T >
void StorageBuffer<T>::copy_and_write(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size)
{
	glBindBuffer(GL_COPY_WRITE_BUFFER, writeBuffer);
	glBindBuffer(GL_COPY_READ_BUFFER, readBuffer);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size);

	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
}

template<class T>
void StorageBuffer<T>::invalid() const
{
	glInvalidateBufferData(id_);
}
