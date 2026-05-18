#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <thread>
#include <chrono>
#include <fstream>
#include <complex>
#include <random>
#ifndef M_PI
#define M_PI 3.1415926953558979323846
#endif
using namespace glm;
using namespace std;

// --- variables ---
const double zmSpeed = 10.0;
const float a0 = 1;
float electron_r = 1.5f; // radius for spheres
const double hbar = 1;
const double m_e = 1;
float orbitDistance = 50.0f;

// --- random devices ---
random_device rd; mt19937 gen(rd()); uniform_real_distribution<float> dis(0.0f, 1.0f);


struct WavePoint { vec2 localPos; vec2 dir;  };
struct Wave{
    vec2 pos, dir;
    float energy, wavelength , frequency;
    float sigma = 40.0f, k = 0.4f, phase = 0.0f, a = 10.0f, angleR;
    vector<WavePoint> points;
    vec3 col;

    
    Wave(float e , vec2 pos , vec2 dir , vec3 col = vec3(0.0f,1.0f,1.0f)): energy(e) , pos(pos) , dir(dir), col(col) {
        dir = normalize(dir);
        for (float x = -sigma; x<= sigma; x+= 0.1f )
            points.push_back({ pos + x*dir , dir*200.0f});
        angleR = atan2(dir.y, dir.x);
    }

    void draw() {

        glColor3f(col.r, col.g, col.b);
        glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP);
        for (WavePoint& p: points){
            vec2 perp(-p.dir.y , p.dir.x);
            perp = normalize(perp);
            // -- https://chem.libretexts.org/Bookshelves/Physical_and_Theoretical_Chemistry_Textbook_Maps/Physical_Chemistry_(LibreTexts)/02%3A_The_Classical_Wave_Equation/2.01%3A_The_One-Dimensional_Wave_Equation
            // -- A(x,t) = A_o \sin (kx - \omega t + \phi) -- 
            float y_disp = a * sin (k*length(p.localPos) - phase);
            vec2 drawPos = p.localPos + perp * y_disp;
            glVertex2f(drawPos.x, drawPos.y);
        }
        glEnd();

    }

    bool update(float dt) {
       phase += 30.0f * dt; // continuous phase
        for (WavePoint& p : points) {
            p.localPos += p.dir * dt;
        }
        
        // Return true if the center of the wave is off-screen
       return (length(points[points.size()/2].localPos) > 1000.0f);
   }
};
vector<Wave> waves {
};
struct Particle{
    vec2 pos;
    int charge;
    float angle;
    int n = 1;
    float excitedTimer = 0.0f; 
    Particle(vec2 pos ,int charge) : pos(pos), charge(charge), angle(0.0f) {}
    
    void draw(vec2 center, int segments = 50){
        float r;
        if (charge == -1 ) {
            //outline for the electrons
            glLineWidth(0.4f);
            glBegin(GL_LINE_LOOP);
            glColor3f(0.4f,0.4f,0.4f);
            for (int i = 0 ; i <= segments ; i++){
                float angle = 2.0f * M_PI * i/segments;
                float x = cos(angle) * n * orbitDistance;
                float y = sin(angle) * n * orbitDistance;
                glVertex2f(x + center.x , y + center.y);
            }
            glEnd(); 
        } 
        if (charge == -1)       { r = 4; glColor3f(0.0f, 1.0f, 1.0f); } 
        else if (charge == 1)   { r = 8; glColor3f(1.0f, 0.0f, 0.0f); } 
        else                    { r = 8; glColor3f(0.5f, 0.5f, 0.5f); }
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(pos.x , pos.y);
        for (int i = 0 ; i <= segments ; i++){
            float angle = 2.0f * M_PI * i/segments;
            float x = cos(angle) * r;
            float y = sin(angle) * r;
            glVertex2f(x + pos.x , y + pos.y);
        }
        glEnd();
    }
    void update (vec2 c) {
        float r = n * orbitDistance;
        angle += 0.05;
        pos = vec2(c.x + cos(angle) * r , c.y + sin(angle) * r );
    
    // If we are excited (n > 1) and the timer runs out, emit!
    if (n > 1 && excitedTimer <= 0.0f) {
        float energyDiff = (-13.6f / (n * n)) - (-13.6f / ((n - 1) * (n - 1)));


        // Random direction for emission
        float randAngle = (rand() % 360) * (M_PI / 180.0f);
        vec2 emitDir(cos(randAngle), sin(randAngle));

        waves.emplace_back(abs(energyDiff), pos, emitDir, vec3(1.0f, 1.0f, 0.0f));

        n--;
        excitedTimer += 0.003f;
    }
        
    }
};
vector<Particle> particles = {
    Particle(vec2(0.0f) , 1 ),
    Particle(vec2(-50.0f , 0.0f) , -1)
};

