#include "gfx/mesh.h"
#include "gfx/gl.h"
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Mesh buildSphereMesh(float radius, int stacks, int sectors){
    std::vector<float> verts; std::vector<unsigned int> idx;
    for (int i = 0; i <= stacks; ++i) {
        float phi = (float)M_PI * i / stacks;       
        for (int j = 0; j <= sectors; ++j) {
            float theta = 2.0f * (float)M_PI * j / sectors; 
            float x = std::sin(phi) * std::cos(theta);
            float y = std::cos(phi);
            float z = std::sin(phi) * std::sin(theta);
            verts.push_back(x * radius); verts.push_back(y * radius); verts.push_back(z * radius);
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
        }
    }
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < sectors; ++j) {
            unsigned int a = i * (sectors + 1) + j, b = a + (sectors + 1);
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    Mesh mesh; mesh.indexCount = (int)idx.size();
    glGenVertexArrays(1, &mesh.vao); glGenBuffers(1, &mesh.vbo); glGenBuffers(1, &mesh.ebo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo); glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo); glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glBindVertexArray(0); 

    glCheck("buildSphereMesh");
    return mesh;
}
