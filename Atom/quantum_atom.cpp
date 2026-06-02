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
#include <sstream>

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
static const int SCR_H = 1200;

// =====================================================
// ORBITAL STATE  (shared between callbacks and main)
// =====================================================

struct OrbitalState {
    int  n = 4, l = 2, m = 0;
    int  N = 10000;
    bool trigger_resample = true;   // set true when params change → triggers resample
};


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
uniform vec3 objectColor;

out vec4 fragColor;

void main()
{
    vec3 glowColor = objectColor * 4.5;
    fragColor = vec4(glowColor, 0.05); 
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

// =====================================================
// WINDOW STATE  (camera + orbital, passed via user ptr)
// =====================================================

struct WindowState {
    Camera*       cam;
    OrbitalState* orb;
};

// ─────────────────────────────────────────────
//  GLFW CALLBACKS
//  We store the Camera pointer in the window's user pointer.
// ─────────────────────────────────────────────
static void cb_mouseButton(GLFWwindow* win, int button, int action, int)
{
    auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
    ws->cam->onMouseButton(button, action, win);
}
static void cb_mouseMove(GLFWwindow* win, double x, double y)
{
    auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
    ws->cam->onMouseMove(x, y);
}
static void cb_scroll(GLFWwindow* win, double dx, double dy)
{
    auto* ws = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
    ws->cam->onScroll(dx, dy);
}
// Helper for valid quantum numbers
static void clampQuantumNumbers(OrbitalState& orb)
{   
    // Let's still have fun tho, fuck some limits.
    // if (orb.n < 1) orb.n = 1; 
    // if (orb.n > 7) orb.n = 7;
    if (orb.l < 0)       orb.l = 0;
    if (orb.l > orb.n-1) orb.l = orb.n - 1;
    if (orb.m < -orb.l)  orb.m = -orb.l;
    if (orb.m >  orb.l)  orb.m =  orb.l;
    // if (orb.N < 500)    orb.N = 500;
    // if (orb.N > 100000) orb.N = 100000;
}

static void cb_resize(GLFWwindow*, int w, int h)
{
    glViewport(0, 0, w, h);
}


static void cb_key(GLFWwindow* win, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
        return;
    }
    // Only act on press or repeat (held key)
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    auto* ws  = static_cast<WindowState*>(glfwGetWindowUserPointer(win));
    OrbitalState& orb = *ws->orb;

    bool changed = false;

    
    // n - Principal quantum number
    if (key == GLFW_KEY_UP) {orb.n++; changed = true; }
    if (key == GLFW_KEY_DOWN) {orb.n--; changed = true; }

    // l - Azimuthal quantum number
    if (key == GLFW_KEY_RIGHT) { orb.l = std::min(orb.l + 1, orb.n - 1); changed = true; }
    if (key == GLFW_KEY_LEFT)  { orb.l = std::max(orb.l - 1, 0);         changed = true; }

    // n - Magnetic quantum number
    if (key == GLFW_KEY_D)     { orb.m = std::min(orb.m + 1, orb.l);  changed = true; }
    if (key == GLFW_KEY_A)     { orb.m = std::max(orb.m - 1, -orb.l); changed = true; }

    // N  — particle count  (minus = halve, equals/plus = double)
    if (key == GLFW_KEY_S) { orb.N /= 2; changed = true; }
    if (key == GLFW_KEY_W) { orb.N *= 2; changed = true; }

    if (changed) {
        // clamp it
        clampQuantumNumbers(orb);
        orb.trigger_resample = true;

        std::ostringstream ss;
        ss << "Hydrogen Orbital  |  n=" << orb.n
           << "  l=" << orb.l
           << "  m=" << orb.m
           << "  N=" << orb.N
           << "  |  Arrows=n/l   ,/.=m   -/+=particles";
           glfwSetWindowTitle(win, ss.str().c_str());
    }
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

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glClearColor(0.01f, 0.01f, 0.03f, 1.0f);


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
        glDepthMask(GL_FALSE);
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


glm::vec3 heatmapInferno(float t)
{
    t = glm::clamp(t, 0.0f, 1.0f);

    struct Stop {
        float p;
        glm::vec3 c;
    };

    static const Stop stops[] = {
        {0.0f, {0.0f, 0.0f, 0.0f}},
        {0.15f,{0.15f, 0.0f, 0.3f}},
        {0.35f,{0.5f, 0.0f, 0.6f}},
        {0.55f,{0.9f, 0.1f, 0.2f}},
        {0.75f,{1.0f, 0.5f, 0.0f}},
        {0.9f, {1.0f, 0.9f, 0.1f}},
        {1.0f, {1.0f, 1.0f, 1.0f}}
    };

    for (int i = 0; i < 6; ++i)
    {
        if (t >= stops[i].p && t <= stops[i + 1].p)
        {
            float local =
                (t - stops[i].p) /
                (stops[i + 1].p - stops[i].p);

            return glm::mix(
                stops[i].c,
                stops[i + 1].c,
                local
            );
        }
    }

    return stops[6].c;
}

struct Particle {
    glm::vec3 pos;
    glm::vec3 color;

    // spherical coordinates
    float r;        // Radial distance
    float theta;    // Polar angle from y axis
    float phi;      // Azimuthal angle in xz-plane
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
    std::vector<Particle> particles ;
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
        // https://en.wikipedia.org/wiki/Gamma_function Gamma (x+1 interpolates the factorial function to non-integer values.
        double constant = std::sqrt( std::pow(2.0 / n , 3) * (std::tgamma(n - l) / ( 2.0 * n * std::tgamma(n+l)))); 
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
        particles.clear();
        particles.reserve(N);
        
        // Define bounding box size 
        double rMax = (n *n +3.0*n) / Z;
        std::uniform_real_distribution<double> distPos(-rMax , rMax);

        // Find Max Density empirically to normalize our rejection threshold
        double maxDensity = 0.0;

        for (int i = 0 ; i < 10000; i++) {
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

        int attempts = 0;
        while ((int) particles.size() < N) {
            ++attempts;
            double x = distPos(rng);
            double y = distPos(rng);
            double z = distPos(rng);
            double r = std::sqrt(x*x + y*y + z*z);

            if (r > rMax) continue; // Outside bounding sphere

            double density = evaluateDensity(x,y,z,n,l,m,Z);
            double roll = distProb(rng);

            // Accept the particle if random roll falls under the density curve
            if ( roll < density) {
                Particle p;
                p.pos = glm::vec3(x,y,z);

                // Store spherical coords so the current update is exact
                p.r = (float)r;
                p.theta = (float)std::acos(y / r);
                p.phi = (float)std::atan2(z,x);
                
                // Color 
                float intensity = (float)(density / maxDensity);
                // boost faint regions
                intensity = std::pow(intensity, 0.35f);
                // clamp
                intensity = glm::clamp(intensity, 0.0f, 1.0f);
                p.color = heatmapInferno(intensity);

                particles.push_back(p);
            }
        }

        float acceptance = 100.f * N / attempts;
        std::cout << "n=" << n << " l=" << l << " m=" << m
                << "  particles=" << N
                << "  acceptance=" << acceptance << "%\n";
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

};

int main() {


    Engine engine(SCR_W, SCR_H, "Hydrogen Orbital  |  Arrows=n/l   ,/.=m   -/+=particles");
    // Orbital
    OrbitalState orb;   
    // ── Camera + callbacks ──────────────────────
    Camera camera;
    WindowState ws{ &camera, &orb };
    glfwSetWindowUserPointer(engine.window, &ws);
    glfwSetMouseButtonCallback   (engine.window, cb_mouseButton);
    glfwSetCursorPosCallback     (engine.window, cb_mouseMove);
    glfwSetScrollCallback        (engine.window, cb_scroll);
    glfwSetKeyCallback           (engine.window, cb_key);
    glfwSetFramebufferSizeCallback(engine.window, cb_resize);
    
    ParticleSystem particles;
    // particles.generateRandom(5000, 15.0f);


    particles.sampleWaveFunction(4, 1, 0, 80000 , 1.0);

    glm::vec3 lightPos(20.0f, 20.0f, 20.0f);
    
    while (!glfwWindowShouldClose(engine.window))
    {
        glfwPollEvents();

        // ── Resample only when parameters changed ──────────────
        if (orb.trigger_resample) {
            particles.sampleWaveFunction(orb.n, orb.l, orb.m, orb.N);
            orb.trigger_resample = false;

            // Zoom camera to fit the orbital
            float rMax = (float)((orb.n * orb.n + 3.0 * orb.n));
            camera.radius = rMax * 2.6f;
        }

        engine.beginFrame();
 
        glm::mat4 view       = camera.viewMatrix();
        glm::mat4 projection = engine.projectionMatrix();
 
        glUniformMatrix4fv(engine.uView,       1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(engine.uProjection, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(engine.uLightPos, 1, glm::value_ptr(lightPos));
        glUniform3fv(engine.uViewPos,  1, glm::value_ptr(camera.position()));
 
        for (Particle& p : particles.particles)
            engine.drawSphere(p.pos , p.color , 0.16f);
 
        glfwSwapBuffers(engine.window);
    }
    return 0;
}