struct Camera {
    vec3 target = vec3(0.0f, 0.0f, 0.0f);
    float radius = 50.0f;
    float azimuth = 0.0f;
    float elevation = M_PI / 2.0f;
    float orbitSpeed = 0.01f;
    float panSpeed = 0.01f;
    double zoomSpeed = zmSpeed;
    bool dragging = false;
    bool panning = false;
    double lastX = 0.0, lastY = 0.0;


    vec3 position() const {
        float clampedElevation = clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        return vec3(
            radius * sin(clampedElevation) * cos(azimuth),
            radius * cos(clampedElevation),
            radius * sin(clampedElevation) * sin(azimuth)
        );
    }
    void update() {
        target = vec3(0.0f, 0.0f, 0.0f);
    }

    void processMouseMove(double x, double y) {
        float dx = float(x - lastX);
        float dy = float(y - lastY);
        if (dragging) {
            azimuth += dx * orbitSpeed;
            elevation -= dy * orbitSpeed;
            elevation = glm::clamp(elevation, 0.01f, float(M_PI) - 0.01f);
        }
        lastX = x;
        lastY = y;
        update();
    }

    void processMouseButton(int button, int action, int mods, GLFWwindow* win) {
        if (button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) {
                dragging = true;
                glfwGetCursorPos(win, &lastX, &lastY);
            } else if (action == GLFW_RELEASE) {
                dragging = false;
            }
        }
    }
    
    void processScroll(double xoffset, double yoffset) {
        radius -= yoffset * zoomSpeed;
        if (radius < 1.0f) radius = 1.0f;
        update();
    };
};


Camera camera;

struct Engine {

    GLFWwindow* window;
    int WIDTH = 1600 , HEIGHT = 1200;


    // renders vars
    GLuint sphereVAO, sphereVBO;
    int sphereVertexCount;
    GLuint shaderProgram;
    GLint modelLoc, viewLoc, projLoc, colorLoc;

    Engine () {
        // --- Init GLFW ---
        if (!glfwInit()) {
            cerr << "failed to init glfw";
            exit(EXIT_FAILURE);
        }

        // --- Create Window ---
        window = glfwCreateWindow(WIDTH, HEIGHT, "Quantum hydrogen", nullptr, nullptr);
        if (!window) {
            cerr << "failed to create window, LOLOLOL";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        glewInit();
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
    }
    void run() {
        glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        // set origin to centre
        double halfWidth = WIDTH / 2.0f, halfHeight = HEIGHT / 2.0f;
        glOrtho(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0, 1.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }
    
    void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const vector<float>& vertices) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
    }
    void CreateVBOVAO(GLuint& VAO, GLuint& VBO, const float* vertices, size_t vertexCount) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

        void drawSpheres(vector<Particle>& particles) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram); // Use our new shaded system

        mat4 projection = perspective(radians(45.0f), 800.0f/600.0f, 0.1f, 2000.0f);
        mat4 view = lookAt(camera.position(), camera.target, vec3(0, 1, 0)); 

        // Send view and projection to the shader
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, value_ptr(projection));

        glBindVertexArray(sphereVAO);

        for (auto& p : particles) {
            if (p.pos.x < 0 && p.pos.y > 0) continue;
            mat4 model = translate(mat4(1.0f), vec3(p.pos,0.0f));
            model = scale(model, vec3(electron_r));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(model));
            // glUniform4f(colorLoc, p.color.r, p.color.g, p.color.b, p.color.a);
            
            glDrawArrays(GL_TRIANGLES, 0, sphereVertexCount);
        }
    }

};
Engine engine;


