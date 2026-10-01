# Fixes, Corrections & Roadmap

This document lists (A) concrete bugs to fix, (B) the vision for turning this
into a genuine educational tool, and (C) three major feature directions:
SONIFICATION (hearing the wavefunction), EDUCATIONAL OVERLAYS, and
SUPERPOSITION STATES.

--------------------------------------------------------------------------------
CONTENTS
--------------------------------------------------------------------------------
  A.  Bug Fixes (do these first)
  B.  Honesty Fixes (labels & framing)
  C.  Engineering Cleanups
  D.  Feature 1 — Sonification: Hearing the Wavefunction
  E.  Feature 2 — Educational Overlays
  F.  Feature 3 — Superposition States
  G.  Suggested Priority Order


================================================================================
A.  BUG FIXES (DO THESE FIRST)
================================================================================

A1. evaluateDensity ignores Z.
    CURRENT:  double rho = (2.0 * r) / n;
    FIX:      double rho = (2.0 * r * Z) / n;
    WHY:      Positions come from the CDF (which uses Z) but colours come from
              evaluateDensity (which doesn't). They disagree for Z ≠ 1.
              Harmless today only because Z is always 1.

A2. Wrong Gamma argument in the normalisation constant.
    CURRENT:  std::tgamma(n + l)          // = (n+l−1)!
    FIX:      std::tgamma(n + l + 1)      // = (n+l)!
    WHY:      The correct constant is √[(2/n)³ · (n−l−1)! / (2n·(n+l)!)].
              Off by a factor of (n+l). Invisible now (colours are relative),
              but wrong and would corrupt any absolute-density comparison.

A3. maxDensity estimated by biased uniform box sampling.
    CURRENT:  10,000 uniform samples in a big cube; take the max.
    PROBLEM:  For high n, l the density lives in thin shells/lobes; uniform
              sampling almost always misses the true peak, so maxDensity is
              underestimated → colours saturate → dynamic range is lost.
    FIX:      Estimate maxDensity over the ALREADY-SAMPLED particles (they
              cluster exactly where density is high):
                  for (auto& p : particles)
                      maxDensity = max(maxDensity,
                                       evaluateDensity(p.pos..., n,l,m,Z));
              One loop, no wasted randoms, tracks the real peak.

A4. Shader compile/link errors are silent.
    FIX:      After glCompileShader / glLinkProgram, query GL_COMPILE_STATUS /
              GL_LINK_STATUS and print glGet*InfoLog on failure. A typo
              currently yields a black screen with no message — this is the
              highest-value single fix in the project.


================================================================================
B.  HONESTY FIXES (LABELS & FRAMING)
================================================================================

For an EDUCATIONAL tool, mislabelling is worse than a rendering glitch — it
teaches the wrong thing. These are cheap and important.

B1. "m (Magnetic Spin)" → "m (Magnetic — orbital)".
    m_l is the magnetic ORBITAL quantum number. It has nothing to do with spin
    (which is a separate quantum number, m_s = ±½, not modelled here). As
    written, the UI actively teaches a misconception.

B2. Reframe the probability flow.
    Current intro text: "a physical momentum wave configuration driven directly
    by the magnetic quantum numbers." This overstates it. Replace with, e.g.:

        "The swirl is a STYLISED depiction of the quantum probability CURRENT —
         a real, steady circulation of probability that a complex m ≠ 0 orbital
         carries around its axis (the origin of orbital magnetism). Note: the
         probability DENSITY itself does not move in a single orbital; the
         shapes shown are 'real' orbitals whose net current is zero. The motion
         illustrates the current field, not literal movement of the cloud."

    (Full physics rationale in README_PHYSICS.md, Section 12.)

B3. Clarify that dt / "speed" is not physical time.
    Everything is in atomic units and the flow law is softened; label the slider
    "Animation speed (stylised)," not "Time step (physical)."


================================================================================
C.  ENGINEERING CLEANUPS
================================================================================

C1. Split into the modules proposed in README.md, Section 11.
C2. Cache uniform locations once at link time (a UniformCache struct), instead
    of calling glGetUniformLocation every frame.
C3. Collapse OrbitalState / WindowState / AppState into a single App object;
    pass &app as the GLFW user pointer. Removes duplicate pointers that can
    drift out of sync.
C4. Make the static positions/colors vectors in drawParticlesInstanced into
    Engine members (they are a hidden singleton today).
C5. Break sampleWaveFunctionCDF into named helpers:
        buildRadialCDF(), buildPolarCDF(), buildAzimuthalCDF(),
        estimateMaxDensity(), drawSamples().
    Each becomes independently testable — especially valuable for the physics.
C6. Delete or wire up the dead point-sprite path and drawProton. Dead code that
    LOOKS load-bearing is a tax on every future reader.
C7. Branch the GLSL #version string on __EMSCRIPTEN__ (ES 3.00 for web, a
    desktop-appropriate version natively) rather than requesting ES3 against a
    desktop 3.3 core context.


