#pragma once

#include "../Utilty/Error.h"
#include <vector>
#include <glad/glad.h>
#include <iostream>

template<class T>
class StorageBuffer
{
protected:
	GLuint id;
	unsigned int size;

public:
	StorageBuffer() : id(0), size(0) {}
	StorageBuffer(const std::vector<T>& v);

	StorageBuffer(StorageBuffer&& other) noexcept;
	StorageBuffer(const StorageBuffer& other) = delete;

	~StorageBuffer();

	StorageBuffer& operator=(const StorageBuffer& other) = delete;
	StorageBuffer& operator=(StorageBuffer&& other) noexcept;

	virtual void initialize(unsigned int _size, GLuint usageMode = GL_STATIC_DRAW) = 0;
	virtual void initialize(const std::vector<T>& v, GLuint usageMode = GL_STATIC_DRAW) = 0;

	virtual void bind() const = 0;
	virtual void unbind() const = 0;
	void invalid() const;

	static void copyAndWrite(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size);

	inline unsigned int getSize() const { return size; }
	inline unsigned int getId() const { return id; }
};

template< typename T >
void StorageBuffer<T>::copyAndWrite(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size)
{
	GLCall(glBindBuffer(GL_COPY_WRITE_BUFFER, writeBuffer));
	GLCall(glBindBuffer(GL_COPY_READ_BUFFER, readBuffer));
	GLCall(glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size));

	GLCall(glBindBuffer(GL_COPY_WRITE_BUFFER, 0));
	GLCall(glBindBuffer(GL_COPY_READ_BUFFER, 0));
}

template<class T>
StorageBuffer<T>::StorageBuffer(const std::vector<T>& v)
{
	initialize(v);
}

template<class T>
StorageBuffer<T>::StorageBuffer(StorageBuffer<T>&& other) noexcept : id(other.id), size(other.size)
{
	other.id = 0;
}


template<class T>
StorageBuffer<T>::~StorageBuffer()
{
	if (id != 0)
	{
		GLCall(glDeleteBuffers(1, &id));
	}
}

template<class T>
StorageBuffer<T>& StorageBuffer<T>::operator=(StorageBuffer<T>&& other) noexcept
{
	if (this != &other)
	{
		if (id != 0)
		{
			GLCall(glDeleteBuffers(1, &id));
		}

		id = other.id;
		other.id = 0;
	}

	return *this;
}

template<class T>
void StorageBuffer<T>::invalid() const
{
	GLCall(glInvalidateBufferData(id));
}
