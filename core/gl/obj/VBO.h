#pragma once
#include <glad/glad.h>

#include <vector>
#include <iostream>

#include "StorageBuffer.h"
#include "BufferObject.h"

template<class T>
class VBO: public StorageBuffer<T>
{
public:
	VBO() : StorageBuffer<T>() {}
	explicit VBO(const std::vector<T>& v, GLuint usageMode);

	VBO(VBO&& other) noexcept;
	~VBO() override = default;

	VBO& operator=(VBO&& other) noexcept;

	void initialize(unsigned int _size, GLuint usageMode) override;
	void initialize(const std::vector<T>& v, GLuint usageMode) override;

	void bind() const override;
	void unbind() const override;
	void set_data(const std::vector<T>& v, GLuint usageMode);
};

template<class T>
VBO<T>::VBO(const std::vector<T>& v, const GLuint usageMode): StorageBuffer<T>()
{
	VBO<T>::initialize(v, usageMode);
}

template<class T>
VBO<T>::VBO(VBO<T>&& other) noexcept : StorageBuffer<T>(std::move(other))
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
void VBO<T>::initialize(unsigned int _size, const GLuint usageMode)
{
	glGenBuffers(1, &this->id_);
	glBindBuffer(GL_ARRAY_BUFFER, this->id_);
	glBufferData(GL_ARRAY_BUFFER, _size, nullptr, usageMode);
	this->size_ = _size;
}

template<class T>
void VBO<T>::initialize(const std::vector<T>& v, const GLuint usageMode)
{
	if (v.empty())
	{
		std::cout << "VBO initialization fails: Empty vector." << std::endl;
		return;
	}

	glGenBuffers(1, &this->id_);
	glBindBuffer(GL_ARRAY_BUFFER, this->id_);
	glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(T), v.data(), usageMode);
	this->size_ = v.size();
}

template<class T>
void VBO<T>::bind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, this->id_);
}

template<class T>
void VBO<T>::unbind() const
{
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

template<class T>
void VBO<T>::set_data(const std::vector<T>& v, GLuint usageMode)
{
	glBindBuffer(GL_ARRAY_BUFFER, this->id_);
	glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(T), v.data(), usageMode);
	this->size_ = v.size();
}
