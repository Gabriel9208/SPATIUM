#pragma once

#include <glad/glad.h>

class UBO
{
private:
	GLuint id;
	unsigned int size;

public:
	UBO() : id(0) {}
	UBO(unsigned int _size);
	UBO(UBO&& other) noexcept;
	UBO(const UBO& other) = delete;

	~UBO();

	UBO& operator=(const UBO& other) = delete;
	UBO& operator=(UBO&& other) noexcept;

	void associateWithShaderBlock(unsigned int program, const char* uniformBlockName, unsigned int bindingPoint);
	void fillInData(GLintptr offset, GLintptr size, const void* data);

	void initialize(unsigned int _size);
	void bind() const;
	void unbind() const;
};

