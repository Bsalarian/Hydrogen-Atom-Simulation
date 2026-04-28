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

struct Particle{
    vec2 pos;
    int charge;
    float angle;
    Particle(vec2 pos ,int charge) : pos(pos), charge(charge), angle(0.0f) {}

    void draw(int segments = 50){
        float r;
        if (charge == -1 ) {
            r = 2 ;
            glColor3f(0.0f , 1.0f, 1.0f );
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
    void update () {
        angle += 0.001;
        pos.x = cos(angle) * orbitDistance;
        pos.y = sin(angle) * orbitDistance;
    }
};

vector<Particle> particles = {
    Particle(vec2(0.0f) , 1 ),
    Particle(vec2(-50.0f , 0.0f) , -1)
};


struct Engine {

    GLFWwindow* window;
    int WIDTH = 800 , HEIGHT = 600;

    Engine () {
        // --- Init GLFW ---
        if (!glfwInit()) {
            cerr << "failed to init glfw, LOL";
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


int main() {
    Engine engine;

    while (!glfwWindowShouldClose(engine.window)) {

        glfwPollEvents();
        engine.run();
        glClear(GL_COLOR_BUFFER_BIT);     
        for ( Particle& p : particles) {
            p.draw();
            if (p.charge == -1) {
                p.update();
            }
        }
        glfwSwapBuffers(engine.window);
    }

    glfwTerminate();
    return 0;
}