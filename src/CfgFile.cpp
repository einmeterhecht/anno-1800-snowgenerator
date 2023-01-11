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

CfgFile::CfgFile(std::filesystem::path input_filepath, CliOptions cli_options) {
		//std::wcout << L"Reading " << input_filepath << std::endl;

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

		xml_node<> *models_tag = doc.first_node()->first_node("Models", 6);

		if (models_tag) {
			for (xml_node<>* model_tag = models_tag->first_node();
				model_tag;
				model_tag = model_tag->next_sibling())
			{
				//std::cout << "Found a model tag" << std::endl;
				try {
				    cfg_models.push_back(CfgModel(model_tag, find_datapath(input_filepath), cli_options));
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
int CfgFile::load_models_and_textures(std::map<std::wstring, GLuint>* loaded_textures) {
	for (int i = 0; i < cfg_models.size(); i++) {
		cfg_models[i].load_models_and_textures(loaded_textures);
	}
	return 0;
}


CfgModel::CfgModel(rapidxml::xml_node<>* input_node, fs::path data_path, CliOptions cli_options) {
	// std::cout << "FileName: " << input_node->first_node("FileName", 8)->value() << std::endl;

    std::string relative_path = std::string(input_node->first_node(cfg_constants::rdm_filename_in_cfg)->value());
	rdm_filename = backward_to_forward_slashes(fs::path(data_path).append(relative_path));

	//rdm_filename = rdm_filename.substr(0, rdm_filename.size() - 4) + L"_lod0.rdm";

	// This one is mainly for data/dlc09/graphics/buildings/residence/residence_tier05_01/residence_tier05_01_02.cfg
	// which has a Model with an empty <FileName></FileName> and no Models tag.
	if (!rdm_filename.string().ends_with(".rdm")) throw snow_exception("FileName is not an RDM file");

	xml_node<>* materials_tag = input_node->first_node("Materials", 9);
	for (xml_node<>* material_tag = materials_tag->first_node();
		material_tag;
		material_tag = material_tag->next_sibling())
	{
		//std::cout << "Found a material tag" << std::endl;
		cfg_materials.push_back(CfgMaterial(material_tag, data_path, cli_options));
	}
}

void CfgModel::load_models_and_textures(std::map<std::wstring, GLuint>* loaded_textures) {
	load_model();
	for (int i=0; i<cfg_materials.size(); i++) {
		cfg_materials[i].load_textures(loaded_textures);
	}
}

void CfgModel::load_model()
{
	mesh.load_rdm(rdm_filename);
}


CfgMaterial::CfgMaterial(rapidxml::xml_node<>* input_node, std::filesystem::path data_path, CliOptions cli_options)
{
	//std::cout << "\n\nReading a material of ConfigType " << input_node->first_node("ConfigType")->value() << std::endl;
	vertex_format = input_node->first_node("VertexFormat")->value();

	for (int i=0; i< texture_types_count; i++){
		rapidxml::xml_node<>* texture_exists_node = input_node->first_node(cfg_constants::textures_exist_tagname_in_cfg[i]);
		rapidxml::xml_node<>* texture_path_node   = input_node->first_node(cfg_constants::textures_path_tagname_in_cfg [i]);

		if (texture_exists_node == nullptr) {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is neither enabled or disabled." << std::endl;
			std::cout << "There is no <" << cfg_constants::textures_exist_tagname_in_cfg[i] << "> tag. Use default texture." << std::endl;

			rel_texture_paths[i] = cfg_constants::default_textures[i];
		}
		else if (texture_exists_node->value_size() != 1) {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is disabled. Use default instead." << std::endl;
			rel_texture_paths[i] = cfg_constants::default_textures[i];
		}
		else if (texture_exists_node->value()[0] != '1') {
			std::cout << "WARNING: " << cfg_constants::texture_names[i] << " is disabled. Use default instead." << std::endl;
			std::cout << cfg_constants::textures_exist_tagname_in_cfg[i] << " = \"" << texture_exists_node->value() << "\"" << std::endl;
			rel_texture_paths[i] = cfg_constants::default_textures[i];
		}
		else if (texture_path_node == nullptr) {
			// Only in data/graphics/ui/3d_objects/world_map/world_map_01.cfg
			std::cout << "WARNING: There is no <" << cfg_constants::textures_path_tagname_in_cfg[i] << "> tag. Use default texture." << std::endl;

			rel_texture_paths[i] = cfg_constants::default_textures[i];
		}
		else{
			rel_texture_paths[i] = std::string(texture_path_node->value(), texture_path_node->value_size()); // Read from xml
			int n = 0;
			while ((n = rel_texture_paths[i].find("\\\\", n)) != std::string::npos) {
				rel_texture_paths[i].replace(n, 2, "/");
				n++;
			}
		}
		if (is_forbidden_texture(rel_texture_paths[i])){
			std::cout << "Use default instead of blacklisted texture " << rel_texture_paths[i] << std::endl;
			rel_texture_paths[i] = cfg_constants::default_textures[i];
		}

		std::replace(rel_texture_paths[i].begin(), rel_texture_paths[i].end(), '/', '\\');
		if (rel_texture_paths[i].ends_with(".psd") || rel_texture_paths[i].ends_with(".png")) {
			rel_texture_paths[i] = rel_texture_paths[i].substr(0, rel_texture_paths[i].size() - 4) + "_0.dds";
		}

		abs_texture_paths[i] = backward_to_forward_slashes(fs::path(data_path).append(rel_texture_paths[i]));

		// Count mipmaps
		fs::path abs_texture_path_until_mipmap_indication = fs::path(data_path).append(
			rel_texture_paths[i].substr(0, rel_texture_paths[i].size() - 5));
		for (mipmap_count[i] = 0;
			std::filesystem::exists(
				fs::path(abs_texture_path_until_mipmap_indication).concat(
					std::to_string(mipmap_count[i])).concat(  // The mipmap level as a string
						".dds"));
			mipmap_count[i]++);
		
		out_texture_paths[i] = backward_to_forward_slashes(fs::path(cli_options.out_path).append(
			rel_texture_paths[i].substr(0, rel_texture_paths[i].size() - 5)));

		//if (std::filesystem::exists(std::filesystem::path(out_texture_paths[i] + L"0.dds"))) abs_texture_paths[i] = out_texture_paths[i] + L"0.dds";

		/*
		std::cout << "relative texture path: " << rel_texture_paths[i] << std::endl;
		std::wcout << L"absolute texture path: " << abs_texture_paths[i] << std::endl;
		std::wcout << L"output texture path: " << out_texture_paths[i] << std::endl;
		*/
	}
}

void CfgMaterial::load_textures(std::map<std::wstring, GLuint>* loaded_textures) {
	for (int i=0; i<texture_types_count; i++){
		if (loaded_textures->contains(abs_texture_paths[i])) {
			texture_ids[i] = loaded_textures->at(abs_texture_paths[i]);
		}
		else{
		    texture_ids[i] = dds_file_to_gl_texture(abs_texture_paths[i]);
			(*loaded_textures)[abs_texture_paths[i]] = texture_ids[i];
            // std::cout << "Loaded texture " << texture_ids[i];
		}
	}
}

void CfgMaterial::bind_textures(GLuint shader_program_id)
{
	for (int i = 0; i < texture_types_count; i++) {
		GLuint texture_location_in_shader = glGetUniformLocation(shader_program_id, cfg_constants::texture_names[i]);

		glActiveTexture(GL_TEXTURE0 + i); // = TEXTURE1 when i=1 and so on
		glBindTexture(GL_TEXTURE_2D, texture_ids[i]);
		// std::cout << "bind" << texture_ids[i] << std::endl;
		glUniform1i(texture_location_in_shader, i);
	}
}

CfgMaterial::~CfgMaterial() {
	glDeleteTextures(texture_types_count, texture_ids);
}
