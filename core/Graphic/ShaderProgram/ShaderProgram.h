#pragma once
#include <string>
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
	~ShaderProgram();

	const GLchar* ReadShader(const char* filename);
	
	virtual GLuint load(ShaderInfo* shaders) { return 0; }
	virtual GLuint load(const char* shaderFile) { return 0; }

	virtual void use() const = 0;
	void unUse() const;

	inline unsigned int getId() const { return program; }
};