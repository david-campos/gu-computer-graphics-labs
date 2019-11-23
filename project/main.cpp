

#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"
#include "ParticleSystem.h"
#include "heightfield.h"


using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window *g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
bool lightManualOnly = false;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = {-1, -1};
bool g_isMouseDragging = false;

enum Print {
    Normals,
    SSAO,
    Final
};

enum CameraMode {
    NORMAL,
    FIRST_PERSON,
    THIRD_PERSON
};

CameraMode cameraMode = NORMAL;
bool cameraModeChangePressed = false;
int print = (int) Final;

bool useSsao = true;
float margin = 0.001f;
bool useRotation = true;
bool blurSsao = true;
bool drawHeightField = true;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint normalShaderProgram; // Normals shader
GLuint ssaoShaderProgram;
GLuint texToScreenShaderProgram;
GLuint horizontalBlurShader, verticalBlurShader;
GLuint particlesShaderProgram;
GLuint heightFieldShaderProgram, hfNormalsShaderProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;

int requiredSamplesNum = 50;
int samplesNum = requiredSamplesNum;
float hemisphereSize = 10.f;
vec3 *ssaoPoints;
const unsigned ANGLES_RESOL = 64;
GLuint ssaoAnglesTex;
FboInfo normalsBuffer(1);
FboInfo ssaoBuffer(1);

///////////////////////////////////////////////////////////////////////////////
// Shadow map
///////////////////////////////////////////////////////////////////////////////
enum ClampMode {
    Edge = 1,
    Border = 2
};

FboInfo shadowMapFB(1);
int shadowMapResolution = 1024;
int shadowMapClampMode = ClampMode::Edge;
bool shadowMapClampBorderShadowed = false;
bool usePolygonOffset = true;
bool useSoftFalloff = true;
bool useSpotLight = true;
float innerSpotlightAngle = 20.0f;
float outerSpotlightAngle = 23.0f;
bool useHardwarePCF = true;
float polygonOffset_factor = 3.0f;
float polygonOffset_units = 1.0f;

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 20.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model *fighterModel = nullptr;
labhelper::Model *landingpadModel = nullptr;
labhelper::Model *sphereModel = nullptr;

// Particle system
ParticleSystem particleSystem(10000, 200, 20.f, 0.5f, 50.f, 1.f);
bool manualParticleRate = false;

// Height field
HeightField heightField;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;
mat4 R, T;

vec3 fighterSpeed(0.f);
float rotating = 0.f; // -1 left, 1 right, 0 no
float stoppedFactor = 0.f; // for stop animation
const float FIGHTER_ACC = 1.f;
const float FIGHTER_MAX_SPEED = 5.f;
const float FIGHTER_TURN_SPEED = 5.0f;


void recreateSamplePoints() {
    if (ssaoPoints != NULL) {
        free(ssaoPoints);
    }
    samplesNum = requiredSamplesNum;
    ssaoPoints = new vec3[samplesNum];
    for (unsigned int i = 0; i < samplesNum; i++) {
        ssaoPoints[i] = labhelper::cosineSampleHemisphere() * labhelper::randf() * 0.95f + 0.05f;
    }
}

