#include "app/app.h"
#include <algorithm>

namespace {

void enforceLimits(App& app) {
    QuantumNumbers& q = app.quantum;
    q.n = std::clamp(q.n, limits::kMinPrincipal, limits::kMaxPrincipal);
    q.l = std::clamp(q.l, 0, q.n - 1);
    q.m = std::clamp(q.m, -q.l, q.l);

    app.particleCount = std::clamp(app.particleCount, limits::kMinParticles, limits::kMaxParticles);
    app.flowSpeed     = std::clamp(app.flowSpeed,     limits::kMinFlowSpeed, limits::kMaxFlowSpeed);
    }
}

void requestQuantumNumbers(App& app, QuantumNumbers wanted) {
    const QuantumNumbers before = app.quantum;
    app.quantum = wanted;
    enforceLimits(app);
    if (app.quantum != before) app.needsResample = true;
}

void requestParticleCount(App& app, int wanted) {
    const int before = app.particleCount;
    app.particleCount = wanted;
    enforceLimits(app);
    if (app.particleCount != before) app.needsResample = true;
}

void requestFlowSpeed(App& app, float wanted) {
    app.flowSpeed = wanted;
    enforceLimits(app);
    // No resample: speed only changes the animation, not the cloud itself.
}

float framingDistance(int n) {
    // The cloud's radius grows roughly like n², so the camera backs off at the same rate.
    return (float)(n * n + 3.0 * n) * 2.6f;
}