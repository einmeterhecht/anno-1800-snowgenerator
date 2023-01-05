#pragma once

#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"

struct Material {
    uint32_t offset;
    uint32_t size;
    uint32_t index;
};

class HardwareRdm
{
public:
    HardwareRdm();
	
    ~HardwareRdm();
    int load_rdm(std::filesystem::path input_path);
    void bind_buffers();
    void cleanup();

    void print_information();
    GLenum get_corner_datatype();

    uint32_t vertices_size = 0; // byte size
    uint32_t vertices_count = 0;

    uint32_t corner_size = 0;  // byte size
    uint32_t corner_count = 0; // count of corners (=count of triangles * 3)

    std::vector<Material> materials;
    uint32_t materials_count = 0;

    GLuint vertexbuffer = GLuint(0);
    GLuint indexbuffer = GLuint(0);
};

