#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 uv;
uniform mat4 projectionMatrix;
uniform vec3 cameraPosition;

out vec2 uvOut;

void main()
{
	vec4 pos = vec4(position.xyz - cameraPosition.xyz, 1);
	uvOut = uv;
	gl_Position = projectionMatrix * pos;
}