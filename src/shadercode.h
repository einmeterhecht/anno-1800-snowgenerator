#pragma once

#include <string>
const std::string texcoord_as_position_vertexshader_code = R"<shadercode>(
#version 330 core

// Vertex Attributes
layout(location = 0) in vec3 vertex_p;
layout(location = 4) in vec2 vertex_t;

// Output data ; will be interpolated for each fragment.
out vec2 out_t;

void main(){
	gl_Position =  vec4(vertex_t*2.0 - vec2(1.0, 1.0), 0.5, 1.); //vec4(vertex_p.x*0.6, vertex_p.y*0.6, 0.5, 1.);
    out_t = vertex_t;
})<shadercode>";
const std::string texcoord_as_positon_2_vertexshader_code = R"<shadercode>(
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

void main(){
	// Output position is texture coordinate
    vec2 uv_as_st = vec2(vertex_t.x, 1 - vertex_t.y);
	gl_Position = vec4(vertex_t*2.0 - vec2(1.0, 1.0), 0.5, 1.); //vec4(vertex_p.x*0.6, vertex_p.y*0.6, 0.5, 1.);
	
    ngb_matrix = mat3(vertex_n * 2.0 - vec3(1., 1., 1.),
                      vertex_g * 2.0 - vec3(1., 1., 1.),
                      vertex_b * 2.0 - vec3(1., 1., 1.));
    out_t = vertex_t;//uv_as_st;
})<shadercode>";

const std::string clear_snowmap_fragmentshader_code= R"<shadercode>(
#version 330 core
in vec2 out_t;

layout(location = 0) out vec3 color; // Special! Used for saving the computed texture

void main(){
    color = vec3(0., 0., 0.);
})<shadercode>";

const std::string snow_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
in mat3 ngb_matrix;

layout(location = 0) out vec3 color; // Special! Used for saving the computed texture

uniform sampler2D diff_texture;
uniform sampler2D norm_texture;
uniform sampler2D metallic_texture;

void main(){
    vec3 normalmap_color = texture2D(norm_texture, out_t).rgb;
    vec3 normalmap_vector = vec3(0.0, normalmap_color.x*(-2.0) + 1.0, normalmap_color.y*(-2.0) + 1.0);
    // Calculate the blue component (missing in norm_texture)
    normalmap_vector.x = 1 - normalmap_vector.y * normalmap_vector.y - normalmap_vector.z * normalmap_vector.z;
    vec3 norm_vector = ngb_matrix * normalmap_vector;

    // Calculate the likeliness of snow on this fragment, depending only on its inclination
    // With regards to the y-component of norm_vector:
    // Below 0.4  -> No snow
    // 0.4 to 0.8 -> Interpolate linearly
    // 0.8 to 1.0 -> Always snow
    float snow_likeliness = clamp((norm_vector.y-0.4)*2.5, 0., 1.);
    if (texture2D(diff_texture, out_t).a < 0.1){
        snow_likeliness = 0.;
    }
    // Save snow likeliness to texture. Only one channel needed.
    color = vec3(snow_likeliness, 0., 0.5);
    //color = (norm_vector + vec3(1., 1., 1.) ) * 0.5;
    //color = vec3((vertical_component + 1.0) * 0.5, (vertical_component + 1.0) * 0.5, 1.0);
    //color = (normalmap_vector + vec3(1., 1., 1.) ) * 0.5;
})<shadercode>";
const std::string copy_from_texture_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;
uniform sampler2D diff_texture;
layout(location = 0) out vec4 frag_color;

void main(){
    vec3 diff_color = texture2D(diff_texture, out_t).rgb;
    frag_color.rgb = diff_color;
})<shadercode>";


const std::string combine_to_snowed_textures_fragmentshader_code = R"<shadercode>(
#version 330 core
in vec2 out_t;

// Input textures
uniform sampler2D snowmap;
uniform sampler2D noise;

uniform sampler2D diff_texture;
uniform sampler2D norm_texture;
uniform sampler2D metallic_texture;

// Output targets
layout(location = 0) out vec4 diff_output;
layout(location = 1) out vec4 normal_output;
layout(location = 2) out vec4 metallic_output;

void main(){
    ivec2 tex_coord = ivec2(round(out_t * vec2(textureSize(diff_texture, 0))));
    vec4 diff_color     = texture2D(diff_texture, out_t);
    vec4 norm_color     = texture2D(norm_texture, out_t);
    vec4 metallic_color = texture2D(metallic_texture, out_t);

    float noise_value = texture2D(noise, out_t).r;

    float snow_likeliness = 0;
    for (int x_off=-1; x_off<2; x_off++){
        for (int y_off=-1; y_off<2; y_off++){
            float new_color = texelFetch(snowmap, tex_coord + ivec2(x_off, y_off), 0).r;
            if (new_color > snow_likeliness){
                snow_likeliness = (snow_likeliness + new_color) / 2.;
            }
        }
    }


    if (snow_likeliness > noise_value || snow_likeliness > 0.9){
        metallic_color.rgb = vec3(0., 0., 0.);
        if (snow_likeliness > 0.9){
            // Snow covers original texture entirely

            // Make sure snow is not the same everywhere
            float snow_offset = (noise_value - 0.5) * 0.0625;
            diff_color = clamp(vec4(0.755 + snow_offset, 0.791 + snow_offset, 0.806 + snow_offset, 1.), 0., 1.);
        }
        else{
            // The final color is mixted from a (global constant) snow color
            // and some remainders of the original texture
            float snow_color_part = (0.5 + snow_likeliness * 0.5);
            float orig_color_part = 1. - snow_color_part;
            diff_color = vec4(0.755, 0.791, 0.806, 1.) * snow_color_part + diff_color * orig_color_part;
        }
    }
    
    diff_output     = diff_color;
    normal_output   = norm_color;
    metallic_output = metallic_color; 
})<shadercode>";
