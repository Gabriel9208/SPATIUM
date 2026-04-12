#include "ShaderProgram.h"
#include "../../Utilty/Error.h"

#include <fstream>
#include <sstream>
#include <iostream>

ShaderProgram::~ShaderProgram()
{
	std::cout << "Program " << program << " has been deleted.\n";
	GLCall(glDeleteProgram(program));
}

void ShaderProgram::unUse() const
{
	GLCall(glUseProgram(0));
}

const GLchar* ShaderProgram::ReadShader(const char* filename)
{
	FILE* in = fopen(filename, "rb");

	if (!in) {
		std::cout << "Shader file: " << filename << " cannot be opened." << std::endl;
		return NULL;
	}

	fseek(in, 0, SEEK_END);
	int length = ftell(in);
	rewind(in);

	GLchar* originalBuffer = new GLchar[length + 1];
	fread(originalBuffer, 1, length, in);
	fclose(in);

	GLchar* shaderSource = originalBuffer;
	int newLength = length;

	if (length >= 3 && 
	    static_cast<unsigned char>(shaderSource[0]) == 0xEF &&
	    static_cast<unsigned char>(shaderSource[1]) == 0xBB &&
	    static_cast<unsigned char>(shaderSource[2]) == 0xBF) 
	{
		shaderSource += 3;
		newLength -= 3;
		std::cout << "Note: Stripped UTF-8 BOM from shader: " << filename << std::endl;
	}

	if (newLength != length) {
	    GLchar* cleanSource = new GLchar[newLength + 1];
	    memcpy(cleanSource, shaderSource, newLength);
	    cleanSource[newLength] = '\0';
	    delete[] originalBuffer;
	    return cleanSource;
	}
    else {
        originalBuffer[newLength] = '\0';
        return originalBuffer;
    }
}