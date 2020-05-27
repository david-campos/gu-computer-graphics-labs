#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
uniform vec3 lightColor;
uniform mat4 viewMatrix;

in vec3 viewSpaceNormal;
in vec3 vertPos;

void main()
{
	// Infinite light comming from -Z
	vec3 L = normalize((viewMatrix * vec4(0.f, 0.f, -1.f, 0.f)).xyz);
	vec3 N = normalize(viewSpaceNormal);
	float lambertian = max(dot(N, L), 0.0);
	vec3 R = reflect(-L, N);
	vec3 V = normalize(-vertPos);
	// Compute the specular term
	float specAngle = max(dot(R, V), 0.0);
	float specular = pow(specAngle, 80);
	fragmentColor = vec4(lightColor * (lambertian + 0.2f) + vec3(specular), 1.f);
}