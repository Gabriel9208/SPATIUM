#pragma once

#include <glad/glad.h>

class FBO
{
private:
	GLuint id = 0;

public:
	FBO();
	FBO(FBO&& other) noexcept;
	FBO(const FBO& other) = delete;

	~FBO();

	FBO& operator=(const FBO& other) = delete;
	FBO& operator=(FBO&& other) noexcept;


	void bind() const;
	static void unbind() ;
	void copy(const FBO& other, int width, int height) const;
};