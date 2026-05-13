#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>
#ifndef M_PI
#define M_PI 3.1415926953558979323846
#endif
using namespace glm;
using namespace std;

// --- variables ---

float orbitDistance = 50.0f;

struct Engine {

    GLFWwindow* window;
    int WIDTH = 1600 , HEIGHT = 1200;

    Engine () {
        // --- Init GLFW ---
        if (!glfwInit()) {
            cerr << "failed to init glfw";
            exit(EXIT_FAILURE);
        }

        // --- Create Window ---
        window = glfwCreateWindow(WIDTH, HEIGHT, "2D atom sim by kavan", nullptr, nullptr);
        if (!window) {
            cerr << "failed to create window, LOLOLOL";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);
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

};
Engine engine;

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
    float energy = -13.6f;
    float n_continuous = 1.0f;
    float orbitScale = 1.0f;
    int n = 1;
    float excitedTimer = 0.0f; 
    Particle(vec2 pos ,int charge) : pos(pos), charge(charge), angle(0.0f) {}
    
    void draw(vec2 center, int segments = 50){
        float r;
        if (charge == -1 ) {
            //outline for the electrons
            segments = 5000;
            glLineWidth(0.4f);
            glBegin(GL_LINE_LOOP);
            glColor3f(0.4f,0.4f,0.4f);

            float numOsolations = -13.6f / energy;
            float baseOrbit = orbitScale * orbitDistance;
            float amplitude = 8.0f;

            for (int i = 0 ; i <= segments ; i++){
                float loop_angle = 2.0f * M_PI * i / segments;
                float r_osc = baseOrbit + amplitude * sin(n_continuous * loop_angle);
                float x = cos(loop_angle) * r_osc;
                float y = sin(loop_angle) * r_osc;
                glVertex2f(x + center.x, y + center.y);
            }
            glEnd(); 
        } 
        if (charge == -1)       { r = 4; glColor3f(0.0f, 1.0f, 1.0f); } 
        else if (charge == 1)   { r = 8; glColor3f(1.0f, 0.0f, 0.0f); } 
        else                    { r = 8; glColor3f(0.5f, 0.5f, 0.5f); }
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(pos.x , pos.y);
        for (int i = 0 ; i <= segments ; i++){
            float a = 2.0f * M_PI * i / segments;
            glVertex2f(cos(a) * r + pos.x, sin(a) * r + pos.y);
        }
        glEnd();
    }
    void update (vec2 c) {

        // set radius with oscillation 
        float numOsolation = 0; 
        if ( energy < 0){
            numOsolation = -13.6f / energy;
        }
        float baseOrbit = orbitScale * orbitDistance;
        float amplitude = 8.0f;
        float r = baseOrbit + amplitude * sin(n_continuous * angle);
        angle += 0.05;
        pos = vec2(c.x + cos(angle) * r , c.y + sin(angle) * r );
    
    // If we are excited (n > 1) and the timer runs out, emit!
    if (n > 1 && excitedTimer <= 0.0f) {
        float energyDiff = (-13.6f / (n * n)) - (-13.6f / ((n-1) * (n-1)));
        float randAngle = (rand() % 360) * (M_PI / 180.0f);
        vec2 emitDir(cos(randAngle), sin(randAngle));
        waves.emplace_back(abs(energyDiff), pos, emitDir, vec3(1.0f, 1.0f, 0.0f));
        n--;
        n_continuous = (float)n;
        orbitScale = (float)n;
        excitedTimer = 1.0f;
    }
        
    }
};

vector<Particle> particles = {
    Particle(vec2(0.0f) , 1 ),
    Particle(vec2(-50.0f , 0.0f) , -1)
};




struct Atom {

    vec2 pos;
    vector<Particle> particles;

    Atom(vec2 pos) : pos(pos) , particles ({
        Particle(pos, 1 ),
        Particle(pos , -1)
    }) {}
};

vector<Atom> atoms {
    Atom(vec2(0.0f , 250.f)),
    Atom(vec2(0.0f , 200.f)),
    Atom(vec2(0.0f , 150.f)),
    Atom(vec2(0.0f , 100.f)),
    Atom(vec2(0.0f , 50.f)),
    Atom(vec2(0.0f , 0.0f)),
    Atom(vec2(0.0f , -50.f)),
    Atom(vec2(0.0f , -100.f)),
    Atom(vec2(0.0f , -150.f)),
};


