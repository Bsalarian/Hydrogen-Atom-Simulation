# The Physics of the Hydrogen Orbital Visualiser

This document explains, from the ground up, the mathematics the simulation
computes and why each piece exists. It is written to be read by a curious
person who remembers some calculus but wants the "why," not just the "what."
We go down several rabbit holes deliberately — that is the point.

--------------------------------------------------------------------------------
CONTENTS
--------------------------------------------------------------------------------
  1. The Question the Hydrogen Atom Answers
  2. The Schrödinger Equation and Separation of Variables
  3. The Three Quantum Numbers
  4. The Radial Part — and Why Laguerre Polynomials Appear
  5. Rabbit Hole: What IS a Laguerre Polynomial?
  6. Rabbit Hole: Why the Gamma Function?
  7. The Angular Part — and Why Legendre Functions Appear
  8. Rabbit Hole: What IS a Legendre Polynomial?
  9. Spherical Harmonics — Real vs Complex
  10. Nodes: Reading the Shape of an Orbital
  11. From |ψ|² to Particles: Sampling and Jacobians
  12. The Probability Current — What It Really Is
  13. Physical Constants and Atomic Units in the Code


================================================================================
1.  THE QUESTION THE HYDROGEN ATOM ANSWERS
================================================================================

A hydrogen atom is one proton and one electron. The old "planetary" picture —
the electron orbiting like a tiny planet — is wrong. A charged particle moving
in a circle radiates energy; a classical electron would spiral into the nucleus
in about 10⁻¹¹ seconds. Atoms are stable, so the classical picture must fail.

Quantum mechanics replaces the orbit with a WAVEFUNCTION ψ(r, θ, φ). This is a
complex number attached to every point in space. It is not directly observable.
What IS observable is its squared magnitude:

    |ψ(r, θ, φ)|²  =  probability density of finding the electron there.

This visualiser scatters points according to that density. The cloud you see IS
|ψ|². Each pearl is a "the electron could be here" vote, weighted by
probability.


================================================================================
2.  THE SCHRÖDINGER EQUATION AND SEPARATION OF VARIABLES
================================================================================

The time-independent Schrödinger equation for the electron in the proton's
Coulomb field is:

    −(ℏ²/2mₑ) ∇²ψ  −  (e²/4πε₀r) ψ  =  E ψ

Because the potential depends only on r (it is spherically symmetric), the
equation can be SEPARATED. We guess a product solution:

    ψ(r, θ, φ)  =  R(r) · Θ(θ) · Φ(φ)

Substituting and dividing through, the equation splits into three independent
ordinary differential equations — one for each coordinate. This separation is
the mathematical reason the atom has exactly THREE quantum numbers: one
constant of separation emerges from each equation.

    R(r)      → radial equation      → gives n and l, produces Laguerre polys
    Θ(θ)      → polar equation       → gives l and m, produces Legendre funcs
    Φ(φ)      → azimuthal equation   → gives m, produces e^(imφ)

The angular pieces Θ·Φ combine into the SPHERICAL HARMONICS Y_lm(θ, φ).


================================================================================
3.  THE THREE QUANTUM NUMBERS
================================================================================

    n — PRINCIPAL. n = 1, 2, 3, …
        Sets the energy: E_n = −13.6 eV / n². Larger n = larger, higher-energy
        orbital, electron farther from the nucleus.

    l — AZIMUTHAL (orbital angular momentum). l = 0, 1, …, n−1.
        Sets the SHAPE and the total angular momentum |L| = ℏ√(l(l+1)).
        l = 0 (s) sphere, l = 1 (p) dumbbell, l = 2 (d) cloverleaf, l = 3 (f).

    m — MAGNETIC. m = −l, …, 0, …, +l.
        Sets the ORIENTATION and the z-component of angular momentum L_z = mℏ.
        Called "magnetic" because it determines how the orbital's energy splits
        in a magnetic field (the Zeeman effect) — and, as we'll see, because a
        complex orbital with m ≠ 0 carries a real circulating current.

The constraints l < n and |m| ≤ l are not arbitrary — they fall out of
requiring the solutions to be finite and single-valued. The code enforces them
in clampQuantumNumbers.


================================================================================
4.  THE RADIAL PART — AND WHY LAGUERRE POLYNOMIALS APPEAR
================================================================================

The radial equation, after substitutions, becomes a differential equation
whose well-behaved solutions are:

    R_nl(r)  =  N_nl · e^(−ρ/2) · ρ^l · L_{n−l−1}^{2l+1}(ρ),   ρ = 2r/(n·a₀)

