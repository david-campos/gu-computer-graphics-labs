#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;

in vec3 viewSpaceNormal;

void main()
{
	vec3 n = normalize(viewSpaceNormal);
	if (any(isnan(n))) {
		n = vec3(1.f, 1.f, 1.f);
	}
	fragmentColor = vec4(n * 0.5f + vec3(0.5f), 1.0f);
}