void initGL() {
    ///////////////////////////////////////////////////////////////////////
    //		Load Shaders
    ///////////////////////////////////////////////////////////////////////
    backgroundProgram = labhelper::loadShaderProgram("../../project/background.vert",
                                                     "../../project/background.frag");
    shaderProgram = labhelper::loadShaderProgram("../../project/shading.vert", "../../project/shading.frag");
    simpleShaderProgram = labhelper::loadShaderProgram("../../project/simple.vert", "../../project/simple.frag");
    normalShaderProgram = labhelper::loadShaderProgram("../../project/normals.vert", "../../project/normals.frag");
    ssaoShaderProgram = labhelper::loadShaderProgram("../../project/postFx.vert", "../../project/ssao.frag");
    texToScreenShaderProgram = labhelper::loadShaderProgram("../../project/postFx.vert",
                                                            "../../project/textureToScreen.frag");
    horizontalBlurShader = labhelper::loadShaderProgram("../../project/postFx.vert",
                                                        "../../project/horizontal_blur.frag");
    verticalBlurShader = labhelper::loadShaderProgram("../../project/postFx.vert",
                                                      "../../project/vertical_blur.frag");
    particlesShaderProgram = labhelper::loadShaderProgram("../../project/particle.vert",
                                                          "../../project/particle.frag");
    heightFieldShaderProgram = labhelper::loadShaderProgram("../../project/heightfield.vert",
                                                            "../../project/heightfield.frag");
    hfNormalsShaderProgram = labhelper::loadShaderProgram("../../project/heightfield.vert",
                                                          "../../project/normals.frag");

    ///////////////////////////////////////////////////////////////////////
    // Load models and set up model matrices
    ///////////////////////////////////////////////////////////////////////
    fighterModel = labhelper::loadModelFromOBJ("../../scenes/NewShip.obj");
    landingpadModel = labhelper::loadModelFromOBJ("../../scenes/landingpad.obj");
    sphereModel = labhelper::loadModelFromOBJ("../../scenes/sphere.obj");

    roomModelMatrix = mat4(1.0f);
    T = translate(15.0f * worldUp);
    R = mat4(1);
    landingPadModelMatrix = mat4(1.0f);

    recreateSamplePoints();

    float randomAngles[ANGLES_RESOL * ANGLES_RESOL];
    for (auto &angle: randomAngles) {
        angle = labhelper::randf();
    }
    glGenTextures(1, &ssaoAnglesTex);
    glBindTexture(GL_TEXTURE_2D, ssaoAnglesTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, ANGLES_RESOL, ANGLES_RESOL, 0, GL_RGB, GL_UNSIGNED_BYTE, randomAngles);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    ///////////////////////////////////////////////////////////////////////
    // Load environment map
    ///////////////////////////////////////////////////////////////////////
    const int roughnesses = 8;
    std::vector<std::string> filenames;
    for (int i = 0; i < roughnesses; i++)
        filenames.push_back("../../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

    reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
    environmentMap = labhelper::loadHdrTexture("../../scenes/envmaps/" + envmap_base_name + ".hdr");
    irradianceMap = labhelper::loadHdrTexture("../../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");

    ///////
    // Buffers
    ///////
    int w, h;
    SDL_GetWindowSize(g_window, &w, &h);
    normalsBuffer.resize(w, h);
    ssaoBuffer.resize(w, h);

    ///////////////////////////////////////////////////////////////////////
    // Setup Framebuffer for shadow map rendering
    ///////////////////////////////////////////////////////////////////////
    shadowMapFB.resize(shadowMapResolution, shadowMapResolution);
    glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

    // Particles
    particleSystem.init();

    // Heightfield
    heightField.generateMesh(750);
    heightField.loadHeightField("../../scenes/nlsFinland/L3123F.png");
    heightField.loadDiffuseTexture("../../scenes/nlsFinland/L3123F_downscaled.jpg");

    glEnable(GL_DEPTH_TEST); // enable Z-buffering
    glEnable(GL_CULL_FACE);  // enables backface culling
    // Enable shader program point size modulation.
    glEnable(GL_PROGRAM_POINT_SIZE);
    // Enable primitive restart for triangle strips in heightField
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(UINT32_MAX);
    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void debugDrawLight(const glm::mat4 &viewMatrix,
                    const glm::mat4 &projectionMatrix,
                    const glm::vec3 &worldSpaceLightPos) {
    mat4 modelMatrix = glm::translate(worldSpaceLightPos);
    glUseProgram(shaderProgram);
    labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
                              projectionMatrix * viewMatrix * modelMatrix);
    labhelper::render(sphereModel);
}


void drawBackground(const mat4 &viewMatrix, const mat4 &projectionMatrix) {
    glUseProgram(backgroundProgram);
    labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
    labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
    labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
    labhelper::drawFullScreenQuad();
}

void drawScene(GLuint currentShaderProgram,
               const mat4 &viewMatrix,
               const mat4 &projectionMatrix,
               const mat4 &lightViewMatrix,
               const mat4 &lightProjectionMatrix) {
    glUseProgram(currentShaderProgram);
    // Light source
    vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
    labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
    labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
                              point_light_intensity_multiplier);
    labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
    labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
                              normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


    // Environment
    labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

    // camera
    labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

    // landing pad
    labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                              projectionMatrix * viewMatrix * landingPadModelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
                              inverse(transpose(viewMatrix * landingPadModelMatrix)));

    labhelper::render(landingpadModel);

    // Fighter
    labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                              projectionMatrix * viewMatrix * fighterModelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
    labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
                              inverse(transpose(viewMatrix * fighterModelMatrix)));

    labhelper::render(fighterModel);
}

void debugFullscreen(GLuint texture) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(texToScreenShaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    labhelper::drawFullScreenQuad();
}

