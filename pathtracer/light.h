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

namespace pathtracer {
    class Light {
    public:
        glm::vec3 color;
        float intensity;

        Light(const glm::vec3 &color, float intensity) : color(color), intensity(intensity) {}

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
            return intensity * color / glm::length2(_position - ref);
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

    class RectangleLight : public AreaLight {
    private:
        glm::vec3 _origin;
        glm::vec3 _side1, _side2; // Should be orthogonal, normal is cross(side1, side2)
        glm::vec3 _n;
    public:
        RectangleLight(const glm::vec3 &origin, const glm::vec3 &side1, const glm::vec3 &side2, const glm::vec3 &_color,
                float _intensity)
                : _origin(origin), _side1(side1), _side2(side2), AreaLight(_color, _intensity) {
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
            // Emits light only from the normal side
            if (dot(*wi, _n) > 0.f) {
                return glm::vec3(0.f);
            }
            return color * intensity;
        }

        float area() const override {
            return glm::length(_side1) * glm::length(_side2);
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

    /**
     * Use to "draw" the light and see visually the effects of transformations
     */
    class LightHelper {
    protected:
        Light* light;
    public:
        LightHelper(Light *light) : light(light) {}
        virtual void init() = 0;
        virtual void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const = 0;
    };

    class RectangleLightHelper: public LightHelper {
    private:
        GLuint m_vao, m_positionBuffer, m_indexBuffer;
        RectangleLight* recLight;
    public:
        RectangleLightHelper(RectangleLight *light) : LightHelper(light), recLight(light) {}
        void init() override {
            glm::vec3 vertices[] = {
                    glm::vec3(0.f, 0.f, 0.f),
                    recLight->getSide1(),
                    recLight->getSide2(),
                    recLight->getSide1() + recLight->getSide2()
            };

            glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);

            // Send them already, to erase them from memory
            glGenBuffers(1, &m_positionBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 4, vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);
            // Enable the attribute
            glEnableVertexAttribArray(0);
        }

        void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const override {
            glm::mat4 modelMatrix = glm::translate(recLight->getOrigin());
            labhelper::setUniformSlow(shader, "modelViewProjectionMatrix",
                    projectionMatrix * viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "lightColor", light->color);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    };

    class CircleLightHelper: public LightHelper {
    private:
        GLuint m_vao, m_positionBuffer, m_indexBuffer;
        CircleLight* circleLight;
        int resolution;
    public:
        CircleLightHelper(CircleLight *light) : LightHelper(light), circleLight(light) {}
        void init() override {
            resolution = 100;

            glm::vec3 vertices[resolution];
            glm::vec3 tan = glm::normalize(perpendicular(circleLight->getN()));
            glm::vec3 cotan = glm::normalize(glm::cross(circleLight->getN(), tan));

            for (int i = 0; i < resolution; i++) {
                float ang = i * 2 * M_PIf32 / resolution;
                vertices[i] = tan * cos(ang) + cotan * sin(ang);
            }

            glGenVertexArrays(1, &m_vao);
            glBindVertexArray(m_vao);

            // Send them already, to erase them from memory
            glGenBuffers(1, &m_positionBuffer);
            glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
            glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * resolution, vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);
            // Enable the attribute
            glEnableVertexAttribArray(0);
        }

        void draw(const GLuint shader, const glm::mat4 &projectionMatrix, const glm::mat4 &viewMatrix) const override {
            glm::mat4 modelMatrix = glm::translate(circleLight->getOrigin()) * glm::scale(glm::vec3(circleLight->_r));
            labhelper::setUniformSlow(shader, "modelViewProjectionMatrix",
                    projectionMatrix * viewMatrix * modelMatrix);
            labhelper::setUniformSlow(shader, "lightColor", light->color);
            glBindVertexArray(m_vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, resolution);
        }
    };
}
#endif //COMPUTER_GRAPHICS_LABS_LIGHT_H
