#include "CfgFile.h"

#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "dds2gl.h"
#include "snow_exception.h"
#include "filelist.h"
#include "../external/rapidxml/rapidxml.hpp"
using namespace rapidxml;


std::wstring find_datapath(std::wstring a_filepath_containing_datapath) {
	size_t begin_index = std::min<size_t>(a_filepath_containing_datapath.rfind(L"\\data\\"), a_filepath_containing_datapath.rfind(L"/data/"));
	return a_filepath_containing_datapath.substr(0, begin_index + 1);
}


CfgFile::CfgFile(std::wstring input_filepath, std::wstring out_path) {
		//std::wcout << L"Reading " << input_filepath << std::endl;

		// Reading file to buffer copied from https://stackoverflow.com/a/29915752
		// (Answer from ANjaNA licensed CC BY-SA 3.0)
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
				    cfg_models.push_back(CfgModel(model_tag, find_datapath(input_filepath), out_path));
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


CfgModel::CfgModel(rapidxml::xml_node<>* input_node, std::wstring data_path, std::wstring out_path) {
	// std::cout << "FileName: " << input_node->first_node("FileName", 8)->value() << std::endl;

	rdm_filename = data_path + string_to_16bit_unicode_wstring(input_node->first_node(cfg_constants::rdm_filename_in_cfg)->value());

	// https://stackoverflow.com/a/20412841
	int n = 0;
	while ((n = rdm_filename.find(L"\\\\", n)) != std::string::npos) {
		rdm_filename.replace(n, 2, L"/");
		n ++;
	}
	//rdm_filename = rdm_filename.substr(0, rdm_filename.size() - 4) + L"_lod0.rdm";

	// This one is mainly for data/dlc09/graphics/buildings/residence/residence_tier05_01/residence_tier05_01_02.cfg
	// which has a Model with an empty <FileName></FileName> and no Models tag.
	if (!rdm_filename.ends_with(L".rdm")) throw snow_exception("FileName is not an RDM file");

	xml_node<>* materials_tag = input_node->first_node("Materials", 9);
	for (xml_node<>* material_tag = materials_tag->first_node();
		material_tag;
		material_tag = material_tag->next_sibling())
	{
		//std::cout << "Found a material tag" << std::endl;
		cfg_materials.push_back(CfgMaterial(material_tag, data_path, out_path));
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
	mesh.load_rdm(std::filesystem::path(rdm_filename));
}


CfgMaterial::CfgMaterial(rapidxml::xml_node<>* input_node, std::wstring data_path, std::wstring out_path)
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
		if (is_forbidden_texture(rel_texture_paths[i])) rel_texture_paths[i] = cfg_constants::default_textures[i];

		std::replace(rel_texture_paths[i].begin(), rel_texture_paths[i].end(), '/', '\\');
		if (rel_texture_paths[i].ends_with(".psd") || rel_texture_paths[i].ends_with(".png")) {
			rel_texture_paths[i] = rel_texture_paths[i].substr(0, rel_texture_paths[i].size() - 4) + "_0.dds";
		}

		abs_texture_paths[i] = data_path + string_to_16bit_unicode_wstring(rel_texture_paths[i]);
		mipmap_count[i] = 0;
		while (std::filesystem::exists(std::filesystem::path(
			abs_texture_paths[i].substr(0, abs_texture_paths[i].size() - 5) // Path until mipmap indication
			+ std::wstring(1, (wchar_t)(mipmap_count[i] + 48))              // The mipmap level as a wstring
			+ std::wstring(L".dds")))){
			mipmap_count[i]++;
		}
		
		out_texture_paths[i] = out_path + string_to_16bit_unicode_wstring(rel_texture_paths[i]);

		out_texture_paths[i] = out_texture_paths[i].substr(0, out_texture_paths[i].size() - 5);// + L".png";

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
