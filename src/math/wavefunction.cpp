#include "math/wavefunction.h"
#include "math/special_functions.h" 
#include <cmath>
#include <random>    




double evaluateDensity(double x ,double y, double z, int n, int l, int m, double Z){

    double r = std::sqrt(x*x + y*y + z*z);
    if ( r < 1e-6) return 0.0; // Prevent singularity

    // Standard physics coordinates (theta = vertical inclination, phi = horizontal azimuth)
    double theta = std::acos(y / r);       
    double phi = std::atan2(z, x);         
    if (phi < 0.0) phi += 2.0 * M_PI;
    
    // Radial part -- determines distance of the electron
    // Square root part for normalization constant.
    // Exponential $e^{-\rho / 2}$ to make sure the wf decays to 0 when far
    // The Laguerre polynomial L for generating alternating peaks and valleys of density.

    double rho = (2.0 * r * Z) /n; // for a0=1.0 aka hydrogen
    // https://en.wikipedia.org/wiki/Gamma_function Gamma (x+1 interpolates the factorial function to non-integer values.)
    double constant = std::sqrt( std::pow(2.0 / n , 3) * (std::tgamma(n - l) / ( 2.0 * n * std::tgamma(n+l+1)))); 
    double radial = constant * std::exp(-rho / 2.0) * std::pow(rho ,l) * assocLaguerre(n-l-1 , 2.0 * l +1 , rho);


    // Angular part -- defines the shape of the orbitals.
    // We use Real Spherical Harmonics and sphLegendre to apply the normalization factor $N_{lm}$.

    double angular = sphLegendre(l , std::abs(m) , theta);

    // Apply real spherical harmonics mapping for horizental rotation
    if (m > 0) {
        angular *= std::sqrt(2.0) * std::cos(m * phi);
    } 
    else if (m < 0 ) {
        angular *= std::sqrt(2.0) * std::sin(std::abs(m)* phi);
    }

    // Probability density is the squared magnitude of the wave function
    double psi = radial * angular;
    return psi * psi;
}


double computeRMax(int n, double Z) {
    return (n * n + 3.0 * n) / Z * 1.5;
}



std::vector<double> buildRadialCDF(int n, int l, double Z, double rMax, int resolution) {
    std::vector<double> cdf(resolution);
    double sum = 0.0;
    for (int i = 0; i < resolution; ++i) {
        
        double r = i * (rMax / (resolution - 1));
        if (r < 1e-6) { cdf[i] = 0; continue; }
        double rho = (2.0 * r * Z) / n;
        double R_val = std::exp(-rho / 2.0) * std::pow(rho, l) * assocLaguerre(n - l - 1, 2.0 * l + 1, rho);

        // PDF = r^2 * |R(r)|^2
        sum += (r * r) * (R_val * R_val);
        cdf[i] = sum;
    }
    for (double& v : cdf) v /= sum; // Normalize to 1.0
    return cdf;
}


std::vector<double> buildPolarCDF(int l, int m, int resolution) {
    std::vector<double> cdf(resolution);
    double sum = 0.0;
    for (int i = 0; i < resolution; ++i) {
            double theta = i * (M_PI / (resolution - 1));
            double Y_val = sphLegendre(l, std::abs(m), theta);
            
            // PDF = sin(theta) * |Y(theta)|^2
            sum += std::sin(theta) * (Y_val * Y_val);
            cdf[i] = sum;
        }
    for (double& v : cdf) v /= sum; // Normalize to 1.0
    return cdf;
}


std::vector<double> buildAzimuthalCDF(int m, int resolution) {
    std::vector<double> cdf(resolution);
    double sum = 0.0;
    for (int i = 0; i < resolution; ++i) {
        double phi = i * (2.0 * M_PI / (resolution - 1));
        
        // Real spherical harmonics mapping
        double Phi_val = 1.0;
        if (m > 0) Phi_val = std::cos(m * phi);
        else if (m < 0) Phi_val = std::sin(std::abs(m) * phi);
        
        sum += (Phi_val * Phi_val);
        cdf[i] = sum;
    }
    for (double& v : cdf) v /= sum;
    return cdf;
}



double estimateMaxDensity(int n, int l, int m, double Z,
                          double rMax, std::mt19937& rng) {
    double maxDensity = 0.0;
    std::uniform_real_distribution<double> distPos(-rMax, rMax);
    for (int i = 0; i < 10000; ++i) {
        double x = distPos(rng), y = distPos(rng), z = distPos(rng);
        double d = evaluateDensity(x, y, z, n, l, m, Z);
        if (d > maxDensity) maxDensity = d;
    }
    return maxDensity;
}