================================================================================
D.  FEATURE 1 — SONIFICATION: HEARING THE WAVEFUNCTION
================================================================================

The intuition is excellent: humans parse WAVES most naturally through SOUND,
and the hydrogen atom is, at heart, a system of standing waves. This is not a
gimmick — there are several physically-honest ways to map the atom to audio.

D0. Why this is legitimate (not arbitrary).
    The energy levels of hydrogen ARE a spectrum, literally. Atomic spectral
    lines are frequencies. Mapping quantum transitions to audible pitches is a
    faithful analogy, not a stretch: we are just shifting the same relational
    structure down into the range human ears can hear.

D1. MAP ENERGY LEVELS TO PITCH (the cleanest, most honest mapping).
    E_n = −13.6 eV / n². Emission/absorption frequencies come from DIFFERENCES:

        f_transition ∝ (1/n_low² − 1/n_high²)     (the Rydberg formula)

    Idea: assign each accessible level a tone. When the user changes n, play the
    TRANSITION tone f ∝ |1/n_old² − 1/n_new²|, scaled into the audible range
    (say 200–2000 Hz). Moving up the ladder literally sounds like climbing the
    hydrogen spectrum. This is the single most physically defensible sonification
    and I'd build it first.

D2. MAP THE PROBABILITY CURRENT TO A DRONE / ROTATION RATE.
    The m-driven circulation has a characteristic angular frequency. Map |m| (or
    the mean rotation rate of the particles) to:
        • a low-frequency oscillation (LFO) modulating a drone, or
        • the stereo PANNING rate — the sound literally circles the listener as
          the cloud swirls, so you HEAR the current's handedness (sign of m).
    Sign of m → direction of pan rotation (clockwise vs anticlockwise). This
    turns an abstract quantum number into a spatial audio sensation.

D3. MAP RADIAL NODE STRUCTURE TO TIMBRE (harmonics).
    A wavefunction with more radial nodes is a "higher harmonic" of the radial
    standing wave — directly analogous to overtones on a vibrating string. Map:
        • number of radial nodes (n−l−1)  → number of harmonics in the timbre,
        • number of angular nodes (l)     → brightness / spectral tilt.
    Then a 1s sounds like a pure sine (fundamental only), while a high-n, high-l
    orbital sounds rich and bright. The AUDIO complexity mirrors the VISUAL
    complexity, reinforcing the node-counting intuition from README_PHYSICS §10.

D4. GRANULAR "DENSITY AUDIO" (advanced, most immersive).
    Treat each particle as a tiny grain of sound (granular synthesis). Grain
    density ∝ local |ψ|², grain pitch ∝ radial shell. The listener hears a
    texture whose "thickness" tracks where probability concentrates. Combined
    with the swirl-panning of D2, moving through the cloud becomes an audio-
    tactile experience of probability density.

D5. IMPLEMENTATION NOTES (browser).
    • Use the Web Audio API (native to the browser build) — no C++ audio needed.
    • Expose the current mean rotation rate and node counts from C++ via new
      EMSCRIPTEN_KEEPALIVE getters (e.g. getMeanOmega(), getRadialNodes()).
    • Drive oscillators/panners in JavaScript from those values each frame.
    • Keep it OPT-IN and gentle (the existing music toggle is a good home).
    • CAVEAT for honesty: label it "sonification / analogy," and note that the
      absolute pitches are scaled for hearing, not literal atomic frequencies
      (which are in the UV/visible, ~10¹⁵ Hz).


================================================================================
E.  FEATURE 2 — EDUCATIONAL OVERLAYS
================================================================================

Turn the pretty toy into a teaching instrument by SURFACING the physics the
code already computes.

E1. Orbital name + spectroscopic label.
    Map (n, l) → "4d", "3p", "1s", etc. (l = 0,1,2,3 → s,p,d,f; then g,h,…).
    Display prominently. Instantly connects the sliders to chemistry class.

E2. Live node counts.
    Show:  radial nodes = n−l−1,  angular nodes = l,  total = n−1.
    Let the user VERIFY the counting rule by eye (cutaway on for radial nodes).
    This is one of the most satisfying "aha" moments in QM.

E3. Energy readout.
    E_n = −13.6 eV / n². Show the value and, optionally, a little energy-level
    ladder with the current level highlighted. Ties directly into the
    sonification of D1.

E4. Angular momentum readout.
    |L| = ℏ√(l(l+1)),  L_z = mℏ. Optional, for more advanced users.

