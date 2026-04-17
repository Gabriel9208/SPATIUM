#pragma once
#include "ShaderProgram.h"
#include <glad/glad.h>


class GraphicShader: public ShaderProgram
{
public:
	GraphicShader(): ShaderProgram() {}
	~GraphicShader() override = default;

	GLuint load(ShaderInfo* shaders) override;

	void use() const override;
};