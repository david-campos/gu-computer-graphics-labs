#include "material.h"
#include "sampling.h"

#include "glm/ext.hpp"

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
            float r = randf();
            float phi = 2.0f * M_PI * randf();

            float cos_theta = 1.f / sqrt(1.f + roughness * roughness * r / (1 - r));
            float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
            vec3 wh = normalize(sin_theta * cos(phi) * tangent +
                                sin_theta * sin(phi) * bitangent +
                                cos_theta * n);

            // Probability for the microfacet
            float p_wh = D(wh, n) * abs(dot(wh, n));
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
        float wh_n = dot(wh, n);
        if (wh_n > 0) {
            float s_sqr = roughness * roughness;
            float tan = sqrt(max(0.f, 1 - wh_n * wh_n)) / wh_n; // max to avoid numerical errors when close to 0
            if (s_sqr + tan * tan > 0)
                return s_sqr / (M_PI * pow(wh_n, 4.f) * pow(s_sqr + tan * tan, 2.f));
        }
        return 0.f;
    }

    float BlinnPhong::G(const vec3 &wi, const vec3 &wo, const vec3 &wh, const vec3 &n) {
        float wo_wh = dot(wo, wh);
        float wo_n = dot(wo, n);
        if (wo_wh / wo_n > 0) {
            float tan = sqrt(1 - wo_n * wo_n) / wo_n;
            return 2.f / (1.f + sqrt(1.f + roughness * roughness * tan * tan));
        } else return 0.f;
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
    float BTDF::F(const vec3 &wi, const vec3 &wh, float eta_i, float eta_o) {
        // Eq. 22 in the paper
        float c = abs(dot(wi, wh));
        float sqr = eta_o * eta_o / (eta_i * eta_i) - 1 + c * c;
        if (sqr < 0) return 1.f;
        float g = sqrt(sqr);
        float g_m_c = g - c;
        float g_p_c = g + c;
        float num = c * g_p_c - 1;
        float den = c * g_m_c + 1;
        return g_m_c * g_m_c / (2 * g_p_c * g_p_c) * (1 + num * num / (den * den));

        // Schlick approximation for the Fresnel
//        return R0 + (1.f - R0) * pow(max(0.f, 1.f - abs(dot(wh, wi))), 5.f);

        // Book version (c is the same as in paper version)
//        float sinThetaI = sqrtf(max(0.f, 1.f - c * c));
//        float sinThetaT = eta_i / eta_o * sinThetaI;
//        if (sinThetaT >= 1.f) return 1.f;
//        float cosThetaT = sqrt(max(0.f, 1 - sinThetaT * sinThetaT));
//        float Rparl = ((eta_o * c) - (eta_i * cosThetaT)) /
//                      ((eta_o * c) + (eta_i * cosThetaT));
//        float Rperp = ((eta_i * c) - (eta_o * cosThetaT)) /
//                      ((eta_i * c) + (eta_o * cosThetaT));
//        return (Rparl * Rparl + Rperp * Rperp) / 2;
    }

    // Eq. 33
    float BTDF::D(const vec3 &wh, const vec3 &n) {
        float wh_n = dot(wh, n);
        if (wh_n > 0) {
            float s_sqr = roughness * roughness;
            float tan_m = sqrt(max(0.f, 1 - wh_n * wh_n)) / wh_n; // max to avoid numerical errors when close to 0
            float power_term = s_sqr + tan_m * tan_m;
            if (power_term != 0) // If 0 division by 0
                return s_sqr / (M_PI * pow(wh_n, 4.f) * power_term * power_term);
        }
        return 0.f;
    }

    // Eq. 23
    float BTDF::G(const vec3 &wi, const vec3 &wo, const vec3 &wh, const vec3 &n) {
        return G1(wo, wh, n) * G1(wi, wh, n);
    }

    // Eq. 34
    float BTDF::G1(const vec3 &v, const vec3 &m, const vec3 &n) {
        float v_m = dot(v, m);
        float v_n = dot(v, n);
        if (v_n != 0 && v_m / v_n > 0) {
            float tan_v = sqrt(max(0.f, 1 - v_n * v_n)) / v_n;
            return 2.f / (1.f + sqrt(1.f + roughness * roughness * tan_v * tan_v));
        } else return 0.f;
    }

    vec3 BTDF::reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        vec3 hr = normalize(sign(dot(wi, n)) * (wo + wi));
        if (dot(wi, n) <= 0.f || !sameHemisphere(wi, wo, n)) return vec3(0.0f);
        float eta_i = dot(wo, n) > 0 ? 1 : refraction_index;
        float eta_o = dot(wo, n) > 0 ? refraction_index : 1;
        return vec3(F(wi, hr, eta_i, eta_o) * D(hr, n) * G(wi, wo, hr, n) / (4 * abs(dot(n, wo) * dot(n, wi))));
    }

    vec3 BTDF::refraction_brdf(const vec3 &i, const vec3 &o, const vec3 &n) {
        float eta_i = dot(o, n) > 0 ? refraction_index : 1;
        float eta_o = dot(o, n) > 0 ? 1 : refraction_index;

        vec3 ht = normalize(-(eta_i * i + eta_o * o));

        float o_n = dot(o, n);
        float i_ht = dot(i, ht);
        float i_n = dot(i, n);
        float o_ht = dot(o, ht);

        if (i_n == 0.f || o_n == 0.f) return vec3(0.f); // division by 0
        if (i_ht == 0.f && o_ht == 0.f) return vec3(0.f); // division by 0

        float g = G(i, o, ht, n);
        float f = F(i, ht, eta_i, eta_o);
        float d = D(ht, n);

        float frac0 = abs(i_ht * o_ht / (i_n * o_n));
        float frac1Num = eta_o * eta_o * (1 - f) * g * d;
        float frac1Den = eta_i * i_ht + eta_o * o_ht;

        return vec3(frac0 * frac1Num / (frac1Den * frac1Den));
    }

    vec3 BTDF::sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) {
        // Select a random microfacet
        vec3 tangent = normalize(perpendicular(n));
        vec3 bitangent = normalize(cross(tangent, n));
        float r = randf();
        float phi = 2.0f * M_PI * randf();

        float cos_theta = 1.f / sqrt(1.f + roughness * roughness * r / (1 - r));
        float sin_theta = sqrt(max(0.0f, 1.0f - cos_theta * cos_theta));
        vec3 m = normalize(sin_theta * cos(phi) * tangent +
                           sin_theta * sin(phi) * bitangent +
                           cos_theta * n);

        // Probability for the microfacet
        float p_m = D(m, n) * abs(dot(m, n));

        float eta_i = dot(wo, n) > 0 ? 1 : refraction_index;
        float eta_o = dot(wo, n) > 0 ? refraction_index : 1;

        float fresnel = F(wo, m, eta_i, eta_o);
        if (randf() < fresnel) {
            // Reflection
            p = p_m / (4 * abs(dot(wo, m)));
            wi = reflect(-wo, m);
            p *= fresnel;
        } else {
            // Refraction

            float eta = eta_i / eta_o;

            float c = dot(wo, m);

            // Eq. 40
            float sq = 1 + eta * eta * (c * c - 1);
            if (sq < 0) {
                printf("Fresnel didn't catch total internal reflection!!\n");
                return vec3(0.f);
            }
            wi = (eta * c - sign(dot(wo, n)) * sqrt(sq)) * m - eta * wo;

            // Eq. 16
            // Should be colinear with m, but for some reason it is not :/
            vec3 ht = -(eta_o * wo + eta_i * wi);
            ht = normalize(ht);

            // Eq. 38 ^ Eq. 17 (p_m * jacobian)
            float sq_den = eta_i * dot(wi, ht) + eta_o * dot(wo, ht);
            p = p_m * eta_o * eta_o * abs(dot(wo, ht)) / (sq_den * sq_den);

            p *= 1.f - fresnel;
        }
        return f(wi, wo, n);
    }

    vec3 BTDF::f(const vec3 &wi, const vec3 &wo, const vec3 &n) {
        vec3 refl = reflection_brdf(wi, wo, n);
        vec3 refr = refraction_brdf(wi, wo, n);
        return refl + refr;
    }
} // namespace pathtracer