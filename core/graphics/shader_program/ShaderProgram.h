#pragma once
#include <glad/glad.h>

struct ShaderInfo
{
	GLenum type;
	const char* filename;
};

class ShaderProgram
{
protected:
	unsigned int program;

public:
	ShaderProgram(): program(0) {}
	virtual ~ShaderProgram();

	virtual const GLchar* ReadShader(const char* filename);
	
	virtual GLuint load(ShaderInfo* shaders) = 0;
	virtual GLuint load(const char* shaderFile) = 0;

	virtual void use() const = 0;
	static void unUse() ;

	[[nodiscard]] inline unsigned int getId() const { return program; }
};