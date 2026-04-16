#include "FBO.h"

#include <glad/glad.h>


FBO::FBO()
{
	glGenFramebuffers(1, &id);
	//glBindFramebuffer(GL_FRAMEBUFFER, id);
}

FBO::FBO(FBO&& other) noexcept : id(other.id)
{
	other.id = 0;
}

FBO::~FBO()
{
	glDeleteFramebuffers(1, &id);
}

FBO& FBO::operator=(FBO&& other) noexcept
{
	if (this != &other)
	{
		if (id != 0)
		{
			glDeleteFramebuffers(1, &id);
		}

		id = other.id;
		other.id = 0;
	}

	return *this;
}

void FBO::bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, id);
}

void FBO::copy(const FBO& other, const int width, const int height) const
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, other.id);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FBO::unbind()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
