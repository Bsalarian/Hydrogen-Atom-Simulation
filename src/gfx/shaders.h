#pragma once
#include "gfx/gl.h"

#ifdef __EMSCRIPTEN__
inline constexpr const char* GLSL_VERSION = "#version 300 es\nprecision highp float;\n";
#else
inline constexpr const char* GLSL_VERSION = "#version 330 core\n";
#endif

extern const char* const VERT_SPHERE_BODY;
extern const char* const FRAG_SPHERE_BODY;

// Returns 0 if anything fails
GLuint buildProgram(const char* vertBody, const char* fragBody);