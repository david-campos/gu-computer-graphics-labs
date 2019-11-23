#version 420
///////////////////////////////////////////////////////////////////////////////
// Input vertex attributes
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoordIn;

layout(binding = 0) uniform sampler2D heightMap;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////

uniform mat4 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;

///////////////////////////////////////////////////////////////////////////////
// Output to fragment shader
///////////////////////////////////////////////////////////////////////////////
out vec2 texCoord;
out vec3 viewSpacePosition;
out vec3 viewSpaceNormal;

float height(vec2 unitCubePos) {
    return texture(heightMap, unitCubePos * 0.5f + vec2(0.5f)).r;
}

vec3 computeNormal(vec3 O) {
    vec2 pixelStepUnitCube = 2.f / textureSize(heightMap, 0);

    vec2 a = - vec2(pixelStepUnitCube.x, 0.f);
    vec2 b =  vec2(pixelStepUnitCube.x, 0.f);
    vec2 c = - vec2(0.f, pixelStepUnitCube.y);
    vec2 d =  vec2(0.f, pixelStepUnitCube.y);

    vec3 A = vec3(a.x, height(O.xz + a), a.y);
    vec3 B = vec3(b.x, height(O.xz + b), b.y);
    vec3 C = vec3(c.x, height(O.xz + c), c.y);
    vec3 D = vec3(d.x, height(O.xz + d), d.y);

    A = normalize(A);
    B = normalize(B);
    C = normalize(C);
    D = normalize(D);

    vec3 normAC = normalize(cross(C, A));
    vec3 normCB = normalize(cross(B, C));
    vec3 normBD = normalize(cross(D, B));
    vec3 normDA = normalize(cross(A, D));

    return normalize(normAC + normCB + normBD + normDA);
}

void main()
{
    float height = texture(heightMap, texCoordIn).r;
    vec4 pos = vec4(position.x, height, position.z, 1.0);
    vec3 normal = computeNormal(pos.xyz);

    viewSpacePosition = (modelViewMatrix * pos).xyz;
    gl_Position = modelViewProjectionMatrix * pos;
    texCoord = texCoordIn;
    viewSpaceNormal = (normalMatrix * vec4(normal, 0.0)).xyz;
}
