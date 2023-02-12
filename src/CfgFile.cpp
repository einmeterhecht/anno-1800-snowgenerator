#include "CfgFile.h"

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "dds2gl.h"
#include "snow_exception.h"
#include "filelist.h"
#include "../external/rapidxml/rapidxml.hpp"
#include "gl_stuff.h"

using namespace rapidxml;
namespace fs = std::filesystem;

fs::path find_datapath(fs::path path_into_data) {
	fs::path a_filepath_containing_datapath = path_into_data;
	bool found_datapath = false;
	while (a_filepath_containing_datapath.has_parent_path()) {
		if (a_filepath_containing_datapath.filename() == "data") {
			return a_filepath_containing_datapath.parent_path();
		}
		a_filepath_containing_datapath = a_filepath_containing_datapath.parent_path();
	}
	return path_into_data;
}

fs::path backward_to_forward_slashes(fs::path original) {
	std::string target = original.string();
	// https://stackoverflow.com/a/20412841
	int n = 0;
	while ((n = target.find("\\\\", n)) != std::string::npos) {
		target.replace(n, 2, "/");
		n ++;
	}
	n = 0;
	while ((n = target.find("\\", n)) != std::string::npos) {
		target.replace(n, 1, "/");
		n++;
	}
	return fs::path(target);
}

std::vector<Texture> load_default_textures()
{
	std::vector<Texture> default_textures = std::vector<Texture>();
		DirectX::Image image{};
	for (int i = 0; i < texture_types_count; i++) {
		default_textures.push_back(Texture(cfg_constants::default_texture_paths[i], fs::path(), fs::path(), i, false));

		image.width = 1;
		image.height = 1;
		image.pixels = (uint8_t*)(&cfg_constants::default_texture_colors[i]);
		image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		image.rowPitch = image.width * 4;
		image.slicePitch = image.width * image.height * 4;
		(&default_textures[i])->texture_id = directx_image_to_gl_texture(&image);
		(&default_textures[i])->is_loaded = true;
	}
	return default_textures;
}

CfgFile::CfgFile(std::filesystem::path input_filepath, std::vector<Texture>* default_textures, CliOptions cli_options) {
	
	if (!fs::exists(input_filepath)) {
		throw snow_exception("The .cfg file does not exist.");
	}

	// Reading file to buffer copied from https://stackoverflow.com/a/29915752
	std::ifstream xml_stream(input_filepath);
	// get pointer to associated buffer object
	std::filebuf* pbuf = xml_stream.rdbuf();
	// get file size using buffer's members
	std::size_t size = pbuf->pubseekoff(0, xml_stream.end, xml_stream.in);
	pbuf->pubseekpos(0, xml_stream.in);
	// allocate memory to contain file data
	char* xml_buffer_to_parse = new char[size];
	for (uint32_t i = 0; i < size; i++) xml_buffer_to_parse[i] = 0; // Make sure buffer is empty
	// get file data
	pbuf->sgetn(xml_buffer_to_parse, size);
	xml_stream.close();

	for (uint32_t i = 0; i < size; i++) {
		if (xml_buffer_to_parse[i] == 92) {
			xml_buffer_to_parse[i] = 47; // Replace \ with /
		}
		if (xml_buffer_to_parse[i] == -51) {
			//std::cout << "found 9552 (or -51) char"; // Sometimes there are strange chars at the end of file. No clue where they came from.
			xml_buffer_to_parse[i] = 0;
		}
	}

	// std::cout << "File: \n" << xml_buffer_to_parse << std::endl;

	// Reading XML file using rapidxml
	xml_document<> doc;
	doc.parse<0>(xml_buffer_to_parse);
	xml_node<>* mesh_radius_tag = doc.first_node()->first_node("MeshRadius", 10);
    mesh_radius = 4;
	if (mesh_radius_tag) {
		try {
			mesh_radius = std::stof(std::string(mesh_radius_tag->value(), mesh_radius_tag->value_size()));
		}
		catch (std::exception) {
			mesh_radius = 4;
		}
	}

	xml_node<> *models_tag = doc.first_node()->first_node("Models", 6);

	if (models_tag) {
		for (xml_node<>* model_tag = models_tag->first_node();
			model_tag;
			model_tag = model_tag->next_sibling())
		{
			//std::cout << "Found a model tag" << std::endl;
			try {
				cfg_models.push_back(CfgModel(model_tag, find_datapath(input_filepath), &all_textures, default_textures, cli_options));
			}
			catch (snow_exception exception) {
				std::cout << "Skip this Model, but process the rest of this cfg" << std::endl;
			}
		}
	}
	else {
		std::cout << "Has no models tag" << std::endl;
	}
	delete[] xml_buffer_to_parse;
}

