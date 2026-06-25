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
#define GL_GLEXT_PROTOTYPES 1
#define GLFW_INCLUDE_ES3 
#include <GLFW/glfw3.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
// SHADERS - SPHERES (Soft Glossy Pearls)
// =====================================================
static const char* VERT_SPHERE = R"glsl(#version 300 es
precision highp float;
 
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aInstancePos;   // INSTANCING
layout(location = 3) in vec3 aInstanceColor; // INSTANCING
 
uniform mat4 view;
uniform mat4 projection;
uniform float uScale;
 
out vec3 fragPos;    
out vec3 fragNormal;
out vec3 particleColor; 
 
void main() {
    vec3 worldPos = (aPos * uScale) + aInstancePos;
    fragPos    = worldPos;
    fragNormal = aNormal;
    particleColor = aInstanceColor;
    gl_Position = projection * view * vec4(worldPos, 1.0);
}
)glsl";

static const char* FRAG_SPHERE = R"glsl(#version 300 es
precision highp float;

in vec3 fragPos;
in vec3 fragNormal;
in vec3 particleColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool uCutaway;

out vec4 fragColor;

void main() {
    // 1. Onion Slice Logic
    if (uCutaway && fragPos.x > 0.0) discard;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos - fragPos);

    // 2. Wrap-Lambert Lighting (Fixes the "wonky" hard shadows, makes it soft)
    float diff = dot(N, L) * 0.5 + 0.5;
    
    // 3. Fresnel Gloss (Smooth pearl look)
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3 rim = mix(particleColor, vec3(1.0), 0.5) * fresnel;

    fragColor = vec4(particleColor * diff + rim, 1.0);
}
)glsl";

// =====================================================
// SHADERS - POINTS (Ethereal Mist)
// =====================================================
static const char* VERT_POINT = R"glsl(#version 300 es
precision highp float;
 
layout(location = 2) in vec3 aInstancePos;   // Reusing buffer locations
layout(location = 3) in vec3 aInstanceColor;
 
uniform mat4 view;
uniform mat4 projection;
uniform float uScale;
 
out vec3 particleColor;
out vec3 fragPos;
 
void main() {
    fragPos = aInstancePos;
    vec4 viewPos = view * vec4(aInstancePos, 1.0);
    gl_Position = projection * viewPos;
    
    // Scale mist particles by distance
    gl_PointSize = (uScale * 150.0) / -viewPos.z; 
    particleColor = aInstanceColor;
}
)glsl";

static const char* FRAG_POINT = R"glsl(#version 300 es
precision highp float;

in vec3 particleColor;
in vec3 fragPos;
uniform bool uCutaway;

out vec4 fragColor;

void main() {
    // 1. Onion Slice Logic
    if (uCutaway && fragPos.x > 0.0) discard;

    vec2 pt = gl_PointCoord - vec2(0.5);
    float r2 = dot(pt, pt);
    
    if (r2 > 0.25) discard;
    
    // Soft Gaussian glow
    float glow = exp(-12.0 * r2);
    fragColor = vec4(particleColor * glow * 1.5, glow * 0.15); // Low alpha for fluid stacking
}
)glsl";

// =====================================================
// SHADER HELPERS & MATH
// =====================================================
static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    return shader;
}
 
static GLuint buildProgram(const char* vSrc, const char* fSrc) {
    GLuint vert = compileShader(GL_VERTEX_SHADER, vSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert); glAttachShader(prog, frag); glLinkProgram(prog);
    glDeleteShader(vert); glDeleteShader(frag);
    return prog;
}

struct Mesh { GLuint vao = 0, vbo = 0, ebo = 0; int indexCount = 0; };
static Mesh buildSphereMesh(float radius, int stacks, int sectors){
    std::vector<float> verts; std::vector<unsigned int> idx;
    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)M_PI * i / stacks;       
        for (int j = 0; j <= sectors; ++j) {
            float theta = 2.0f * (float)M_PI * j / sectors; 
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            verts.push_back(x * radius); verts.push_back(y * radius); verts.push_back(z * radius);
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
        }
    }
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int a = i * (sectors + 1) + j, b = a + (sectors + 1);
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    Mesh mesh; mesh.indexCount = (int)idx.size();
    glGenVertexArrays(1, &mesh.vao); glGenBuffers(1, &mesh.vbo); glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo); glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0); 
    return mesh;
}

