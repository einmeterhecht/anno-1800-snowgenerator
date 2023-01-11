#include "filelist.h"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "dds2gl.h"

bool ends_in_cfg(std::string path) {
	return path.ends_with(".cfg");
}

void get_file_list(std::filesystem::path directory_path, std::vector<std::filesystem::path>* target_files, bool (*check_function)(std::string))
{
	//std::cout << "Parse: " << directory_path.string() << std::endl;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_path)) {
		if ((*check_function)(entry.path().generic_string())) {
			target_files->push_back(entry.path());
		}
	}
}

bool is_old_world(std::string path)
{
	int i;
	for (i = 0; i < BLACKLIST_SIZE; i++) {
		if (path.find(blacklist[i]) != std::string::npos) break;
	}
	if (i == BLACKLIST_SIZE) return true;
	else {
		for (i = 0; i < WHITELIST_SIZE; i++) {
			if (path.find(whitelist[i]) != std::string::npos) break;
		}
		return i != WHITELIST_SIZE;
	}
}

bool is_forbidden_texture(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	int i;
	for (i = 0; i < TEXTURE_BLACKLIST_SIZE; i++) {
		if (path.find(texture_blacklist[i]) != std::string::npos) break;
	}
	return i != TEXTURE_BLACKLIST_SIZE;
}

bool is_old_world_cfg(std::string path)
{
	if (!ends_in_cfg(path)) return false;
	std::replace(path.begin(), path.end(), '\\', '/');
	if (!is_old_world(path)) return false;
	return true;
}
