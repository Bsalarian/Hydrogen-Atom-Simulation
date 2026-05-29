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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

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
float diff = max(dot(fragNormal, lightDir), 0.0);

vec3 color =
    objectColor * (0.25 + diff * 1.5);

color = pow(color, vec3(1.0 / 2.2));

fragColor = vec4(color, 0.06);
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
    float zoomSpeed  = 1.5f;
 
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
 

// ENGINE  — owns window, GL context, shader, sphere mesh, uniform locs
// =====================================================================
struct Engine {
    GLFWwindow* window   = nullptr;
    GLuint      shader   = 0;
    Mesh        sphere;
 
    GLint uModel       = -1;
    GLint uView        = -1;
    GLint uProjection  = -1;
    GLint uObjectColor = -1;
    GLint uLightPos    = -1;
    GLint uViewPos     = -1;
 
    Engine(int w, int h, const char* title)
    {
        if (!glfwInit()) { std::cerr << "glfwInit failed\n"; std::exit(-1); }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
 
        window = glfwCreateWindow(w, h, title, nullptr, nullptr);
        if (!window) { std::cerr << "Window creation failed\n"; glfwTerminate(); std::exit(-1); }
 
        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
 
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) { std::cerr << "glewInit failed\n"; std::exit(-1); }
 
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
 
        shader       = buildShaderProgram();
        uModel       = glGetUniformLocation(shader, "model");
        uView        = glGetUniformLocation(shader, "view");
        uProjection  = glGetUniformLocation(shader, "projection");
        uObjectColor = glGetUniformLocation(shader, "objectColor");
        uLightPos    = glGetUniformLocation(shader, "lightPos");
        uViewPos     = glGetUniformLocation(shader, "viewPos");
 
