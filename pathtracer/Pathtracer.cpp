#include "Pathtracer.h"
#include <memory>
#include <iostream>
#include <map>
#include <algorithm>
#include <glm/ext.hpp>
#include "material.h"
#include "embree.h"
#include "sampling.h"
#include "light.h"

using namespace std;
using namespace glm;

namespace pathtracer {
///////////////////////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////////////////////
    Settings settings;
    Environment environment;
    Image rendered_image;
    std::vector<Light *> lights;

///////////////////////////////////////////////////////////////////////////
// Restart rendering of image
///////////////////////////////////////////////////////////////////////////
    void restart() {
        // No need to clear image,
        rendered_image.number_of_samples = 0;
    }

///////////////////////////////////////////////////////////////////////////
// On window resize, window size is passed in, actual size of pathtraced
// image may be smaller (if we're subsampling for speed)
///////////////////////////////////////////////////////////////////////////
    void resize(int w, int h) {
        rendered_image.width = w / settings.subsampling;
        rendered_image.height = h / settings.subsampling;
        rendered_image.data.resize(rendered_image.width * rendered_image.height);
        restart();
    }

///////////////////////////////////////////////////////////////////////////
// Return the radiance from a certain direction wi from the environment
// map.
///////////////////////////////////////////////////////////////////////////
    vec3 Lenvironment(const vec3 &wi) {
        if (!settings.environment_light) return vec3(0.f);

        const float theta = acos(std::max(-1.0f, std::min(1.0f, wi.y)));
        float phi = atan(wi.z, wi.x);
        if (phi < 0.0f)
            phi = phi + 2.0f * M_PI;
        vec2 lookup = vec2(phi / (2.0 * M_PI), theta / M_PI);
        return environment.multiplier * environment.map.sample(lookup.x, lookup.y);
    }

