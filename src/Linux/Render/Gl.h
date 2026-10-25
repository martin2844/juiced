#pragma once
#ifdef __ANDROID__
#include <GLES3/gl3.h>
#elif defined(__linux__)
#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#else
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif
#include <string>
namespace oj::gles {
std::string platformSource(std::string source);
GLuint compile(GLenum kind, const std::string &source);
GLuint link(GLuint vertex, GLuint fragment);
} // namespace oj::gles
