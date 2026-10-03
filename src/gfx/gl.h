// This will be our central place to decide OpenGl implementation.
#pragma once

#ifdef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1 
#endif
#ifndef GLFW_INCLUDE_ES3
#define GLFW_INCLUDE_ES3
#endif
#include <GLFW/glfw3.h>
#include <iostream>


// Debug-only GL error check.

inline void glCheck(const char* where) {
#ifndef NDEBUG
    for (GLenum e; (e = glGetError()) != GL_NO_ERROR; )
        std::cerr << "[GL ERROR] 0x" << std::hex << e << std::dec
                  << " at " << where << "\n";
#else
    (void)where;
#endif
}