void drawShadowMap(mat4 &lightViewMatrix, mat4 &lightProjMatrix) {
    glUseProgram(simpleShaderProgram);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFB.framebufferId);
    glViewport(0, 0, shadowMapFB.width, shadowMapFB.height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
    if (shadowMapClampMode == ClampMode::Edge) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    if (shadowMapClampMode == ClampMode::Border) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        vec4 border(shadowMapClampBorderShadowed ? 0.f : 1.f);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border.x);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useHardwarePCF ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, useHardwarePCF ? GL_LINEAR : GL_NEAREST);

    if (usePolygonOffset) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(polygonOffset_factor, polygonOffset_units);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawScene(simpleShaderProgram, lightViewMatrix, lightProjMatrix, lightViewMatrix, lightProjMatrix);

    if (usePolygonOffset) {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }
}

void display(void) {
    ///////////////////////////////////////////////////////////////////////////
    // Check if window size has changed and resize buffers as needed
    ///////////////////////////////////////////////////////////////////////////
    {
        int w, h;
        SDL_GetWindowSize(g_window, &w, &h);
        if (w != windowWidth || h != windowHeight) {
            windowWidth = w;
            windowHeight = h;
            normalsBuffer.resize(w, h);
            ssaoBuffer.resize(w, h);
            glViewport(0, 0, w, h);
        }
    }

    if (shadowMapFB.width != shadowMapResolution || shadowMapFB.height != shadowMapResolution) {
        shadowMapFB.resize(shadowMapResolution, shadowMapResolution);
    }

    ///////////////////////////////////////////////////////////////////////////
    // setup matrices
    ///////////////////////////////////////////////////////////////////////////
    mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 10000.0f);
    mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

    if (length(fighterSpeed) == 0.f) {
        stoppedFactor = glm::min(1.f, stoppedFactor + deltaTime);
    } else {
        stoppedFactor = glm::max(0.f, stoppedFactor - deltaTime);
    }

    // Animation when stopped and rotating when turning
    if (stoppedFactor > 0.f) {
        mat4 rotated = rotate(T * R, cos(currentTime) * 0.03f * stoppedFactor, vec3(0.f, 0.f, 1.f));
        fighterModelMatrix = translate(
                rotate(rotated, rotating * -0.5f * (1.f - stoppedFactor), vec3(1.f, 0.f, 0.f)),
                vec3(0.0f, sin(currentTime) * 0.5 * stoppedFactor, 0.0f));
    } else {
        mat4 rotated = T * R;
        fighterModelMatrix = rotate(rotated, rotating * -0.5f * (1.f - stoppedFactor), vec3(1.f, 0.f, 0.f));
    }

    if (!lightManualOnly) {
        lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
    } else {
        lightPosition = lightStartPosition;
    }
    mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
    mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

    ///////////////////////////////////////////////////////////////////////////
    // Bind the environment map(s) to unused texture units
    ///////////////////////////////////////////////////////////////////////////
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, environmentMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, irradianceMap);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, reflectionMap);

    if (useSsao || print != Final) {
        // Draw normals and depth
        glBindFramebuffer(GL_FRAMEBUFFER, normalsBuffer.framebufferId);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawScene(normalShaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
        if (drawHeightField) {
            heightField.draw(hfNormalsShaderProgram, viewMatrix, projMatrix, environment_multiplier, false);
        }

        if (print == (int) Normals) {
            debugFullscreen(normalsBuffer.colorTextureTargets[0]);
            debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
            return;
        }

        // SSAO drawing
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBuffer.framebufferId);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(ssaoShaderProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, normalsBuffer.depthBuffer);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalsBuffer.colorTextureTargets[0]);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, ssaoAnglesTex);
        labhelper::setUniformSlow(ssaoShaderProgram, "useRotation", useRotation);
        labhelper::setUniformSlow(ssaoShaderProgram, "projectionMatrix", projMatrix);
        labhelper::setUniformSlow(ssaoShaderProgram, "inverseProjectionMatrix", inverse(projMatrix));
        if (requiredSamplesNum != samplesNum) {
            recreateSamplePoints();
        }
        labhelper::setUniformSlow(ssaoShaderProgram, "number_samples", samplesNum);
        labhelper::setUniformSlow(ssaoShaderProgram, "hemisphere_radius", hemisphereSize);
        labhelper::setUniformSlow(ssaoShaderProgram, "samples", samplesNum, ssaoPoints);
        labhelper::setUniformSlow(ssaoShaderProgram, "epsilon", margin);
        labhelper::drawFullScreenQuad();

        if (blurSsao) {
            // We use the normals buffer as auxiliar buffer for this
            glBindFramebuffer(GL_FRAMEBUFFER, normalsBuffer.framebufferId);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(horizontalBlurShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ssaoBuffer.colorTextureTargets[0]);
            labhelper::drawFullScreenQuad();

            glBindFramebuffer(GL_FRAMEBUFFER, ssaoBuffer.framebufferId);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glUseProgram(verticalBlurShader);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, normalsBuffer.colorTextureTargets[0]);
            labhelper::drawFullScreenQuad();
        }

        if (print == (int) SSAO) {
            debugFullscreen(ssaoBuffer.colorTextureTargets[0]);
            debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
            return;
        }
    }

    drawShadowMap(lightViewMatrix, lightProjMatrix);
    glViewport(0, 0, windowWidth, windowHeight);

    ///////////////////////////////////////////////////////////////////////////
    // Draw from camera
    ///////////////////////////////////////////////////////////////////////////
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawBackground(viewMatrix, projMatrix);

    // Others
    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, shadowMapFB.depthBuffer);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, ssaoBuffer.colorTextureTargets[0]);
    mat4 lightMatrix =
            translate(vec3(0.5f)) * scale(vec3(0.5f)) * lightProjMatrix * lightViewMatrix * inverse(viewMatrix);
    labhelper::setUniformSlow(shaderProgram, "lightMatrix", lightMatrix);
    labhelper::setUniformSlow(shaderProgram, "useSsao", useSsao);
    labhelper::setUniformSlow(shaderProgram, "useSpotLight", useSpotLight);
    labhelper::setUniformSlow(shaderProgram, "useSoftFalloff", useSoftFalloff);
    labhelper::setUniformSlow(shaderProgram, "spotOuterAngle", std::cos(radians(outerSpotlightAngle)));
    labhelper::setUniformSlow(shaderProgram, "spotInnerAngle", std::cos(radians(innerSpotlightAngle)));

    drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);

    // Height terrain
    if (drawHeightField) {
        heightField.draw(heightFieldShaderProgram, viewMatrix, projMatrix, environment_multiplier, useSsao);
    }

    particleSystem.spawnModelMatrix = fighterModelMatrix * mat4(
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            19.5f, 3.f, 0.f, 1.f
    );
    particleSystem.draw(particlesShaderProgram, viewMatrix, projMatrix, (float) windowWidth, (float) windowHeight);
    glUseProgram(shaderProgram);

    debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
}

