#pragma once


#include <GL/glew.h>
#include <vector>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>
#include <labhelper.h>

struct Particle {
    float lifetime;
    float life_length;
    glm::vec3 velocity;
    glm::vec3 pos;
};

class ParticleSystem {
private:
    GLuint vao = 0;
    GLuint buffer = 0;
    GLuint texture = 0;
    float particles_to_spawn = 0.f;

    void kill(unsigned idx);

    void spawn();

    void uploadGpuData(const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix);

public:
    std::vector<Particle> particles;
    int max_size;
    float gravity;
    float air_resistance;
    float initial_velocity;
    float life_length;
    bool halted;
    float pps;
    glm::mat4 spawnModelMatrix;

    ParticleSystem() : ParticleSystem(0, 0.f, 0.f, 0.f, 0.f, 0.f) {}

    explicit ParticleSystem(int size) : ParticleSystem(size, 1256.f, 0.f, 0.f, 30.f, 1.f) {}

    ParticleSystem(int size, float particles_per_second, float gravity, float air_res, float velocity, float lifetime)
            : max_size(size), gravity(gravity), air_resistance(air_res), spawnModelMatrix(1.f),
              pps(particles_per_second), initial_velocity(velocity), life_length(lifetime), halted(false) {
    }

    ~ParticleSystem() = default;

    void init();

    void process_particles(float dt, float ct);

    void draw(GLuint currentShaderProgram, const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix,
            float windowWidth, float windowHeight);
};
