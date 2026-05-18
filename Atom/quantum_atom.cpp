/*
CPU code (C++)
    ↓
Uploads data to GPU
    ↓
GPU shaders process vertices/pixels
    ↓
Framebuffer (image)
    ↓
Window shows final image

Modern OpenGL is mostly:

Create GPU buffers
Upload geometry
Write shaders
Draw

GLFW

Handles:

window creation
keyboard/mouse input
OpenGL context creation

GLEW

GLEW = OpenGL function loader.

Why needed?

Modern OpenGL functions are not exposed automatically by the OS.

GLM

GLM = math library for graphics.

VBO — Vertex Buffer Object : A VBO is GPU memory.

VAO — Vertex Array Object : VAO does NOT store vertices, it stores HOW vertex data is interpreted

Sphere geometry
    stored once on GPU

Particle system
    stores positions/colors only

Renderer
    reuses same sphere mesh repeatedly


gl_Position=projection⋅view⋅model⋅vec4(aPos,1.0)
*/

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static const int SCR_W = 1800;
static const int SCR_H = 1020;

// =====================================================
// SHADERS
// =====================================================
static const char* VERT_SRC = R"glsl(
#version 330 core
 
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
 
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
 
out vec3 fragPos;    // world-space position
out vec3 fragNormal; // world-space normal
 
void main()
{
    vec4 world = model * vec4(aPos, 1.0);
    fragPos    = vec3(world);
 
    // Normal matrix: inverse-transpose of the upper-left 3×3.
    // For a uniform scale this equals the model matrix,
    // but we do it properly so the code survives non-uniform scales.
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    fragNormal = normalize(normalMatrix * aNormal);
 
    gl_Position = projection * view * world;
}
)glsl";
 
static const char* FRAG_SRC = R"glsl(
#version 330 core
 
in vec3 fragPos;
in vec3 fragNormal;
 
uniform vec3  objectColor;
uniform vec3  lightPos;   // world-space light position
uniform vec3  viewPos;    // camera position (for specular)
 
out vec4 fragColor;
 
void main()
{
    // --- ambient ---
    float ambientStrength = 0.15;
    vec3  ambient = ambientStrength * objectColor;
 
    // --- diffuse ---
    vec3  lightDir = normalize(lightPos - fragPos);
    float diff     = max(dot(fragNormal, lightDir), 0.0);
    vec3  diffuse  = diff * objectColor;
 
    // --- specular (Blinn-Phong) ---
    float specStrength = 0.5;
    vec3  viewDir   = normalize(viewPos - fragPos);
    vec3  halfwayDir = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(fragNormal, halfwayDir), 0.0), 64.0);
    vec3  specular  = specStrength * spec * vec3(1.0);
 
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)glsl";


// =====================================================
// SHADER HELPERS
// =====================================================

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
 
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}
 
static GLuint linkProgram(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
 
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << "\n";
    }
    return prog;
}
 
static GLuint buildShaderProgram()
{
    GLuint vert = compileShader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
    GLuint prog = linkProgram(vert, frag);
    glDeleteShader(vert); // shaders are baked into the program – safe to delete
    glDeleteShader(frag);
    return prog;
}


struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    int indexCount = 0;
};

static Mesh buildSphereMesh(float radius, int stacks, int sectors){
    std::vector<float> verts;   // x y z  nx ny nz
    std::vector<unsigned int> idx;
 
    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)M_PI * i / stacks;       // 0 … π
        for (int j = 0; j <= sectors; ++j) {
            float theta = 2.0f * (float)M_PI * j / sectors; // 0 … 2π

            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);

            // position
            verts.push_back(x * radius);
            verts.push_back(y * radius);
            verts.push_back(z * radius);
            // normal (same as unit position for a sphere centred at origin)
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
        }
    }

    // Two triangles per quad between adjacent rings
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int a = i * (sectors + 1) + j;
            unsigned int b = a + (sectors + 1);
 
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(a + 1);
 
            idx.push_back(b);
            idx.push_back(b + 1);
            idx.push_back(a + 1);
        }
    }

    Mesh mesh;
    mesh.indexCount = (int)idx.size();
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);
 
    glBindVertexArray(mesh.vao);
 
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
 
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
 
    // layout(location = 0) → position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
 
    // layout(location = 1) → normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
 
    glBindVertexArray(0); // unbind VAO *before* unbinding EBO – order matters!
 
    return mesh;
}



// ─────────────────────────────────────────────
//  ORBIT CAMERA
//  Stored as: radius, azimuth (yaw), elevation (pitch).
//  Call position() to get the eye point.
//  target is always the origin here; trivially extensible.
// ─────────────────────────────────────────────
struct Camera {
    float radius    = 10.0f;
    float azimuth   = 0.0f;                    // horizontal angle, radians
    float elevation = (float)M_PI / 4.0f;      // vertical angle, radians
 
    float orbitSpeed = 0.005f;
    float zoomSpeed  = 0.5f;
 
    bool  dragging = false;
    double lastX = 0, lastY = 0;
 
    glm::vec3 position() const
    {
        float e = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        return glm::vec3(
            radius * std::sin(e) * std::cos(azimuth),
            radius * std::cos(e),
            radius * std::sin(e) * std::sin(azimuth)
        );
    }
 