struct Camera {
    float radius = 10.0f, azimuth = 0.0f, elevation = (float)M_PI / 2.0f;
    float orbitSpeed = 0.005f; bool dragging = false; double lastX = 0, lastY = 0;
    glm::vec3 position() const {
        float e = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f);
        return glm::vec3(radius * std::sin(e) * std::cos(azimuth), radius * std::cos(e), radius * std::sin(e) * std::sin(azimuth));
    }
    glm::mat4 viewMatrix() const { return glm::lookAt(position(), glm::vec3(0.0f), glm::vec3(0, 1, 0)); }
    void onMouseButton(int b, int a, GLFWwindow* win) {
        if (b == GLFW_MOUSE_BUTTON_LEFT) { dragging = (a == GLFW_PRESS); if (dragging) glfwGetCursorPos(win, &lastX, &lastY); }
    }
    void onMouseMove(double x, double y) {
        if (!dragging) return;
        azimuth -= (float)(x - lastX) * orbitSpeed; elevation += (float)(y - lastY) * orbitSpeed;
        elevation = glm::clamp(elevation, 0.01f, (float)M_PI - 0.01f); lastX = x; lastY = y;
    }
    void onScroll(double, double dy) {
        float scrollDir = (dy > 0.0) ? 1.0f : ((dy < 0.0) ? -1.0f : 0.0f);
        radius -= scrollDir * radius * 0.1f;
        if (radius < 0.5f) radius = 0.5f; if (radius > 300.0f) radius = 300.0f;
    }
};

struct Particle {     glm::vec3 pos;
    glm::vec3 color;

    // spherical coordinates
    float r;        // Radial distance
    float theta;    // Polar angle from y axis
    float phi;      // Azimuthal angle in xz-plane
};

// ENGINE  — owns window, GL context, shader, sphere mesh, uniform locs
// =====================================================================
struct Engine {
    GLFWwindow* window = nullptr;
    Mesh sphere;
    GLuint shaderSphere;
    GLuint instanceVBO_pos = 0, instanceVBO_col = 0;
 
    Engine(int w, int h, const char* title) {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(w, h, title, nullptr, nullptr);
        glfwMakeContextCurrent(window); glfwSwapInterval(1);
 
        glClearColor(0.06f, 0.09f, 0.12f, 1.0f);
        
        // Pearls require depth testing but NO blending
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        shaderSphere = buildProgram(VERT_SPHERE, FRAG_SPHERE);
        sphere = buildSphereMesh(1.0f, 12, 12); 
        
        glGenBuffers(1, &instanceVBO_pos);
        glGenBuffers(1, &instanceVBO_col);

        glBindVertexArray(sphere.vao);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_pos);
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); glVertexAttribDivisor(2, 1);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_col);
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); glVertexAttribDivisor(3, 1);
        glBindVertexArray(0);
    }
 
    glm::mat4 projectionMatrix(float fovDeg = 45.0f, float nearZ = 0.1f, float farZ = 500.0f) {
        int w, h; glfwGetFramebufferSize(window, &w, &h);
        return glm::perspective(glm::radians(fovDeg), (float)w / (h ? h : 1), nearZ, farZ);
    }

    void drawParticlesInstanced(const std::vector<Particle>& particles, float scale) {
        static std::vector<glm::vec3> positions;
        static std::vector<glm::vec3> colors;
        if (positions.size() != particles.size()) {
            positions.resize(particles.size()); colors.resize(particles.size());
        }
        for (size_t i = 0; i < particles.size(); ++i) {
            positions[i] = particles[i].pos; colors[i] = particles[i].color;
        }

        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_pos);
        glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_col);
        glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);

        glUniform1f(glGetUniformLocation(shaderSphere, "uScale"), scale);
        
        glBindVertexArray(sphere.vao);
        glDrawElementsInstanced(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, nullptr, particles.size());
        glBindVertexArray(0);
    }

    void drawProton(glm::vec3 pos, glm::vec3 color, float scale) {
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_pos);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3), &pos, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_col);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3), &color, GL_DYNAMIC_DRAW);

        glUniform1f(glGetUniformLocation(shaderSphere, "uScale"), scale);
        glBindVertexArray(sphere.vao);
        glDrawElementsInstanced(GL_TRIANGLES, sphere.indexCount, GL_UNSIGNED_INT, nullptr, 1);
        glBindVertexArray(0);
    }
};
glm::vec3 heatmapInferno(float t) {
    t = glm::clamp(t, 0.0f, 1.0f); struct Stop { float p; glm::vec3 c; };
    static const Stop stops[] = {
        {0.0f, {0.0f, 0.0f, 0.0f}}, {0.15f,{0.15f, 0.0f, 0.3f}}, {0.35f,{0.5f, 0.0f, 0.6f}},
        {0.55f,{0.9f, 0.1f, 0.2f}}, {0.75f,{1.0f, 0.5f, 0.0f}}, {0.9f, {1.0f, 0.9f, 0.1f}}, {1.0f, {1.0f, 1.0f, 1.0f}}
    };
    for (int i = 0; i < 6; ++i) {
        if (t >= stops[i].p && t <= stops[i + 1].p) {
            float local = (t - stops[i].p) / (stops[i + 1].p - stops[i].p);
            return glm::mix(stops[i].c, stops[i + 1].c, local);
        }
    }
    return stops[6].c;
}

