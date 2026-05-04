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

struct Particle{
    vec2 pos;
    int charge;
    float angle;
    int n = 1;
    Particle(vec2 pos ,int charge) : pos(pos), charge(charge), angle(0.0f) {}

    void draw(vec2 center, int segments = 50){
        


        float r;
        if (charge == -1 ) {
            r = 4 ;
            glColor3f(0.0f , 1.0f, 1.0f );
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
        else if (charge == 1) {
            r = 10;
            glColor3f(1.0f , 0.0f, 0.0f );
        }
        else {
            glColor3f(0.5f , 0.5f, 0.5f );
        }

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

        angle += 0.01;
        pos.x = c.x + cos(angle) * r;
        pos.y = c.y + sin(angle) * r;
    }
};

vector<Particle> particles = {
    Particle(vec2(0.0f) , 1 ),
    Particle(vec2(-50.0f , 0.0f) , -1)
};

struct WavePoint { vec2 localPos; vec2 dir;  };
struct Wave{
    vec2 pos, dir;
    float energy, wavelength , frequency;
    float sigma = 40.0f, k = 0.4f, phase = 0.0f, a = 10.0f, angleR;
    vector<WavePoint> points;
    bool absorbed = false; 

    
    Wave(float e , vec2 pos , vec2 dir): energy(e) , pos(pos) , dir(dir) {
        dir = normalize(dir);
        for (float x = -sigma; x<= sigma; x+= 0.1f )
            points.push_back({ pos + x*dir , dir*200.0f});
        angleR = atan2(dir.y, dir.x);
    }

    void draw() {

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
           // move along velocity
           p.localPos += p.dir * dt;


            if (p.localPos.x < -engine.WIDTH/2.0f || p.localPos.x > engine.WIDTH/2.0f || p.localPos.y < -engine.HEIGHT/2.0f || p.localPos.y > engine.HEIGHT/2.0f) {
                return true;
            }
       }
       return false;
   }
};
vector<Wave> waves {
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

    
    float energyN1toN2 = -13.6f - (-13.6f/(2*2));

    for (int i = 0; i < 10; i++) {
        waves.push_back(Wave(energyN1toN2, vec2(400, i*50 -50 ), vec2(-1.0f, 0.0f)));
    }


    while (!glfwWindowShouldClose(engine.window)) {

        glfwPollEvents();
        engine.run();
        
        for (Atom &a : atoms){
            for ( Particle& p : a.particles) {
                p.draw(a.pos);
                // --- electrons --- 
                if (p.charge == -1) {
                    p.update(a.pos);
                    bool hit = false;
                    for (Wave& w: waves) { 
                        if (w.energy == 0.0f) continue;
                        for (WavePoint& wp : w.points){
                            float dist = length(p.pos - wp.localPos);
                            // --- photon hits the atom ---
                            if ( dist < 20) {
                                w.energy == 0.0f;
                                p.n += 1; // update energy level
                                hit = true;
                                break;
                        }
                    }
                    if (hit) break;
                }
            }
        }

            for (Wave& w: waves){
                if (w.energy == 0.0f) continue;
                w.draw();
                w.update(0.001f);
            }
    }
    glfwSwapBuffers(engine.window);
}
    glfwTerminate();
    return 0;
}