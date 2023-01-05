#pragma once
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <algorithm>
#include "../external/glew-2.2.0/include/GL/glew.h"
#include "../external/glfw-3.3.6/include/GLFW/glfw3.h"
#include "../external/rapidxml/rapidxml.hpp"

#include "rdm2gl.h"

#define texture_types_count 3

namespace cfg_constants{
    enum class texture_names {diff, norm, metallic};

	// These 'texture names' are used as names in the shader and for debug output
	inline const char* texture_names[] = { "diff_texture", "norm_texture", "metallic_texture" };
	// In a .cfg file, the tag <DIFFUSE_ENABLED> tells whether the diffuse texture is enabled (similar for the other textures)
	inline const char* textures_exist_tagname_in_cfg[] = {"DIFFUSE_ENABLED", "NORMAL_ENABLED", "METALLIC_TEX_ENABLED"};
	// In a .cfg file, the tag <cModelDiffTex> tells the relative diffuse texture path (similar for the other textures)
	inline const char* textures_path_tagname_in_cfg[] = {"cModelDiffTex", "cModelNormalTex", "cModelMetallicTex"};
	// Use these textures when no real texture specified
	inline const char* default_textures[] = {
		"data/graphics/effects/default_model_diffuse.psd",
		"data/graphics/effects/default_model_normal.psd",
		"data/graphics/effects/default_model_mask.psd"};
	inline const char* rdm_filename_in_cfg = "FileName";
}
std::wstring find_datapath(std::wstring a_filepath_containing_datapath);

class CfgMaterial
{
public:
	std::string rel_texture_paths[texture_types_count];
	std::wstring abs_texture_paths[texture_types_count];
	std::wstring out_texture_paths[texture_types_count];
	std::uint8_t mipmap_count[texture_types_count];

	std::string vertex_format;

	GLuint texture_ids[texture_types_count];
	CfgMaterial(rapidxml::xml_node<>* input_node, std::wstring data_path, std::wstring out_path);
	~CfgMaterial();
	
	void load_textures(std::map<std::wstring, GLuint>* loaded_textures);
	void bind_textures(GLuint shader_program_id);
};

class CfgModel
{
public:
	CfgModel(rapidxml::xml_node<>* input_node, std::wstring data_path, std::wstring out_path);
	void load_models_and_textures(std::map<std::wstring, GLuint>* loaded_textures);
	void load_model();

	std::wstring rdm_filename;

	std::vector<CfgMaterial> cfg_materials;
	HardwareRdm mesh;
};

class CfgFile
{
public:
	CfgFile(std::wstring input_filepath, std::wstring out_path);
	int load_models_and_textures(std::map<std::wstring, GLuint>* loaded_textures);

	std::vector<CfgModel> cfg_models;
	std::map<std::wstring, GLuint> loaded_textures{};
};