/* 
Defining the Associated Laguerre polynomial
Uses the standard 3-term recurrence:

    L_0 = 1
    L_1 = 1 + alpha - x
    L_k = ((2k-1+alpha-x)*L_{k-1} - (k-1+alpha)*L_{k-2}) / k

*/
static double assocLaguerre(int n, double alpha, double x) {
    if (n == 0) return 1.0; if (n == 1) return 1.0 + alpha - x;
    double L0 = 1.0, L1 = 1.0 + alpha - x, Lk = 0.0;
    for (int k = 2; k <= n; ++k) {
        Lk = ((2*k - 1 + alpha - x) * L1 - (k - 1 + alpha) * L0) / k;
        L0 = L1; L1 = Lk;
    }
    return Lk;
}

/*
Defining the Associated Legendre polynomial P_l^m(cos θ) 
*/

static double sphLegendre(int l, int m, double theta) {
    double cosT = std::cos(theta); double sinT = std::sin(theta);
    double Pmm = 1.0; double factor = 1.0;
    for (int i = 1; i <= m; ++i) { Pmm *= -factor * sinT; factor += 2.0; }
    if (l == m) {
        double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
        for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i)); 
        return norm * Pmm;
    }
    double Pmp1m = cosT * (2.0*m + 1.0) * Pmm;
    if (l == m + 1) {
        double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
        for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i));
        return norm * Pmp1m;
    }
    double Plm = 0.0;
    for (int ll = m + 2; ll <= l; ++ll) {
        Plm = ((2.0*ll - 1.0) * cosT * Pmp1m - (ll + m - 1.0) * Pmm) / (ll - m);
        Pmm = Pmp1m; Pmp1m = Plm;
    }
    double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
    for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i));
    return norm * Plm;
}

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


