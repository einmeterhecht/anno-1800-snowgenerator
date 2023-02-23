/*
Main program.

Written by Lirvan, 2022.

What this program does:
- First fetch a list of .cfg files located in the same directory.                 [-> filelist.h]
- Initialize some stuff related to GL                                             [-> gl_stuff.h]
- For each .cfg file:
  - Load the .cfg's xml using rapidxml                                            [-> CfgFile.h]
    - Everything besides <Models> will be ignored (decals, particles, cloth, ...)
  - Load all the resources used by this .cfg file:
    - .rdm meshes (using some code copied from Kskudliks rdm-obj converter)       [-> rdm2gl.h]
    - .dds textures (using DirectXTex library by Microsoft)                       [-> dds2gl.h]
  - For each Model of the .cfg file:
    - For each Material of the corresponding mesh (not the materials from the .cfg!):
      - Fetch the corresponding material of the .cfg file
      - Create a so-called "snowmap" that looks like the UV map but saves the likeliness for each pixel to have snow.
        - The snowmap is identified by the diffuse texture of the material
        - Materials using the same diffuse texture will share the snowmap
        - When the snowmap is first created, a black quad is rendered to it to make sure it is empty
      - Calculate the snowmap: The model is rendered to the snowmap using the standard pipeline, but:
        - The Vertexshader does not return the transformed-projected vertex position, but the vertex texture coordinate.
        - Fragmentshader gets the Y (Up) component of the normal (in model space) and stores the likeliness of snow for this fragment in the snowmap.
      - Render the snowmap to the screen so the user has something to look at. (It is flipped vertically due to DirectXs UV vs OpenGLs ST texcoord system)
  - Now, For each material of the .cfg:
    - Skip it, if the materials' textures have been saved by another material from this .cfg before
    - Skip it, if it isn't used by the mesh (= if there is no snowmap for it)
    - Create a render target: 3 output textures textures and a framebuffer
    - "Render" to the textures: Inputs are the snowmap, the original textures and a noise texture.
    - Fragmentshader will combine the input and write snowed versions of the input textures to the 3 output textures.
      - Simplified version of the algorithm:
	    snow_likeliness = normal_vector_of_this_fragment.y
        if ((snow_likeliness > noise) || (snow_likeliness > 0.9)) {
            diffuse  = vec4(0.755, 0.791, 0.806, 1.);
            metallic = vec4(0., 0., 0., original_metallic.a);
        }
    - The output textures are stored as .dds files; including as many mipmaps as the original had. The compression of the textures to BC7_UNORM takes extremely long.
- When done, print a list of .cfg files that were skipped
*/

#include <iostream>
#include <filesystem>
#include <vector>
#include <set>
#include <string>

#include "src/filelist.h"
#include "src/CfgFile.h"
#include "src/gl_stuff.h"
#include "src/shaders.h"
#include "src/dds2gl.h"
#include "src/snow_exception.h"
#include "src/cli_options.h"
#include "src/matrix2gl.h"

namespace fs = std::filesystem;
using namespace std;