void CfgFile::load_models_and_textures()
{
	for (auto& [texture_path, texture] : all_textures) {
		texture.load();
	}
	for (auto& cfg_model : cfg_models) {
		cfg_model.load_model();
	}
}


CfgModel::CfgModel(rapidxml::xml_node<>* input_node, std::filesystem::path data_path,
	std::unordered_map<std::string, Texture>* all_textures, std::vector<Texture>* default_textures, CliOptions cli_options)
{
	// std::cout << "FileName: " << input_node->first_node("FileName", 8)->value() << std::endl;

    std::string relative_path = std::string(input_node->first_node(cfg_constants::rdm_filename_in_cfg)->value());
	rdm_filename = backward_to_forward_slashes(fs::path(data_path).append(relative_path));

	//rdm_filename = rdm_filename.substr(0, rdm_filename.size() - 4) + L"_lod0.rdm";

	// This one is mainly for data/dlc09/graphics/buildings/residence/residence_tier05_01/residence_tier05_01_02.cfg
	// which has a Model with an empty <FileName></FileName> and no Materials tag.
	if (!rdm_filename.string().ends_with(".rdm")) throw snow_exception("FileName is not an RDM file");
	if (cli_options.has_extracted_maindata_path && (!fs::exists(rdm_filename))) {
		rdm_filename = fs::path(cli_options.extracted_maindata_path).append(relative_path);
		std::cout << "Load mesh from extracted maindata: " << rdm_filename.string() << std::endl;
	}

	xml_node<>* materials_tag = input_node->first_node("Materials", 9);
	for (xml_node<>* material_tag = materials_tag->first_node();
		material_tag;
		material_tag = material_tag->next_sibling())
	{
		//std::cout << "Found a material tag" << std::endl;
		cfg_materials.push_back(CfgMaterial(material_tag, data_path, all_textures, default_textures, cli_options));
	}
}

void CfgModel::load_model()
{
	mesh.load_rdm(rdm_filename);
}


