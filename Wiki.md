# Hydrogen Orbital Visualiser — Project Wiki

A real-time, GPU-accelerated visualisation of the hydrogen atom's electron
probability cloud, running natively (desktop) and in the browser (WebAssembly
via Emscripten). It samples the quantum wavefunction |ψ_nlm|² and renders tens
of thousands of "pearls," each marking a point where the electron is likely to
be found, coloured by probability density and optionally animated to depict the
quantum probability current.

--------------------------------------------------------------------------------
TABLE OF CONTENTS
--------------------------------------------------------------------------------
  0.  Quick Start
  1.  The Rendering Pipeline — Conceptual Overview
  2.  The Shaders
  3.  GLUtils — Shader Compilation
  4.  Mesh — Sphere Geometry
  5.  Camera — Orbit Camera
  6.  QuantumMath — The Physics Core
  7.  HydrogenSampler (ParticleSystem) — Math into Points
  8.  Renderer / Engine
  9.  App, Callbacks, and Emscripten Bindings
  10. The HTML Presentation Layer
  11. Proposed File/Class Structure
  12. Known Issues & Dead Code
  13. Glossary


================================================================================
0.  QUICK START
================================================================================

Controls (native / keyboard):
    Arrow Up / Down     n  (principal quantum number — shell / energy)
    Arrow Right / Left  l  (azimuthal — orbital shape; 0..n-1)
    D / A               m  (magnetic — orientation; -l..+l)
    W / S               N  (particle count — double / halve)
    E / Q               dt (flow speed — faster / slower)
    Esc                 quit

Controls (browser):
    Sliders for n, l, m, particle count, time step
    Checkbox for cross-section (cutaway)
    Mouse drag / one-finger drag  = rotate
    Scroll / pinch                = zoom

The simulation resamples the cloud whenever you change n, l, m, or N.


================================================================================
1.  THE RENDERING PIPELINE — CONCEPTUAL OVERVIEW
================================================================================

The high-level data flow is:

    CPU (C++)  ──►  compute particle positions + colours from |ψ|²
       │
       │  upload to GPU buffers (VBOs)
       ▼
    GPU vertex shader   ──►  places each vertex in screen space
    GPU fragment shader ──►  colours each pixel
       │
       ▼
    Framebuffer  ──►  Window / <canvas>

The single most important performance technique in the whole project is
INSTANCING.

Naively, drawing 40,000 spheres would mean issuing 40,000 draw calls and
sending 40,000 copies of the sphere geometry to the GPU — catastrophically
slow. Instead, the author uploads:

    • ONE sphere mesh (shared geometry), and
    • ONE array of 40,000 (position, colour) pairs,

then issues a SINGLE instanced draw call:

    glDrawElementsInstanced(GL_TRIANGLES, indexCount, ..., N);

The GPU redraws the same sphere N times, each time reading the next
(position, colour) from the instance array. This is the difference between
"unusable" and "smooth 60 fps."

Modern OpenGL, distilled:
    • Create GPU buffers
    • Upload geometry
    • Write shaders
    • Draw


================================================================================
2.  THE SHADERS
================================================================================

Shaders are small programs that run ON THE GPU. There are two kinds relevant
here:

    • VERTEX SHADER   — runs once per vertex; decides where it lands on screen.
    • FRAGMENT SHADER — runs once per pixel; decides that pixel's colour.

The project defines two shader PAIRS: "sphere" (used) and "point" (dead code).


2a. SPHERE VERTEX SHADER  (VERT_SPHERE)
---------------------------------------

    layout(location = 0) in vec3 aPos;           // vertex on the unit sphere
    layout(location = 1) in vec3 aNormal;        // surface normal
    layout(location = 2) in vec3 aInstancePos;   // per-particle centre  ◄ instance
    layout(location = 3) in vec3 aInstanceColor; // per-particle colour  ◄ instance

The location numbers 0..3 are "attribute slots." The crucial distinction:

    • Locations 0 and 1 change ONCE PER VERTEX. They describe the base sphere's
      shape and are identical for every particle.
    • Locations 2 and 3 change ONCE PER INSTANCE. This is set on the CPU with
      glVertexAttribDivisor(2, 1) and glVertexAttribDivisor(3, 1) — the "1"
      means "advance this attribute once per sphere, not once per vertex."