Update: Von Neumann is very inefficent as we throw most points away. We swtiched to a CDF invesrse sampling.
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
        double radial = constant * std::exp(-rho / 2.0) * std::pow(rho ,l) * assocLaguerre(n-l-1 , 2.0 * l +1 , rho);


        // Angular part -- defines the shape of the orbitals.
        // We use Real Spherical Harmonics and sphLegendre to apply the normalization factor $N_{lm}$.

        double angular = sphLegendre(l , std::abs(m) , theta);

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


    // Use CDF sampling
    //  Split our probability density into three independent 1D distrubitions
    //  Radial Polar Azimuthal
    //  For each dimension:
    //      1) Discretize the math into an array of slices
    //      2) Calculate the probability at each slice
    //      3) Keep a running total to build a cumlative distribution function (CDF)
    //      4) Normalize the array so the last value is exactly 1.0

    void sampleWaveFunctionCDF(int n, int l , int m, int N, double Z = 1.0){

    particles.clear();
    particles.reserve(N);

    const int RESOLUTION = 2000;
    std::vector<double> cdf_r(RESOLUTION), cdf_theta(RESOLUTION), cdf_phi(RESOLUTION);
    
    // 1) Radial part
    double rMax = (n * n + 3.0 * n) / Z * 1.5;
    double sum_r = 0.0;

    for (int i = 0; i < RESOLUTION; ++i) {
        
        double r = i * (rMax / (RESOLUTION - 1));
        if (r < 1e-6) { cdf_r[i] = 0; continue; }
        double rho = (2.0 * r * Z) / n;
        double R_val = std::exp(-rho / 2.0) * std::pow(rho, l) * assocLaguerre(n - l - 1, 2.0 * l + 1, rho);


        // PDF = r^2 * |R(r)|^2
        sum_r += (r * r) * (R_val * R_val);
        cdf_r[i] = sum_r;
    }
    
    for (double& v : cdf_r) v /= sum_r; // Normalize to 1.0

    // 2) Polar part
    double sum_theta = 0.0;
    for (int i = 0; i < RESOLUTION; ++i) {
        double theta = i * (M_PI / (RESOLUTION - 1));
        double Y_val = sphLegendre(l, std::abs(m), theta);
        
        // PDF = sin(theta) * |Y(theta)|^2
        sum_theta += std::sin(theta) * (Y_val * Y_val);
        cdf_theta[i] = sum_theta;
    }
    for (double& v : cdf_theta) v /= sum_theta; // Normalize to 1.0

    // 3) Azimuthal part
    double sum_phi = 0.0;
    for (int i = 0; i < RESOLUTION; ++i) {
        double phi = i * (2.0 * M_PI / (RESOLUTION - 1));
        
        // Real spherical harmonics mapping
        double Phi_val = 1.0;
        if (m > 0) Phi_val = std::cos(m * phi);
        else if (m < 0) Phi_val = std::sin(std::abs(m) * phi);
        
        sum_phi += (Phi_val * Phi_val);
        cdf_phi[i] = sum_phi;
    }
    for (double& v : cdf_phi) v /= sum_phi;

    // 4) now we find max density (for colors)
    double maxDensity = 0.0;
    std::uniform_real_distribution<double> distPos(-rMax, rMax);
    for (int i = 0; i < 10000; i++) {
        double x = distPos(rng);
        double y = distPos(rng);
        double z = distPos(rng);
        double density = evaluateDensity(x, y, z, n, l, m, Z);
        if (density > maxDensity) maxDensity = density;
    }

    // 5) sample particles using inverse transform
    std::uniform_real_distribution<double> distU(0.0, 1.0);

    for (int i = 0; i < N; ++i) {

        // Roll random number and find index in CDF
        double u_r = distU(rng);
        auto it_r = std::lower_bound(cdf_r.begin(), cdf_r.end(), u_r);
        double r = std::distance(cdf_r.begin(), it_r) * (rMax / (RESOLUTION - 1));
        
        double u_t = distU(rng);
        auto it_t = std::lower_bound(cdf_theta.begin(), cdf_theta.end(), u_t);
        double theta = std::distance(cdf_theta.begin(), it_t) * (M_PI / (RESOLUTION - 1));

        double u_p = distU(rng);
        auto it_p = std::lower_bound(cdf_phi.begin(), cdf_phi.end(), u_p);
        double phi = std::distance(cdf_phi.begin(), it_p) * (2.0 * M_PI / (RESOLUTION - 1));

        // Convert spherical to Cartesian
        double y = r * std::cos(theta);
        double x = r * std::sin(theta) * std::cos(phi);
        double z = r * std::sin(theta) * std::sin(phi);

        Particle p;
        p.pos = glm::vec3(x, y, z);
        p.r = (float)r;
        p.theta = (float)theta;
        p.phi = (float)phi;

        // Color based on exact density
        double density = evaluateDensity(x, y, z, n, l, m, Z);
        float intensity = (float)(density / maxDensity);
        intensity = std::pow(intensity, 0.35f); // Boost faint regions
        intensity = glm::clamp(intensity, 0.0f, 1.0f);
        p.color = heatmapInferno(intensity);

        particles.push_back(p);
    }

    std::cout << "n=" << n << " l=" << l << " m=" << m
                << "  particles=" << N;
}   

    // void sampleWaveFunctionVonNeumannRejectionSampling(int n, int l , int m, int N, double Z = 1.0){
        //     particles.clear();
        //     particles.reserve(N);   
        //     // Define bounding box size 
        //     double rMax = (n *n +3.0*n) / Z;
        //     std::uniform_real_distribution<double> distPos(-rMax , rMax);
        //     // Find Max Density empirically to normalize our rejection threshold
        //     double maxDensity = 0.0;
        //     for (int i = 0 ; i < 10000; i++) {
        //         double x = distPos(rng);
        //         double y = distPos(rng);
        //         double z = distPos(rng);
        //         double density = evaluateDensity(x,y,z,n,l,m,Z);
        //         if (density > maxDensity) maxDensity = density;
        //     }
        //     // Von Neumann Rejection Sampling
        //     // https://en.wikipedia.org/wiki/Rejection_sampling 
        //     //  1. Sample a point on the x axis from the proposal dist
        //     //  2. Draw a vertical line at this x position , up to the y-value of the probability density function of the proposal distribution.
        //     //  3. ample uniformly along this line. 
        //     //     If the sampled value is greater than the value of the desired distribution at this vertical line, 
        //     //                  reject the x value and return to step 1.
        //     //     Else the x value is a sample from the desired distribution.
        //     // Von Neumann was good for first implementation, but highly inefficient. Replaced with CDF.
        //     std::uniform_real_distribution<double> distProb(0.0 , maxDensity);
        //     int attempts = 0;
        //     while ((int) particles.size() < N) {
        //         ++attempts;
        //         double x = distPos(rng);
        //         double y = distPos(rng);
        //         double z = distPos(rng);
        //         double r = std::sqrt(x*x + y*y + z*z);
        //         if (r > rMax) continue; // Outside bounding sphere
        //         double density = evaluateDensity(x,y,z,n,l,m,Z);
        //         double roll = distProb(rng);
        //         // Accept the particle if random roll falls under the density curve
        //         if ( roll < density) {
        //             Particle p;
        //             p.pos = glm::vec3(x,y,z);
        //             // Store spherical coords so the current update is exact
        //             p.r = (float)r;
        //             p.theta = (float)std::acos(y / r);
        //             p.phi = (float)std::atan2(z,x);
        //             // Color 
        //             float intensity = (float)(density / maxDensity);
        //             // boost faint regions
        //             intensity = std::pow(intensity, 0.35f);
        //             // clamp
        //             intensity = glm::clamp(intensity, 0.0f, 1.0f);
        //             p.color = heatmapInferno(intensity);
        //             // p.color = orbitalPalette(intensity);
        //             particles.push_back(p);
        //         }
        //     }
        //     float acceptance = 100.f * N / attempts;
        //     std::cout << "n=" << n << " l=" << l << " m=" << m
        //             << "  particles=" << N
        //             << "  acceptance=" << acceptance << "%\n";
        // }
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
        

    // ── Probability current ────────────────────────────────────
    //
    // The quantum probability current for hydrogen eigenstates only has
    // a φ-component (it swirls around the y-axis):
    //
    //   J_φ = ℏm / (m_e · r · sinθ)
    //
    // In Cartesian, the φ-direction unit vector at azimuth φ is:
    //   φ̂ = (-sinφ, 0, cosφ)
    //
    // So the velocity is:
    //   v = J_φ · φ̂  =  (ℏm / r·sinθ) · (-sinφ, 0, cosφ)
    //
    // Rather than applying this as a Cartesian step (which drifts off
    // the shell over time), we do:
    //   1. Compute the Cartesian step
    //   2. Extract the new φ from the stepped position
    //   3. Reconstruct position exactly at the original (r, θ, new φ)
    //
    // This keeps every particle locked to its sampled shell forever.
    // The only thing that changes is φ — the particle orbits the y-axis.
    //
    void updateProbabilityCurrent(int m_quantum, float dt) {
            if (m_quantum == 0) return;  
            for (Particle& p : particles) {
                float sinTheta = std::max(std::sin(p.theta), 1e-4f);
                p.phi += ((float)m_quantum / (p.r * sinTheta)) * dt;
                p.pos = glm::vec3(p.r * std::sin(p.theta) * std::cos(p.phi), p.r * std::cos(p.theta), p.r * std::sin(p.theta) * std::sin(p.phi));
            }
        }
    };


