// gfx/camera.cpp
#include "gfx/camera.h"
#include "gfx/gl.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace {
    constexpr float kPi       = 3.14159265358979323846f;
    constexpr float kMinElev  = 0.01f;
    constexpr float kMaxElev  = kPi - 0.01f;
    constexpr float kMinRad   = 0.5f;
    constexpr float kMaxRad   = 300.0f;
}

glm::vec3 Camera::position() const {
    float e = glm::clamp(elevation, kMinElev, kMaxElev);
    return { radius * std::sin(e) * std::cos(azimuth),
             radius * std::cos(e),
             radius * std::sin(e) * std::sin(azimuth) };
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
}

void Camera::rotate(float dx, float dy) {
    azimuth   -= dx * orbitSpeed;
    elevation  = glm::clamp(elevation + dy * orbitSpeed, kMinElev, kMaxElev);
}

void Camera::zoom(float steps) {
    targetRadius -= steps * targetRadius * 0.1f;
    targetRadius  = glm::clamp(targetRadius, kMinRad, kMaxRad);
}

void Camera::update() {
    radius += (targetRadius - radius) * zoomSmoothing;
}

void Camera::onMouseButton(int button, int action, GLFWwindow* win) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;
    dragging = (action == GLFW_PRESS);
    if (dragging) glfwGetCursorPos(win, &lastX, &lastY);
}

void Camera::onMouseMove(double x, double y) {
    if (!dragging) return;
    rotate((float)(x - lastX), (float)(y - lastY));
    lastX = x;
    lastY = y;
}

void Camera::onScroll(double, double dy) {
    zoom(dy > 0.0 ? 1.0f : (dy < 0.0 ? -1.0f : 0.0f));
}
