//
// Created by david on 23/4/20.
//

#ifndef COMPUTER_GRAPHICS_LABS_LIGHT_H
#define COMPUTER_GRAPHICS_LABS_LIGHT_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "labhelper.h"
#include "sampling.h"
#include "geometry.h"
#include "embree.h"

namespace pathtracer {
    class Light {
    public:
        glm::vec3 color;
        float intensity;

        Light(const glm::vec3 &color, float intensity) : color(color), intensity(intensity) {}

        virtual glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const = 0;

        virtual bool isDelta() const = 0;

        /**
         * Check the intersection with the light, and if the light is intersected,
         * change the tfar of the ray accordingly. Notice this does not check
         * any other geometry in the scene, occlusion should be tested after.
         * @param ray the ray to intersect with the light
         * @return true if the ray intersects the light, or false if not
         */
        virtual bool checkIntersection(Ray &ray) const = 0;

        virtual float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n,
                             const glm::vec3 &ref, const glm::vec3 &wi) const = 0;
    };

    class PointLight : public Light {
    private:
        glm::vec3 _position;
    public:
        PointLight(const glm::vec3 &color, float intensity, const glm::vec3 &position) : Light(color, intensity),
                                                                                         _position(position) {}

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            *wi = normalize(_position - ref);
            *pdf = 1.f;
            return intensity * color / glm::length2(_position - ref);
        }

        bool checkIntersection(Ray &ray) const override {
            return false;
        }

        bool isDelta() const override {
            return true;
        }

        float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n, const glm::vec3 &ref,
                     const glm::vec3 &wi) const override {
            return 0;
        }
    };

    class AreaLight : public Light {
    public:
        AreaLight(const glm::vec3 &color, float intensity) : Light(color, intensity) {}

        virtual float area() const = 0;

        bool isDelta() const override {
            return false;
        }

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
        glm::vec3 _n;
    public:
        float _r;
        glm::vec3 _origin;

        CircleLight(const glm::vec3 &origin, const glm::vec3 &n, float r, const glm::vec3 &_color, float _intensity)
                : _origin(origin), _n(n), _r(r), AreaLight(_color, _intensity) {}

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            float dx, dy;
            concentricSampleDisk(&dx, &dy);
            glm::vec3 tan = normalize(perpendicular(_n));
            glm::vec3 cotan = normalize(cross(_n, tan));
            glm::vec3 light_hit = _origin + (tan * dx + cotan * dy) * _r;
            *wi = normalize(light_hit - ref);
            *pdf = pdf_li(light_hit, _n, ref, *wi);
            // Emits light only from the normal side
            if (dot(*wi, _n) > 0.f) {
                return glm::vec3(0.f);
            }
            return color * intensity;
        }

        bool checkIntersection(Ray &ray) const override {
            float dn = dot(ray.d, _n);
            if (dn >= 0) return false; // Can only be intersected from the front
            float t = dot(_n, _origin - ray.o) / dn;
            if (t < ray.tnear || t > ray.tfar) return false; // outside ray segment
            glm::vec3 p = ray.o + t * ray.d;
            bool intersects = glm::length2(p) <= _r * _r;
            if (intersects) { ray.tfar = t; }
            return intersects;
        }

        float area() const override {
            return float(M_PI * _r * _r);
        }

        const glm::vec3 &getOrigin() const {
            return _origin;
        }

        const glm::vec3 &getN() const {
            return _n;
        }
    };

    class ParallelogramLight : public AreaLight {
    private:
        glm::vec3 _side1, _side2; // Should be orthogonal, normal is cross(side1, side2)
        glm::vec3 _n;
    public:
        glm::vec3 _origin;

        ParallelogramLight(const glm::vec3 &origin, const glm::vec3 &side1, const glm::vec3 &side2, const glm::vec3 &_color,
                           float _intensity)
                : _origin(origin), _side1(side1), _side2(side2), AreaLight(_color, _intensity) {
            _n = normalize(cross(_side1, _side2));
        }

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            // Uniform sampling over the area of the rectangle
            glm::vec3 light_hit = _origin + _side1 * randf() + _side2 * randf();
            *wi = normalize(light_hit - ref);
            *pdf = pdf_li(light_hit, _n, ref, *wi);
            // Emits light only from the normal side
            if (dot(*wi, _n) > 0.f) {
                return glm::vec3(0.f);
            }
            return color * intensity;
        }

        bool checkIntersection(Ray &ray) const override {
            float dn = dot(ray.d, _n);
            if (dn >= 0) return false; // Can only be intersected from the front
            float t = dot(_n, _origin - ray.o) / dn;
            if (t < ray.tnear || t > ray.tfar) return false; // outside ray segment
            glm::vec3 p = ray.o + t * ray.d;
            glm::vec3 C0 = p - _origin;
            glm::vec3 C1 = p - _origin - _side1;
            glm::vec3 C2 = p - _origin - _side1 - _side2;
            glm::vec3 C3 = p - _origin - _side2;
            if (dot(_n, cross(_side1, C0)) > 0
                && dot(_n, cross(_side2, C1)) > 0
                && dot(_n, cross(-_side1, C2)) > 0
                && dot(_n, cross(-_side2, C3)) > 0) {
                ray.tfar = t;
                return true;
            } else {
                return false;
            }
        }

        float area() const override {
            return length(cross(_side1, _side2));
        }

        const glm::vec3 &getSide1() const {
            return _side1;
        }

        const glm::vec3 &getSide2() const {
            return _side2;
        }

        const glm::vec3 &getOrigin() const {
            return _origin;
        }
    };

    class SphereLight : public Light {
    public:
        glm::vec3 center;
        float radius;

        SphereLight(const glm::vec3 &center, float radius, const glm::vec3 &_color,
                    float _intensity)
                : center(center), radius(radius), Light(_color, _intensity) {
        }

        bool isDelta() const override {
            return false;
        }

        glm::vec3 sample_li(const glm::vec3 &ref, glm::vec3 *wi, float *pdf) const override {
            if (glm::distance2(ref, center) <= radius * radius) {
                // Whole sphere always visible from inside
                glm::vec3 p = radius * uniformSampleSphere();
                *wi = glm::normalize(p - ref);
                *pdf = 1 / (2 * M_PIf32);
                return color * intensity;
            }

            // Compute coordinate system for sphere sampling
            glm::vec3 wc = glm::normalize(center - ref);
            glm::vec3 wcX, wcY;
            coordinateSystem(wc, &wcX, &wcY);

            // Compute theta and phi values for sample in cone
            float u0 = randf();
            float u1 = randf();
            float sinThetaMax2 = radius * radius / glm::distance2(ref, center);
            float cosThetaMax = glm::sqrt(glm::max(0.f, 1 - sinThetaMax2));
            float cosTheta = (1 - u0) + u0 * cosThetaMax;
            float sinTheta = glm::sqrt(glm::max(0.f, 1 - cosTheta * cosTheta));
            float phi = u1 * 2 * M_PIf32;

            // Compute angle from center of sphere to sampled point on surface>>
            float dc = glm::distance(ref, center);
            float ds = dc * cosTheta - glm::sqrt(glm::max(0.f,
                    radius * radius - dc * dc * sinTheta * sinTheta));
            float cosAlpha = (dc * dc + radius * radius - ds * ds) /
                             (2 * dc * radius);
            float sinAlpha = glm::sqrt(glm::max(0.f, 1 - cosAlpha * cosAlpha));

            // Compute surface normal and sampled point on sphere
            glm::vec3 nObj = sinAlpha * glm::cos(phi) * (-wcX) +
                             sinAlpha * glm::sin(phi) * (-wcY) + cosAlpha * (-wc);
            glm::vec3 pObj = center + radius * nObj;

            *wi = glm::normalize(pObj - ref);
            *pdf = pdf_li(pObj, nObj, ref, *wi);
            return color * intensity;
        }

        bool checkIntersection(Ray &ray) const override {
            glm::vec3 oc = ray.o - center;
            float r2 = radius * radius;
            if (glm::length2(oc) <= r2) {
                ray.tfar = 0.f;
                return true;
            }
            float a = dot(ray.d, ray.d);
            float b = 2.f * dot(oc, ray.d);
            float c = dot(oc, oc) - r2;
            float discriminant = b * b - 4 * a * c;
            if (discriminant < 0) {
                return false;
            } else {
                float t = (-b - sqrtf(discriminant)) / (2.f * a);
                if (t > 0) {
                    ray.tfar = t;
                    return true;
                } else {
                    return false;
                }
            }
        }

        float pdf_li(const glm::vec3 &light_hit, const glm::vec3 &n, const glm::vec3 &ref,
                     const glm::vec3 &wi) const override {
            if (glm::distance2(ref, center) <= radius * radius) {
                return 1.f / (2.f * M_PIf32);
            }

            float sinThetaMax2 = radius * radius / glm::distance2(ref, center);
            float cosThetaMax = glm::sqrt(glm::max(0.f, 1.f - sinThetaMax2));
            return 1.f / (2.f * M_PIf32 * (1.f - cosThetaMax));
        }
    };

    /**
     * Use to "draw" the light and see visually the effects of transformations
     */
    class LightHelper {
    protected:
        Light *light;
    public:
        LightHelper(Light *light) : light(light) {}

        virtual void init() = 0;

        virtual void
        draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const = 0;
    };

    class RectangleLightHelper : public LightHelper {
    private:
        GLuint m_vao, m_positionBuffer, m_normalBuffer;
        ParallelogramLight *recLight;
    public:
        RectangleLightHelper(ParallelogramLight *light) : LightHelper(light), recLight(light) {}

        void init() override {
            glm::vec3 vertices[] = {
                    glm::vec3(0.f, 0.f, 0.f),
                    recLight->getSide1(),
                    recLight->getSide2(),
                    recLight->getSide1() + recLight->getSide2()
            };

            glm::vec3 normal = cross(recLight->getSide1(), recLight->getSide2());
            glm::vec3 normals[] = {normal, normal, normal, normal};

            glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);

            glGenBuffers(1, &m_positionBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);

            glGenBuffers(1, &m_normalBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_normalBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, normals, GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, nullptr);

            // Enable the attribute
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
        }

        void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const override {
            glm::mat4 modelMatrix = glm::translate(recLight->getOrigin());
            labhelper::setUniformSlow(shader, "modelViewMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "projectionMatrix", projectionMatrix);
            labhelper::setUniformSlow(shader, "viewMatrix", viewMatrix);
            labhelper::setUniformSlow(shader, "normalMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "lightColor", light->color);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    };

    class CircleLightHelper : public LightHelper {
    private:
        GLuint m_vao, m_positionBuffer, m_normalBuffer;
        CircleLight *circleLight;
        int resolution;
    public:
        CircleLightHelper(CircleLight *light) : LightHelper(light), circleLight(light) {}

        void init() override {
            resolution = 100;

            glm::vec3 vertices[resolution];
            glm::vec3 normals[resolution];
            glm::vec3 tan = glm::normalize(perpendicular(circleLight->getN()));
            glm::vec3 cotan = glm::normalize(glm::cross(circleLight->getN(), tan));

            for (int i = 0; i < resolution; i++) {
                float ang = i * 2 * M_PIf32 / resolution;
                vertices[i] = tan * cos(ang) + cotan * sin(ang);
                normals[i] = circleLight->getN();
            }

            glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);

            // Send them already, to erase them from memory
            glGenBuffers(1, &m_positionBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * resolution, vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);

            glGenBuffers(1, &m_normalBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_normalBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * resolution, normals, GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, nullptr);

            // Enable the attribute
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
        }

        void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const override {
            glm::mat4 modelMatrix = glm::translate(circleLight->getOrigin()) * glm::scale(glm::vec3(circleLight->_r));
            labhelper::setUniformSlow(shader, "modelViewMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "viewMatrix", viewMatrix);
            labhelper::setUniformSlow(shader, "projectionMatrix", projectionMatrix);
            labhelper::setUniformSlow(shader, "normalMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "lightColor", light->color);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, resolution);
        }
    };

    class SphereLightHelper : public LightHelper {
    private:
        labhelper::Model *model;
        SphereLight *sphereLight;
    public:
        SphereLightHelper(SphereLight *light) : LightHelper(light), sphereLight(light) {}

        void init() override {
            model = labhelper::loadModelFromOBJ("../../scenes/sphere.obj");
        }

        void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const override {
            glm::mat4 modelMatrix = glm::scale(glm::translate(sphereLight->center), glm::vec3(sphereLight->radius));
            labhelper::setUniformSlow(shader, "modelViewMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "viewMatrix", viewMatrix);
            labhelper::setUniformSlow(shader, "projectionMatrix", projectionMatrix);
            labhelper::setUniformSlow(shader, "normalMatrix", viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "lightColor", light->color);
            labhelper::render(model, false);
        }
    };
}
#endif //COMPUTER_GRAPHICS_LABS_LIGHT_H