The core transform:

    vec3 worldPos = (aPos * uScale) + aInstancePos;
    gl_Position   = projection * view * vec4(worldPos, 1.0);

Step by step:
    1. aPos * uScale       — shrink the unit sphere (uScale = 0.15, tiny pearls)
    2. + aInstancePos      — move it to this particle's location
    3. view                — transform world → camera space
    4. projection          — transform camera space → clip/screen space

This is the canonical:  gl_Position = projection · view · model · position.
Here the "model" matrix is replaced by a manual scale-and-translate — a valid
optimisation because every pearl uses uniform scaling and no rotation.

The shader passes fragPos, fragNormal, and particleColor to the fragment
shader. These are interpolated smoothly across each triangle by the GPU.


2b. SPHERE FRAGMENT SHADER  (FRAG_SPHERE)
-----------------------------------------

This creates the "soft glossy pearl" look. It is ARTISTIC lighting, not
physically-based rendering.

    if (uCutaway && fragPos.x > 0.0) discard;

The cutaway/cross-section: when enabled, any pixel with x > 0 is thrown away,
slicing the cloud in half so you can see the internal shell structure.
`discard` = "do not draw this pixel at all."

    float diff = dot(N, L) * 0.5 + 0.5;   // HALF-LAMBERT diffuse

Standard Lambert lighting is max(dot(N,L), 0), which makes surfaces facing away
from the light pure black. The "* 0.5 + 0.5" remap (a Valve / Half-Life 2
trick) keeps back-facing surfaces softly lit instead of black. This is why the
pearls look soft rather than harshly shadowed.

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3  rim     = mix(particleColor, vec3(1.0), 0.5) * fresnel;

A FAKE FRESNEL rim light. Real Fresnel: surfaces reflect more at grazing
angles. Here, when the surface normal N is nearly perpendicular to the view
direction V (i.e. at the silhouette edge of the sphere), fresnel → 1, adding a
bright whitish halo. This gives the pearls their glossy, glowing-edge quality.
Physically motivated, hand-tuned, not accurate.

    fragColor = vec4(particleColor * diff + rim, 1.0);

Final colour = shaded base colour + rim glow, fully opaque.


2c. POINT SHADERS  (VERT_POINT / FRAG_POINT)  —  DEAD CODE
---------------------------------------------------------

These are DEFINED BUT NEVER USED. They implement an alternative "ethereal mist"
render mode using GPU point sprites instead of sphere meshes.

    gl_PointSize = (uScale * 150.0) / -viewPos.z;   // closer = larger

Sizes each point by distance for correct perspective shrinking.

    float glow = exp(-12.0 * r2);                    // Gaussian falloff
    fragColor  = vec4(particleColor * glow * 1.5, glow * 0.15);  // low alpha

Draws a soft radial glow with low alpha, DESIGNED to be additively blended so
overlapping points accumulate into fog. However, the Engine calls
glDisable(GL_BLEND), so this path could not work as intended even if wired up.
It is a fossil of an earlier design — see Section 12.


================================================================================
3.  GLUTILS — SHADER COMPILATION
================================================================================

    static GLuint compileShader(GLenum type, const char* src);
    static GLuint buildProgram(const char* vSrc, const char* fSrc);

compileShader uploads GLSL source to the GPU and compiles it into a shader
object. buildProgram compiles a vertex + fragment shader, links them into a
usable "program object," and deletes the now-redundant intermediate shaders.

CRITICAL OMISSION (see Section 12): neither function checks for compile or link
errors. glGetShaderiv(..., GL_COMPILE_STATUS, ...) and glGetProgramiv are never
called. A single typo in a shader therefore produces a SILENT BLACK SCREEN with
no diagnostic. Adding error logging here is the single highest-value fix in the
project.


================================================================================
4.  MESH — SPHERE GEOMETRY
================================================================================

    struct Mesh { GLuint vao, vbo, ebo; int indexCount; };

Three GPU object handles:

    • VBO (Vertex Buffer Object)  — raw vertex data in GPU memory: positions
                                    and normals, interleaved.
    • EBO (Element Buffer Object) — indices describing which vertices form each
                                    triangle. Lets vertices be REUSED between
                                    adjacent triangles instead of duplicated.
    • VAO (Vertex Array Object)   — stores HOW to interpret the buffers (which
                                    bytes are position, which are normal, the
                                    stride, etc.). It stores the interpretation,
                                    NOT the data.

