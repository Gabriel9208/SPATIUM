#pragma once
#include "StorageBuffer.h"
#include "../Utilty/Error.h"
#include <vector>
#include <glad/glad.h>
#include <iostream>

template<class T>
class VBO: public StorageBuffer<T>
{
public:
	VBO() : StorageBuffer<T>() {}
	VBO(const std::vector<T>& v);

	VBO(VBO&& other) noexcept;
	VBO(const VBO& other) = delete;

	~VBO();

	VBO& operator=(const VBO& other) = delete;
	VBO& operator=(VBO&& other) noexcept;

	void initialize(unsigned int _size, GLuint usageMode = GL_STATIC_DRAW);
	void initialize(const std::vector<T>& v, GLuint usageMode = GL_STATIC_DRAW);

	void bind() const;
	void unbind() const;
	void setData(const std::vector<T>& v, GLuint usageMode);
};

template<class T>
VBO<T>::VBO(const std::vector<T>& v): StorageBuffer<T>()
{
	initialize(v);
}

template<class T>
VBO<T>::VBO(VBO<T>&& other) noexcept : StorageBuffer<T>(std::move(other))
{}


template<class T>
VBO<T>::~VBO()
{}

template<class T>
VBO<T>& VBO<T>::operator=(VBO<T>&& other) noexcept
{
	if (this != &other)
	{
		StorageBuffer<T>::operator=(std::move(other));
	}

	return *this;
}

template<class T>
void VBO<T>::initialize(unsigned int _size, GLuint usageMode)
{
	GLCall(glGenBuffers(1, &this->id));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, this->id));
	GLCall(glBufferData(GL_ARRAY_BUFFER, _size, NULL, usageMode));
	this->size = _size;
}

template<class T>
void VBO<T>::initialize(const std::vector<T>& v, GLuint usageMode)
{
	if (v.size() == 0)
	{
		std::cout << "VBO initialization fails: Empty vector." << std::endl;
		return;
	}

	GLCall(glGenBuffers(1, &this->id));
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, this->id));
	GLCall(glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(T), v.data(), usageMode));
	this->size = v.size();
}

template<class T>
void VBO<T>::bind() const
{
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, this->id));
}

template<class T>
void VBO<T>::unbind() const
{
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

template<class T>
inline void VBO<T>::setData(const std::vector<T>& v, GLuint usageMode)
{
	GLCall(glBindBuffer(GL_ARRAY_BUFFER, this->id));
	GLCall(glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(T), v.data(), usageMode));
	this->size = v.size();
}
