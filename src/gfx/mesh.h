#pragma once
#include "gfx/gl.h"

struct Mesh {
    GLuint vao = 0, vbo = 0, ebo = 0;
    int indexCount = 0;
};

Mesh buildSphereMesh(float radius, int stacks, int sectors);