#pragma once
#include <string>
#include <random>
#include "gl_stuff.h"
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"

inline const int WINDOW_WIDTH = 800;
inline const int WINDOW_HEIGHT = 800;

inline const char* VERTEX_ATTRIBUTES = "PNGBTCCIIIIWWWW";
#define MAX_VERTEX_ATTRIBUTES 15; // length of VERTEX_ATTRIBUTES

GLuint create_empty_texture(int width, int height, GLenum format, GLenum mag_filter, GLenum min_filter, GLenum wrap_method);
GLuint create_empty_depth_texture(int width, int height);
void get_dimensions(GLuint texture_id, int* width, int* height);
GLuint create_framebuffer(uint8_t number_of_drawbuffers);
bool is_framebuffer_ok();

class GlStuff{
public:
    GLFWwindow* window;
    GLuint global_vertex_array;
    GLuint square_vertexbuffer = GLuint(0);
    GLuint square_indexbuffer = GLuint(0);
    GLuint noise_texture = GLuint(0);
    int window_w;
    int window_h;

    int status;

    GlStuff(bool init=true);
    ~GlStuff();

    void bind_or_unbind_vertexformat(std::string vertex_format, uint32_t vertices_size, bool unbind);
    void bind_vertexformat(std::string vertex_format, uint32_t vertices_size);
    void unbind_vertexformat(std::string vertex_format);

    int load_square_vertexbuffer();
    void load_noise_texture(int sidelength);

    void bind_square_buffers();
    void unbind_square_buffers();

    void cleanup();
};
