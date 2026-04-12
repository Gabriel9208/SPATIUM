#pragma once

#include <glad/glad.h>

#include "BufferObject.h"

class UBO: public BufferObject
{
private:
	unsigned int size = 0;

public:
	UBO() : BufferObject() {}
	explicit UBO(unsigned int _size);
	UBO(UBO&& other) noexcept:
	BufferObject(std::move(other)), size(std::exchange(other.size, 0)) {}
	UBO(const UBO& other) = delete;
	~UBO() override = default;

	UBO& operator=(const UBO& other) = delete;
	UBO& operator=(UBO&& other) noexcept;

	void associateWithShaderBlock(unsigned int program, const char* uniformBlockName, unsigned int bindingPoint) const;
	void fillInData(GLintptr offset, GLintptr size, const void* data) const;

	void initialize(unsigned int _size);
	void bind() const override;
	void unbind() const override;
};

