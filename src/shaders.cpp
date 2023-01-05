#include "shaders.h"
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"
#include <vector>
#include <iostream>


GLuint compile_shader(const std::string shader_code, GLenum type)
{
    // type is GL_VERTEX_SHADER, GL_FRAGMENT_SHADER or GL_COMPUTE_SHADER
    GLuint shader_id = glCreateShader(type);

    const char* shader_code_pointer = shader_code.c_str();

    glShaderSource(shader_id, 1, &shader_code_pointer, NULL);
    glCompileShader(shader_id);

    // Check Shader
    GLint check_result = GL_FALSE;
    int info_log_length;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &check_result);
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> error_message(info_log_length + 1);
        glGetShaderInfoLog(shader_id, info_log_length, NULL, &error_message[0]);
        std::cout << "Shader compilation failed. Message:\n " << &error_message[0] << std::endl;
        std::cout << "Shader code:\n\n" << shader_code << "\n\n" << std::endl;
    }
    while (glGetError() != GL_NO_ERROR) std::cout << "ERROR while compiling shader" << std::endl;

    return shader_id;
}

GLuint compile_shaders_to_program(const std::string vertexshader_code, const std::string fragmentshader_code){
    GLuint vertex_shader = compile_shader(vertexshader_code, GL_VERTEX_SHADER);
    GLuint fragment_shader = compile_shader(fragmentshader_code, GL_FRAGMENT_SHADER);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, vertex_shader);
    glAttachShader(shader_program, fragment_shader);
    glLinkProgram(shader_program);

    // Check the program
    GLint check_result = GL_FALSE;
    int info_log_length;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &check_result);
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> error_message(info_log_length + 1);
        glGetProgramInfoLog(shader_program, info_log_length, NULL, &error_message[0]);
        std::cout << "Shader program linking failed. Message: " << &error_message[0] << std::endl;
    }

    glDetachShader(shader_program, vertex_shader);
    glDetachShader(shader_program, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return shader_program;
}

GLuint compile_compute_shader_to_program(const std::string computeshader_code)
{
    GLuint compute_shader = compile_shader(computeshader_code, GL_COMPUTE_SHADER);

    GLuint shader_program = glCreateProgram();
    glAttachShader(shader_program, compute_shader);
    glLinkProgram(shader_program);

    // Check the program
    GLint check_result = GL_FALSE;
    int info_log_length;
    glGetProgramiv(shader_program, GL_LINK_STATUS, &check_result);
    glGetProgramiv(shader_program, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
        std::vector<char> error_message(info_log_length + 1);
        glGetProgramInfoLog(shader_program, info_log_length, NULL, &error_message[0]);
        std::cout << "Shader program linking failed. Message: " << &error_message[0] << std::endl;
    }

    glDetachShader(shader_program, compute_shader);
    glDeleteShader(compute_shader);

    return shader_program;
}