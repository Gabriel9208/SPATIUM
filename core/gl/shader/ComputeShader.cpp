#include "ComputeShader.h"

#include <cmath>
#include <cstdio>

#include <iostream>

const GLchar* ComputeShader::read_shader(const char* filename)
{
	FILE* in = fopen(filename, "rb");

	if (!in) {
		std::cout << "Shader file: " << filename << " cannot be opened." << std::endl;
		return nullptr;
	}

	fseek(in, 0, SEEK_END);
	int length = ftell(in);
	rewind(in);

	GLchar* shaderSource = new GLchar[length + 1];

	fread(shaderSource, 1, length, in);
	fclose(in);

	shaderSource[length] = '\0';

	return shaderSource;
}

ComputeShader::ComputeShader(const char* shaderFile, unsigned int x, unsigned int y, unsigned int z):
	numGroupsX_(x),
	numGroupsY_(y),
	numGroupsZ_(z)
{
	load(shaderFile);
}

void ComputeShader::set_group_amount(unsigned int x, unsigned int y, unsigned int z)
{
	numGroupsX_ = x;
	numGroupsY_ = y;
	numGroupsZ_ = z;
}

GLuint ComputeShader::load(const char* shaderFile)
{
	if (shaderFile == nullptr)
	{
		return 0;
	}

	program = glCreateProgram();

	const GLchar* source = read_shader(shaderFile);

	if (source == NULL)
	{
		return 0;
	}

	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	int compileResult;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
	if (compileResult == GL_FALSE)
	{
		std::cout << "Shader: \"" << shaderFile << "\" compile failed." << std::endl;
	}

	glAttachShader(program, shader);

	delete[] source;

	glLinkProgram(program);

	GLint linkResult;
	glGetProgramiv(program, GL_LINK_STATUS, &linkResult);
	if (!linkResult)
	{
		if (linkResult == GL_FALSE)
		{
			std::cout << "Shader: \"" << shaderFile << "\" link failed." << std::endl;
		}

		return 0;
	}

	return program;
}

void ComputeShader::use() const
{
	glUseProgram(program);
}

void ComputeShader::compute(unsigned int particleAmount /* = -1 */)
{
	if (particleAmount == -1)
	{
		glDispatchCompute(numGroupsX_, numGroupsY_, numGroupsZ_);
	}
	else
	{
		glDispatchCompute(std::floor(particleAmount / 256.0) + 1, 1, 1);
	}
}
