#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

#define BLACKLIST_SIZE 34
const inline char* blacklist[] = {
	"colony", // Filters New World, Arctic, Enbesa
	"south_america",
	"polar",
	"africa",
	"/eoy21/",
	"/ui/",
	"/cutout_objects/",
	"/effects/",
	"/portraits/",
	"/units/",
	"/vehicle",
	"/cdlc05/",     // Pedestrian Zone - Also used in NW
	"/dlc03",        // No additional snow for the arctic. Gas Power Plant on whitelist.
	"/dlc06",        // Land of Lions - Scholar houses + research institute on whitelist
	"3rd_party_04", // Jean La Fortune
	"3rd_party_05", // Isabel Sarmento
	//"3rd_party_07", // Eli Bleakworth - Snow on his buildings, but not on the stone does not look right.
	"3rd_party_09", // Inuit
	"3rd_party_10", // Waha Desher
	"3rd_party_13", // Ketema
	"ship",
	"feedback",
	"fb",
	"quest_unit",
	"shared_ornaments",
	"quay_system", // Generates incorrect snow and also used in NW
	"jungle",
	"/props/clothing_elements/", // No snow on hats
	"data/graphics/buildings/special/bridges", // Also used in NW
	"/quest/objects/",
	//"cultural_03_module_02", // Botanica ponds
	//"cultural_03_module_03", // Botanica trees
	//"music_pavi",
	"/campaign/burned_ruins/",
	"/campaign/fisher_village/",
	"/campaign/magistrate_shack/",
	"/residence_tier03_01/", // Broken file
	"[Winter]", // Don't generate snow twice (CFGs for test purposes in the output directory)
};

#define TEXTURE_BLACKLIST_SIZE 4
const inline char* texture_blacklist[] = {
	"atlas", //"data/graphics/props/shared_textures/prop_atlas_4k",//
	"quay_system",
	"data/graphics/buildings/3rd_party/3rd_party_06/maps/3rd_party_06_", //Old Nate
	"bridges",
	"_ground"
	//"data/graphics/ui/3d_objects/world_map/maps/afric", //(sic)
    //"data/graphics/ui/3d_objects/world_map/maps/c",
	//"data/graphics/ui/3d_objects/world_map/maps/polar",
	//"data/graphics/ui/3d_objects/world_map/maps/kontor_colony",
	//"data/graphics/ui/3d_objects/world_map/maps/south_america",
	//"data/graphics/ui/3d_objects/world_map/maps/world"
};

#define WHITELIST_SIZE 4
const inline char* whitelist[] = {
	//"data/graphics/ui/3d_objects/world_map/",
	"data/dlc03/graphics/buildings/special/electricity_02", // Gas Power Plant
	"data/dlc06/graphics/buildings/public/monument_02", // Research institute
	"data/dlc06/graphics/buildings/residence/residence_tier06", // Scholar houses
	"shipyard" // This also generates snow for the shipyard of Angereb.
};

bool ends_in_cfg(std::string path);
bool is_old_world(std::string path);
bool is_old_world_cfg(std::string path);
bool is_forbidden_texture(std::string path);
void get_file_list(std::filesystem::path directory_path, std::vector<std::filesystem::path>* target_files, bool (*check_function)(std::string));
