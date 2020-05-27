#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform mat4 normalMatrix;

out vec3 viewSpaceNormal;
out vec3 vertPos;

void main()
{
	vertPos = (modelViewMatrix * vec4(position, 1.0)).xyz;
	viewSpaceNormal = (normalMatrix * vec4(normal, 0.0)).xyz;
	gl_Position = projectionMatrix * vec4(vertPos, 1.f);
}