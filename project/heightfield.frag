#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(binding = 1) uniform sampler2D diffuseMap;
layout(binding = 4) uniform sampler2D ssao;
uniform bool useSsao;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
uniform float environment_multiplier;

layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

vec2 sphericalCoords(vec3 dir) {
    float theta = acos(max(-1.0f, min(1.0f, dir.y)));
    float phi = atan(dir.z, dir.x);
    if(phi < 0.0f)
    {
        phi = phi + 2.0f * PI;
    }
    return vec2(phi / (2.0 * PI), theta / PI);
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
    vec3 N = mat3(viewInverse) * n;
    vec2 lookup = sphericalCoords(N);
    vec4 irradiance = environment_multiplier * texture(irradianceMap, lookup);
    float ssaoValue = texture(ssao, gl_FragCoord.xy / textureSize(ssao, 0)).r;
    vec3 material_color = texture(diffuseMap, texCoord).xyz;
    vec3 diffuse_term = material_color * (1 / PI) * irradiance.rgb * (useSsao ? ssaoValue : 1.0f);

    vec3 wo_w = mat3(viewInverse) * wo;
    vec3 wi = normalize(reflect(-wo_w, N));
    float roughness = sqrt(sqrt(2.f/(material_shininess+2.f)));
    lookup = sphericalCoords(wi);
    vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;

    vec3 wh = normalize(wi + wo);
    float F = material_fresnel + (1 - material_fresnel) * pow(1 - dot(wh, wi), 5);
    vec3 dielectric_term = F * Li + (1 - F) * diffuse_term;
    vec3 metal_term = F * material_color * Li;
    vec3 microfacet_term = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;
    return material_reflectivity * microfacet_term + (1-material_reflectivity) * diffuse_term;
}
void main() {
    vec3 wo = -normalize(viewSpacePosition);
    vec3 n = normalize(viewSpaceNormal);

    fragmentColor = vec4(calculateIndirectIllumination(wo, n), 1.f);
}
