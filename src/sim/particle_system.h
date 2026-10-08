#pragma once
#include <vector>
#include <random>
#include "sim/particle.h"

class ParticleSystem {
    public:
        // our particles
        std::vector<Particle> particles;
        // Samples N particles from our psi squared, returns False if invalid distrubtion.
        bool sampleWaveFunctionCDF(int n , int l , int m , int N, double Z=1.0);
        // Rotate dem particles around the y-axis
        void updateProbabilityCurrent(int m , float dt);
    private:
        std::mt19937 rng{42}; // fixed seed so it's reproducibles
};