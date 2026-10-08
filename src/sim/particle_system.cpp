#include "sim/particle_system.h"
#include "math/wavefunction.h"
#include "color/heatmap.h"
#include <algorithm>
#include <cmath>
#include <iostream>  


namespace {

constexpr double kPi         = 3.14159265358979323846;
constexpr float  kTwoPiF     = 6.28318530717958647692f;
constexpr int    kResolution = 2000;    // slices per CDF
constexpr float  kColorGamma = 0.35f;   // <1 brightens faint regions

// spherical -> cartesian, y up
glm::dvec3 toCartesian(double r, double theta, double phi) {
    return {
        r * sin(theta) * cos(phi),
        r * cos(theta),
        r * sin(theta) * sin(phi)
     };
}

// inverse-transform sampling: turn a uniform value u [0,1] into a value in our distrubito
double sampleFromCDF(const std::vector<double>& cdf, double range, double u) {
    auto it = std::lower_bound(cdf.begin(), cdf.end(), u);
    double atomicNum = std::min<double>(std::distance(cdf.begin(), it), cdf.size() -1);
    return atomicNum * (range / (cdf.size() -1));
}

glm::vec3 colorFromDensity(double density, double maxDensity) {
    if (maxDensity <= 0.0) return heatmapInferno(0.5f);
    float t = glm::clamp((float)(density / maxDensity), 0.0f, 1.0f);
    return heatmapInferno(std::pow(t, kColorGamma));
}

} // namespace end. cpp trick to private the code

bool ParticleSystem::sampleWaveFunctionCDF(int n, int l, int m, int N, double Z){

    // 1) build three 1D distributions
    const double rMax = computeRMax(n,Z);
    const auto cdfR = buildRadialCDF(n, l, Z, rMax, kResolution);
    const auto cdfTheta = buildPolarCDF(l, m, kResolution);
    const auto cdfPhi = buildAzimuthalCDF(m, kResolution);

    // Keep the previous cloud if current one is invalid
    if (cdfR.empty() || cdfTheta.empty() || cdfPhi.empty()) {
        std::cerr << "[SAMPLER] invalid distribution for n=" << n << " l=" << l
                  << " m=" << m << " — keeping previous cloud\n";
        return false;
    }

    // 2) find the peak density, for the colors to scale
    const double maxDensity = estimateMaxDensity(n,l,m,Z,rMax,rng);

    // 3) draw N samples. swapped in if success
    std::uniform_real_distribution<double> U(0.0,1.0);
    std::vector<Particle> fresh;
    fresh.reserve(N);

    for (int i = 0; i < N; i++) {

        const double r = sampleFromCDF(cdfR, rMax, U(rng));
        const double theta = sampleFromCDF(cdfTheta, kPi , U(rng));
        const double phi = sampleFromCDF(cdfPhi, 2.0*kPi , U(rng));

        // to cartesian
        const glm::dvec3 c = toCartesian(r,theta,phi);


        Particle p;
        p.pos = glm::vec3(c);
        p.r = (float) r;
        p.theta = (float) theta;
        p.phi = (float) phi;
        p.color = colorFromDensity(evaluateDensity(c.x,c.y,c.z,n,l,m,Z), maxDensity);
        fresh.push_back(p);
    }

    particles.swap(fresh);
    std::cout << "n=" << n << " l=" << l << " m=" << m << "  particles=" << N << "\n";
    return true;
}

void ParticleSystem::updateProbabilityCurrent(int m_quantum, float dt) {
        if (m_quantum == 0) return;  // no azimuthal current
        for (Particle& p : particles) {
            float sinTheta = std::max(std::sin(p.theta), 1e-4f);
            // Should be r2 sin2theta, but that get's pretty crazy so lowered the power for visuals sake
            p.phi += ((float)m_quantum / (p.r * sinTheta)) * dt;
            p.phi  = std::fmod(p.phi, kTwoPiF);  // keep phi small
            p.pos = glm::vec3(toCartesian(p.r, p.theta, p.phi)); 
    }
}