#pragma once

#include <string>

const std::string empty_vertexshader_code = R"<shadercode>(
#version 330 core

// Vertex Attributes
layout(location = 0) in vec3 vertex_p;
layout(location = 4) in vec2 vertex_t;

// Output data ; will be interpolated for each fragment.
out vec2 out_t;

void main() {
	gl_Position =  vec4(vertex_p, 1.);
    out_t = vertex_t;
})<shadercode>";

const std::string vertically_flip_position_vertexshader_code = R"<shadercode>(
#version 330 core

// Vertex Attributes
layout(location = 0) in vec3 vertex_p;
layout(location = 4) in vec2 vertex_t;

// Output data ; will be interpolated for each fragment.
out vec2 out_t;

void main() {
	gl_Position =  vec4(vertex_p * vec3(1., -1., 1.), 1.); // Scale y by -1
    out_t = vertex_t;
})<shadercode>";

const std::string texcoord_as_positon_with_tangents_vertexshader_code = R"<shadercode>(
#version 330 core

// Vertex Attributes
layout(location = 0) in vec3 vertex_p;
layout(location = 1) in vec3 vertex_n;
layout(location = 2) in vec3 vertex_g;
layout(location = 3) in vec3 vertex_b;
layout(location = 4) in vec2 vertex_t;

// Output data ; will be interpolated for each fragment.
out vec2 out_t;
// Multiply a normal from the normalmap by this matrix
// to get the normal in model space
out mat3 ngb_matrix; // normal-tangent-bitangent

void main() {
	// Output position is texture coordinate
    gl_Position = vec4(fract(vertex_t)*2.0 - vec2(1.0, 1.0), 0.5, 1.);
	
    ngb_matrix = mat3(vertex_n * 2.0 - vec3(1., 1., 1.),
                      vertex_g * 2.0 - vec3(1., 1., 1.),
                      vertex_b * 2.0 - vec3(1., 1., 1.));
    out_t = vertex_t;
})<shadercode>";

const std::string snow_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
in mat3 ngb_matrix;

out float gl_FragDepth;

uniform sampler2D diff_texture;
uniform sampler2D norm_texture;
uniform sampler2D metallic_texture;

void main() {
    float geometry_normal_y_component = ngb_matrix[0][1];
    if (geometry_normal_y_component < 0.3) {
        // Do not generate snow where the geometry is steep
        // On the upper edge of bricks, the normal map would make the fragments appear inclined horizontally.
        gl_FragDepth = 0.; //color = vec3(0., 0, 0.5);
        return;
    }
    
    vec3 normalmap_color = texture2D(norm_texture, out_t).rgb;
    vec3 normalmap_vector = vec3(0.0, normalmap_color.x*(-2.0) + 1.0, normalmap_color.y*(-2.0) + 1.0);
    // Calculate the blue component (missing in norm_texture)
    normalmap_vector.x = 1 - normalmap_vector.y * normalmap_vector.y - normalmap_vector.z * normalmap_vector.z;
    vec3 norm_vector = ngb_matrix * normalmap_vector;

    float normal_y = clamp(norm_vector.y, 0., 0.95);
    if (texture2D(diff_texture, out_t).a < 0.1) {
        // No snow on transparent parts
        normal_y = 0.;
    }
    // Save normal_y to texture. Only one channel needed.
    gl_FragDepth = normal_y;
})<shadercode>";

const std::string copy_from_texture_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
uniform sampler2D diff_texture;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = texture2D(diff_texture, out_t);
})<shadercode>";


const std::string combine_to_snowed_textures_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;

// Input textures
uniform sampler2D snowmap;
uniform sampler2D noise;

uniform sampler2D diff_texture;
uniform sampler2D metallic_texture;

// Output targets
layout(location = 0) out vec4 diff_output;
layout(location = 2) out vec4 metallic_output;

void main() {
    ivec2 tex_coord = ivec2(floor(out_t * vec2(textureSize(diff_texture, 0))));
    vec4 diff_color     = texelFetch(diff_texture, tex_coord + ivec2(1., 0.), 0);
    vec4 metallic_color = texture2D(metallic_texture, out_t);

    float noise_value = texture2D(noise, out_t*4.).r;

    float normal_y = 0.;
    if (texture2D(snowmap, out_t).r < 0.98) { // snowmap is 1 at the parts not used by the mesh
        for (int x_off=-1; x_off<2; x_off++) {
            for (int y_off=-1; y_off<2; y_off++) {
                float new_color = texelFetch(snowmap, tex_coord + ivec2(x_off, y_off), 0).r;
                if (new_color > normal_y && new_color < 0.98) {
                    normal_y = (normal_y + new_color) / 2.;
                }
            }
        }
    }

    // Calculate the likeliness of snow on this fragment, depending only on its inclination
    // With regards to the y-component of norm_vector:
    // Below 0.4  -> No snow
    // 0.4 to 0.8 -> Interpolate linearly
    // 0.8 to 1.0 -> Always snow

    // Make sure snow is not the same everywhere
    if (normal_y > 0.8) {
        // Snow covers original texture entirely
        float snow_offset = (noise_value - 0.5) * 0.0625;
        diff_color = clamp(vec4(0.755 + snow_offset, 0.791 + snow_offset, 0.806 + snow_offset, 1.), 0., 1.);
        metallic_color.rgb = vec3(0., 0., 0.);
    }
    else if (normal_y > 0.3) {
        // The final color is mixed from a (global constant) snow color
        // and some remainders of the original texture
        float snow_color_part = clamp((normal_y-0.4)*5. - noise_value*1., 0., 1.);
        float orig_color_part = 1. - snow_color_part;
        diff_color = vec4(0.755, 0.791, 0.806, 1.) * snow_color_part + diff_color * orig_color_part;
        metallic_color.rgb = metallic_color.rgb * orig_color_part;
    }
    
    diff_output     = diff_color;
    metallic_output = metallic_color; 
})<shadercode>";


const std::string simple_diff_to_screen_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
uniform sampler2D diff_texture;
out vec3 color;

void main() {
    color.rgb = texture2D(diff_texture, out_t).rgb;
})<shadercode>";

const std::string simple_diff_to_texture_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
uniform sampler2D diff_texture;
layout(location = 0) out vec3 diff_output;

void main() {
    vec4 diff_color = texture2D(diff_texture, out_t);
    if (diff_color.a == 0.) discard;
    diff_output = diff_color.rgb;//vec3(diff_color.a, diff_color.a, diff_color.a);//
})<shadercode>";


const std::string simple_matrix_transform_vertexshader_code = R"<shadercode>(
#version 330 core

// Vertex Attributes
layout(location = 0) in vec3 vertex_p;
layout(location = 4) in vec2 vertex_t;

// Transformation matrix
uniform mat4 transformation_matrix;

// Output data ; will be interpolated for each fragment.
out vec2 out_t;

void main() {
	gl_Position =  transformation_matrix * vec4(vertex_p, 1.);
    out_t = vertex_t;
})<shadercode>";

