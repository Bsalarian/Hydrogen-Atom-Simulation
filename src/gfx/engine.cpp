#include "gfx/engine.h"
#include "gfx/shaders.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

// --- helpers ----

static void glfwErrorCallback(int code, const char* desc) {
    std::cerr << "[GLFW " << code << "] " << desc << "\n";
}


static void uploadVec3(GLuint vbo, const std::vector<glm::vec3>& data, size_t& capacity) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    const GLsizeiptr bytes = (GLsizeiptr)(data.size() * sizeof(glm::vec3));
    if (data.size() > capacity) {
        glBufferData(GL_ARRAY_BUFFER, bytes, data.data(), GL_DYNAMIC_DRAW);
        capacity = data.size();
    } else if (bytes > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, data.data());
    }
}

void UniformCache::init(GLuint program) {
    auto get = [program](const char* name) {
        GLint loc = glGetUniformLocation(program, name);
        if (loc < 0)
            std::cerr << "[UNIFORM] '" << name
                      << "' not found (typo, or optimised away as unused)\n";
        return loc;
    };
    view       = get("view");
    projection = get("projection");
    uScale     = get("uScale");
    lightPos   = get("lightPos");
    viewPos    = get("viewPos");
    uCutaway   = get("uCutaway");
}

// --- main ---


    Engine::Engine(int w, int h, const char* title) {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) { std::cerr << "[ENGINE] glfwInit failed\n"; return; }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    #ifdef __EMSCRIPTEN__
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0); 
    #else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    #endif


    window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window) { std::cerr << "[ENGINE] window/context creation failed\n"; return; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);


    glClearColor(0.06f, 0.09f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);   // opaque balls: depth test on
    glDisable(GL_BLEND);       // blending off
    

    program = buildProgram(VERT_SPHERE_BODY, FRAG_SPHERE_BODY);
    if (!program) { std::cerr << "[ENGINE] shader program failed — see log above\n"; return; }
    uniforms.init(program);

    sphere = buildSphereMesh(1.0f, 12, 12); // the all-mighty ball
    

    // Per-instance attributes live in the sphere's VAO (locations 2 and 3, divisor 1).
    glGenBuffers(1, &vboPos);
    glGenBuffers(1, &vboCol);
    glBindVertexArray(sphere.vao);
    glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glVertexAttribDivisor(2, 1);
    glBindBuffer(GL_ARRAY_BUFFER, vboCol);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glVertexAttribDivisor(3, 1);
    glBindVertexArray(0);

    glCheck("Engine::Engine");
}


Engine::~Engine() {
    if (window) {
        glDeleteBuffers(1, &vboPos);
        glDeleteBuffers(1, &vboCol);
        glDeleteBuffers(1, &sphere.vbo);
        glDeleteBuffers(1, &sphere.ebo);
        glDeleteVertexArrays(1, &sphere.vao);
        if (program) glDeleteProgram(program);
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

// --- per frame ----

glm::mat4 Engine::projectionMatrix(float fovDeg, float nearZ, float farZ) const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    return glm::perspective(glm::radians(fovDeg), (float)w / (h ? h : 1), nearZ, farZ);
}

void Engine::beginFrame(const FrameUniforms& u) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program);
    glUniformMatrix4fv(uniforms.view,       1, GL_FALSE, glm::value_ptr(u.view));
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, glm::value_ptr(u.projection));
    glUniform3fv(uniforms.lightPos, 1, glm::value_ptr(u.lightPos));
    glUniform3fv(uniforms.viewPos,  1, glm::value_ptr(u.viewPos));
    glUniform1i (uniforms.uCutaway, u.cutaway ? 1 : 0);
}

void Engine::uploadColors(const std::vector<Particle>& particles) {
    scratch.resize(particles.size());
    for (size_t i = 0; i < particles.size(); ++i) scratch[i] = particles[i].color;
    uploadVec3(vboCol, scratch, colCapacity);
    colorCount = particles.size();
}

void Engine::drawParticles(const std::vector<Particle>& particles, float scale) {
    scratch.resize(particles.size());
    for (size_t i = 0; i < particles.size(); ++i) scratch[i] = particles[i].pos;
    uploadVec3(vboPos, scratch, posCapacity);

    // don't draw more instances than we have colours for
    const GLsizei count = (GLsizei)std::min(particles.size(), colorCount);
    if (count == 0) return;

    glUniform1f(uniforms.uScale, scale);
    glBindVertexArray(sphere.vao);
    glDrawElementsInstanced(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, nullptr, count);
    glBindVertexArray(0);
}

void Engine::endFrame() {
    glCheck("frame");
    glfwSwapBuffers(window);
}