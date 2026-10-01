#pragma once
#include <vector>
#include <random>

double evaluateDensity(double x, double y, double z, int n, int l, int m, double Z);

double computeRMax(int n, double Z);

// our three muskateers 

std::vector<double> buildRadialCDF   (int n, int l, double Z, double rMax, int resolution);
std::vector<double> buildPolarCDF    (int l, int m, int resolution);
std::vector<double> buildAzimuthalCDF(int m, int resolution);

double estimateMaxDensity(int n, int l, int m, double Z, double rMax, std::mt19937& rng);