void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action != GLFW_PRESS) return;

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Convert Screen Pixels to World Coordinates
    // Screen (0,0) is top-left. World (0,0) is center.
    float worldX = (float)xpos - (engine.WIDTH / 2.0f);
    float worldY = (engine.HEIGHT / 2.0f) - (float)ypos;
    vec2 mousePos(worldX, worldY);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        // Spawn a burst of 8 photons in a circle
        float energy1to2 = (-13.6f / 4.0f) - (-13.6f);
        for (int i = 0; i < 8; i++) {
            float angle = i * (2.0f * M_PI / 8.0f);
            vec2 dir(cos(angle), sin(angle));
            waves.emplace_back(energy1to2, mousePos, dir, vec3(0.0f, 1.0f, 1.0f));
        }
        cout << "Spawned photons at: " << worldX << ", " << worldY << endl;
    } 
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        // Place a new Atom
        atoms.emplace_back(mousePos);
        cout << "Placed atom at: " << worldX << ", " << worldY << endl;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    for (Atom& a : atoms) {
        for (Particle& p : a.particles) {
            if (p.charge != -1) continue;
            if (key == GLFW_KEY_UP)    p.n_continuous += 0.1f;   // more oscillations
            if (key == GLFW_KEY_DOWN)  p.n_continuous -= 0.1f;   // fewer oscillations
            if (key == GLFW_KEY_RIGHT) { p.orbitScale += 1.0f; p.n_continuous = p.orbitScale; } // next shell
            if (key == GLFW_KEY_LEFT)  { p.orbitScale -= 1.0f; p.n_continuous = p.orbitScale; } // prev shell
            if (p.n_continuous < 0.1f) p.n_continuous = 0.1f;
            if (p.orbitScale < 1.0f)   p.orbitScale = 1.0f;
        }
    }
}

void drawHUD() {
    // find first electron
    float n_c = 1.0f, orb = 1.0f;
    int n = 1;
    for (Atom& a : atoms) {
        for (Particle& p : a.particles) {
            if (p.charge == -1) {
                n_c = p.n_continuous;
                orb = p.orbitScale;
                n = p.n;
                goto found;
            }
        }
    }
    found:

    // check if wave closes perfectly
    float remainder = fmod(n_c, 1.0f);
    bool isQuantized = (remainder < 0.05f || remainder > 0.95f);

    // draw text via window title (simple, no font library needed)
    char title[256];
    snprintf(title, sizeof(title),
        "n_continuous: %.2f | shell (orbitScale): %.0f | n: %d | %s  [UP/DOWN = oscillations, LEFT/RIGHT = shell]",
        n_c, orb, n,
        isQuantized ? "*** STANDING WAVE ***" : "wave broken"
    );
    glfwSetWindowTitle(engine.window, title);
}

int main() {

    // Initialize 20 atoms in a circle at the center
    // {
    //     int num_atoms = 5;
    //     float radius = 150.0f; // Radius of the circle
    //     for (int i = 0; i < num_atoms; i++) {
    //         float angle = 2.0f * M_PI * i / num_atoms;
    //         float x = cos(angle) * radius;
    //         float y = sin(angle) * radius;
    //         atoms.emplace_back(vec2(x, y));
    //     }
    // }
    
    float energy1to2 = (-13.6f / 4.0f) - (-13.6f);
    for (int i = 0; i < 10; i++) {
        waves.push_back(Wave(energy1to2, vec2(400, i*50 -50 ), vec2(-1.0f, 0.0f)));
    }

    while (!glfwWindowShouldClose(engine.window)) {

        glfwPollEvents();
        engine.run();
        glfwSetMouseButtonCallback(engine.window, mouse_button_callback);
        glfwSetMouseButtonCallback(engine.window, mouse_button_callback);
        glfwSetKeyCallback(engine.window, key_callback);
        
        for (Atom &a : atoms){
            for ( Particle& p : a.particles) {
                p.draw(a.pos);
                // --- electrons --- 
                if (p.charge == -1) {
                    if (p.excitedTimer > 0.0f) p.excitedTimer -= 0.01f;
                    p.update(a.pos);

                    float energyforUp = (-13.6f / ((p.n + 1) * (p.n + 1))) - (-13.6f / (p.n * p.n));
                    for (Wave& w: waves) { 
                        if (w.energy <= 0.0f || w.col == vec3(1.0f, 1.0f, 0.0f)) continue;
                        for (WavePoint& wp : w.points){
                            float dist = length(p.pos - wp.localPos);
                            
                            // --- photon hits the atom ---
                            if (length(p.pos - w.points[w.points.size()/2].localPos) < 30.0f) {
                                if (abs(w.energy - energyforUp) < 0.1f) {
                                w.energy = 0.0f;
                                p.n++;
                                p.n_continuous = (float)p.n;
                                p.orbitScale = (float)p.n;
                                p.excitedTimer = 1.0f; // Stay excited for a bit
                                break;
                            }
                    }
                }
            }
        }
    }}
    // Starts a pointer at the first photon , 
    for (auto it = waves.begin(); it != waves.end();){
        if (it->energy <= 0.0f) {
            it = waves.erase(it); // remove absorbed photon. removes the photon and returns a new pointer to the very next item.
            continue;
        }
        it->draw();
        if (it->update(0.03f)) { // if update returns true, it's off-screen
            it = waves.erase(it);
            } 
        else {
         ++it;
        }
    }

        glfwSwapBuffers(engine.window);
        drawHUD();
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}