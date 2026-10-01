#pragma once

// Associated Laguerre polynomial via 3-term recurrence.
double assocLaguerre(int n, double alpha, double x);

// Normalised associated Legendre P_l^m(cos θ).
double sphLegendre(int l, int m, double theta);