struct Grid {
    GLuint gridVAO, gridVBO;
    vector<float> vertices;
    Grid() {
        vertices = CreateGridVertices(500.0f, 2);
        engine.CreateVBOVAO(gridVAO, gridVBO, vertices.data(), vertices.size());
    }
    void Draw (GLint objectColorLoc) {
        glUseProgram(engine.shaderProgram);
        glUniform4f(objectColorLoc, 1.0f, 1.0f, 1.0f, 0.5f);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
        DrawGrid(engine.shaderProgram, gridVAO, vertices.size());
    }
    void DrawGrid(GLuint shaderProgram, GLuint gridVAO, size_t vertexCount) {
        glUseProgram(shaderProgram);
        glm::mat4 model = glm::mat4(1.0f); // Identity matrix for the grid
        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

        glBindVertexArray(gridVAO);
        glPointSize(2.0f);
        glDrawArrays(GL_LINES, 0, vertexCount / 3);
        glBindVertexArray(0);
    }
    vector<float> CreateGridVertices(float size, int divisions) {
        
        std::vector<float> vertices;
        float step = size / divisions;
        float halfSize = size / 2.0f;

        // amount to extend the central X-axis line (in same units as size)
        float extra = step * 3.0f; // adjust this factor to make the line stick out more/less
        int midZ = divisions / 2;

        // x axis
        for (int yStep = 3; yStep <= 3; ++yStep) {
            float y = 0;
            for (int zStep = 0; zStep <= divisions; ++zStep) {
                float z = -halfSize + zStep * step;
                for (int xStep = 0; xStep < divisions; ++xStep) {
                    float xStart = -halfSize + xStep * step;
                    float xEnd = xStart + step;

                    // If this is the central line (middle z), extend the very first and last segment
                    if (zStep == midZ) {
                        if (xStep == 0) {
                            xStart -= extra; // extend left end
                        }
                        if (xStep == divisions - 1) {
                            xEnd += extra;   // extend right end
                        }
                    }

                    vertices.push_back(xStart); vertices.push_back(y); vertices.push_back(z);
                    vertices.push_back(xEnd);   vertices.push_back(y); vertices.push_back(z);
                }
            }
        }
        // zaxis
        for (int xStep = 0; xStep <= divisions; ++xStep) {
            float x = -halfSize + xStep * step;
            for (int yStep = 3; yStep <= 3; ++yStep) {
                float y = 0;
                for (int zStep = 0; zStep < divisions; ++zStep) {
                    float zStart = -halfSize + zStep * step;
                    float zEnd = zStart + step;
                    vertices.push_back(x); vertices.push_back(y); vertices.push_back(zStart);
                    vertices.push_back(x); vertices.push_back(y); vertices.push_back(zEnd);
                }
            }
        }

        return vertices;

    }
};

Grid grid;



int main () {
    // GLint modelLoc = glGetUniformLocation(engine.shaderProgram, "model");
    // GLint objectColorLoc = glGetUniformLocation(engine.shaderProgram, "objectColor");
    // glUseProgram(engine.shaderProgram);
    

    for (int i = 0; i < 10000 ; i++ ){
        float x = -15 + dis(gen) * 30.0f;
        float y = -15 + dis(gen) * 30.0f;
        float z = -15 + dis(gen) * 30.0f;
        particles.emplace_back(vec2(x,y) , -1);
    }


    float dt = 0.5f;
    cout << "Starting simulation..." << endl;
    while (!glfwWindowShouldClose(engine.window)) {
        // grid.Draw(objectColorLoc);

        // ------ Draw Particles ------
        engine.drawSpheres(particles);

        glfwSwapBuffers(engine.window);
        glfwPollEvents();
    }


    glfwDestroyWindow(engine.window);
    glfwTerminate();
    return 0;
}