int main(int argc, char *argv[])
{
    CliOptions cli_options = CliOptions(argc, argv, std::filesystem::path(__argv[0]).parent_path());

    vector<std::filesystem::path> target_files;
    if (cli_options.dir_to_parse.string().ends_with(".cfg")) {
        target_files.push_back(fs::path(cli_options.dir_to_parse));
    }
    else {
        try {
            if (cli_options.disable_filenamefilters) {
                get_file_list(cli_options.dir_to_parse, &target_files, &ends_in_cfg);
            }
            else {
                get_file_list(cli_options.dir_to_parse, &target_files, &is_old_world_cfg);
            }
            std::cout << "Found " << target_files.size() << " files: " << endl;
            for (int cfg_index = 0; cfg_index < target_files.size(); cfg_index++) {
                 std::cout << target_files.at(cfg_index).string() << endl;
            }
        }
        catch (std::filesystem::filesystem_error) {
            std::cout << "Error while parsing " << cli_options.dir_to_parse << "" << endl;
            if (!fs::exists(cli_options.dir_to_parse)) std::cout << "The specified directory does not exist." << endl;
        }
    }

    GlStuff context_gl = GlStuff();
    while (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while initializing" << endl;
    GLuint snow_program = compile_shaders_to_program(
        texcoord_as_positon_with_tangents_vertexshader_code, snow_fragmentshader_code);
    GLuint copy_texture_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, copy_from_texture_fragmentshader_code);
    GLuint combine_to_snowed_textures_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, combine_to_snowed_textures_fragmentshader_code);
    GLuint clear_snowmap_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, clear_snowmap_fragmentshader_code);
    GLuint render_isometric_program = compile_shaders_to_program(
        simple_matrix_transform_vertexshader_code, simple_diff_fragmentshader_code);

    while (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while compiling shaders" << endl;

    context_gl.load_square_vertexbuffer();
    context_gl.load_noise_texture(4096);
    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while loading shaders & data" << endl;

    vector<std::filesystem::path> error_files;
    vector<Texture> default_textures = load_default_textures();

    for (int cfg_index = 0; cfg_index < target_files.size(); cfg_index++) {
        string cfg_path = target_files.at(cfg_index).string();
        std::cout << "\n\n\nCfg file " << cfg_index + 1 << " / " << target_files.size() << endl;
        std::cout << cfg_path << endl;

        try {
            CfgFile cfg_file = CfgFile(cfg_path, &default_textures, cli_options);
            
            bool no_textures_to_save = true;
            for (auto& [texture_rel_path, texture] : cfg_file.all_textures) {
                if (texture.save_snowed_texture) no_textures_to_save = false;
            }
            
            if (no_textures_to_save) {
                std::cout << "No textures to generate snow for were found. Move on to next file." << endl;
                continue;
            }

            cfg_file.load_models_and_textures();
            if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while loading textures" << endl;

            map<string, GLuint> rendered_snowmaps{};
            map<string, GLuint> framebuffer_ids{};
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                HardwareRdm& mesh = cfg_file.cfg_models[i].mesh;
                for (int j = 0; j < mesh.materials_count; j++) {
                    int cfg_material_index = min(mesh.materials[j].index, cfg_file.cfg_models[i].cfg_materials.size() - 1);
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[cfg_material_index];

                    int width, height;

                    fs::path texture_out_path = cfg_material.textures[0]->out_path;
                    string texture_rel_path = cfg_material.textures[0]->rel_path;

                    // GLuint old_render_target_texture = render_target_texture;
                    if (!rendered_snowmaps.contains(texture_rel_path)) {
                        get_dimensions(cfg_material.textures[0]->texture_id, &width, &height);
                        
                        // Create render target
                        rendered_snowmaps[texture_rel_path] = create_empty_depth_texture(width, height);
                        framebuffer_ids[texture_rel_path] = create_framebuffer(0);
                        
                        GLuint depthrenderbuffer;
                        glGenRenderbuffers(1, &depthrenderbuffer);
                        glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
                        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
                        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);
                        
                        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, rendered_snowmaps[texture_rel_path], 0);
                        
                        is_framebuffer_ok();

                        // Clear snowmap
                        glUseProgram(clear_snowmap_program);
                        context_gl.bind_square_buffers();
                        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
                        context_gl.unbind_square_buffers();
                    }
                    else {
                        get_dimensions(rendered_snowmaps[texture_rel_path], &width, &height);
                    }

                    //std::cout << "MATERIA" << endl;

                    // Render a snowmap to pre-existing rendered_snowmaps[texture_rel_path]
                    /*GLint old_framebuffer;
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_framebuffer);
                    if (old_framebuffer != framebuffer_ids[texture_rel_path]) {
                        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_rel_path]); // Render to snowmap
                    }*/

                    glViewport(0, 0, width, height);
                    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_rel_path]); // Render to snowmap

                    // Render snowmap
                    glUseProgram(snow_program);
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_GREATER);
                    
                    /*glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_COLOR, GL_SRC_COLOR);
                    glBlendEquation(GL_MAX);*/

                    cfg_material.bind_textures(snow_program);

                    mesh.bind_buffers();
                    context_gl.bind_vertexformat(cfg_material.vertex_format, mesh.vertices_size);

                    glDrawElements(
                        GL_TRIANGLES,
                        mesh.materials[j].size,
                        mesh.get_corner_datatype(),
                        (void*)(mesh.materials[j].offset * mesh.corner_size)
                    );
                    context_gl.unbind_vertexformat(cfg_material.vertex_format);

                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while rendering" << endl;
                }
            }
            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                HardwareRdm& mesh = cfg_file.cfg_models[i].mesh;
                for (int j = 0; j < cfg_file.cfg_models[i].cfg_materials.size(); j++) {
                    int cfg_material_index = min(mesh.materials[j].index, cfg_file.cfg_models[i].cfg_materials.size() - 1);
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[cfg_material_index];
                    
                    bool all_textures_have_snow = true;
                    for (int k = 0; k < texture_types_count && all_textures_have_snow; k++)
                        all_textures_have_snow &= cfg_material.textures[k]->is_snow_generated;
                    
                    // Skip this material if all the textures were already saved by another material or if it hasn't been used by the mesh
                    if (all_textures_have_snow || (!rendered_snowmaps.contains(cfg_material.textures[0]->rel_path))) continue;

                    int width, height;
                    get_dimensions(cfg_material.textures[0]->texture_id, &width, &height);
                    
                    GLuint framebuffer_id = create_framebuffer(texture_types_count);                    
                    for (int k = 0; k < texture_types_count; k++) {
                        // get_dimensions(cfg_material->texture_ids[k], &width, &height);
                        // Assume all related textures to have the same dimensions
                        glFramebufferTexture(
                            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + k,
                            cfg_material.textures[k]->snowed_texture_id, 0);
                    }

                    glViewport(0, 0, width, height);
                    glUseProgram(combine_to_snowed_textures_program);
                    glDisable(GL_BLEND);

                    cfg_material.bind_textures(combine_to_snowed_textures_program);

                    GLuint texture_location_in_shader = glGetUniformLocation(combine_to_snowed_textures_program, "snowmap");
                    glActiveTexture(GL_TEXTURE3);
                    glBindTexture(GL_TEXTURE_2D, rendered_snowmaps[cfg_material.textures[0]->rel_path]);
                    glUniform1i(texture_location_in_shader, 3);

                    texture_location_in_shader = glGetUniformLocation(combine_to_snowed_textures_program, "noise");
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, context_gl.noise_texture);
                    glUniform1i(texture_location_in_shader, 4);

                    context_gl.bind_square_buffers();
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
                    context_gl.unbind_square_buffers();

                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while combining original texture and snowmap" << endl;
                    
                    for (int k = 0; k < texture_types_count; k++)
                        cfg_material.textures[k]->is_snow_generated = true;
                    glDeleteFramebuffers(1, &framebuffer_id);
                }
            }

            // Render model to screen so that the user has something to look at
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, context_gl.window_w, context_gl.window_h);
            glUseProgram(render_isometric_program);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            
            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                HardwareRdm& mesh = cfg_file.cfg_models[i].mesh;
                for (int j = 0; j < mesh.materials_count; j++) {
                    int cfg_material_index = min(mesh.materials[j].index, cfg_file.cfg_models[i].cfg_materials.size() - 1);
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[cfg_material_index];
                    
                    GLuint texture_location_in_shader = glGetUniformLocation(render_isometric_program, "diff");
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, cfg_material.textures[0]->snowed_texture_id);
                    glUniform1i(texture_location_in_shader, 0);

                    mesh.bind_buffers();
                    context_gl.bind_vertexformat(cfg_material.vertex_format, mesh.vertices_size);

                    GLuint matrix_location_in_shader = glGetUniformLocation(render_isometric_program, "transformation_matrix");
                    load_isometric_matrix(matrix_location_in_shader, 1.f / cfg_file.mesh_radius);

                    glDrawElements(
                        GL_TRIANGLES,
                        mesh.materials[j].size,
                        mesh.get_corner_datatype(),
                        (void*)(mesh.materials[j].offset* mesh.corner_size)
                    );

                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while rendering" << endl;

                    glfwSwapBuffers(context_gl.window);
                    glfwPollEvents();
                }
            }

            // Set Window title to the filename of the .cfg currently displayed
            glfwSetWindowTitle(context_gl.window, target_files.at(cfg_index).filename().string().c_str());

            for (auto& [texture_rel_path, texture] : cfg_file.all_textures) {
                if (texture.type == 1) {
                    // Do not save normalmaps
                    texture.is_snowed_version_saved = true;
                }
                else if (texture.is_snowed_version_saved) {
                    // std::cout << "Do not safe texture twice" << endl;
                } else if (texture.rel_path.find("default_model_") != string::npos) {
                    // std::cout << "Do not save the default texture " << cfg_constants::texture_names[k];
                }
                else if (!texture.save_snowed_texture) {
                    std::cout << "Do not save vanilla texture " << texture.abs_path << std::endl;
                    texture.is_snowed_version_saved = true;
                }
                else{
                    if (is_forbidden_texture(texture.rel_path)) {
                        // If filenamefilters are active, default textures are loaded instead of blacklisted ones.
                        // If the program reaches this points, filenamefilters are disabled.
                        std::cout << "Save blacklisted texture " << texture.out_path;
                    }
                    if (cli_options.save_png) {
                        gl_texture_to_png_file(texture.snowed_texture_id, texture.out_path, true);
                    }
                    if (cli_options.save_dds) {
                        gl_texture_to_dds_mipmaps(texture.snowed_texture_id, texture.out_path, texture.mipmap_count);
                    }
                    texture.is_snowed_version_saved = true;
                }
                if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while saving texture" << endl;
                texture.cleanup();
            }
        }
        catch (snow_exception exception) {
            std::cout << "Error processing file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << endl;
        }
        catch (rapidxml::parse_error exception) {
            std::cout << "Damaged cfg file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << endl;
        }
        catch (exception exception) {
            std::cout << "Uncaught exception in cfg file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << endl;
        }
        if (glfwWindowShouldClose(context_gl.window)) {
            std::cout << "Window was closed by user. Quit process." << endl;
            break;
        }
    }
    std::cout << std::endl << "Done. "
        << target_files.size() << " files processed, "
        << target_files.size() - error_files.size() << " successful." << endl;
    if (error_files.size() == 0) std::cout << "No errors" << endl;
    else {
        std::cout << "ERRORS in these " << error_files.size() << " files:" << endl;
        for (fs::path error_path : error_files) {
            std::cout << error_path.string() << endl;
        }
    }
    context_gl.cleanup();
    glfwTerminate();
    
    if (!cli_options.no_prompt) {
        // Let the user press enter to close window
        char* _ = new char[2];
        std::cin.getline(_, 2);
        delete[] _;
    }
}