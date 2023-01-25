#include "matrix2gl.h"

void load_isometric_matrix(GLuint position_in_shader, float scale){
	float s = scale;
	float transformation_matrix[16] = {
		s *  0.707107f,  s * 0.408248f, 0.1 * 0.577350f,  0.0f,
		s *  0.0f,       s * 0.816497f, 0.1 * -0.577350f,  0.0f,
		s * -0.707107f,  s * 0.408248f, 0.1 * 0.577350f,  0.0f,
		s * 0.0f,        s * -1.2f,  0.0f,  1.0f
	};
	for (int i = 0; i < 16; i++) {
		transformation_matrix[i] = transformation_matrix[i] * scale;
	}
	glUniformMatrix4fv(position_in_shader, 1, GL_FALSE, &transformation_matrix[0]);
}