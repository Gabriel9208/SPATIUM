#pragma once

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

class EBO
{
private:
	GLuint id;
	unsigned int count;

public:
	EBO();
	EBO( const std::vector<unsigned int>& v );
	EBO( EBO&& other ) noexcept;
	EBO( const EBO& other ) = delete;
	~EBO();

	EBO& operator=( EBO&& other ) noexcept;
	EBO& operator=( const EBO& other ) = delete;

	void initialize( unsigned int size );
	void initialize( const std::vector<unsigned int>& v );

	void bind();
	void unbind();

	static void copyAndWrite(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size);

	inline int getCount() const { return count; }
	inline GLuint getId() const { return id; }
};