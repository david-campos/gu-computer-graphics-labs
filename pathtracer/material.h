#pragma once

#include <glm/glm.hpp>
#include "Pathtracer.h"
#include "sampling.h"

using namespace glm;

namespace pathtracer {
///////////////////////////////////////////////////////////////////////////
// The interface for any BRDF.
///////////////////////////////////////////////////////////////////////////
    class BSDF {
    public:
        // Return the value of the brdf for specific directions
        virtual vec3 f(const vec3 &wi, const vec3 &wo, const vec3 &n) = 0;

        // Sample a suitable direction and return the brdf in that direction as
        // well as the weight for the ray in the trace.
        virtual vec3 sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &weight) = 0;
    };

///////////////////////////////////////////////////////////////////////////
// A Lambertian (diffuse) material
///////////////////////////////////////////////////////////////////////////
    class Diffuse : public BSDF {
    public:
        vec3 color;

        Diffuse(vec3 c) : color(c) {
        }

        virtual vec3 f(const vec3 &wi, const vec3 &wo, const vec3 &n) override;

        virtual vec3 sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) override;
    };

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Dielectric Microfacet BRFD
///////////////////////////////////////////////////////////////////////////
    class BlinnPhong : public BSDF {
    public:
        float roughness;
        float R0;
        BSDF *refraction_layer;

        BlinnPhong(float _roughness, float _R0, BSDF *_refraction_layer = NULL)
                : roughness(_roughness), R0(_R0), refraction_layer(_refraction_layer) {
        }

        virtual vec3 refraction_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n);

        virtual vec3 reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n);

        virtual vec3 f(const vec3 &wi, const vec3 &wo, const vec3 &n) override;

        inline float F(const vec3 &wi, const vec3 &wh);

        inline float D(const vec3 &wh, const vec3 &n);

        inline float G(const vec3 &wi, const vec3 &wo, const vec3 &wh, const vec3 &n);

        virtual vec3 sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) override;
    };

///////////////////////////////////////////////////////////////////////////
// A Blinn Phong Metal Microfacet BRFD (extends the BlinnPhong class)
///////////////////////////////////////////////////////////////////////////
    class BlinnPhongMetal : public BlinnPhong {
    public:
        vec3 color;

        BlinnPhongMetal(vec3 c, float _shininess, float _R0) : color(c), BlinnPhong(_shininess, _R0) {
        }

        virtual vec3 refraction_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n);

        virtual vec3 reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n);
    };


/**
 * Attempt to implement the refraction as showed in https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
 */
    class BTDF : public BSDF {
    private:
        vec3 color;
        float refraction_index;
        float roughness;
        float R0;
    public:
        BTDF(float ri, float _roughness, float _R0, vec3 _color) : refraction_index(ri), roughness(_roughness), R0(_R0), color(_color) {}
        vec3 f(const vec3 &wi, const vec3 &wo, const vec3 &n) override;
        inline float F(const vec3 &wi, const vec3 &wh, float eta_i, float eta_o);
        inline float D(const vec3 &wh, const vec3 &n);
        inline float G(const vec3 &wi, const vec3 &wo, const vec3 &wh, const vec3 &n);
        inline float G1(const vec3 &v, const vec3 &m, const vec3 &n);
        vec3 sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) override;

        vec3 refraction_brdf(const vec3 &wi, const vec3 &wh, const vec3 &n);
        virtual vec3 reflection_brdf(const vec3 &wi, const vec3 &wo, const vec3 &n);
    };

    ///////////////////////////////////////////////////////////////////////////
// A Linear Blend between two BRDFs
///////////////////////////////////////////////////////////////////////////
    class LinearBlend : public BSDF {
    public:
        float w;
        BSDF *bsdf0;
        BSDF *bsdf1;

        LinearBlend(float _w, BSDF *a, BSDF *b) : w(_w), bsdf0(a), bsdf1(b) {};

        virtual vec3 f(const vec3 &wi, const vec3 &wo, const vec3 &n) override;

        virtual vec3 sample_wi(vec3 &wi, const vec3 &wo, const vec3 &n, float &p) override;
    };

} // namespace pathtracer