    glm::mat4 viewMatrix() const
    {
        return glm::lookAt(position(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
    }
 
    void onMouseButton(int button, int action, GLFWwindow* win)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            dragging = (action == GLFW_PRESS);
            if (dragging) glfwGetCursorPos(win, &lastX, &lastY);
        }
    }
 
    void onMouseMove(double x, double y)
    {
        if (!dragging) { lastX = x; lastY = y; return; }
        azimuth   -= (float)(x - lastX) * orbitSpeed;
        elevation += (float)(y - lastY) * orbitSpeed;    // y-down in screen space
        elevation  = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        lastX = x;  lastY = y;
    }
 
    void onScroll(double /*dx*/, double dy)
    {
        radius -= (float)dy * zoomSpeed;
        if (radius < 0.5f) radius = 0.5f;
    }
};



// ─────────────────────────────────────────────
//  GLFW CALLBACKS
//  We store the Camera pointer in the window's user pointer.
// ─────────────────────────────────────────────
static void cb_mouseButton(GLFWwindow* win, int button, int action, int /*mods*/)
{
    auto* cam = static_cast<Camera*>(glfwGetWindowUserPointer(win));
    cam->onMouseButton(button, action, win);
}
static void cb_mouseMove(GLFWwindow* win, double x, double y)
{
    auto* cam = static_cast<Camera*>(glfwGetWindowUserPointer(win));
    cam->onMouseMove(x, y);
}
static void cb_scroll(GLFWwindow* win, double dx, double dy)
{
    auto* cam = static_cast<Camera*>(glfwGetWindowUserPointer(win));
    cam->onScroll(dx, dy);
}
static void cb_key(GLFWwindow* win, int key, int /*scan*/, int action, int /*mods*/)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}
static void cb_resize(GLFWwindow* /*win*/, int w, int h)
{
    glViewport(0, 0, w, h);
}
 

// struct Engine {
//     GLFWwindow* window;

//     // renders vars
//     GLuint sphereVAO, sphereVBO;
//     int sphereVertexCount;
//     GLuint shaderProgram;
//     GLint modelLoc, viewLoc, projLoc, colorLoc;

//     Engine() {


//     }





// }






int main() {
    if (!glfwInit()){
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glewExperimental = GL_TRUE;

    GLFWwindow* window = glfwCreateWindow(800  , 600, "Modern GL – Sphere", nullptr, nullptr);
    if (!window) {
        std::cerr << "glfwCreateWindow failed\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "GLEW init failed: " << glewGetErrorString(err) << "\n";
        return -1;
    }

    glfwSwapInterval(1); // vsync

    // ── Camera + callbacks ──────────────────────
    Camera camera;
    glfwSetWindowUserPointer(window, &camera);
    glfwSetMouseButtonCallback(window, cb_mouseButton);
    glfwSetCursorPosCallback(window,   cb_mouseMove);
    glfwSetScrollCallback(window,      cb_scroll);
    glfwSetKeyCallback(window,         cb_key);
    glfwSetFramebufferSizeCallback(window, cb_resize);
 
    // ── Shader program ──────────────────────────
    GLuint shader = buildShaderProgram();

    GLint uModel       = glGetUniformLocation(shader, "model");
    GLint uView        = glGetUniformLocation(shader, "view");
    GLint uProjection  = glGetUniformLocation(shader, "projection");
    GLint uObjectColor = glGetUniformLocation(shader, "objectColor");
    GLint uLightPos    = glGetUniformLocation(shader, "lightPos");
    GLint uViewPos     = glGetUniformLocation(shader, "viewPos");
 
    Mesh sphere = buildSphereMesh(1.0f, 32, 32);
    
    glm::vec3 lightPos(10.0f, 10.0f, 10.0f);
    
    // ── Projection matrix ───────────────────────
    //  Rebuilt each frame so window resize is handled gracefully.
    auto makeProjection = [&]() {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (h == 0) h = 1;
        return glm::perspective(glm::radians(45.0f),
                                (float)w / (float)h,
                                0.1f, 500.0f);
    };
    
    while (!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glClearColor(0.1f,0.1f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        // Camera / light uniforms
        glm::mat4 view       = camera.viewMatrix();
        glm::mat4 projection = makeProjection();

        glUniformMatrix4fv(uView,       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(uLightPos, 1, glm::value_ptr(lightPos));
        glUniform3fv(uViewPos,  1, glm::value_ptr(camera.position()));
        

        // ── Draw a grid of spheres (5 × 5) as a stress / layout test ──
        //    Later this will become the particle cloud.
        for (int ix = -2; ix <= 2; ++ix) {
            for (int iy = -2; iy <= 2; ++iy) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                    glm::vec3(ix * 2.5f, iy * 2.5f, 0.0f));
 
                // Colour varies across the grid – handy sanity check
                float r = (float)(ix + 2) / 4.0f;
                float g = (float)(iy + 2) / 4.0f;
                float b = 0.6f;
 
                glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
                glUniform3f(uObjectColor, r, g, b);
 
                glBindVertexArray(sphere.vao);
                glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, nullptr);
            }
        }

        glfwSwapBuffers(window);


    }
    
    glDeleteVertexArrays(1, &sphere.vao);
    glDeleteBuffers(1, &sphere.vbo);
    glDeleteBuffers(1, &sphere.ebo);
    glDeleteProgram(shader);
 
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}