E5. A "guided tour" mode.
    Scripted sequence: 1s → 2s (see the first radial node appear) → 2p (first
    angular node) → 3d → 4f, with a sentence of narration at each step. Pairs
    beautifully with the sonification (each step plays its transition tone).

E6. On-hover / tap definitions.
    The existing info panel is a start, but its current text is vague (and, per
    B1, partly wrong). Replace with the crisp definitions from README_PHYSICS
    §3, and add the node/energy formulae.

E7. A phase-colouring toggle (bridges to the physics of nodes).
    Currently nodes appear only as "less dense" regions, which undersells them.
    Add an optional mode colouring particles by sign(ψ) (e.g. blue for +, red
    for −). Nodal surfaces then appear as sharp colour boundaries — the single
    most illuminating way to SEE the wave nature. (This also sets up
    superposition visualisation below.)


================================================================================
F.  FEATURE 3 — SUPERPOSITION STATES
================================================================================

This is where the tool could become genuinely special — and where it stops
being static and starts being TRULY quantum-dynamical.

F0. Why it matters.
    A single eigenstate is stationary: its density never moves (README_PHYSICS
    §12). The interesting, time-dependent quantum behaviour — the electron
    "sloshing" — only appears in SUPERPOSITIONS of different-energy states. This
    is real, observable physics (it underlies chemical bonding, spectroscopy,
    and quantum beats), and it is visually stunning.

F1. The physics.
    Let the state be a mix of two eigenstates:

        Ψ(t) = c₁ ψ_{n₁l₁m₁} e^(−iE₁t/ℏ) + c₂ ψ_{n₂l₂m₂} e^(−iE₂t/ℏ)

    The density is:

        |Ψ(t)|² = |c₁ψ₁|² + |c₂ψ₂|²
                  + 2 Re(c₁c₂* ψ₁ψ₂*) · cos((E₁−E₂)t/ℏ + Δphase)

    The first line is static; the CROSS TERM oscillates at the BOHR FREQUENCY:

        ω = (E₁ − E₂)/ℏ  ∝  (1/n₂² − 1/n₁²).

    So the cloud physically breathes/sloshes back and forth at ω. Choose, e.g.,
    1s + 2p_z and the density oscillates up and down the axis — a textbook
    "quantum beat" and the seed of how atoms radiate light.

F2. Implementation sketch.
    • Extend the state to hold TWO (n,l,m) triples plus complex weights c₁, c₂
      (amplitude + relative phase sliders).
    • The density is no longer separable, so pure inverse-CDF per-axis won't
      work directly. Two viable routes:
        (i)  Resample periodically from |Ψ(t)|² using rejection or a 3D-grid
             CDF at a few phase snapshots, interpolating between them; or
        (ii) Keep a fixed particle set and re-WEIGHT/re-colour it each frame by
             the live |Ψ(t)|² (cheaper; particles stay put but brightness
             breathes) — a good first version.
    • Time now becomes PHYSICAL (proportional to real Bohr periods), so the
      "speed" slider finally has an honest meaning here.

F3. The payoff, tied to the other features.
    • VISUAL: the cloud sloshes — real quantum dynamics, not a stylised swirl.
    • SONIC (links to D): map the Bohr frequency ω directly to an audible beat/
      pitch. The user HEARS the superposition's oscillation while SEEING it —
      wave interference made simultaneously visible and audible. This is the
      strongest realisation of your "waves as music" intuition, because here the
      thing oscillating is genuinely a frequency, honestly mapped.
    • EDUCATIONAL (links to E): overlay ω and the transition energy; connect to
      why atoms emit light at specific colours (the same 1/n² differences the
      sonification plays).


================================================================================
G.  SUGGESTED PRIORITY ORDER
================================================================================

    1. A4 (shader error logging)            — unblocks all future debugging.
    2. A1, A2, A3 (physics correctness)     — cheap, makes the tool trustworthy.
    3. B1, B2, B3 (honesty of labels)       — essential for an EDU tool.
    4. E1, E2, E3 (basic overlays)          — high teaching value, low effort;
                                              uses data already computed.
    5. D1 (energy→pitch sonification)       — the cleanest, most honest audio.
    6. E7 (phase colouring)                 — makes nodes visible; sets up F.
    7. F2(ii) (weight-reweight superposition) — first taste of real dynamics.
    8. D2/D3 (swirl-pan + timbre audio)     — deepens the sonification.
    9. F2(i) + F3 (full superposition + beat audio) — the flagship feature.
    10. C1–C7 (refactor) — ideally BEFORE 7–9, or interleaved, so the codebase
        can absorb the bigger features cleanly.