CfgMaterial::CfgMaterial(rapidxml::xml_node<>* input_node, std::filesystem::path data_path,
	std::unordered_map<std::string, Texture>* all_textures, std::vector<Texture>* default_textures, CliOptions cli_options)
{
	//std::cout << "\n\nReading a material of ConfigType " << input_node->first_node("ConfigType")->value() << std::endl;
	vertex_format = input_node->first_node("VertexFormat")->value();

	for (int i=0; i< texture_types_count; i++) {
		rapidxml::xml_node<>* texture_exists_node = input_node->first_node(cfg_constants::textures_exist_tagname_in_cfg[i]);
		rapidxml::xml_node<>* texture_path_node   = input_node->first_node(cfg_constants::textures_path_tagname_in_cfg [i]);

		bool is_texture_valid = false;
		std::string texture_rel_path = cfg_constants::default_texture_paths[i];
		textures[i] = &(*default_textures)[i];

		if (texture_exists_node == nullptr) {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is neither enabled or disabled." << std::endl;
			std::cout << "There is no <" << cfg_constants::textures_exist_tagname_in_cfg[i] << "> tag. ";
		}
		else if (texture_exists_node->value_size() != 1) {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is disabled.";
		}
		else if (texture_exists_node->value()[0] != '1') {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is disabled. ";
			std::cout << cfg_constants::textures_exist_tagname_in_cfg[i] << " = \"" << texture_exists_node->value() << "\"" << std::endl;
		}
		else if (texture_path_node == nullptr) {
			// Only in data/graphics/ui/3d_objects/world_map/world_map_01.cfg
			std::cout << "WARNING: There is no <" << cfg_constants::textures_path_tagname_in_cfg[i] << "> tag. ";
		}
		else {
			// Program reaches this point if there is a texture specified in the .cfg

			texture_rel_path = std::string(texture_path_node->value(), texture_path_node->value_size()); // Read from xml
			int n = 0;
			while ((n = texture_rel_path.find("\\\\", n)) != std::string::npos) {
				texture_rel_path.replace(n, 2, "/");
				n++;
			}

			if (is_forbidden_texture(texture_rel_path)) {
				std::cout << "Texture is blacklisted: " << texture_rel_path << std::endl;
			}
			else {
				std::replace(texture_rel_path.begin(), texture_rel_path.end(), '/', '\\');
				if (texture_rel_path.ends_with(".psd") || texture_rel_path.ends_with(".png")) {
					texture_rel_path = texture_rel_path.substr(0, texture_rel_path.size() - 4) + "_0.dds";
				}
				if (all_textures->contains(texture_rel_path)) {
					textures[i] = &(all_textures->at(texture_rel_path));
					is_texture_valid = true;
				}
				else {
					// Texture is not used by other materials yet. Try to load it from file.
					fs::path texture_abs_path = backward_to_forward_slashes(fs::path(data_path).append(texture_rel_path));
					if (fs::exists(texture_abs_path)) {
						all_textures->insert(std::pair<std::string, Texture>(texture_rel_path,
							Texture(texture_rel_path, texture_abs_path, cli_options.out_path, i, true)));
						textures[i] = &(all_textures->at(texture_rel_path));
						is_texture_valid = true;
					}
					else if (cli_options.has_extracted_maindata_path) {
						// Try to load from the extracted maindata if the texture is not part of the mod we are generating snow for
						fs::path texture_abs_path = backward_to_forward_slashes(fs::path(cli_options.extracted_maindata_path).append(texture_rel_path));
						if (fs::exists(texture_abs_path)) {
							all_textures->insert(std::pair<std::string, Texture>(texture_rel_path,
								Texture(texture_rel_path, texture_abs_path, cli_options.out_path, i, cli_options.save_non_mod_textures)));
							textures[i] = &(all_textures->at(texture_rel_path));
							is_texture_valid = true;
						}
						else {
							std::cout << "Texture not found: " << texture_rel_path << std::endl;
						}
					}
					else {
						std::cout << "Texture not found: " << texture_rel_path << std::endl;
					}
				}
			}
		}

		if (!is_texture_valid) {
			textures[i] = &((*default_textures)[i]);
			std::cout << "Use default texture instead." << std::endl;
		}
	}
}

void CfgMaterial::bind_textures(GLuint shader_program_id)
{
	for (int i = 0; i < texture_types_count; i++) {
		GLuint texture_location_in_shader = glGetUniformLocation(shader_program_id, cfg_constants::texture_names[i]);

		glActiveTexture(GL_TEXTURE0 + i); // = TEXTURE1 when i=1 and so on
		glBindTexture(GL_TEXTURE_2D, textures[i]->texture_id);
		glUniform1i(texture_location_in_shader, i);
	}
}

Texture::Texture(std::string texture_rel_path, std::filesystem::path texture_abs_path, std::filesystem::path out_base_path, int texture_type, bool texture_save_snowed_texture)
{
	rel_path = texture_rel_path;
	abs_path = texture_abs_path;
	out_path = backward_to_forward_slashes(fs::path(out_base_path).append(
		rel_path.substr(0, rel_path.size() - 5)));

	type = texture_type;
	save_snowed_texture = texture_save_snowed_texture;

	// Count mipmaps
	std::string abs_path_until_mipmap_indication = abs_path.string().substr(0, abs_path.string().size() - 5);
	for (mipmap_count = 0;
		std::filesystem::exists(
			fs::path(abs_path_until_mipmap_indication).concat(
				std::to_string(mipmap_count)).concat(  // The mipmap level as a string
					".dds"));
		mipmap_count++);

	//if (std::filesystem::exists(std::filesystem::path(out_texture_paths[i] + L"0.dds"))) abs_texture_paths[i] = out_texture_paths[i] + L"0.dds";
}

void Texture::load()
{
	if (is_loaded) return;
	texture_id = dds_file_to_gl_texture(abs_path);

	int width, height;
	get_dimensions(texture_id, &width, &height);
	snowed_texture_id = create_empty_texture(
		width, height, GL_RGBA, GL_NEAREST, GL_NEAREST, GL_REPEAT);

	is_loaded = true;
}

void Texture::cleanup()
{
	glDeleteTextures(1, &texture_id);
	glDeleteTextures(1, &snowed_texture_id);
}

Texture::~Texture()
{
	cleanup();
}
