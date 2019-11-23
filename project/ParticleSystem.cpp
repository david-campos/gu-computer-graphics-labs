#include "ParticleSystem.h"
#include "hdr.h"

#include <algorithm>
#include <stb_image.h>

void ParticleSystem::init() {
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * max_size, nullptr, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(0, 4, GL_FLOAT, false, 0, nullptr);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    int w, h, comp;
    unsigned char* image = stbi_load("../../scenes/explosion.png", &w, &h, &comp, STBI_rgb_alpha);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    free(image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void ParticleSystem::uploadGpuData(const glm::mat4 &viewMatrix,
                                   const glm::mat4 &projectionMatrix) {
    std::vector<glm::vec4> data;
    for (auto particle: particles) {
        glm::vec4 pos = viewMatrix * glm::vec4(particle.pos, 1.f);
        data.emplace_back(
                pos.x, pos.y, pos.z,
                particle.lifetime / particle.life_length
        );
    }
    std::sort(data.begin(), std::next(data.begin(), data.size()),
              [](const glm::vec4 &lhs, const glm::vec4 &rhs) { return lhs.z < rhs.z; });

    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4) * data.size(), &data[0]);
}

void ParticleSystem::kill(unsigned idx) {
    particles[idx] = particles.back();
    particles.pop_back();
}

void ParticleSystem::spawn() {
    if (particles.size() < max_size) {
        const float theta = labhelper::uniform_randf(0.f, 2.f * M_PI);
        const float u = labhelper::uniform_randf(/*-1.f*/0.99f, 1.f);
        Particle particle{};
        particle.velocity = glm::normalize(
                glm::vec3(spawnModelMatrix * (glm::vec4(
                        u,
                        sqrt(1.f - u * u) * cosf(theta),
                        sqrt(1.f - u * u) * sinf(theta),
                        0.0f
                )))) * initial_velocity;
        particle.pos = glm::vec3(spawnModelMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f));
        particle.lifetime = 0.f;
        particle.life_length = life_length;
        particles.push_back(particle);
    }
}

void ParticleSystem::process_particles(float dt, float currentTime) {
    for (unsigned i = 0; i < particles.size(); ++i) {
        Particle &particle = particles[i];
        if (particle.lifetime >= particle.life_length) {
            kill(i);
            i--;
        }
    }
    for (auto &particle: particles) {
        particle.lifetime += dt;
        particle.velocity.y -= gravity * dt;
        glm::vec3 resistance = particle.velocity * air_resistance * dt;
        if (length(resistance) < length(particle.velocity)) particle.velocity -= resistance;
        else particle.velocity = glm::vec3(0);
        particle.pos += particle.velocity * dt;
    }

    if (halted) return;

    particles_to_spawn += pps * dt;
    while (particles_to_spawn > 1.f) {
        spawn();
        particles_to_spawn -= 1.f;
    }
}

void ParticleSystem::draw(GLuint currentShaderProgram,
                          const glm::mat4 &viewMatrix, const glm::mat4 &projectionMatrix,
                          float windowWidth, float windowHeight) {
    uploadGpuData(viewMatrix, projectionMatrix);
    glUseProgram(currentShaderProgram);
    labhelper::setUniformSlow(currentShaderProgram, "screen_x", windowWidth);
    labhelper::setUniformSlow(currentShaderProgram, "screen_y", windowHeight);
    labhelper::setUniformSlow(currentShaderProgram, "P", projectionMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, particles.size());
}
