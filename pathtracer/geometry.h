//
// Created by david on 25/5/20.
//

#ifndef COMPUTER_GRAPHICS_LABS_GEOMETRY_H
#define COMPUTER_GRAPHICS_LABS_GEOMETRY_H

#include <glm/glm.hpp>
#include <cstring>

inline uint32_t floatToBits(float f) {
    uint32_t ui;
    memcpy(&ui, &f, sizeof(float));
    return ui;
}
inline float bitsToFloat(uint32_t ui) {
    float f;
    memcpy(&f, &ui, sizeof(uint32_t));
    return f;
}

inline float NextFloatUp(float v) {
    if (std::isinf(v) && v > 0.) {
        return v;
    }
    if (v == -0.f)
        v = 0.f;
    uint32_t ui = floatToBits(v);
    if (v >= 0) ++ui;
    else        --ui;
    return bitsToFloat(ui);
}

inline float NextFloatDown(float v) {
    if (std::isinf(v) && v > 0.) {
        return v;
    }
    if (v == -0.f)
        v = 0.f;
    uint32_t ui = floatToBits(v);
    if (v >= 0) --ui;
    else        ++ui;
    return bitsToFloat(ui);
}

inline glm::vec3 offsetRayOrigin(const glm::vec3 &p, const glm::vec3 &pError,
                               const glm::vec3 &n, const glm::vec3 &w) {
    float d = glm::dot(glm::abs(n), pError);
    glm::vec3 offset = d * glm::vec3(n);
    if (glm::dot(w, n) < 0)
        offset = -offset;
    glm::vec3 po = p + offset;
    // Round offset point po away from p
    for (int i = 0; i < 3; ++i) {
        if (offset[i] > 0)      po[i] = NextFloatUp(po[i]);
        else if (offset[i] < 0) po[i] = NextFloatDown(po[i]);
    }
    return po;
}

inline void
coordinateSystem(const glm::vec3 &v1, glm::vec3 *v2, glm::vec3 *v3) {
    if (std::abs(v1.x) > std::abs(v1.y))
        *v2 = glm::vec3(-v1.z, 0, v1.x) /
              glm::sqrt(v1.x * v1.x + v1.z * v1.z);
    else
        *v2 = glm::vec3(0, v1.z, -v1.y) /
              glm::sqrt(v1.y * v1.y + v1.z * v1.z);
    *v3 = glm::cross(v1, *v2);
}

#endif //COMPUTER_GRAPHICS_LABS_GEOMETRY_H
