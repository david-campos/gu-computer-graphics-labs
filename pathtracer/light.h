//
// Created by david on 23/4/20.
//

#ifndef COMPUTER_GRAPHICS_LABS_LIGHT_H
#define COMPUTER_GRAPHICS_LABS_LIGHT_H

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "sampling.h"

namespace pathtracer {
    class Light {
    public:
        glm::vec3 color;

        Light(const glm::vec3 &color) : color(color) {}

        virtual glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const = 0;

        virtual float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n,
                             const glm::vec3 &ref, const glm::vec3 &wi) const = 0;
    };

    class PointLight : public Light {
    private:
        glm::vec3 _position;
    public:
        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            *wi = normalize(_position - ref);
            *pdf = 1.f;
            return color / glm::length2(_position - ref);
        }

        float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n, const glm::vec3 &ref,
                     const glm::vec3 &wi) const override {
            return 0;
        }
    };

    class AreaLight : public Light {
    public:
        AreaLight(const glm::vec3 &color) : Light(color) {}

        virtual float area() const = 0;

        float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n, const glm::vec3 &ref,
                     const glm::vec3 &wi) const override {
            // We are assuming ray from wi is intersecting the surface!
            // Supposing uniform distribution over the area probability density over point is 1/area
            // Transforming it to solid angle requires dividing by cos(n, wi)/r²
            return glm::length2(ref - light_hit) / (abs(dot(n, wi)) * area());
        }
    };

    class CircleLight : public AreaLight {
    private:
        glm::vec3 _origin;
        glm::vec3 _n;
    public:
        float _r;
        CircleLight(const glm::vec3 &origin, const glm::vec3 &n, float r, const glm::vec3 &_color)
                : _origin(origin), _n(n), _r(r), AreaLight(_color) {}

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            float dx, dy;
            concentricSampleDisk(&dx, &dy);
            glm::vec3 tan = perpendicular(_n);
            glm::vec3 cotan = normalize(cross(_n, tan));
            glm::vec3 light_hit = _origin + tan * dx + cotan * dy;
            *wi = normalize(light_hit - ref);
            *pdf = pdf_li(light_hit, _n, ref, *wi);
            // Emits light only from the normal side
            if (dot(*wi, _n) > 0.f) {
                return glm::vec3(0.f);
            }
            return color;
        }

        float area() const override {
            return float(M_PI * _r * _r);
        }
    };

    class RectangleLight : public AreaLight {
    private:
        glm::vec3 _origin;
        glm::vec3 _side1, _side2; // Should be orthogonal, normal is cross(side1, side2)
        glm::vec3 _n;
    public:
        RectangleLight(const glm::vec3 &origin, const glm::vec3 &side1, const glm::vec3 &side2, const glm::vec3 &_color)
                : _origin(origin), _side1(side1), _side2(side2), AreaLight(_color) {
            if (abs(dot(_side1, _side2)) > 0.0001) {
                printf("ERROR in RectangleLight: Sides must be orthogonal!\n");
            }
            _n = normalize(cross(_side1, _side2));
        }

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            // Uniform sampling over the area of the rectangle
            glm::vec3 light_hit = _origin + _side1 * randf() + _side2 * randf();
            *wi = normalize(light_hit - ref);
            *pdf = pdf_li(light_hit, _n, ref, *wi);
            return color;
        }

        float area() const override {
            return glm::length(_side1) * glm::length(_side2);
        }
    };
}
#endif //COMPUTER_GRAPHICS_LABS_LIGHT_H
