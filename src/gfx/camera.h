#pragma once
#include <glm/glm.hpp>
struct GLFWwindow;

struct Camera {
    float radius = 10.0f, targetRadius = 10.0f;
    float azimuth = 0.0f;
    float elevation = 1.5707963f;   // pi/2 : start on the equator
    float orbitSpeed = 0.005f;
    float zoomSmoothing = 0.05f; 
    bool dragging = false;
    double lastX = 0, lastY = 0;

    glm::vec3 position() const;
    glm::mat4 viewMatrix() const;

    // shared by desktop input AND the web/JS bindings.
    void rotate(float dx, float dy);
    void zoom(float steps);         // 10% closer
    void update();                  // smoothing toward targetRadius

    // Desktop (GLFW) adapters
    void onMouseButton(int button, int action, GLFWwindow* win);
    void onMouseMove(double x, double y);
    void onScroll(double dx, double dy);
};