buildSphereMesh — UV-SPHERE GENERATION
--------------------------------------

    for (int i = 0; i <= stacks; ++i) {
        float phi = π * i / stacks;          // latitude: 0 → π (pole to pole)
        for (int j = 0; j <= sectors; ++j) {
            float theta = 2π * j / sectors;  // longitude: around the equator
            float x = sin(phi) * cos(theta);
            float y = cos(phi);
            float z = sin(phi) * sin(theta);

A classic "UV sphere": walk a grid of latitude (phi) × longitude (theta),
converting each grid point to a 3D position via spherical coordinates.

A neat property: on a UNIT sphere, the position (x, y, z) is ALSO the surface
normal. The code exploits this — it pushes the same values twice, once as
position (scaled by radius) and once as the normal.

The second loop stitches the grid into triangles. Each grid cell with corners
(a, b, a+1, b+1) becomes two triangles:

    a───a+1        triangle 1: a, b, a+1
    │ ╲ │          triangle 2: b, b+1, a+1
    b───b+1

Called as buildSphereMesh(1.0f, 12, 12): a low-poly sphere. That is fine
because each rendered pearl is only a couple of pixels wide — extra polygons
would be invisible and wasteful.


================================================================================
5.  CAMERA — ORBIT CAMERA
================================================================================

An ORBIT CAMERA (arcball): the camera always looks at the origin and moves on
the surface of an imaginary sphere around it.

    float radius, azimuth, elevation;   // camera's spherical position
    float targetRadius;                  // for smooth zoom easing

State is stored in SPHERICAL coordinates. position() converts to Cartesian:

    vec3( radius·sin(e)·cos(azimuth),
          radius·cos(e),
          radius·sin(e)·sin(azimuth) )

viewMatrix() calls glm::lookAt(position, origin, up) to build the matrix that
transforms world space into camera space.

Input handling:
    • onMouseMove — changes azimuth/elevation by the mouse delta while dragging.
    • elevation is CLAMPED to [0.01, π − 0.01]. This prevents flipping over the
      poles, where the "up" vector becomes ambiguous and the view snaps
      violently.
    • SMOOTH ZOOM via the targetRadius/radius split: scrolling sets a target,
      and the main loop eases the actual radius toward it each frame:
          radius += (targetRadius − radius) * 0.05;
      The comment "no more snaps" refers to this — instant zoom feels jarring,
      especially on touch devices.


================================================================================
6.  QUANTUMMATH — THE PHYSICS CORE
================================================================================

(For the deep mathematical treatment, see README_PHYSICS.md. This section
covers what each function DOES within the program.)

The wavefunction being modelled:

    ψ_nlm(r, θ, φ) = R_nl(r) · Y_lm(θ, φ)

    n (principal) — shell / energy level.  E_n = −13.6 / n²  eV.
    l (azimuthal) — orbital shape.          0 ≤ l ≤ n−1.
    m (magnetic)  — orientation.            −l ≤ m ≤ +l.

assocLaguerre(n, alpha, x)
    Computes the associated Laguerre polynomial via the stable three-term
    recurrence. This polynomial lives in the RADIAL part and is responsible for
    the RADIAL NODES — the concentric spherical shells where density vanishes.

sphLegendre(l, m, theta)
    Computes the NORMALISED associated Legendre function P_l^m(cos θ). This is
    the backbone of the ANGULAR part and creates the ANGULAR NODES — the lobes
    and nodal planes that distinguish s, p, d, f orbitals.

evaluateDensity(x, y, z, n, l, m, Z)
    Assembles |ψ|² at a Cartesian point:
        1. Convert (x,y,z) → spherical (r, θ, φ).
        2. Radial part:  R = N · e^(−ρ/2) · ρ^l · L(ρ),  ρ = 2r/n.
        3. Angular part: sphLegendre, then the REAL spherical harmonic mapping:
               m > 0 :  × √2 · cos(mφ)
               m < 0 :  × √2 · sin(|m|φ)
           (Real orbitals give the recognisable lobed shapes — p_x, p_y, etc.)
        4. Return ψ².
    NOTE: this function contains two known bugs (dropped Z; wrong Gamma
    argument) that are masked by relative colour normalisation. See Section 12
    and README_PHYSICS.md.

heatmapInferno(t)
    A piecewise-linear colour ramp (black → purple → red → orange → white)
    approximating the "inferno" colourmap. Maps a normalised density in [0,1]
    to an RGB colour so denser regions glow hotter.


================================================================================
7.  HYDROGENSAMPLER (ParticleSystem) — MATH INTO POINTS
================================================================================

THE SAMPLING STRATEGY — INVERSE-CDF SAMPLING
--------------------------------------------

To scatter N particles distributed ACCORDING TO |ψ|², the author uses inverse
transform sampling, exploiting the fact that the distribution factorises into
three INDEPENDENT 1D distributions (radial, polar, azimuthal).

For each of r, θ, φ:
    1. DISCRETISE the axis into RESOLUTION (=2000) slices.
    2. COMPUTE the 1D PDF at each slice — with the correct JACOBIANS:
           radial   PDF ∝  r² · R(r)²        (r² = volume element)
           polar    PDF ∝  sin θ · |Y|²      (sin θ = solid-angle element)
           azimuth  PDF ∝  |Φ(φ)|²
    3. ACCUMULATE into a running total → a CDF (a monotonically increasing
       array ending at 1.0 after normalisation).

To sample a value: roll a uniform u ∈ [0,1], then std::lower_bound finds where
u sits in the CDF; that index maps back to a value of r (or θ, φ). Regions
where the PDF is large occupy more of the CDF's range and are therefore hit
proportionally more often. This is exactly correct.

Each particle stores BOTH:
    • pos          — Cartesian, for rendering.
    • (r, θ, φ)    — spherical, kept for the flow animation (Section below).

Particles are coloured by their local density via heatmapInferno.

THE OLD METHOD (commented out): sampleWaveFunctionVonNeumannRejectionSampling
threw random points into a box and kept each with probability proportional to
density. Correct, but for a diffuse cloud in a large box it rejects >99% of
points. The switch to inverse-CDF is a real improvement, and the author
documents the reasoning honestly in the comments.

updateProbabilityCurrent(m, dt) — THE ANIMATION
-----------------------------------------------

Each frame, advances every particle's azimuthal angle φ:

    p.phi += (m / (p.r · sinθ)) · dt;
    p.pos  = rebuild Cartesian from (r, θ, new φ);

Design note: it ONLY changes φ, then rebuilds position from the ORIGINAL r and
θ. This locks each particle to its sampled shell permanently — particles orbit
the vertical (y) axis at fixed radius and latitude, faster where |m| is larger,
and not at all when m = 0.

The physical meaning, subtleties, and honest caveats of this animation are
covered in depth in README_PHYSICS.md (Section: The Probability Current).


================================================================================
8.  RENDERER / ENGINE
================================================================================

Engine owns the window, the GL context, the sphere mesh, the instance buffers,
and the draw calls.

    drawParticlesInstanced(particles, scale)
        • Copies particle positions/colours into flat arrays.
        • Re-uploads them each frame with GL_DYNAMIC_DRAW (they move).
        • Issues ONE glDrawElementsInstanced call.

    drawProton(pos, color, scale)
        • Draws a single instanced sphere at the origin (currently disabled at
          the call site).

Engine also builds the projection matrix from the live framebuffer size, so the
view stays correct through window resizes.


================================================================================
9.  APP, CALLBACKS, AND EMSCRIPTEN BINDINGS
================================================================================

State containers (currently three overlapping ones — see Section 11 for the
proposed consolidation):
    • OrbitalState — n, l, m, N, dt, cutaway, resample flag.
    • Camera       — camera position/target.
    • AppState     — global pointers tying it all together (gApp).

GLFW callbacks read the window "user pointer" to reach the shared state, then
forward events to the camera or mutate the orbital parameters.

clampQuantumNumbers enforces the physical rules l < n and |m| ≤ l (with the n
limit deliberately left off "for fun").

EMSCRIPTEN_KEEPALIVE functions (setN, setL, setM, setParticleCount, setSpeed,
setCutaway, rotateCamera, zoomCamera, resizeViewport) expose the controls to
JavaScript so the HTML UI can drive the C++ engine.

mainLoopIteration — the heartbeat:
    poll input → resample if parameters changed → ease camera zoom →
    advance the flow → clear → set uniforms → draw → swap buffers.

On desktop this runs in a while loop; under Emscripten it is registered with
emscripten_set_main_loop to sync with the browser's animation frames.


================================================================================
10. THE HTML PRESENTATION LAYER
================================================================================

index.html provides:
    • An intro overlay explaining the concept and an "Initialize" button.
    • A control panel: sliders for n, l, m, particle count, and time step; a
      cross-section checkbox; a collapsible info panel; a music toggle.
    • JavaScript that:
        – wraps the exported C++ functions with Module.cwrap;
        – enforces the quantum rules in the UI (l ≤ n−1, |m| ≤ l) so sliders
          stay physical;
        – handles touch gestures (one finger = rotate, pinch = zoom) mapped to
          rotateCamera / zoomCamera;
        – adapts canvas resolution to device pixel ratio.

NOTE: some UI labels are physically misleading (e.g. "m (Magnetic Spin)" — m is
the magnetic ORBITAL number, unrelated to spin). See README_FIXES.md.


================================================================================
11. PROPOSED FILE / CLASS STRUCTURE
================================================================================

The project currently lives in one translation unit. The struct boundaries
already imply a clean split:

    File                 Responsibility
    ─────────────────    ────────────────────────────────────────────────────
    GLUtils.h/.cpp       compileShader, buildProgram, shader sources,
                         + shader error checking (to be added)
    Mesh.h/.cpp          Mesh, buildSphereMesh
    Camera.h/.cpp        orbit camera + input + smoothing
    QuantumMath.h/.cpp   assocLaguerre, sphLegendre, evaluateDensity,
                         heatmapInferno — pure, OpenGL-free, unit-testable
    HydrogenSampler.*    ParticleSystem sampling + updateProbabilityCurrent
    Renderer.h/.cpp      Engine draw methods + uniform handling
    App.h/.cpp           AppState, main loop, callbacks, Emscripten bindings

Recommended cleanups regardless of the split:
    • Add shader compile/link error logging.
    • Cache uniform locations once at link time (not per frame).
    • Collapse the three overlapping state containers into one App object.
    • Make the static positions/colors vectors into Engine members.
    • Break the ~90-line sampler into named sub-functions.


================================================================================
12. KNOWN ISSUES & DEAD CODE
================================================================================

Physics (detail in README_PHYSICS.md, fixes in README_FIXES.md):
    • evaluateDensity ignores Z (hard-codes ρ = 2r/n) while the sampler uses Z.
    • Normalisation constant uses Γ(n+l) instead of Γ(n+l+1) — off by (n+l).
    • maxDensity estimated by uniform box sampling — biased/noisy for high n,l.
    • The probability-flow speed law is simplified (∝ 1/(r·sinθ) rather than
      the true 1/(r²·sin²θ)) and depicts a current on REAL orbitals, which have
      zero net current. Artistically motivated; presentationally overstated.

Engineering:
    • No shader error checking (silent black screen on typos).
    • VERT_POINT / FRAG_POINT and drawProton are dead / disabled code.
    • Uniform locations queried every frame.
    • Three overlapping state containers can drift out of sync.
    • ES3 shaders (#version 300 es) requested against a desktop GL 3.3 context —
      works on many drivers but not guaranteed; branch on __EMSCRIPTEN__.

UI:
    • "m (Magnetic Spin)" mislabels the magnetic ORBITAL quantum number.
    • Intro text presents the stylised flow as literal physical motion.


================================================================================
13. GLOSSARY
================================================================================

    VBO         Vertex Buffer Object — vertex data in GPU memory.
    VAO         Vertex Array Object — stored interpretation of vertex data.
    EBO         Element Buffer Object — triangle indices.
    Instancing  Drawing one mesh many times in a single call.
    Shader      GPU program (vertex = geometry, fragment = pixels).
    GLFW        Library for windows, input, and GL context creation.
    GLEW        OpenGL function loader (loads modern GL entry points).
    GLM         Header-only math library for graphics.
    Emscripten  Compiles C++ to WebAssembly for the browser.
    Wavefunction  ψ — complex amplitude whose |ψ|² is a probability density.
    Orbital     A single-electron wavefunction ψ_nlm.
    Node        A surface where ψ = 0 (density vanishes).