        sphere = buildSphereMesh(1.0f, 32,32);
    }
 
    // Call once per frame before any drawing
    void beginFrame()
    {
        // KEY: must clear depth buffer too, not just color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shader);
    }
 
    // Draw one sphere: translate to pos, scale by scale, color it
    void drawSphere(glm::vec3 pos, glm::vec3 color, float scale = 1.0f)
    {
        glm::mat4 model = glm::scale(
            glm::translate(glm::mat4(1.0f), pos),
            glm::vec3(scale)
        );
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(uObjectColor, 1, glm::value_ptr(color));
        glBindVertexArray(sphere.vao);
        glDrawElements(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, nullptr);
    }
 
    glm::mat4 projectionMatrix(float fovDeg = 45.0f, float nearZ = 0.1f, float farZ = 500.0f)
    {
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (h == 0) h = 1;
        return glm::perspective(glm::radians(fovDeg), (float)w / h, nearZ, farZ);
    }
 
    ~Engine()
    {
        glDeleteVertexArrays(1, &sphere.vao);
        glDeleteBuffers(1, &sphere.vbo);
        glDeleteBuffers(1, &sphere.ebo);
        glDeleteProgram(shader);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

/*

Reference: 
https://chem.libretexts.org//Courses/University_of_California_Davis/Chem_107B:_Physical_Chemistry_for_Life_Scientists/
Chapters/4:_Quantum_Theory/4.10:_The_Schr%C3%B6dinger_Wave_Equation_for_the_Hydrogen_Atom
 
PARTICLE SYSTEM
|ψ_nlm|²

n — which shell (1, 2, 3…). Bigger n = electron lives farther out. This is the energy level.
l — the shape of the orbital within that shell (0=sphere, 1=dumbbell, 2=cloverleaf). Must be less than n.
m — which orientation the lobe points (ranges from -l to +l). For l=1 you get m = -1, 0, or 1 — three different dumbbell orientations.

ψ(r, θ, φ) = R(r) · Y(θ, φ)

two main parts:
R(r) — radial part : Controls shell size and shape 
        Controlled by n and l
        n = principal (shell number 1,2,3…)
        l = azimuthal (0=s, 1=p, 2=d…)
        l must be < n

Y(θ, φ) — angular part : Controls 3D orientation/lobes
        Controlled by l and m
        l = shape of orbital lobe
        m = magnetic (orientation)
        −l ≤ m ≤ l

We will use the Von Neumann Rejection Sampling instead of the CDF sampling.
    Rejection sampling works by picking a random point in a 3D box, 
    calculating the probability density P at that point, and then rolling a random number between 0 and the maximum possible density.
    If your random roll is less than P, you spawn a particle.
    If not, you throw the point away and try again.
*/


struct ParticleSystem {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> colors;
    std::mt19937 rng{42};


    double evaluateDensity(double x ,double y, double z, int n, int l, int m, double Z){

        double r = std::sqrt(x*x + y*y + z*z);
        if ( r < 1e-6) return 0.0; // Prevent singularity

        // Standard physics coordinates (theta = vertical inclination, phi = horizontal azimuth)
        double theta = std::acos(y / r);       
        double phi = std::atan2(z, x);         
        if (phi < 0.0) phi += 2.0 * M_PI;
        
        // Radial part -- determines distance of the electron
        // Square root part for normalization constant.
        // Exponential $e^{-\rho / 2}$ to make sure the wf decays to 0 when far
        // The Laguerre polynomial L for generating alternating peaks and valleys of density.

        double rho = (2.0 * r) /n; // for a0=1.0 aka hydrogen

        double constant = std::sqrt( std::pow(2.0 / n , 3) * (std::tgamma(n - l -1) / ( 2.0 * n * std::tgamma(n+l))));

        double radial = constant * std::exp(-rho / 2.0) * std::pow(rho ,l) * std::assoc_laguerre(n-l-1 , 2.0 * l +1 , rho);


        // Angular part -- defines the shape of the orbitals.
        // We use Real Spherical Harmonics and std::sph_legendre to automatically apply the normalization factor $N_{lm}$.

        double angular = std::sph_legendre(l , std::abs(m) , theta);

        // Apply real spherical harmonics mapping for horizental rotation
        if (m > 0) {
            angular *= std::sqrt(2.0) * std::cos(m * phi);
        } 
        else if (m < 0 ) {
            angular *= std::sqrt(2.0) * std::sin(std::abs(m)* phi);
        }

        // Probability density is the squared magnitude of the wave function
        double psi = radial * angular;
        return psi * psi;
    }

    void sampleWaveFunction(int n, int l , int m, int N, double Z = 1.0){
        positions.clear();
        colors.clear();
        
        // Define bounding box size 
        double rMax = (n *n +3.0*n) / Z;
        std::uniform_real_distribution<double> distPos(-rMax , rMax);

        // Find Max Density empirically to normalize our rejection threshold
        double maxDensity = 0.0;

        for (int i = 0 ; i < 5000; i++) {
            double x = distPos(rng);
            double y = distPos(rng);
            double z = distPos(rng);
            double density = evaluateDensity(x,y,z,n,l,m,Z);
            if (density > maxDensity) maxDensity = density;
        }

        // Von Neumann Rejection Sampling
        // https://en.wikipedia.org/wiki/Rejection_sampling 
        //  1. Sample a point on the x axis from the proposal dist
        //  2. Draw a vertical line at this x position , up to the y-value of the probability density function of the proposal distribution.
        //  3. ample uniformly along this line. 
        //     If the sampled value is greater than the value of the desired distribution at this vertical line, 
        //                  reject the x value and return to step 1.
        //     Else the x value is a sample from the desired distribution.


        std::uniform_real_distribution<double> distProb(0.0 , maxDensity);

        while (positions.size() < N) {
            double x = distPos(rng);
            double y = distPos(rng);
            double z = distPos(rng);
            double r = std::sqrt(x*x + y*y + z*z);

            // Keep only points inside the spherical bounding volume
            if (r > rMax) continue;

            double density = evaluateDensity(x,y,z,n,l,m,Z);
            double roll = distProb(rng);

            // Accept the particle if random roll falls under the density curve
            if ( roll < density) {
                positions.push_back(glm::vec3(x,y,z));

                // Color based on radius 
                float t = (float)(r / rMax);
                colors.push_back({ 0.2f + 0.8f * t, 0.4f - 0.4f * t, 0.9f - 0.7f * t });
            }
        }
    }
 
    // void generateRandom(int N, float spread)
    // {
    //     positions.clear();
    //     colors.clear();
    //     srand(42);
    //     for (int i = 0; i < N; ++i) {
    //         // Rejection sample: keep only points inside unit sphere
    //         // so density is uniform (not corner-heavy like a cube sample)
    //         glm::vec3 p;
    //         do {
    //             p = glm::vec3(
    //                 (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
    //                 (rand() / (float)RAND_MAX) * 2.0f - 1.0f,
    //                 (rand() / (float)RAND_MAX) * 2.0f - 1.0f
    //             );
    //         } while (glm::length(p) > 1.0f);
 
    //         positions.push_back(p * spread);
 
    //         // Color: purple at centre → orange at edge (mimics density heatmap)
    //         float t = glm::length(p);
    //         colors.push_back({ 0.3f + 0.7f * t, 0.1f + 0.2f * t, 0.9f - 0.6f * t });
    //     }
    // }
 
    int size() const { return (int)positions.size(); }
};



int main() {


    Engine engine(1800,1000,"");
    // ── Camera + callbacks ──────────────────────
    Camera camera;
    glfwSetWindowUserPointer(engine.window, &camera);
    glfwSetMouseButtonCallback(engine.window,     cb_mouseButton);
    glfwSetCursorPosCallback(engine.window,       cb_mouseMove);
    glfwSetScrollCallback(engine.window,          cb_scroll);
    glfwSetKeyCallback(engine.window,             cb_key);
    glfwSetFramebufferSizeCallback(engine.window, cb_resize);
 
    ParticleSystem particles;
    // particles.generateRandom(5000, 15.0f);
    particles.sampleWaveFunction(3, 1, 0, 5000 , 1.0);

    glm::vec3 lightPos(20.0f, 20.0f, 20.0f);
    
    while (!glfwWindowShouldClose(engine.window))
    {
        glfwPollEvents();
        engine.beginFrame();
 
        glm::mat4 view       = camera.viewMatrix();
        glm::mat4 projection = engine.projectionMatrix();
 
        glUniformMatrix4fv(engine.uView,       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(engine.uProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(engine.uLightPos, 1, glm::value_ptr(lightPos));
        glUniform3fv(engine.uViewPos,  1, glm::value_ptr(camera.position()));
 
        for (int i = 0; i < particles.size(); ++i)
            engine.drawSphere(particles.positions[i], particles.colors[i], 0.3f);
 
        glfwSwapBuffers(engine.window);
    }
    return 0;
}