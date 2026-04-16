#include "EBO.h"

EBO::EBO() : BufferObject(), count_( 0 )
{
	glCreateBuffers( 1, &id_ );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id_ );
}

EBO::EBO( const std::vector<unsigned int>& v )
{
	glCreateBuffers( 1, &id_ );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id_ );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, v.size() * sizeof(unsigned int), &v[0], GL_STATIC_DRAW );
	count_ = v.size();
}

EBO::~EBO()
{
	if ( id_ != 0 ) {
		glDeleteBuffers( 1, &id_ );
	}
}

EBO& EBO::operator=( EBO&& other ) noexcept
{
	if ( this != &other )
	{
		if ( id_ != 0 )
		{
			glDeleteBuffers( 1, &id_ );
		}

		count_ = other.count_;

		std::exchange( other.id_, 0 );
	}

	return *this;
}


void EBO::initialize(unsigned int size) const
{
	bind();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
}

void EBO::initialize( const std::vector<unsigned int>& v )
{
	bind();
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, v.size() * sizeof(unsigned int), &v[0], GL_STATIC_DRAW );
	count_ = v.size();
}

void EBO::bind() const
{
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id_ );
}

void EBO::unbind() const
{
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}

void EBO::copy_and_write(
 const GLuint readBuffer,
 const GLuint writeBuffer,
 const GLintptr readOffset,
 const GLintptr writeOffset,
 const unsigned int size
 )
{
	glBindBuffer(GL_COPY_WRITE_BUFFER, writeBuffer);
	glBindBuffer(GL_COPY_READ_BUFFER, readBuffer);
	glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size);

	glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
	glBindBuffer(GL_COPY_READ_BUFFER, 0);
}
