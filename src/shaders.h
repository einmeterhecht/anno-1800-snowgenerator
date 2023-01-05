#pragma once
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"
#include <string>
#include "shadercode.h"

GLuint compile_shaders_to_program(const std::string vertexshader_code, const std::string fragmentshader_code);
GLuint compile_compute_shader_to_program(const std::string computeshader_code);
GLuint compile_shader(std::string shader_code, GLenum type);
