#pragma once
#include "ShaderProgram.h"
#include <glad/glad.h>

class ComputeShader: public ShaderProgram
{
private:
	unsigned int numGroupsX = 0;
	unsigned int numGroupsY = 0;
	unsigned int numGroupsZ = 0;


public:
	ComputeShader(): ShaderProgram() {}
	ComputeShader(const char* shaderFile, unsigned int x, unsigned int y, unsigned int z);
	~ComputeShader() override = default;

	const GLchar* read_shader(const char* filename) override;

	void set_group_amount(unsigned int x, unsigned int y, unsigned int z);
	GLuint load(const char* shaderFile) override;

	void use() const override;
	void compute(unsigned int particleAmount = -1);
};