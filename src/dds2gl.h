#pragma once
#include <string>
#include <filesystem>
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"
#include "../external/DirectXTex/DirectXTex.h"

std::wstring string_to_16bit_unicode_wstring(std::string input_string);
GLuint directx_image_to_gl_texture(const DirectX::Image * image);
GLuint dds_file_to_gl_texture(std::wstring dds_filepath);

int gl_texture_to_png_file(GLuint texture_id, std::filesystem::path filename, boolean append_extension);
void gl_texture_to_dds_mipmaps(GLuint texture_id, std::filesystem::path filename_until_mipmap_indication, size_t mipmap_count);
