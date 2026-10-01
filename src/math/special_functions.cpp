#include "math/special_functions.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double sphLegendre(int l, int m, double theta) {
    double cosT = std::cos(theta); double sinT = std::sin(theta);
    double Pmm = 1.0; double factor = 1.0;
    for (int i = 1; i <= m; ++i) { Pmm *= -factor * sinT; factor += 2.0; }
    if (l == m) {
        double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
        for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i)); 
        return norm * Pmm;
    }
    double Pmp1m = cosT * (2.0*m + 1.0) * Pmm;
    if (l == m + 1) {
        double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
        for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i));
        return norm * Pmp1m;
    }
    double Plm = 0.0;
    for (int ll = m + 2; ll <= l; ++ll) {
        Plm = ((2.0*ll - 1.0) * cosT * Pmp1m - (ll + m - 1.0) * Pmm) / (ll - m);
        Pmm = Pmp1m; Pmp1m = Plm;
    }
    double norm = std::sqrt((2.0*l + 1.0) / (4.0 * M_PI));
    for (int i = 1; i <= m; ++i) norm *= std::sqrt(1.0 / (2.0 * i));
    return norm * Plm;
}



/* 
Defining the Associated Laguerre polynomial
Uses the standard 3-term recurrence:

    L_0 = 1
    L_1 = 1 + alpha - x
    L_k = ((2k-1+alpha-x)*L_{k-1} - (k-1+alpha)*L_{k-2}) / k

*/
static double assocLaguerre(int n, double alpha, double x) {
    if (n == 0) return 1.0; if (n == 1) return 1.0 + alpha - x;
    double L0 = 1.0, L1 = 1.0 + alpha - x, Lk = 0.0;
    for (int k = 2; k <= n; ++k) {
        Lk = ((2*k - 1 + alpha - x) * L1 - (k - 1 + alpha) * L0) / k;
        L0 = L1; L1 = Lk;
    }
    return Lk;
}
