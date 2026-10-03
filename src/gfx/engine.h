#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "gfx/mesh.h"
#include "sim/particle.h"

// C2: cache uniform locations once, fail loudly on typos.
struct UniformCache {
    GLint view = -1, projection = -1, uScale = -1;
    GLint lightPos = -1, viewPos = -1, uCutaway = -1;
    void init(GLuint program);
};

class Engine {
public:
    Engine(int w, int h, const char* title);

    glm::mat4 projectionMatrix(float fovDeg = 45.0f,
                               float nearZ = 0.1f, float farZ = 500.0f);

    void drawParticlesInstanced(const std::vector<Particle>& particles, float scale);

    GLFWwindow* window = nullptr;
    GLuint shaderSphere = 0;
    UniformCache uniforms;
    Mesh sphere;

private:
    GLuint instanceVBO_pos = 0, instanceVBO_col = 0;

    // C4: these were function-local `static` vectors (hidden global state).
    // Now honest members.
    std::vector<glm::vec3> instancePositions;
    std::vector<glm::vec3> instanceColors;
};