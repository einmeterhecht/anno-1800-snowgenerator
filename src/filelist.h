#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

#define BLACKLIST_SIZE 0//33
const inline wchar_t* blacklist[] = {
	L"colony", // Filters New World, Arctic, Enbesa
	L"south_america",
	L"polar",
	L"africa",
	L"/eoy21/",
	L"/ui/",
	L"/cutout_objects/",
	L"/effects/",
	L"/portraits/",
	L"/units/",
	L"/vehicle",
	L"/cdlc05/",     // Pedestrian Zone - Also used in NW
	L"/dlc03",        // No additional snow for the arctic. Gas Power Plant on whitelist.
	L"/dlc06",        // Land of Lions - Scholar houses + research institute on whitelist
	L"3rd_party_04", // Jean La Fortune
	L"3rd_party_05", // Isabel Sarmento
	//L"3rd_party_07", // Eli Bleakworth - Snow on his buildings, but not on the stone does not look right.
	L"3rd_party_09", // Inuit
	L"3rd_party_10", // Waha Desher
	L"3rd_party_13", // Ketema
	L"ship",
	L"feedback",
	L"fb",
	L"quest_unit",
	L"shared_ornaments",
	L"quay_system", // Generates incorrect snow and also used in NW
	L"jungle",
	L"/props/clothing_elements/", // No snow on hats
	L"data/graphics/buildings/special/bridges", // Also used in NW
	L"/quest/objects/",
	//L"cultural_03_module_02", // Botanica ponds
	//L"cultural_03_module_03", // Botanica trees
	//L"music_pavil",
	L"/campaign/burned_ruins/",
	L"/campaign/fisher_village/",
	L"/campaign/magistrate_shack/",
	L"[Winter]", // Don't generate snow twice (CFGs for test purposes in the output directory)
};

#define TEXTURE_BLACKLIST_SIZE 0
const inline char* texture_blacklist[] = {
	"atlas", //"data/graphics/props/shared_textures/prop_atlas_4k",//
	"quay_system",
	//"data/graphics/buildings/3rd_party/3rd_party_06/maps/3rd_party_06_",
	//"data/graphics/ui/3d_objects/world_map/maps/afric",//(sic)
    //"data/graphics/ui/3d_objects/world_map/maps/c",
	//"data/graphics/ui/3d_objects/world_map/maps/polar",
	//"data/graphics/ui/3d_objects/world_map/maps/kontor_colony",
	//"data/graphics/ui/3d_objects/world_map/maps/south_america",
	//"data/graphics/ui/3d_objects/world_map/maps/world",
	"bridges"
};

#define WHITELIST_SIZE 0
const inline wchar_t* whitelist[] = {
	//L"data/graphics/ui/3d_objects/world_map/",
	L"data/dlc03/graphics/buildings/special/electricity_02", // Gas Power Plant
	L"data/dlc06/graphics/buildings/public/monument_02", // Research institute
	L"data/dlc06/graphics/buildings/residence/residence_tier06", // Scholar houses
	L"shipyard" // This also generates snow for the shipyard of Angereb.
};

bool ends_in_cfg(std::wstring path);
bool is_old_world(std::wstring path);
bool is_old_world_cfg(std::wstring path);
bool is_forbidden_texture(std::string path);
void get_file_list(std::string directory_path, std::vector<std::wstring>* target, bool (*check_function)(std::wstring));