Three factors, each with a job:

    e^(−ρ/2)   — exponential decay. Guarantees the electron is BOUND: the
                 probability must vanish at infinity. Without it, ψ would not
                 be normalisable.

    ρ^l        — behaviour near the nucleus. For l > 0 the electron is pushed
                 away from r = 0 by the "centrifugal barrier" of angular
                 momentum, so density starts at zero and rises.

    L(ρ)       — the ASSOCIATED LAGUERRE POLYNOMIAL. This is the part that
                 oscillates, creating the alternating shells of high and low
                 density (the RADIAL NODES).

In the code, assocLaguerre computes L via the three-term recurrence:

    L_0 = 1
    L_1 = 1 + α − x
    L_k = [ (2k−1+α−x)·L_{k−1} − (k−1+α)·L_{k−2} ] / k

Recurrence is used instead of the explicit factorial sum because it is
numerically STABLE (no catastrophic cancellation between huge factorials) and
fast.


================================================================================
5.  RABBIT HOLE: WHAT IS A LAGUERRE POLYNOMIAL?
================================================================================

Laguerre polynomials L_n(x) are a family of polynomials that are ORTHOGONAL on
the interval [0, ∞) with respect to the weight e^(−x):

    ∫₀^∞ e^(−x) L_m(x) L_n(x) dx  =  0   whenever m ≠ n.

"Orthogonal" is the key word. Just as the unit vectors x̂, ŷ, ẑ are mutually
perpendicular and let you build any 3D vector from independent components, a
family of orthogonal polynomials lets you build any reasonable function on
[0, ∞) from independent "polynomial directions." They are the natural
coordinate system for problems on a half-line with exponential decay — exactly
the situation for an electron bound near a nucleus.

The ASSOCIATED Laguerre polynomials L_n^α(x) generalise this to the weight
x^α · e^(−x). The superscript α (here 2l+1) tilts the weighting to account for
the angular momentum barrier. They appear whenever you solve a differential
equation of the "Laguerre type," which the hydrogen radial equation is, after
the exponential and power factors are stripped off.

Why do they show up here at all? Because when you plug R(r) = e^(−ρ/2)·ρ^l·f(ρ)
into the radial equation, the leftover equation for f is PRECISELY the
associated Laguerre differential equation. The polynomial solutions are the
only ones that keep ψ finite; any non-polynomial solution blows up at infinity.
So Laguerre polynomials aren't chosen — they are FORCED by the physics.

The degree of the polynomial is n−l−1. That number equals the count of RADIAL
NODES. This is why higher n (at fixed l) shows more concentric shells.


================================================================================
6.  RABBIT HOLE: WHY THE GAMMA FUNCTION?
================================================================================

The normalisation constant N_nl contains factorials:

    N_nl  =  √[ (2/(n·a₀))³ · (n−l−1)! / (2n·(n+l)!) ]

The code computes these factorials with std::tgamma, the GAMMA FUNCTION, using
the identity:

    Γ(k+1) = k!     for non-negative integers k.

Why not just multiply integers in a loop? Two reasons:

    1. Convenience and range. tgamma handles large arguments smoothly and
       returns a double, avoiding integer overflow for large n. (14! already
       overflows a 32-bit int.)

    2. Generality. The Gamma function INTERPOLATES the factorial to all real
       (and complex) numbers:

           Γ(x) = ∫₀^∞ t^(x−1) e^(−t) dt.

       For integers this reproduces (x−1)!, but it is smooth and defined
       everywhere except non-positive integers. This matters if you ever want
       to generalise to non-integer parameters (e.g. certain effective-quantum-
       number or fractional-dimension models). Using Γ from the start keeps the
       code honest about what quantity it really needs: the factorial as a
       special case of a continuous function.

IMPORTANT BUG (see README_FIXES.md): the code uses Γ(n+l) where it should use
Γ(n+l+1) = (n+l)!. It is off by a factor of (n+l) inside the square root. This
only affects the ABSOLUTE scale of the density, and since colours are
normalised by maxDensity (computed from the same function), the error is
invisible on screen — but it is wrong, and would matter for any absolute
comparison.


================================================================================
7.  THE ANGULAR PART — AND WHY LEGENDRE FUNCTIONS APPEAR
================================================================================

The angular equations give the SPHERICAL HARMONICS:

    Y_lm(θ, φ)  =  N_lm · P_l^m(cos θ) · e^(imφ)

    • e^(imφ)     — the azimuthal (φ) dependence. Single-valuedness (going once
                    around, φ → φ+2π, must return the same value) forces m to be
                    an INTEGER. This is where m's integrality comes from.

    • P_l^m(cosθ) — the ASSOCIATED LEGENDRE FUNCTION. This is the polar (θ)
                    dependence and creates the ANGULAR NODES (nodal cones and
                    planes) that give orbitals their lobed shapes.

