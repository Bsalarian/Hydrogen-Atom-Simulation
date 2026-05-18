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


// =====================================================
// SHADERS
// =====================================================

const char* VERT = R"glsl(
#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position =
        projection *
        view *
        model *
        vec4(aPos, 1.0);
}
)glsl";

const char* FRAG = R"glsl(
#version 330 core

out vec4 FragColor;

uniform vec3 objectColor;

void main()
{
    FragColor = vec4(objectColor, 1.0);
}
)glsl";



// =====================================================
// SHADER HELPERS
// =====================================================

GLuint compile(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);

    glShaderSource(shader, 1, &src, nullptr);

    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok)
    {
        char log[1024];

        glGetShaderInfoLog(
            shader,
            sizeof(log),
            nullptr,
            log
        );

        std::cout << log << "\n";
    }

    return shader;
}

GLuint buildProgram()
{
    GLuint vs = compile(GL_VERTEX_SHADER, VERT);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FRAG);

    GLuint prog = glCreateProgram();

    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

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

struct Engine {
    GLFWwindow* window;
    int WIDTH = 800;
    int HEIGHT = 600;

    // renders vars
    GLuint sphereVAO, sphereVBO;
    int sphereVertexCount;
    GLuint shaderProgram;
    GLint modelLoc, viewLoc, projLoc, colorLoc;

    Engine() {






    }





}






int main() {
    if (!glfwInit()){
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Step 1", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;

    while (!glfwWindowShouldClose(window)){
        glfwPollEvents();
        glClearColor(0.1f,0.1f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}