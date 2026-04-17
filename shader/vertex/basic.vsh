#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uViewPos;

uniform mat4 uModel;
uniform mat3 uNormalMatrix;

void main()
{
    gl_Position = uProjection * uView * uModel * vec4(position, 1.0);
}