#include "matrix2gl.h"

void load_isometric_matrix(GLuint position_in_shader, float scale){
	float _transformation_matrix[16] = {
		0.866025f, 0.0f, -0.866025f, 0.0f,
		-0.5f,     1.0f, -0.5f,      0.0f,
		1.0f,      1.0f, 1.0f,       0.0f,
		0.0f,      0.0f, 0.0f,       1.0f
	};
	float transformation_matrix[16] = {
		0.866025f, -0.5f, 1.0f, 0.0f,
		0.0f,     1.0f, 1.0f,      0.0f,
		-0.866025f,      -0.5f, 1.0f,       0.0f,
		0.0f,      0.0f, 0.0f,       1.0f
	};
	for (int i = 0; i < 16; i++) {
		transformation_matrix[i] = transformation_matrix[i] * scale;
	}
	glUniformMatrix4fv(position_in_shader, 1, GL_FALSE, &transformation_matrix[0]);
}