#include "gl_stuff.h"

#include <iostream>
#include <string>
#include <algorithm>

#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"

/* Most of the code in this file was taken from:
https://github.com/opengl-tutorials/ogl/tree/master/tutorial14_render_to_texture
(The example code of https://www.opengl-tutorial.org/ )*/

GlStuff::GlStuff(bool init) {
	glfwInit();
	glfwWindowHint(GLFW_SAMPLES, 16);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(800, 800, "Window providing Context for GL", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window.");
		glfwTerminate();
		status = 1;
	}

	glfwMakeContextCurrent(window);

	glfwGetFramebufferSize(window, &window_w, &window_h);
	glViewport(0, 0, window_w, window_h);
	glClearColor(0.1, 0.1, 0.4, 1.0);

	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW.");
		glfwTerminate();
		status = 1;
	}
	while (glGetError() != GL_NO_ERROR) {} // Delete all the errors generated during GLEW initialisation

	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwPollEvents();

	glGenVertexArrays(1, &global_vertex_array);
	glBindVertexArray(global_vertex_array);
	status = 0;
}

void get_dimensions(GLuint texture_id, int* width, int* height) {
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,height);
}

GLuint create_empty_texture(int width, int height, GLenum format, GLenum mag_filter, GLenum min_filter, GLenum wrap_method) {
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_method);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_method);
	return texture_id;
}

GLuint create_empty_depth_texture(int width, int height) {
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return texture_id;
}

GLuint create_framebuffer(uint8_t number_of_drawbuffers) {
	/*GLint max_viewport_size[2];
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, &max_viewport_size[0]);
	std::cout << "Maximal snowmap size: " << ((int)max_viewport_size[0]) << "; " << ((int)max_viewport_size[1]) << std::endl;*/
	GLuint framebuffer_id;
	glGenFramebuffers(1, &framebuffer_id);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);

	// Set the list of draw buffers.
	GLenum draw_buffers[4] = {
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3};
	if (number_of_drawbuffers > 0) glDrawBuffers(number_of_drawbuffers, draw_buffers);
	else glDrawBuffer(GL_NONE);

	return framebuffer_id;
}

bool is_framebuffer_ok() {
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cout << "Failed to initialize render_target" << std::endl;
		return false;
	}
	return true;
}

void GlStuff::bind_vertexformat(std::string vertex_format, uint32_t vertices_size)
{
	bind_or_unbind_vertexformat(vertex_format, vertices_size, false);
}

void GlStuff::unbind_vertexformat(std::string vertex_format)
{
	bind_or_unbind_vertexformat(vertex_format, 0 /*does not matter*/, true);
}

GLenum get_gl_datatype_for_rdm_datatype(char rdm_datatype) {
	switch (rdm_datatype) {
	case 'f':
		return GL_FLOAT;
		break;
	case 'h':
		return GL_HALF_FLOAT;
		std::cout << "HALF FLOAT" << std::endl;
		break;
	case 'b':
		return GL_UNSIGNED_BYTE;//Actually, this will probably be UNORM (0x00 -> 0.0; 0xFF -> 1.0)
	default:
		return GL_UNSIGNED_BYTE;
		std::cout << "Not f, h, or b" << std::endl;
	}
}

uint8_t get_bytesize_for_rdm_datatype(char rdm_datatype) {
	switch (rdm_datatype) {
	case 'f':
		return 4;
		break;
	case 'h':
		return 2;
		break;
	case 'b':
		return 1;
		break;
	default:
		return 1;
	}
	std::cout << "UNREACHABLE" << std::endl;
}

GLenum is_vertex_attribute_normalized(char vertex_attribute) {
	switch (vertex_attribute) {
	case 'N':
	case 'G':
	case 'B':
		return GL_TRUE;
		break;
	default:
		return GL_FALSE;
	}
}

