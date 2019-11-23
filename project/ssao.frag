#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(binding = 0) uniform sampler2D depthTexture;
layout(binding = 1) uniform sampler2D normalsTexture;

layout(binding = 2) uniform sampler2D randomAngles;
uniform bool useRotation;

layout(location = 0) out vec4 fragmentColor;

uniform mat4 projectionMatrix;
uniform mat4 inverseProjectionMatrix;
uniform int number_samples = 10;
uniform float hemisphere_radius;
uniform vec3 samples[215];
uniform float epsilon;

in vec2 texCoord;

#define PI 3.14159265359

vec3 homogenize(vec4 v) { return vec3((1.0 / v.w) * v); }

// Computes one vector in the plane perpendicular to v
vec3 perpendicular(vec3 v)
{
    vec3 av = abs(v);
    if (av.x < av.y)
    if (av.x < av.z) return vec3(0.0f, -v.z, v.y);
    else return vec3(-v.y, v.x, 0.0f);
    else
    if (av.y < av.z) return vec3(-v.z, 0.0f, v.x);
    else return vec3(-v.y, v.x, 0.0f);
}

vec2 textureCoord(in sampler2D tex, vec2 rectangleCoord)
{
    return rectangleCoord / textureSize(tex, 0);
}

vec4 textureRect(in sampler2D tex, vec2 rectangleCoord) {
    return texture(tex, textureCoord(tex, rectangleCoord));
}

vec3 hemisphericalVisibility(mat3 tbn, vec3 vs_pos) {
    int num_visible_samples = 0;
    int num_valid_samples = 0;
    for (int i = 0; i < number_samples; i++) {
        // Project hemisphere sample onto the local base
        vec3 s = tbn * samples[i];

        // compute view-space position of sample
        vec3 vs_sample_position = vs_pos + s * hemisphere_radius;

        // compute the ndc-coords of the sample
        vec3 sample_coords_ndc = homogenize(projectionMatrix * vec4(vs_sample_position, 1.0));

        // Sample the depth-buffer at a texture coord based on the ndc-coord of the sample
        float blocker_depth = texture(depthTexture, sample_coords_ndc.xy * 0.5f + vec2(0.5f)).r;

        // Find the view-space coord of the blocker
        vec3 vs_blocker_pos = homogenize(
        inverseProjectionMatrix * vec4(
        sample_coords_ndc.xy, blocker_depth * 2.0f - 1.0f, 1.0f));

        // Check that the blocker is closer than hemisphere_radius to vs_pos
        if (distance(vs_blocker_pos, vs_pos) > hemisphere_radius) {
            continue;
        }

        // Check if the blocker pos is closer to the camera than our
        // fragment, otherwise, increase num_visible_samples
        if (length(vs_blocker_pos) + epsilon >= length(vs_pos)) {
            num_visible_samples += 1;
        }

        num_valid_samples += 1;
    }

    if (num_valid_samples == 0)
    return vec3(1.f);
    return vec3(float(num_visible_samples) / float(num_valid_samples));
}

vec4 axis_angle_to_quaternion(vec3 axis, float angle) {
    float half_angle = angle/2;
    return vec4(
    axis * sin(half_angle),
    cos(half_angle)
    );
}

vec3 rotate_by_axis(vec3 pos, vec3 axis, float angle) {
    vec4 q = axis_angle_to_quaternion(axis, angle);
    return pos + 2.0f * cross(q.xyz, cross(q.xyz, pos) + q.w * pos);
}

void main()
{
    float fragmentDepth = texture(depthTexture, texCoord).r;
    vec3 vs_normal = texture(normalsTexture, texCoord).xyz * 2.f - vec3(1.f);

    // Normalized Device Coordinates (clip space)
    vec4 ndc = vec4(texCoord.x * 2.0 - 1.0, texCoord.y * 2.0 - 1.0, fragmentDepth * 2.0 - 1.0, 1.0);
    // Transform to view space
    vec3 vs_pos = homogenize(inverseProjectionMatrix * ndc);

    vec3 vs_tangent = perpendicular(vs_normal);
    if (useRotation) {
        float angle = textureRect(randomAngles, gl_FragCoord.xy).r * 2 * PI;
        vs_tangent = rotate_by_axis(vs_tangent, vs_normal, angle);
    }
    vec3 vs_bitangent = cross(vs_normal, vs_tangent);

    // local base:
    mat3 tbn = mat3(vs_tangent, vs_bitangent, vs_normal);

    vec4 final_color = vec4(hemisphericalVisibility(tbn, vs_pos), 1.0f);
    // Check if we got invalid results in the operations
    if (any(isnan(final_color)))
    {
        final_color.xyz = vec3(1.f, 0.f, 1.f);
    }

    fragmentColor = final_color;
}
