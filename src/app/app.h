#pragma once
#include <glm/glm.hpp>
#include "gfx/camera.h"
#include "sim/particle_system.h"

class Engine;

namespace limits {
    constexpr int kMinPrincipal = 1;
    constexpr int kMaxPrincipal = 8;
    constexpr int kMinParticles = 1000;
    constexpr int kMaxParticles = 400000;
    constexpr float kMinFlowSpeed = 0.0f;
    constexpr float kMaxFlowSpeed = 12.0f;
}

struct QuantumNumbers {
    int n = 4; // principal: shell
    int l = 2; // azimuthal: shape
    int m = 1; // magnetic: orientation
};

inline bool operator==(const QuantumNumbers& a, const QuantumNumbers& b) {
    return a.n == b.n && a.l == b.l && a.m == b.m;
}
inline bool operator!=(const QuantumNumbers& a, const QuantumNumbers& b) { return !(a == b); }


struct App {
    
    Engine* engine = nullptr;
    Camera camera;
    ParticleSystem particles;

    QuantumNumbers quantum;
    int particleCount = 40000;
    float flowSpeed = 0.085f;
    bool showCrossSection = false;
    bool triggerResample = true;

    int viewportWidth = 0;
    int viewportHeight = 0;

    glm::vec3 lightPosition{50.0f, 50.0f, 50.0f};
};

// state changes

void requestQuantumNumbers(App& app, QuantumNumbers wanted);
void requestParticleCount (App& app, int wanted);
void requestFlowSpeed     (App& app, float wanted);

// camera distance to fit the orbital

float framingDistance(int n);