#include "filelist.h"
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include "dds2gl.h"

bool ends_in_cfg(std::wstring path) {
	return path.ends_with(L".cfg");
}

void get_file_list(std::string directory_path, std::vector<std::wstring>* target_files, bool (*check_function)(std::wstring))
{
	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_path)) {
		if ((*check_function)(entry.path().generic_wstring())) {
			target_files->push_back(entry.path().generic_wstring());
		}
	}
}

bool is_old_world(std::wstring path)
{
	int i;
	for (i = 0; i < BLACKLIST_SIZE; i++) {
		if (path.find(blacklist[i]) != std::wstring::npos) break;
	}
	if (i == BLACKLIST_SIZE) return true;
	else {
		for (i = 0; i < WHITELIST_SIZE; i++) {
			if (path.find(whitelist[i]) != std::wstring::npos) break;
		}
		return i != WHITELIST_SIZE;
	}
}

bool is_forbidden_texture(std::string path) {
    return is_forbidden_texture(string_to_16bit_unicode_wstring(path));
}

bool is_forbidden_texture(std::wstring path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	int i;
	for (i = 0; i < TEXTURE_BLACKLIST_SIZE; i++) {
		if (path.find(texture_blacklist[i]) != std::wstring::npos) break;
	}
	return i != TEXTURE_BLACKLIST_SIZE;
}

bool is_old_world_cfg(std::wstring path)
{
	if (!ends_in_cfg(path)) return false;
	std::replace(path.begin(), path.end(), '\\', '/');
	if (!is_old_world(path)) return false;
	return true;
}
