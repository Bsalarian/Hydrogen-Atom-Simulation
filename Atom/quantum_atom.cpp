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
*/



#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>


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
        glClearColor(1.1f,0.1f,0.15f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
    }
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}