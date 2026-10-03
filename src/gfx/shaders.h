#pragma once

#ifdef __EMSCRIPTEN__
    inline constexpr const char* GLSL_VERSION = "#version 300 es\nprecision highp float;\n";
#else
    inline constexpr const char* GLSL_VERSION = "#version 330 core\n";
#endif

// Shader bodies WITHOUT the #version line (prepended at compile time).
extern const char* VERT_SPHERE_BODY;
extern const char* FRAG_SPHERE_BODY;