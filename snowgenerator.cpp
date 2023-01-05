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

namespace fs = std::filesystem;

int main()
{
    std::cout << "Hello World!" << std::endl;
    fs::path containing_dir = fs::path(__argv[0]).parent_path();
    fs::path out_path = fs::path(__argv[0]).parent_path().append("[Winter] Snow/");
    std::vector<std::wstring> target;
    get_file_list(containing_dir.string(), &target, &is_old_world_cfg);

    GlStuff context_gl = GlStuff();
    while (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while initializing" << std::endl;
    GLuint snow_program = compile_shaders_to_program(
        texcoord_as_positon_2_vertexshader_code, snow_fragmentshader_code);
    GLuint copy_texture_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, copy_from_texture_fragmentshader_code);
    GLuint combine_to_snowed_textures_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, combine_to_snowed_textures_fragmentshader_code);
    GLuint clear_snowmap_program = compile_shaders_to_program(
        texcoord_as_position_vertexshader_code, clear_snowmap_fragmentshader_code);

    while (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while compiling shaders" << std::endl;

    context_gl.load_square_vertexbuffer();
    context_gl.load_noise_texture(4096);
    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while loading shaders & data" << std::endl;

    std::cout << "Found " << target.size() << " files: " << std::endl;
    for (int cfg_index = 0; cfg_index < target.size(); cfg_index++) {
        std::wstring cfg_path = target.at(cfg_index);
        std::wcout << cfg_path << std::endl;
    }

    std::vector<std::wstring> error_files;

    for (int cfg_index = 0; cfg_index < target.size(); cfg_index++) {
        std::wstring cfg_path = target.at(cfg_index);
        std::wcout << L"\n\n\nCfg file " << cfg_index + 1 << L" / " << target.size() << std::endl;
        std::wcout << cfg_path << std::endl;

        try {
            CfgFile cfg_file = CfgFile(cfg_path, out_path);
            cfg_file.load_models_and_textures(&cfg_file.loaded_textures);
            if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while loading textures" << std::endl;

            std::map<std::wstring, GLuint> rendered_snowmaps{};
            std::map<std::wstring, GLuint> framebuffer_ids{};

            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                HardwareRdm& mesh = cfg_file.cfg_models[i].mesh;
                //mesh.print_information();
                /*min(mesh.materials_count, cfg_file.cfg_models[i].cfg_materials.size())*/
                for (int j = 0; j < mesh.materials_count; j++) {
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[min(
                        mesh.materials[j].index, cfg_file.cfg_models[i].cfg_materials.size() - 1)];

                    int width, height;

                    std::wstring texture_out_path = cfg_material.out_texture_paths[0];

                    // GLuint old_render_target_texture = render_target_texture;
                    if (!rendered_snowmaps.contains(texture_out_path)) {
                        get_dimensions(cfg_material.texture_ids[0], &width, &height);
                        // Create render target
                        rendered_snowmaps[texture_out_path] = create_empty_texture(
                            width, height, GL_RGB, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE);
                        framebuffer_ids[texture_out_path] = create_framebuffer(1);
                        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rendered_snowmaps[texture_out_path], 0);
                        is_framebuffer_ok();

                        // Clear snowmap
                        glUseProgram(clear_snowmap_program);
                        context_gl.bind_square_buffers();
                        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
                        context_gl.unbind_square_buffers();
                    }
                    else {
                        get_dimensions(rendered_snowmaps[texture_out_path], &width, &height);
                    }
                    glViewport(0, 0, width, height);

                    //std::cout << "MATERIAL" << std::endl;

                    // Render a snowmap to rendered_snowmaps[texture_out_paths]
                    /*GLint old_framebuffer;
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_framebuffer);
                    if (old_framebuffer != framebuffer_ids[texture_out_path]) {
                        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_out_path]); // Render to snowmap
                    }*/
                    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_ids[texture_out_path]); // Render to snowmap

                    // Render snowmap
                    glUseProgram(snow_program);
                    cfg_material.bind_textures(snow_program);

                    mesh.bind_buffers();
                    context_gl.bind_vertexformat(cfg_material.vertex_format, mesh.vertices_size);

                    glDrawElements(
                        GL_TRIANGLES,
                        mesh.materials[j].size,
                        mesh.get_corner_datatype(),
                        (void*)mesh.materials[j].offset
                    );
                    context_gl.unbind_vertexformat(cfg_material.vertex_format);

                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while rendering" << std::endl;

                    // Render snowmap to screen so that the user has something to look at
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(0, 0, context_gl.window_w, context_gl.window_h);

                    glUseProgram(copy_texture_program);
                    glClear(GL_COLOR_BUFFER_BIT);

                    context_gl.bind_square_buffers();

                    GLuint texture_location_in_shader = glGetUniformLocation(copy_texture_program, "diff");
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, rendered_snowmaps[texture_out_path]);
                    glUniform1i(texture_location_in_shader, 0);

                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);

                    context_gl.unbind_square_buffers();

                    glfwSwapBuffers(context_gl.window);
                    glfwPollEvents();

                }
            }
            std::set<GLuint> saved_textures = std::set<GLuint>();
            for (int i = 0; i < cfg_file.cfg_models.size(); i++) {
                for (int j = 0; j < cfg_file.cfg_models[i].cfg_materials.size(); j++) {
                    CfgMaterial& cfg_material = cfg_file.cfg_models[i].cfg_materials[j];
                    bool already_saved_all_textures = true;
                    for (int k = 0; k < texture_types_count && already_saved_all_textures; k++)
                        already_saved_all_textures &= saved_textures.contains(cfg_material.texture_ids[k]);
                    // Skip this material if all the textures were already saved by another material or if it hasn't been used by the mesh
                    if (already_saved_all_textures || (!rendered_snowmaps.contains(cfg_material.out_texture_paths[0]))) continue;

                    int width, height;
                    get_dimensions(cfg_material.texture_ids[0], &width, &height);
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
                    glBindTexture(GL_TEXTURE_2D, rendered_snowmaps[cfg_material.out_texture_paths[0]]);
                    glUniform1i(texture_location_in_shader, 3);

                    texture_location_in_shader = glGetUniformLocation(combine_to_snowed_textures_program, "noise");
                    glActiveTexture(GL_TEXTURE4);
                    glBindTexture(GL_TEXTURE_2D, context_gl.noise_texture);
                    glUniform1i(texture_location_in_shader, 4);

                    context_gl.bind_square_buffers();
                    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
                    context_gl.unbind_square_buffers();

                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while combining original texture and snowmap" << std::endl;
                    for (int k = 0; k < texture_types_count; k++) {
                        if (k == 1) saved_textures.insert(cfg_material.texture_ids[k]); // Do not save normalmaps
                        if (!(saved_textures.contains(cfg_material.texture_ids[k]) || cfg_material.out_texture_paths[k].find(L"default_model_") != std::wstring::npos)) {
                            gl_texture_to_dds_mipmaps(snowed_textures[k], cfg_material.out_texture_paths[k], cfg_material.mipmap_count[k]);
                            saved_textures.insert(cfg_material.texture_ids[k]);
                        }
                        /*std::cout << "Do not save texture " << cfg_constants::texture_names[k];
                        std::wcout << L" " << cfg_material.out_texture_paths[k] << std::endl;*/
                    }
                    if (glGetError() != GL_NO_ERROR) std::cout << "GL ERROR while saving texture" << std::endl;
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
            std::wcout << L"Error processing file " << cfg_path << std::endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << std::endl;
        }
        catch (rapidxml::parse_error exception) {
            std::wcout << L"Damaged cfg file " << cfg_path << std::endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << std::endl;
        }/*
        catch (std::exception exception) {
            std::wcout << L"Uncaught exception in cfg file " << cfg_path << std::endl;
            error_files.push_back(cfg_path);
            std::cout << "Move on to next file" << std::endl;
        }*/
    }
    std::cout << "Done. "
        << target.size() << " files processed, "
        << target.size() - error_files.size() << " successful." << std::endl;
    if (error_files.size() == 0) std::cout << "No errors" << std::endl;
    else {
        std::cout << "ERRORS in these " << error_files.size() << " files:" << std::endl;
        for (std::wstring error_path : error_files) {
            std::wcout << error_path << std::endl;
        }
    }
    context_gl.cleanup();
    glfwTerminate();
    // Let the user press enter to close window
    char* _ = new char[2];
    std::cin.getline(_, 2);
    delete[] _;
}