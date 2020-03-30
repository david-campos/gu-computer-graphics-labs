#include "material.h"
#include "sampling.h"

namespace pathtracer {
///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
    vec3 Diffuse::f(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        if (dot(wi, n) <= 0.0f)
            return vec3(0.0f);
        if (!sameHemisphere(wi, wo, n))
            return vec3(0.0f);
        return (1.0f / M_PI) * color;
    }

    vec3 Diffuse::sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) {
        vec3 tangent = normalize(perpendicular(n));
        vec3 bitangent = normalize(cross(tangent, n));
        vec3 sample = cosineSampleHemisphere();
        wi = normalize(sample.x * tangent + sample.y * bitangent + sample.z * n);
        if (dot(wi, n) <= 0.0f)
            p = 0.0f;
        else
            p = max(0.0f, dot(n, wi)) / M_PI;
        return f(wi, wo, n);
    }

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Dielectric Microfacet BRFD
///////////////////////////////////////////////////////////////////////////
    vec3 BlinnPhong::refraction_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        if (refraction_layer) {
            vec3 wh = normalize(wo + wi);
            return (1 - F(wi, wh)) * refraction_layer->f(wi, wo, n);
        } else {
            return vec3(0.0f);
        }
    }

    vec3 BlinnPhong::reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        vec3 wh = normalize(wo + wi);
        if (dot(wi, n) <= 0.f || !sameHemisphere(wi, wo, n)) return vec3(0.0f);
        return vec3(F(wi, wh) * D(wh, n) * G(wi, wo, wh, n) / (4 * dot(n, wo) * dot(n, wi)));
    }

    vec3 BlinnPhong::f(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        if (dot(wi, n) <= 0.f || !sameHemisphere(wi, wo, n)) return vec3(0.0f);
        else return reflection_brdf(wi, wo, n) + refraction_brdf(wi, wo, n);
    }

    vec3 BlinnPhong::sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) {
        if (!refraction_layer || randf() < 0.5f) {
            vec3 tangent = normalize(perpendicular(n));
            vec3 bitangent = normalize(cross(tangent, n));
            float phi = 2.0f * M_PI * randf();
            float cos_theta = pow(randf(), 1.0f / (shininess + 1));
            float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
            vec3 wh = normalize(sin_theta * cos(phi) * tangent +
                                sin_theta * sin(phi) * bitangent +
                                cos_theta * n);

            float p_wh = (shininess + 1) * pow(abs(dot(n, wh)), shininess) / (2 * M_PI);
            p = p_wh / (4 * abs(dot(wo, wh)));
            p *= refraction_layer ? 0.5f : 1.f;

            if (dot(wo, n) <= 0.f)
                return vec3(0.0f);

            wi = reflect(-wo, wh);
            return reflection_brdf(wi, wo, n);
        } else {
            vec3 brdf = refraction_layer->sample_wi(wi, wo, n, p);
            vec3 wh = normalize(wi + wo);
            p *= 0.5f;
            return (1 - F(wi, wh)) * brdf;
        }
    }

    float BlinnPhong::F(const vec3 &wi, const vec3 &wh) {
        return R0 + (1 - R0) * pow(max(0.f, 1 - abs(dot(wh, wi))), 5);
    }

    float BlinnPhong::D(const vec3 &wh, const vec3 &n) {
        return (shininess + 2) / (2 * M_PI) * pow(abs(dot(n, wh)), shininess);
    }

    float BlinnPhong::G(const vec3 &wi, const vec3 &wo, const vec3 &wh, const vec3 &n) {
        float wo_wh = dot(wo, wh);
        float n_wh_2 = 2 * dot(n, wh);
        return min(1.f, min(n_wh_2 * dot(n, wo) / wo_wh, n_wh_2 * dot(n, wi) / wo_wh));
    }

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Metal Microfacet BRFD (extends the BlinnPhong class)
///////////////////////////////////////////////////////////////////////////
    vec3 BlinnPhongMetal::refraction_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        return vec3(0.0f);
    }

    vec3 BlinnPhongMetal::reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        return BlinnPhong::reflection_brdf(wi, wo, n) * color;
    };

///////////////////////////////////////////////////////////////////////////
// A Linear Blend between two BRDFs
///////////////////////////////////////////////////////////////////////////
    vec3 LinearBlend::f(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        return w * bsdf0->f(wi, wo, n) + (1 - w) * bsdf1->f(wi, wo, n);
    }

    vec3 LinearBlend::sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) {
        if (randf() <= w) {
            vec3 brdf = bsdf0->sample_wi(wi, wo, n, p);
            p *= w;
            return brdf;
        } else {
            vec3 brdf = bsdf1->sample_wi(wi, wo, n, p);
            p *= (1 - w);
            return brdf;
        }
    }

///////////////////////////////////////////////////////////////////////////
// A perfect specular refraction.
///////////////////////////////////////////////////////////////////////////
} // namespace pathtracer