// =====================================================
// ORBITAL STATE  (shared between callbacks and main)
// =====================================================

struct OrbitalState {
    int  n = 4, l = 1, m = 0;
int  N = 40000;
    bool trigger_resample = true;   
    ParticleSystem* ps = nullptr;
    float dt = 0.025f;
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


    // Change time flow
    if (key == GLFW_KEY_E) { orb.dt *= 4; changed = true; }
    if (key == GLFW_KEY_Q) { orb.dt /= 4; changed = true; }


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
struct AppState {
    Engine*        engine    = nullptr;
    Camera*        camera    = nullptr;
    OrbitalState*  orb       = nullptr;
    ParticleSystem* particles = nullptr;
    WindowState*   ws        = nullptr;
    glm::vec3      lightPos  = glm::vec3(50.f, 50.f, 50.f);
};
static AppState gApp;



#ifdef __EMSCRIPTEN__
extern "C" {
    EMSCRIPTEN_KEEPALIVE void setN(int n) {
        gApp.orb->n = n;
        clampQuantumNumbers(*gApp.orb);
        gApp.orb->trigger_resample = true;
    }
    EMSCRIPTEN_KEEPALIVE void setL(int l) {
        gApp.orb->l = l;
        clampQuantumNumbers(*gApp.orb);
        gApp.orb->trigger_resample = true;
    }
    EMSCRIPTEN_KEEPALIVE void setM(int m) {
        gApp.orb->m = m;
        clampQuantumNumbers(*gApp.orb);
        gApp.orb->trigger_resample = true;
    }
    EMSCRIPTEN_KEEPALIVE void setParticleCount(int N) {
        gApp.orb->N = N;
        gApp.orb->trigger_resample = true;
    }
    EMSCRIPTEN_KEEPALIVE void setSpeed(float dt) {
        gApp.orb->dt = dt;
    }
    EMSCRIPTEN_KEEPALIVE int getN() { return gApp.orb->n; }
    EMSCRIPTEN_KEEPALIVE int getL() { return gApp.orb->l; }
    EMSCRIPTEN_KEEPALIVE int getM() { return gApp.orb->m; }
}
#endif

static void mainLoopIteration() {
    glfwPollEvents();

    if (gApp.orb->trigger_resample) {
        gApp.particles->sampleWaveFunctionCDF(gApp.orb->n, gApp.orb->l, gApp.orb->m, gApp.orb->N);
        gApp.orb->trigger_resample = false;
        gApp.camera->radius = (float)((gApp.orb->n * gApp.orb->n + 3.0 * gApp.orb->n)) * 2.6f;
    }

    gApp.particles->updateProbabilityCurrent(gApp.orb->m, gApp.orb->dt);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(gApp.engine->shaderSphere);

    glm::mat4 view = gApp.camera->viewMatrix();
    glm::mat4 projection = gApp.engine->projectionMatrix();

    glUniformMatrix4fv(glGetUniformLocation(gApp.engine->shaderSphere, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(gApp.engine->shaderSphere, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(gApp.engine->shaderSphere, "lightPos"), 1, glm::value_ptr(gApp.lightPos));
    glUniform3fv(glGetUniformLocation(gApp.engine->shaderSphere, "viewPos"), 1, glm::value_ptr(gApp.camera->position()));

    float pulse = 0.45f + 0.1f * sin((float)glfwGetTime() * 2.0f);
    float protonScale = 0.1f + pulse * 0.15f;
    
    // gApp.engine->drawProton(glm::vec3(0), glm::vec3(1.0f,0.25f,0.15f), protonScale * 1.4f);

    gApp.engine->drawParticlesInstanced(gApp.particles->particles, 0.15f);

    glfwSwapBuffers(gApp.engine->window);
}

int main() {


    static Engine        engine(SCR_W, SCR_H, "Hydrogen Orbital");
    static OrbitalState  orb;
    static Camera        camera;
    static WindowState   ws{ &camera, &orb };
    static ParticleSystem particles;

    gApp.engine    = &engine;
    gApp.orb       = &orb;
    gApp.camera    = &camera;
    gApp.ws        = &ws;
    gApp.particles = &particles;
    
    glfwSetWindowUserPointer(engine.window, &ws);
    glfwSetMouseButtonCallback   (engine.window, cb_mouseButton);
    glfwSetCursorPosCallback     (engine.window, cb_mouseMove);
    glfwSetScrollCallback        (engine.window, cb_scroll);
    glfwSetKeyCallback           (engine.window, cb_key);
    glfwSetFramebufferSizeCallback(engine.window, cb_resize);
    

    orb.ps = &particles;
    // particles.generateRandom(5000, 15.0f);

 
    glm::vec3 lightPos(50.f, 50.f, 50.f);
    particles.sampleWaveFunctionCDF(4, 1, 0, 40000 , 1.0);

#ifdef __EMSCRIPTEN__
    // 0 = use requestAnimationFrame rate (60fps)
    // 1 = simulate infinite loop (main() blocks here on native, not needed for web)
    emscripten_set_main_loop(mainLoopIteration, 0, 1);
#else
    // Native desktop: keep the original while loop
    while (!glfwWindowShouldClose(engine.window))
        mainLoopIteration();
#endif

    return 0;
}