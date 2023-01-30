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
        if ((snow_likeliness > noise) || (snow_likeliness > 0.9)){
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
    if (cli_options.disable_filenamefilters){
        get_file_list(cli_options.dir_to_parse, &target_files, &ends_in_cfg);
    }
    else {
        get_file_list(cli_options.dir_to_parse, &target_files, &is_old_world_cfg);
    }

    GlStuff context_gl = GlStuff();
    while (glGetError() != GL_NO_ERROR) cout << "GL ERROR while initializing" << endl;
    GLuint snow_program = compile_shaders_to_program(
        texcoord_as_positon_2_vertexshader_code, snow_fragmentshader_code);
    GLuint copy_texture_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, copy_from_texture_fragmentshader_code);
    GLuint combine_to_snowed_textures_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, combine_to_snowed_textures_fragmentshader_code);
    GLuint clear_snowmap_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, clear_snowmap_fragmentshader_code);
    GLuint render_isometric_program = compile_shaders_to_program(
        simple_matrix_transform_vertexshader_code, simple_diff_fragmentshader_code);

    while (glGetError() != GL_NO_ERROR) cout << "GL ERROR while compiling shaders" << endl;

    context_gl.load_square_vertexbuffer();
    context_gl.load_noise_texture(4096);
    if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while loading shaders & data" << endl;

    cout << "Found " << target_files.size() << " files: " << endl;
    for (int cfg_index = 0; cfg_index < target_files.size(); cfg_index++) {
         cout << target_files.at(cfg_index).string() << endl;
    }

    vector<std::filesystem::path> error_files;
    vector<Texture> default_textures = std::vector<Texture>();

    for (int cfg_index = 0; cfg_index < target_files.size(); cfg_index++) {
        string cfg_path = target_files.at(cfg_index).string();
        cout << "\n\n\nCfg file " << cfg_index + 1 << " / " << target_files.size() << endl;
        cout << cfg_path << endl;

        try {
            CfgFile cfg_file = CfgFile(cfg_path, &default_textures, cli_options);
            cfg_file.load_models_and_textures();
            if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while loading textures" << endl;

            map<string, GLuint> rendered_snowmaps{};
            map<string, GLuint> framebuffer_ids{};
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                HardwareRdm& mesh = cfg_file.cfg_models[i].mesh;
                //mesh.print_information();
                /*min(mesh.materials_count, cfg_file.cfg_models[i].cfg_materials.size())*/
                for (int j = 0; j < mesh.materials_count; j++) {
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[min(
                        *((unsigned long long *) &(mesh.materials[j].index)), cfg_file.cfg_models[i].cfg_materials.size() - 1)];

                    int width, height;

                    fs::path texture_out_path = cfg_material.textures[0]->out_path;
                    string texture_rel_path = cfg_material.textures[0]->rel_path;

                    // GLuint old_render_target_texture = render_target_texture;
                    if (!rendered_snowmaps.contains(texture_rel_path)) {
                        get_dimensions(cfg_material.textures[0]->texture_id, &width, &height);
                        // Create render target
                        rendered_snowmaps[texture_rel_path] = create_empty_texture(
                            width, height, GL_RGB, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE);
                        framebuffer_ids[texture_rel_path] = create_framebuffer(1);
                        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rendered_snowmaps[texture_rel_path], 0);
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

                    //cout << "MATERIA" << endl;

                    // Render a snowmap to rendered_snowmaps[texture_rel_path]
                    /*GLint old_framebuffer;
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_framebuffer);
                    if (old_framebuffer != framebuffer_ids[texture_rel_path]) {
                        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_rel_path]); // Render to snowmap
                    }*/

                    // Render model to screen so that the user has something to look at
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(0, 0, context_gl.window_w, context_gl.window_h);

                    glUseProgram(render_isometric_program);
                    
                    GLuint texture_location_in_shader = glGetUniformLocation(render_isometric_program, "diff");
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, cfg_material.textures[0]->texture_id);
                    glUniform1i(texture_location_in_shader, 0);

                    mesh.bind_buffers();
                    context_gl.bind_vertexformat(cfg_material.vertex_format, mesh.vertices_size);

                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LESS);
                    GLuint matrix_location_in_shader = glGetUniformLocation(render_isometric_program, "transformation_matrix");
                    load_isometric_matrix(matrix_location_in_shader, 0.4);

                    glDrawElements(
                        GL_TRIANGLES,
                        mesh.materials[j].size,
                        mesh.get_corner_datatype(),
                        (void*)(mesh.materials[j].offset*mesh.corner_size)
                    );

                    if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while rendering" << endl;

                    glfwSwapBuffers(context_gl.window);
                    glfwPollEvents();


                    glViewport(0, 0, width, height);
                    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_rel_path]); // Render to snowmap

                    // Render snowmap
                    glUseProgram(snow_program);
                    cfg_material.bind_textures(snow_program);

                    // Buffers of mesh do not have to be bound again

                    glDrawElements(
                        GL_TRIANGLES,
                        mesh.materials[j].size,
                        mesh.get_corner_datatype(),
                        (void*)(mesh.materials[j].offset * mesh.corner_size)
                    );
                    context_gl.unbind_vertexformat(cfg_material.vertex_format);

                    if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while rendering" << endl;
                }
            }
            set<GLuint> saved_textures = set<GLuint>();
            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                for (int j = 0; j < cfg_file.cfg_models[i].cfg_materials.size(); j++) {
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[j];
                    bool already_saved_all_textures = true;
                    for (int k = 0; k < texture_types_count && already_saved_all_textures; k++)
                        already_saved_all_textures &= saved_textures.contains(cfg_material.textures[k]->texture_id);
                    // Skip this material if all the textures were already saved by another material or if it hasn't been used by the mesh
                    if (already_saved_all_textures || (!rendered_snowmaps.contains(cfg_material.textures[0]->rel_path))) continue;

                    int width, height;
                    get_dimensions(cfg_material.textures[0]->texture_id, &width, &height);
                    GLuint framebuffer_id = create_framebuffer(texture_types_count);
                    GLuint snowed_textures[texture_types_count];
                    for (int k = 0; k < texture_types_count; k++) {
                        // get_dimensions(cfg_material->texture_ids[k], &width, &height);
                        // Assume all related textures to have the same dimensions
                        snowed_textures[k] = create_empty_texture(
                            width, height, GL_RGBA,
                            GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE);
                        glFramebufferTexture(
                            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + k,
                            snowed_textures[k], 0);
                    }

                    glViewport(0, 0, width, height);
                    glUseProgram(combine_to_snowed_textures_program);
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

                    if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while combining original texture and snowmap" << endl;
                    for (int k = 0; k < texture_types_count; k++) {
                        if (k == 1) saved_textures.insert(cfg_material.textures[k]->texture_id); // Do not save normalmaps
                        if (saved_textures.contains(cfg_material.textures[k]->texture_id)){
                            // cout << "Do not safe texture twice" << endl;
                        } else if (cfg_material.textures[k]->rel_path.find("default_model_") != string::npos) {
                            // cout << "Do not save the default texture " << cfg_constants::texture_names[k];
                        }
                        else{
                            if (is_forbidden_texture(cfg_material.textures[k]->rel_path)){
                                cout << "Save blacklisted texture " << cfg_material.textures[k]->out_path;
                            }
                            if (cli_options.save_png) gl_texture_to_png_file(snowed_textures[k], cfg_material.textures[k]->out_path, true);
                            if (cli_options.save_dds) gl_texture_to_dds_mipmaps(snowed_textures[k], cfg_material.textures[k]->out_path, cfg_material.textures[k]->mipmap_count);
                            saved_textures.insert(cfg_material.textures[k]->texture_id);

                        }
                        /*cout << "Do not save texture " << cfg_constants::texture_names[k];
                        cout << " " << cfg_material.out_texture_paths[k] << endl;*/
                    }
                    if (glGetError() != GL_NO_ERROR) cout << "GL ERROR while saving texture" << endl;
                    glDeleteTextures(texture_types_count, snowed_textures);
                    glDeleteFramebuffers(1, &framebuffer_id);
                }
            }
            for (const auto& [texture_out_path, snowmap_texture] : rendered_snowmaps) {
                glDeleteTextures(1, &snowmap_texture);
                glDeleteFramebuffers(1, &framebuffer_ids[texture_out_path]);
            }
        }
        catch (snow_exception exception) {
            cout << "Error processing file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            cout << "Move on to next file" << endl;
        }
        catch (rapidxml::parse_error exception) {
            cout << "Damaged cfg file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            cout << "Move on to next file" << endl;
        }/*
        catch (exception exception) {
            cout << "Uncaught exception in cfg file " << cfg_path << endl;
            error_files.push_back(cfg_path);
            cout << "Move on to next file" << endl;
        }*/
    }
    cout << "Done. "
        << target_files.size() << " files processed, "
        << target_files.size() - error_files.size() << " successful." << endl;
    if (error_files.size() == 0) cout << "No errors" << endl;
    else {
        cout << "ERRORS in these " << error_files.size() << " files:" << endl;
        for (fs::path error_path : error_files) {
            cout << error_path.string() << endl;
        }
    }
    context_gl.cleanup();
    glfwTerminate();
    // Let the user press enter to close window
    char* _ = new char[2];
    std::cin.getline(_, 2);
    delete[] _;
}