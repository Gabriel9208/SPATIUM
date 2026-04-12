#include "GraphicShader.h"

#include <fstream>
#include <iostream>
#include <vector>

GraphicShader::~GraphicShader()
{
}

void GraphicShader::use() const
{
	glUseProgram(program);
}

GLuint GraphicShader::load(ShaderInfo* shaders)
{
	if (shaders == NULL)
	{
		return 0;
	}

	program = glCreateProgram();

	ShaderInfo* shaderInfo = shaders;
	while (shaderInfo->type != GL_NONE)
	{
		const GLchar* source = ReadShader(shaderInfo->filename);

		if (source == NULL)
		{
			return 0;
		}

		GLuint shader = glCreateShader(shaderInfo->type);
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		int compileResult;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compileResult);
		if (compileResult == GL_FALSE)
		{
			GLint infoLogLength;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

			if (infoLogLength > 0) {
				std::vector<char> infoLog(infoLogLength);
				glGetShaderInfoLog(shader, infoLogLength, NULL, infoLog.data());
				std::cerr << "Shader compilation error (Type: " << shaderInfo->type << "):\n" << infoLog.data() << std::endl;
			}
			else {
				std::cerr << "Shader compilation failed for unknown reason (Type: " << shaderInfo->type << ").\n";
			}
			glDeleteShader(shader);
			return 0;
		}

		glAttachShader(program, shader);

		delete[] source;
		++shaderInfo;
	}

	glLinkProgram(program);

	GLint linkResult;
	glGetProgramiv(program, GL_LINK_STATUS, &linkResult);
	if (!linkResult)
	{
		if (linkResult == GL_FALSE)
		{
			std::cout << "Shader: \"" << shaders->filename << "\" link failed." << std::endl;
		}

		return 0;
	}

	return program;
}
