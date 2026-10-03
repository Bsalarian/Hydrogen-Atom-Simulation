// gfx/shaders.cpp
#include "gfx/shaders.h"
#include <iostream>

const char* const VERT_SPHERE_BODY = R"glsl(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aInstancePos;
layout(location = 3) in vec3 aInstanceColor;

uniform mat4 view;
uniform mat4 projection;
uniform float uScale;

out vec3 fragPos;
out vec3 fragNormal;
out vec3 particleColor;

void main() {
    vec3 worldPos = (aPos * uScale) + aInstancePos;
    fragPos       = worldPos;
    fragNormal    = aNormal;
    particleColor = aInstanceColor;
    gl_Position   = projection * view * vec4(worldPos, 1.0);
}
)glsl";

const char* const FRAG_SPHERE_BODY = R"glsl(
in vec3 fragPos;
in vec3 fragNormal;
in vec3 particleColor;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool uCutaway;

out vec4 fragColor;

void main() {
    if (uCutaway && fragPos.x > 0.0) discard;

    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos - fragPos);

    float diff    = dot(N, L) * 0.5 + 0.5;              // half-Lambert wrap
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3  rim     = mix(particleColor, vec3(1.0), 0.5) * fresnel;

    fragColor = vec4(particleColor * diff + rim, 1.0);
}
)glsl";

static GLuint compileShader(GLenum type, const char* body) {
    const char* sources[2] = { GLSL_VERSION, body };
    GLuint s = glCreateShader(type);
    glShaderSource(s, 2, sources, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "[SHADER] " << (type == GL_VERTEX_SHADER ? "vertex" : "fragment")
                  << " compile failed:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint buildProgram(const char* vertBody, const char* fragBody) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertBody);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragBody);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "[SHADER] link failed:\n" << log << "\n";
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}