void GlStuff::bind_or_unbind_vertexformat(std::string vertex_format, uint32_t vertices_size, bool unbind)
{
    //std::cout << "Vertexformat: " << vertex_format << std::endl;
	uint32_t offset_to_attribute_in_vertex_format_string = 0;
	uint32_t offset_to_attribute_in_buffer = 0;

	while (offset_to_attribute_in_vertex_format_string != vertex_format.length() + 1) {
		std::string attribute_description = vertex_format.substr(offset_to_attribute_in_vertex_format_string,
			vertex_format.find('_', offset_to_attribute_in_vertex_format_string)- offset_to_attribute_in_vertex_format_string);

		if (attribute_description.length() == 3) { // e.g. P4h
			char    attribute_name     = attribute_description[0]; // e.g. P
			uint8_t attribute_size     = ((uint8_t) attribute_description[1]) - 48; // e.g. 4
			char    attribute_datatype = attribute_description[2]; // e.g. h


			// The position of the attribute_name inside the constant string VERTEX_ATTRIBUTES
			// determines the attribute_id_in_shader

			uint32_t attribute_id_in_shader = std::string(VERTEX_ATTRIBUTES).find(attribute_name);
			// If the attribute occurs multiple times in vertex_format (e.g. P4h_N4b_G4b_B4b_T2h_C4b_C4b)
			// the first C4b gets attribute_id_in_shader=5; the second C4b attribute_id_in_shader=6.
			for (int i = 0; i < offset_to_attribute_in_vertex_format_string; i++) {
				if (vertex_format[i] == attribute_name) attribute_id_in_shader++;
			}
			if (unbind) glDisableVertexAttribArray(attribute_id_in_shader);
			else{
			    // Tell OpenGl about this Attribute
				glEnableVertexAttribArray(attribute_id_in_shader);
				glVertexAttribPointer(
					attribute_id_in_shader,                                   // attribute
					attribute_size,                                           // size
					get_gl_datatype_for_rdm_datatype(attribute_datatype),     // type
					is_vertex_attribute_normalized(attribute_name),           // normalized?
					vertices_size,                                            // stride
					(void*)offset_to_attribute_in_buffer                      // array buffer offset
				);
				/*
				std::cout << "Attribute: " << attribute_name << ": " << attribute_id_in_shader << std::endl;
				std::cout << "Size: "      << attribute_size + 0 << std::endl;
				std::cout << "Datatype: "  << get_gl_datatype_for_rdm_datatype(attribute_datatype) << std::endl;
				std::cout << "Normalized? "<< is_vertex_attribute_normalized(attribute_name) << std::endl;
				std::cout << "Formatsize: "<< vertices_size << std::endl;
				std::cout << "Offset: "    << offset_to_attribute_in_buffer << std::endl;
				*/
				offset_to_attribute_in_buffer += (
					attribute_size * get_bytesize_for_rdm_datatype(attribute_datatype));
			}
		}
		else {
			// Unknown attributes (e.g. the '37' in P3f_N3b_37_T2f)

			offset_to_attribute_in_buffer += std::stoi(attribute_description);
		}
		offset_to_attribute_in_vertex_format_string += attribute_description.length() + 1;
	}
}


int GlStuff::load_square_vertexbuffer() {
	static const GLfloat square_vertexbuffer_data[] = {
		/* Interleaved: Alternating Position and Texture coordinate (In Anno: P3fT2f) */
		-1.0f, -1.0f, 0.0f,
		 0.0f,  0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  0.0f,
		-1.0f,  1.0f, 0.0f,
		 0.0f,  1.0f,
		-1.0f,  1.0f, 0.0f,
		 0.0f,  1.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  0.0f,
		 1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f
	};
	static const GLuint square_indexbuffer_data[] = {
		/* Indexbuffer for the above */
		0, 1, 2,
		3, 4, 5
	};

	glGenBuffers(1, &square_vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, square_vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER,
		sizeof(square_vertexbuffer_data),
		square_vertexbuffer_data,
		GL_STATIC_DRAW);

	glGenBuffers(1, &square_indexbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, square_indexbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		sizeof(square_indexbuffer_data),
		square_indexbuffer_data,
		GL_STATIC_DRAW);

	bind_vertexformat("P3f_T2f", 20);
	return 0;
}
void GlStuff::load_noise_texture(int sidelength)
{
	constexpr float rand_max = float(RAND_MAX);
	float* noise_buffer = new float[sidelength * sidelength];
	for (int i = 0; i < sidelength * sidelength; i++) {
		noise_buffer[i] = (rand() / rand_max);
	}
	glGenTextures(1, &noise_texture);
	glBindTexture(GL_TEXTURE_2D, noise_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, sidelength, sidelength, 0, GL_RED, GL_FLOAT, (void*)noise_buffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR on noise param" << std::endl;
	delete[] noise_buffer;
}
void GlStuff::bind_square_buffers()
{
	glBindBuffer(GL_ARRAY_BUFFER, square_vertexbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, square_indexbuffer);
	bind_vertexformat("P3f_T2f", 20);
}
void GlStuff::unbind_square_buffers()
{
    unbind_vertexformat("P3f_T2f");
}
void GlStuff::cleanup()
{
	glDeleteTextures(1, &noise_texture);
	glDeleteBuffers(1, &square_vertexbuffer);
	glDeleteBuffers(1, &square_indexbuffer);
}
GlStuff::~GlStuff() {}