In the code, sphLegendre computes the NORMALISED P_l^m(cos θ), folding in the
constant N_lm = √[ (2l+1)/(4π) · (l−m)!/(l+m)! ] as it goes, again via stable
recurrences (seed P_m^m, step up in l).


================================================================================
8.  RABBIT HOLE: WHAT IS A LEGENDRE POLYNOMIAL?
================================================================================

Legendre polynomials P_l(x) are the natural orthogonal family on the interval
[−1, 1] with weight 1:

    ∫₋₁^¹ P_m(x) P_n(x) dx  =  0   whenever m ≠ n.

The interval [−1, 1] is exactly the range of cos θ as θ runs from 0 (north
pole) to π (south pole). So Legendre polynomials are the natural language for
functions on a SPHERE'S LATITUDE.

They arise from Laplace's equation ∇²f = 0 in spherical coordinates — the same
operator (the Laplacian) that appears in the Schrödinger equation's kinetic
term. Solve the angular part of ∇² and Legendre polynomials fall out
inevitably. This is why they appear all over physics: gravity, electrostatics,
heat flow on a sphere, and quantum mechanics all share the Laplacian.

The ASSOCIATED Legendre functions P_l^m(x) generalise them to handle the m ≠ 0
cases (orbitals that aren't azimuthally symmetric). The "associated" versions
carry m derivatives of the ordinary polynomial:

    P_l^m(x) = (−1)^m (1−x²)^(m/2) · dᵐ/dxᵐ P_l(x).

The (1−x²)^(m/2) = (sin θ)^m factor pushes the function to zero at the poles for
m ≠ 0 — which is why, for example, a p_x orbital has no density along the axis
perpendicular to its lobes.

The number of angular nodes is l. Split between:
    • m nodal planes/cones from the φ and high-m structure, and
    • l − |m| nodal cones from the polar polynomial.
Together with the n−l−1 radial nodes, the TOTAL node count is always n−1 — a
beautiful, exact bookkeeping rule the visualiser lets you verify by eye.


================================================================================
9.  SPHERICAL HARMONICS — REAL vs COMPLEX
================================================================================

The "textbook" spherical harmonics Y_lm are COMPLEX (they contain e^(imφ)).
These are eigenstates of L_z with a definite z-angular-momentum mℏ. But complex
functions are hard to draw, and their |Y|² is azimuthally symmetric (a ring or
shell), not the familiar lobes.

Chemists therefore use REAL spherical harmonics, formed by combining +m and −m:

    real, m > 0:  ∝ (Y_{l,−m} + (−1)^m Y_{l,m}) ∝ P_l^m(cosθ) · cos(mφ)
    real, m < 0:  ∝ (Y_{l,−m} − (−1)^m Y_{l,m}) ∝ P_l^m(cosθ) · sin(|m|φ)

These are the p_x, p_y, d_xy, d_z² … orbitals you see in textbooks. The code
implements exactly this mapping:

    m > 0 :  angular *= √2 · cos(m·φ)
    m < 0 :  angular *= √2 · sin(|m|·φ)

CONSEQUENCE (crucial for Section 12): the code DISPLAYS real orbitals (nice
lobes) but the probability current it ANIMATES belongs to the COMPLEX orbital.
A real orbital is an equal mix of +m and −m, whose currents cancel to zero.
This is a genuine inconsistency between the shape shown and the motion shown.


================================================================================
10.  NODES: READING THE SHAPE OF AN ORBITAL
================================================================================

    Radial nodes    = n − l − 1     (spherical shells of zero density)
    Angular nodes   = l             (planes/cones of zero density)
    Total nodes     = n − 1

Examples to try in the visualiser:
    (1,0,0)  1s   — no nodes, a solid ball.
    (2,0,0)  2s   — one radial node: a ball inside a shell.
    (2,1,0)  2pz  — one angular node: two lobes along z.
    (3,2,0)  3dz² — two angular nodes: the famous "donut + lobes."
    (4,3,x)  4f   — rich lobe structure; a good stress test.

Turning on the cross-section (cutaway) is the best way to SEE radial nodes,
which are otherwise hidden inside the outer shells.


================================================================================
11.  FROM |ψ|² TO PARTICLES: SAMPLING AND JACOBIANS
================================================================================

We want points distributed as |ψ|². The subtlety is the VOLUME ELEMENT.

In Cartesian coordinates, dV = dx dy dz. In spherical:

    dV = r² sin θ · dr dθ dφ.

The extra factors r² and sin θ are the JACOBIAN of the coordinate change. They
are NOT optional — they encode that there is "more room" at large r and near
the equator than near the origin or the poles.

So the correct 1D probability distributions are:

    P(r)  dr  ∝  r² · R(r)²          dr      ← r² Jacobian
    P(θ) dθ  ∝  sin θ · |Θ(θ)|²      dθ      ← sin θ Jacobian
    P(φ) dφ  ∝  |Φ(φ)|²             dφ

The code includes both Jacobians correctly (verts in the radial CDF use r*r*R*R;
the polar CDF uses sin θ * Y*Y). Forgetting the r² is the classic bug that makes
everyone's first orbital plot pile up wrongly at the origin — this code avoids
it.

INVERSE-CDF sampling then works because the CDF F(x) = ∫ P is monotonic from 0
to 1. If u is uniform on [0,1], then F⁻¹(u) is distributed as P. The code builds
F as a cumulative sum over 2000 slices and inverts it with a binary search
(std::lower_bound). This is O(N log RESOLUTION) and vastly more efficient than
rejection sampling, which discarded >99% of trials for diffuse clouds.


================================================================================
12.  THE PROBABILITY CURRENT — WHAT IT REALLY IS
================================================================================

This is the most conceptually rich — and most easily misrepresented — piece of
physics in the project.

WHAT THE CURRENT IS (real physics)
----------------------------------
The quantum probability current density is:

    J  =  (ℏ / mₑ) · Im(ψ* ∇ψ)

It is the quantum analogue of a fluid flow: it satisfies a continuity equation
∂|ψ|²/∂t + ∇·J = 0, so it literally describes how probability "flows."

For a COMPLEX hydrogen eigenstate ψ_nlm, the radial and polar parts are real,
so they contribute nothing to Im(ψ*∇ψ). Only the e^(imφ) factor survives,
producing a PURELY AZIMUTHAL current circling the z-axis:

    J_φ  =  (ℏ m / (mₑ · r sin θ)) · |ψ|²

This is real, non-trivial physics: a complex m ≠ 0 orbital carries a genuine,
steady circulation of probability around the axis. That circulating charge is a
current loop, which produces a magnetic moment — THIS is the deep reason m is
called the "magnetic" quantum number. The author's core intuition is correct
and, frankly, more sophisticated than most educational visualisers attempt.

THREE CRUCIAL CAVEATS
---------------------
(a) It exists only for COMPLEX orbitals. The code shows REAL orbitals (for
    their nice lobes). A real orbital is an equal superposition of +m and −m;
    its two counter-rotating currents cancel to EXACTLY ZERO net flow. So the
    shape shown and the motion shown belong to different representations.

(b) In a stationary state, the DENSITY does not move. |ψ_nlm|² is
    time-independent by definition — that is what "stationary" means. The
    current J is non-zero but STEADY: probability circulates through an
    unchanging density, like water going round a closed loop that looks
    perfectly still. A faithful render of a single eigenstate's density would
    therefore be completely static. The swirling is a depiction of the current
    FIELD, not of anything a density-observer would see move.

(c) The code's speed law is simplified. The true angular velocity of the flow
    is dφ/dt ∝ m / (r² sin²θ). The code uses dφ/dt ∝ m / (r sin θ) — missing a
    factor of r sin θ. This is almost certainly deliberate: the true law goes
    singular near the axis (sin θ → 0) and near the origin (r → 0), producing
    violent, numerically unstable, ugly speeds. The softened law is an artistic
    compromise.

HONEST FRAMING
--------------
The animation is a legitimate and valuable EVOCATION of a real, usually-ignored
phenomenon (m-driven circulation, the root of orbital magnetism). It is NOT a
literal depiction of the displayed orbital's motion. The right response is
honesty, not deletion: label it clearly as "a stylised depiction of the
probability current associated with mₗ," and note that (i) the density itself is
stationary and (ii) the shown real orbitals have zero NET current while the
current depicted corresponds to the complex state. With that framing, it teaches
something true and rarely shown.


================================================================================
13.  PHYSICAL CONSTANTS AND ATOMIC UNITS IN THE CODE
================================================================================

The code works in ATOMIC UNITS: it sets a₀ = 1 (Bohr radius), ℏ = 1, mₑ = 1,
and drops physical prefactors. This is standard in atomic physics — it strips
the equations of clutter so the STRUCTURE is visible. Distances are measured in
Bohr radii; the "radius" you zoom to scales with n².

Z is the nuclear charge (1 for hydrogen). The sampler threads Z through the
radial scaling ρ = 2rZ/n, but evaluateDensity currently ignores it — a bug
that is harmless only because Z is always 1 in practice (see README_FIXES.md).

Because everything is in atomic units, the "speed" of the probability flow and
the "time step" dt are not calibrated to real seconds. They are visualisation
parameters, not physical time — another reason to frame the animation as
stylised.
