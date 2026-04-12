#pragma once

#include <glad/glad.h>
#include <vector>

#include "BufferObject.h"

class EBO: public BufferObject
{
private:
	unsigned int count;

public:
	EBO();
	explicit EBO( const std::vector<unsigned int>& v );
	EBO( EBO&& other ) noexcept:
	BufferObject( std::move( other )), count( other.count )
	{}
	~EBO() override;

	EBO& operator=( EBO&& other ) noexcept;

	void initialize( unsigned int size ) const;
	void initialize( const std::vector<unsigned int>& v );

	void bind() const override;
	void unbind() const override;

	static void copyAndWrite(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size);

	[[nodiscard]] unsigned int getCount() const { return count; }
	[[nodiscard]] GLuint getId() const { return id; }
};