bool handleEvents(void) {
    // check events (keyboard among other)
    SDL_Event event;
    bool quitEvent = false;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)) {
            quitEvent = true;
        }
        if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g) {
            showUI = !showUI;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
            && (!showUI || !ImGui::GetIO().WantCaptureMouse)) {
            g_isMouseDragging = true;
            int x;
            int y;
            SDL_GetMouseState(&x, &y);
            g_prevMouseCoords.x = x;
            g_prevMouseCoords.y = y;
        }

        if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT))) {
            g_isMouseDragging = false;
        }

        if (event.type == SDL_MOUSEMOTION && g_isMouseDragging) {
            // More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
            int delta_x = event.motion.x - g_prevMouseCoords.x;
            int delta_y = event.motion.y - g_prevMouseCoords.y;
            float rotationSpeed = 1.0f;
            mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
            mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
                                normalize(cross(cameraDirection, worldUp)));
            cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
            g_prevMouseCoords.x = event.motion.x;
            g_prevMouseCoords.y = event.motion.y;
        }
    }

    // check keyboard state (which keys are still pressed)
    const uint8_t *state = SDL_GetKeyboardState(nullptr);
    vec3 cameraRight = cross(cameraDirection, worldUp);

    if (state[SDL_SCANCODE_C]) {
        if (!cameraModeChangePressed) {
            cameraMode = (CameraMode) ((cameraMode + 1) % 3);
            cameraModeChangePressed = true;
        }
    } else {
        cameraModeChangePressed = false;
    }
    if (cameraMode == NORMAL) {
        if (state[SDL_SCANCODE_W]) {
            cameraPosition += cameraSpeed * deltaTime * cameraDirection;
        }
        if (state[SDL_SCANCODE_S]) {
            cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
        }
        if (state[SDL_SCANCODE_A]) {
            cameraPosition -= cameraSpeed * deltaTime * cameraRight;
        }
        if (state[SDL_SCANCODE_D]) {
            cameraPosition += cameraSpeed * deltaTime * cameraRight;
        }
        if (state[SDL_SCANCODE_Q]) {
            cameraPosition -= cameraSpeed * deltaTime * worldUp;
        }
        if (state[SDL_SCANCODE_E]) {
            cameraPosition += cameraSpeed * deltaTime * worldUp;
        }
    }

    // FIGHTER
    float speed = length(fighterSpeed);
    bool forward = (inverse(R) * vec4(fighterSpeed, 0.f)).x < 0.f;
    if (state[SDL_SCANCODE_LEFT]) {
        rotating -= deltaTime * 2.f;
        if (rotating < -1.f) rotating = -1.f;
    }
    if (state[SDL_SCANCODE_RIGHT]) {
        rotating += deltaTime * 2.f;
        if (rotating > 1.f) rotating = 1.f;
    }
    if (!(state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_LEFT])) {
        if (rotating > 0.f) {
            rotating -= deltaTime;
            if (rotating < 0.f) rotating = 0.f;
        } else if (rotating < 0.f) {
            rotating += deltaTime;
            if (rotating > 0.f) rotating = 0.f;
        }
    }
    R[0] += FIGHTER_TURN_SPEED * (forward ? 1.f : -1.f) * rotating * (speed / FIGHTER_MAX_SPEED) * deltaTime * R[2];
    R[0] = normalize(R[0]);
    R[2] = vec4(cross(vec3(R[0]), vec3(R[1])), 0.0f);

    // Preserve 0.1 of the previous speed, add the new one but reduce because of air resistance
    fighterSpeed = fighterSpeed * 0.1f +
            vec3(R * vec4((forward ? -1.f : 1.f) * speed, 0.f, 0.f, 0.f)) * 0.9f;

    float accel = FIGHTER_ACC * deltaTime;
    if (state[SDL_SCANCODE_UP]) {
        if (length(fighterSpeed) < FIGHTER_MAX_SPEED - accel) {
            fighterSpeed += vec3(R * vec4(-1.f, 0.f, 0.f, 0.f)) * accel;
        }
    }
    if (state[SDL_SCANCODE_DOWN]) {
        if (length(fighterSpeed) > -FIGHTER_MAX_SPEED + accel) {
            fighterSpeed -= vec3(R * vec4(-1.f, 0.f, 0.f, 0.f)) * accel;
        }
    }
    if (!(state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_UP])) {
        if (length(fighterSpeed) > 0.f) {
            fighterSpeed -= 1.25f * accel > length(fighterSpeed) ?
                            fighterSpeed :
                            normalize(fighterSpeed) * accel * 1.25f;
        }
    }

    T[3] += vec4(fighterSpeed, 0.f);
    if (!manualParticleRate) {
        particleSystem.pps = forward ? 2000 * length(fighterSpeed) / FIGHTER_MAX_SPEED : 0;
    }

    // PLACE CAMERA
    switch (cameraMode) {
        case FIRST_PERSON: {
            cameraPosition = vec3(T * R * vec4(-10.f, 0.f, 0.f, 1.f));
            cameraDirection = normalize(vec3(R * vec4(-1.f, 0.f, 0.f, 0.f)));
            break;
        }
        case THIRD_PERSON: {
            cameraPosition = vec3(T * R * vec4(35.f, 12.f, 0.f, 1.f));
            cameraDirection = normalize(vec3(R * vec4(-1.f, -0.25f, 0.f, 0.f)));
            break;
        }
        default:
            break;
    }
    return quitEvent;
}

