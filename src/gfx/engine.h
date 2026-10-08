// gfx/engine.h
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "gfx/gl.h"
#include "gfx/mesh.h"
#include "sim/particle.h"

struct UniformCache {
    GLint view = -1, projection = -1, uScale = -1;
    GLint lightPos = -1, viewPos = -1, uCutaway = -1;
    void init(GLuint program);
};

struct FrameUniforms {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 lightPos;
    glm::vec3 viewPos;
    bool      cutaway;
};

class Engine {
public:
    Engine(int w, int h, const char* title);
    ~Engine();
    Engine(const Engine&) = delete;            
    Engine& operator=(const Engine&) = delete;

    // False if window, context or shader creation faile
    bool ok() const { return window != nullptr && program != 0; }

    glm::mat4 projectionMatrix(float fovDeg = 45.0f,
                               float nearZ = 0.1f, float farZ = 500.0f) const;

    void beginFrame(const FrameUniforms& u);                     // clear + set uniforms
    void uploadColors(const std::vector<Particle>& particles);   // call after each resample
    void drawParticles(const std::vector<Particle>& particles, float scale);
    void endFrame();                                             // error check + swap

    GLFWwindow* window = nullptr;

private:
    GLuint program = 0;
    UniformCache uniforms;
    Mesh sphere;

    GLuint vboPos = 0, vboCol = 0;
    size_t posCapacity = 0, colCapacity = 0;   // allocated GPU size 
    size_t colorCount = 0;                     

    std::vector<glm::vec3> scratch;            // was a hidden function-local static in ye old version
};