    bool checkRayLightIntersection(Ray &ray, Light &light) {
        if (!light.checkIntersection(ray)) {
            return false;
        }
        return !occluded(ray);
    }

///////////////////////////////////////////////////////////////////////////
// Calculate the radiance going from one point (r.hitPosition()) in one
// direction (-r.d), through path tracing.
///////////////////////////////////////////////////////////////////////////
    vec3 Li(Ray &primary_ray) {
        vec3 L = vec3(0.0f);
        vec3 path_throughput = vec3(1.0);
        Ray current_ray = primary_ray;
        for (int bounces = 0; bounces <= settings.max_bounces; bounces++) {
            ///////////////////////////////////////////////////////////////////
            // Get the intersection information from the ray
            ///////////////////////////////////////////////////////////////////
            Intersection hit = getIntersection(current_ray);
            ///////////////////////////////////////////////////////////////////
            // Create a Material tree for evaluating brdfs and calculating
            // sample directions.
            ///////////////////////////////////////////////////////////////////
            vec4 color = vec4(hit.material->m_color, 1.f - hit.material->m_transparency);
            if (hit.material->m_color_texture.valid) {
                if (settings.use_bilinear_interp)
                    color = hit.material->m_color_texture.bilinearf4(hit.texture_coords.x, hit.texture_coords.y);
                else
                    color = hit.material->m_color_texture.colorf4(hit.texture_coords.x, hit.texture_coords.y);
            }
            float metalness = hit.material->m_metalness;
            if (hit.material->m_metalness_texture.valid) {
                if (settings.use_bilinear_interp)
                    metalness = hit.material->m_metalness_texture.bilinearf(hit.texture_coords.x, hit.texture_coords.y);
                else
                    metalness = hit.material->m_metalness_texture.colorf(hit.texture_coords.x, hit.texture_coords.y);
            }
            float fresnel = hit.material->m_fresnel;
            if (hit.material->m_fresnel_texture.valid) {
                if (settings.use_bilinear_interp)
                    fresnel = hit.material->m_fresnel_texture.bilinearf(hit.texture_coords.x, hit.texture_coords.y);
                else
                    fresnel = hit.material->m_fresnel_texture.colorf(hit.texture_coords.x, hit.texture_coords.y);
            }
            float roughness = fclamp(hit.material->m_roughness, 0.001f, 1.f);
            if (hit.material->m_roughness_texture.valid) {
                if (settings.use_bilinear_interp)
                    roughness = hit.material->m_roughness_texture.bilinearf(hit.texture_coords.x, hit.texture_coords.y);
                else
                    roughness = hit.material->m_roughness_texture.colorf(hit.texture_coords.x, hit.texture_coords.y);
            }
            float reflectivity = hit.material->m_reflectivity;
            if (hit.material->m_reflectivity_texture.valid) {
                if (settings.use_bilinear_interp)
                    reflectivity = hit.material->m_reflectivity_texture.bilinearf(hit.texture_coords.x,
                            hit.texture_coords.y);
                else
                    reflectivity = hit.material->m_reflectivity_texture.colorf(hit.texture_coords.x,
                            hit.texture_coords.y);
            }

            Diffuse diffuse(color);
//            BSDF &mat = diffuse;
            BlinnPhong dielectric(roughness, fresnel, &diffuse);
            BTDF transparency(1.3f, roughness, fresnel, color);
//            BSDF &mat = transparency;
            BlinnPhongMetal metal(color, roughness, fresnel);
            LinearBlend metal_blend(metalness, &metal, &dielectric);
            LinearBlend reflectivity_blend(reflectivity, &metal_blend, &diffuse);
//            BSDF &mat = reflectivity_blend;
            LinearBlend transparency_blend(color.a, &reflectivity_blend, &transparency);
            BSDF &mat = transparency_blend;

            ///////////////////////////////////////////////////////////////////
            // Calculate Direct Illumination from lights.
            ///////////////////////////////////////////////////////////////////
            for (auto *light: lights) {
                // Sample light source with multiple importance sampling
                Ray shadowRay;
                shadowRay.o = hit.position + hit.geometry_normal * EPSILON;
                vec3 wi;
                float lightPdf, scatteringPdf;
                vec3 li = light->sample_li(shadowRay.o, &wi, &lightPdf);
                shadowRay.d = wi;
                if (lightPdf > 0 && any(greaterThan(abs(li), glm::vec3(EPSILON)))) {
                    vec3 f = mat.f(shadowRay.d, hit.wo, hit.shading_normal) * abs(dot(wi, hit.shading_normal));
                    scatteringPdf = mat.pdf(shadowRay.d, hit.wo, hit.shading_normal);
                    if (any(greaterThan(f, glm::vec3(EPSILON))) && !occluded(shadowRay)) {
                        if (light->isDelta()) {
                            L += f * li / lightPdf;
                        } else {
                            float weight = lightPdf * lightPdf / (lightPdf * lightPdf + scatteringPdf * scatteringPdf);
                            L += f * li * weight / lightPdf;
                        }
                    }
                }
                // Sample BSDF with multiple importance sampling
                if (!light->isDelta()) {
                    vec3 f = mat.sample_wi(wi, hit.wo, hit.shading_normal, scatteringPdf);
                    f *= abs(dot(wi, hit.shading_normal));
                    if (scatteringPdf > 0 && any(greaterThan(abs(f), glm::vec3(EPSILON)))) {
                        float weight = 1;
                                // scatteringPdf * scatteringPdf / (lightPdf * lightPdf + scatteringPdf * scatteringPdf);
                        Ray ray(hit.position, wi);
                        bool lightIntersected = checkRayLightIntersection(ray, *light);
                        li = glm::vec3(0);
                        if (lightIntersected) {
                            li = light->color * light->intensity; // Light emitted (?)
                        }
                        if (any(greaterThan(abs(li), glm::vec3(EPSILON)))) {
                            L += f * li * weight / scatteringPdf;
                        }
                    }
                }
            }

            // Emission
            float emission = hit.material->m_emission;
            if (hit.material->m_emission_texture.valid) {
                emission = hit.material->m_emission_texture.colorf(hit.texture_coords.x, hit.texture_coords.y);
            }
            L += path_throughput * emission * hit.material->m_color;

            // Sample incoming direction
            vec3 wi;
            float pdf;
            vec3 brdf = mat.sample_wi(wi, hit.wo, hit.shading_normal, pdf);

            // return L before division by pdf so we can safely return pdf as 0 from the sample function
            // when there is some error which cuts light
            if (pdf == 0.f || all(lessThan(abs(brdf), vec3(FLT_EPSILON))))
                return L;

            float cosine_term = abs(dot(wi, hit.shading_normal));
            path_throughput *= (brdf * cosine_term) / pdf;

            if (all(lessThan(abs(path_throughput), vec3(FLT_EPSILON))))
                return L;

            current_ray = Ray(hit.position + sign(dot(hit.geometry_normal, wi)) * hit.geometry_normal * EPSILON, wi);

            if (!intersect(current_ray)) {
                L += path_throughput * Lenvironment(wi);
                return L;
            }
        }
        return L;
    }

///////////////////////////////////////////////////////////////////////////
// Used to homogenize points transformed with projection matrices
///////////////////////////////////////////////////////////////////////////
    inline static glm::vec3 homogenize(const glm::vec4 &p) {
        return glm::vec3(p * (1.f / p.w));
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "openmp-use-default-none"

///////////////////////////////////////////////////////////////////////////
// Trace one path per pixel and accumulate the result in an image
///////////////////////////////////////////////////////////////////////////
    void tracePaths(const glm::mat4 &V, const glm::mat4 &P) {
        // Stop here if we have as many samples as we want
        if ((int(rendered_image.number_of_samples) > settings.max_paths_per_pixel)
            && (settings.max_paths_per_pixel != 0)) {
            return;
        }
        vec3 camera_pos = vec3(glm::inverse(V) * vec4(0.0f, 0.0f, 0.0f, 1.0f));
        // Trace one path per pixel (the omp parallel stuf magically distributes the
        // pathtracing on all cores of your CPU).
        int num_rays = 0;
        vector<vec4> local_image(rendered_image.width * rendered_image.height, vec4(0.0f));

#pragma omp parallel for
        for (int y = 0; y < rendered_image.height; y++) {
            for (int x = 0; x < rendered_image.width; x++) {
                vec3 color;
                Ray primaryRay;
                primaryRay.o = camera_pos;
                // Create a ray that starts in the camera position and points toward
                // the current pixel on a virtual screen.
                vec2 screenCoord = vec2(float(x + randf()) / float(rendered_image.width),
                        float(y + randf()) / float(rendered_image.height));
                // Calculate direction
                vec4 viewCoord = vec4(screenCoord.x * 2.0f - 1.0f, screenCoord.y * 2.0f - 1.0f, 1.0f, 1.0f);
                vec3 p = homogenize(inverse(P * V) * viewCoord);
                primaryRay.d = normalize(p - camera_pos);

                // Check in focus
                bool in_focus = false;
                bool intersected = intersect(primaryRay);
                if (intersected) {
                    Intersection hit = getIntersection(primaryRay);
                    if (length2((hit.position - primaryRay.o)) < settings.focal_distance * settings.focal_distance)
                        in_focus = true;
                }
                // Perform focus blur
                if (!in_focus) {
                    auto focalPoint = primaryRay.o + primaryRay.d * settings.focal_distance;
                    viewCoord += vec4(randf() - 0.5f, randf() - 0.5f, 0.f, 0.f) * settings.aperture;
                    if (length2(p - camera_pos) > length2(focalPoint - camera_pos)) {
                        printf("ERROR: Screen further than focal point!\n");
                    }
                    p = homogenize(inverse(P * V) * viewCoord);
                    vec3 new_d = normalize(focalPoint - p);
                    primaryRay = Ray(p, new_d);
                    intersected = intersect(primaryRay);
                }
                // Intersect ray with scene
                if (intersected) {
                    Intersection hit = getIntersection(primaryRay);
                    color = Li(primaryRay);
                } else {
                    // Otherwise evaluate environment
                    color = Lenvironment(primaryRay.d);
                }
                if (any(isnan(color))) {
                    printf("Error: NAN!\n");
//                    color = vec3(1.f, 0.f, 1.f);
                }
                // Accumulate the obtained radiance to the pixels color
                float n = float(rendered_image.number_of_samples);
                rendered_image.data[y * rendered_image.width + x] =
                        rendered_image.data[y * rendered_image.width + x] * (n / (n + 1.0f))
                        + (1.0f / (n + 1.0f)) * color;
            }
        }
        rendered_image.number_of_samples += 1;
    }

#pragma clang diagnostic pop
}; // namespace pathtracer