void gui() {
    // Inform imgui of new frame
    ImGui_ImplSdlGL3_NewFrame(g_window);

    ImGui::PushID("print");
    ImGui::Text("Print");
    ImGui::RadioButton("Normals", &print, (int) Normals);
    ImGui::RadioButton("SSAO", &print, (int) SSAO);
    ImGui::RadioButton("Final", &print, (int) Final);
    ImGui::PopID();

    ImGui::Checkbox("Heightfield", &drawHeightField);

    if (print == Final) {
        ImGui::Checkbox("Use SSAO", &useSsao);
    }

    if (print == SSAO || (print == Final && useSsao)) {
        if (ImGui::CollapsingHeader("SSAO", "ssao_ch", true, true)) {
            ImGui::SliderInt("Number of samples", &requiredSamplesNum, 1, 512);
            ImGui::SliderFloat("Hemisphere size", &hemisphereSize, 1.f, 50.f);
            ImGui::SliderFloat("Margin", &margin, 0.0f, 0.1f);
            ImGui::Checkbox("Blur SSAO", &blurSsao);
            ImGui::Checkbox("Use Rotation", &useRotation);
        }
    }
    if (ImGui::CollapsingHeader("Particle system", "particles_ch", true, true)) {
        ImGui::Checkbox("Manual rate", &manualParticleRate);
        if (manualParticleRate) {
            ImGui::SliderFloat("Particles per second", &particleSystem.pps, 0.f, 10000.f);
        }
        ImGui::SliderFloat("Lifetime", &particleSystem.life_length, 1.0f, 10.f);
        ImGui::SliderFloat("Initial velocity", &particleSystem.initial_velocity, 0.0f, 120.f);
        ImGui::SliderFloat("Gravity", &particleSystem.gravity, 0.0f, 30.f);
        ImGui::SliderFloat("Air resistance", &particleSystem.air_resistance, 0.0f, 5.f);
        ImGui::Checkbox("Stop", &particleSystem.halted);
    }

    if (drawHeightField && ImGui::CollapsingHeader("Height field", "height_ch", true, true)) {
        ImGui::SliderFloat("Fresnel", &heightField.m_fresnel, 0.f, 1.f);
        ImGui::SliderFloat("Metalness", &heightField.m_metalness, 0.f, 1.f);
        ImGui::SliderFloat("Reflectivity", &heightField.m_reflectivity, 0.f, 1.f);
        ImGui::SliderFloat("Shininess", &heightField.m_shininess, 0.f, 25000.f);
    }

    ///////////////////////////////////////////////////////////////////////////
    // Light and environment map
    ///////////////////////////////////////////////////////////////////////////
    if (ImGui::CollapsingHeader("Light sources", "lights_ch", true, true)) {
        ImGui::SliderFloat("Environment multiplier", &environment_multiplier, 0.0f, 10.0f);
        ImGui::ColorEdit3("Point light color", &point_light_color.x);
        ImGui::SliderFloat("Point light intensity multiplier", &point_light_intensity_multiplier, 0.0f,
                           10000.0f, "%.3f", 2.f);
        ImGui::Checkbox("Use spot light", &useSpotLight);

        if (useSpotLight) {
            ImGui::SliderFloat("Inner Deg.", &innerSpotlightAngle, 0.0f, 90.0f);
            ImGui::SliderFloat("Outer Deg.", &outerSpotlightAngle, 0.0f, 90.0f);
            ImGui::Checkbox("Use soft falloff", &useSoftFalloff);
        }

        ImGui::Text("Shadow map");
        ImGui::SliderInt("Shadow Map Resolution", &shadowMapResolution, 32, 2048);
        ImGui::Text("Polygon Offset");
        ImGui::Checkbox("Use polygon offset", &usePolygonOffset);
        ImGui::SliderFloat("Factor", &polygonOffset_factor, 0.0f, 10.0f);
        ImGui::SliderFloat("Units", &polygonOffset_units, 0.0f, 100.0f);
        ImGui::Text("Clamp Mode");
        ImGui::RadioButton("Clamp to edge", &shadowMapClampMode, ClampMode::Edge);
        ImGui::RadioButton("Clamp to border", &shadowMapClampMode, ClampMode::Border);
        ImGui::Checkbox("Border as shadow", &shadowMapClampBorderShadowed);
        ImGui::Checkbox("Use hardware PCF", &useHardwarePCF);

        ImGui::Checkbox("Stop light", &lightManualOnly);
    }

    // ----------------- Set variables --------------------------
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
    // ----------------------------------------------------------
    // Render the GUI.
    ImGui::Render();
}

int main(int argc, char *argv[]) {
    g_window = labhelper::init_window_SDL("OpenGL Project");

    initGL();

    bool stopRendering = false;
    auto startTime = std::chrono::system_clock::now();

    while (!stopRendering) {
        //update currentTime
        std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
        previousTime = currentTime;
        currentTime = timeSinceStart.count();
        deltaTime = currentTime - previousTime;
        particleSystem.process_particles(deltaTime, currentTime);
        // render to window
        display();

        // Render overlay GUI.
        if (showUI) {
            gui();
        }

        // Swap front and back buffer. This frame will now been displayed.
        SDL_GL_SwapWindow(g_window);

        // check events (keyboard among other)
        stopRendering = handleEvents();
    }
    // Free Models
    labhelper::freeModel(fighterModel);
    labhelper::freeModel(landingpadModel);
    labhelper::freeModel(sphereModel);

    // Shut down everything. This includes the window and all other subsystems.
    labhelper::shutDown(g_window);
    return 0;
}
