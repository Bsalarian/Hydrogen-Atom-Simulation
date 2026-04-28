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




struct Particle{
    vec2 pos;
    int charge;
    Particle(vec3 pos ,int charge) : pos(pos), charge(charge) {}
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
        window = glfwCreateWindow(WIDTH, HEIGHT, "2D atom sim", nullptr, nullptr);
        if (!window) {
            cerr << "failed to create window, LOLOLOL";
            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        glfwMakeContextCurrent(window);

        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            cerr << "Failed to initialize GLEW\n";
            exit(EXIT_FAILURE);
        }


        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth , &fbHeight);
        glViewport(0 , 0 , fbWidth , fbHeight); 
    }
};


int main() {
    Engine engine;

    while (!glfwWindowShouldClose(engine.window)) {
        glfwPollEvents();
        glfwSwapBuffers(engine.window);
    }

    glfwTerminate();
    return 0;
}