#include "EBO.h"
#include "../Utilty/Error.h"

EBO::EBO() : count( 0 )
{
	GLCall( glCreateBuffers( 1, &id ) );
	GLCall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id ) );
}

EBO::EBO( const std::vector<unsigned int>& v )
{
	GLCall( glCreateBuffers( 1, &id ) );
	GLCall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id ) );
	GLCall( glBufferData( GL_ELEMENT_ARRAY_BUFFER, v.size() * sizeof(unsigned int), &v[0], GL_STATIC_DRAW ) );
	count = v.size();
}

EBO::EBO( EBO&& other ) noexcept : id( other.id ), count( other.count )
{
	other.id = 0;
}

EBO::~EBO()
{
	if ( id != 0 ) {
		GLCall( glDeleteBuffers( 1, &id ) );
	}
}

EBO& EBO::operator=( EBO&& other ) noexcept
{
	if ( this != &other )
	{
		if ( id != 0 )
		{
			glDeleteBuffers( 1, &id );
		}

		count = other.count;

		id = other.id;
		other.id = 0;
	}

	return *this;
}


void EBO::initialize(unsigned int size)
{
	bind();
	GLCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW));
}

void EBO::initialize( const std::vector<unsigned int>& v )
{
	bind();
	GLCall( glBufferData( GL_ELEMENT_ARRAY_BUFFER, v.size() * sizeof(unsigned int), &v[0], GL_STATIC_DRAW ) );
	count = v.size();
}

void EBO::bind()
{
	GLCall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, id ) );
}

void EBO::unbind()
{
	GLCall( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 ) );
}

void EBO::copyAndWrite(GLuint readBuffer, GLuint writeBuffer, GLintptr readOffset, GLintptr writeOffset, unsigned int size)
{
	GLCall(glBindBuffer(GL_COPY_WRITE_BUFFER, writeBuffer));
	GLCall(glBindBuffer(GL_COPY_READ_BUFFER, readBuffer));
	GLCall(glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, readOffset, writeOffset, size));

	GLCall(glBindBuffer(GL_COPY_WRITE_BUFFER, 0));
	GLCall(glBindBuffer(GL_COPY_READ_BUFFER, 0));
}