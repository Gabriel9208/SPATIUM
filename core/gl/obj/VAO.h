#pragma once
#include <utility>
#include <glad/glad.h>

#include "../interfaces/IGPUResource.h"

class VAO : public IGPUResource
{
private:
	GLuint id_ = 0;

public:
	VAO();
	VAO(VAO&& other) noexcept : 
		id_(std::exchange(other.id_, 0))
		{}
	VAO(const VAO& other) = delete;
	~VAO() override;

	VAO& operator=(VAO&& other) noexcept;
	VAO& operator=(const VAO& other) = delete;

	void bind() const override;
	void unbind() const override;
};