#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"
#include "../external/rapidxml/rapidxml.hpp"

#include "rdm2gl.h"
#include "cli_options.h"

#define texture_types_count 3

namespace cfg_constants {
	enum class texture_names { diff, norm, metallic };

	// These 'texture names' are used as names in the shader and for debug output
	inline const char* texture_names[] = { "diff_texture", "norm_texture", "metallic_texture" };
	// In a .cfg file, the tag <DIFFUSE_ENABLED> tells whether the diffuse texture is enabled (similar for the other textures)
	inline const char* textures_exist_tagname_in_cfg[] = { "DIFFUSE_ENABLED", "NORMAL_ENABLED", "METALLIC_TEX_ENABLED" };
	// In a .cfg file, the tag <cModelDiffTex> tells the relative diffuse texture path (similar for the other textures)
	inline const char* textures_path_tagname_in_cfg[] = { "cModelDiffTex", "cModelNormalTex", "cModelMetallicTex" };
	// Use these textures when no real texture specified
	inline const char* default_texture_paths[] = {
		"data/graphics/effects/default_model_diffuse.psd",
		"data/graphics/effects/default_model_normal.psd",
		"data/graphics/effects/default_model_mask.psd" };
	inline const uint32_t default_texture_colors[] = {
		255,
		8421376,
		0,
	};
	inline const char* rdm_filename_in_cfg = "FileName";
}
std::filesystem::path find_datapath(std::filesystem::path path_into_data);

std::string backward_to_forward_slashes(std::string original);
std::filesystem::path backward_to_forward_slashes(std::filesystem::path original);

class Texture
{
public:
    std::string rel_path;
	std::filesystem::path abs_path;
	std::filesystem::path out_path;

	int type; // 0=diffuse; 1=normal; 2=metallic
	
	size_t mipmap_count;
	GLuint texture_id;
	GLuint snowed_texture_id;

	bool is_loaded = false;
	bool is_snow_generated = false;
	bool save_snowed_texture = false;
	bool is_snowed_version_saved = false;

	Texture(std::string texture_rel_path, std::filesystem::path texture_abs_path, std::filesystem::path out_base_path, int texture_type, bool texture_save_snowed_texture);
	void load();
	void cleanup();
	~Texture();
};

std::vector<Texture> load_default_textures(void);

class CfgMaterial
{
public:
	Texture* textures[texture_types_count];
	std::string vertex_format;

	CfgMaterial(rapidxml::xml_node<>* input_node, std::filesystem::path data_path,
		std::unordered_map<std::string, Texture>* all_textures, std::vector<Texture>* default_textures, CliOptions cli_options);
	
	void bind_textures(GLuint shader_program_id);
};

class CfgModel
{
public:
	CfgModel(rapidxml::xml_node<>* input_node, std::filesystem::path data_path,
		std::unordered_map<std::string, Texture>* all_textures, std::vector<Texture>* default_textures, CliOptions cli_options);
	void load_model();

	std::filesystem::path rdm_filename;

	std::vector<CfgMaterial> cfg_materials;
	HardwareRdm mesh;

};

class CfgFile
{
public:
	CfgFile(std::filesystem::path input_filepath, std::vector<Texture>* default_textures, CliOptions cli_options);
	void load_models_and_textures();

	std::vector<CfgModel> cfg_models;
	std::unordered_map<std::string, Texture> all_textures;
	float mesh_radius;
};
