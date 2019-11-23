
#include "heightfield.h"

#include <iostream>
#include <stdint.h>
#include <vector>
#include <stb_image.h>
#include <labhelper.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;
using std::string;

HeightField::HeightField(void)
        : m_meshResolution(0), m_vao(UINT32_MAX), m_positionBuffer(UINT32_MAX), m_uvBuffer(UINT32_MAX),
          m_indexBuffer(UINT32_MAX), m_numIndices(0), m_texid_hf(UINT32_MAX), m_texid_diffuse(UINT32_MAX),
          m_heightFieldPath(""), m_diffuseTexturePath("") {
}

void HeightField::loadHeightField(const std::string &heigtFieldPath) {
    int width, height, components;
    stbi_set_flip_vertically_on_load(true);
    float *data = stbi_loadf(heigtFieldPath.c_str(), &width, &height, &components, 1);
    if (data == nullptr) {
        std::cout << "Failed to load image: " << heigtFieldPath << ".\n";
        return;
    }

    if (m_texid_hf == UINT32_MAX) {
        glGenTextures(1, &m_texid_hf);
    }
    glBindTexture(GL_TEXTURE_2D, m_texid_hf);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT,
                 data); // just one component (float)

    m_heightFieldPath = heigtFieldPath;
    std::cout << "Successfully loaded heigh field texture: " << heigtFieldPath << ".\n";
}

void HeightField::loadDiffuseTexture(const std::string &diffusePath) {
    int width, height, components;
    stbi_set_flip_vertically_on_load(true);
    uint8_t *data = stbi_load(diffusePath.c_str(), &width, &height, &components, 3);
    if (data == nullptr) {
        std::cout << "Failed to load image: " << diffusePath << ".\n";
        return;
    }

    if (m_texid_diffuse == UINT32_MAX) {
        glGenTextures(1, &m_texid_diffuse);
    }

    glBindTexture(GL_TEXTURE_2D, m_texid_diffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); // plain RGB
    glGenerateMipmap(GL_TEXTURE_2D);

    std::cout << "Successfully loaded diffuse texture: " << diffusePath << ".\n";
}


void HeightField::generateMesh(int tesselation) {
    float uvStep = 1.f / tesselation;
    float step = uvStep * 2.f;
    int nVertices = tesselation + 1; // 1 vertex more than faces

    int verticesSize = nVertices * nVertices;
    // Allocate in the heap because in the stack we get segmentation fault with 1024 of tesselation (1 million vecs)
    vec3 *vertices = new vec3[verticesSize];
    vec2 *uv = new vec2[verticesSize];

    for (int i = 0; i < nVertices; i++) {
        for (int j = 0; j < nVertices; j++) {
            vertices[i * nVertices + j] = vec3(j * step - 1.f, 0.f, i * step - 1.f);
            uv[i * nVertices + j] = vec2(j * uvStep, i * uvStep);
        }
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // Send them already, to erase them from memory
    glGenBuffers(1, &m_positionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * verticesSize, vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, nullptr);
    // Enable the attribute
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &m_uvBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_uvBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * verticesSize, uv, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, nullptr);
    // Enable the attribute
    glEnableVertexAttribArray(1);

    delete[] vertices;
    delete[] uv;

    // 2 per strip + 1 extra for primitive restart, as many strips as tesselation
    m_numIndices = (2 * nVertices + 1) * tesselation;
    int *indices = new int[m_numIndices];
    int next = 0;

    // We use tesselation cause there is no need for an extra strip
    for (int i = 0; i < tesselation; i++) {
        // But we do need an extra couple of vertices to close each strip
        for (int j = 0; j < nVertices; j++) {
            indices[next++] = i * nVertices + j;
            indices[next++] = (i + 1) * nVertices + j;
        }
        indices[next++] = UINT32_MAX; // primitive restart
    }

    glGenBuffers(1, &m_indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * m_numIndices, indices, GL_STATIC_DRAW);

    delete[] indices;

    m_meshResolution = tesselation;
}

void HeightField::draw(GLuint currentShaderProgram, const mat4 &viewMatrix, const mat4 &projectionMatrix,
                       const float environment_multiplier, bool use_ssao) {
    if (m_vao == UINT32_MAX) {
        std::cout << "No vertex array is generated, cannot draw anything.\n";
        return;
    }
    glUseProgram(currentShaderProgram);

    mat4 modelMatrix = scale(translate(mat4(1), vec3(0.f, -300.f, 0.f)), vec3(20000.f, 1000.f, 20000.f));
    labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                              projectionMatrix * viewMatrix * modelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
                              inverse(transpose(viewMatrix)));
    labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));
    labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);
    labhelper::setUniformSlow(currentShaderProgram, "useSsao", use_ssao);
    // Material
    glUniform1fv(glGetUniformLocation(currentShaderProgram, "material_reflectivity"), 1, &m_reflectivity);
    glUniform1fv(glGetUniformLocation(currentShaderProgram, "material_metalness"), 1, &m_metalness);
    glUniform1fv(glGetUniformLocation(currentShaderProgram, "material_fresnel"), 1, &m_fresnel);
    glUniform1fv(glGetUniformLocation(currentShaderProgram, "material_shininess"), 1, &m_shininess);

//    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texid_hf);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_texid_diffuse);
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLE_STRIP, m_numIndices, GL_UNSIGNED_INT, nullptr);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}