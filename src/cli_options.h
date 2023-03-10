#pragma once

#include <filesystem>
#include <string>

class CliOptions
{
public:
    CliOptions(int argc, char* argv[], std::filesystem::path containing_dir);

    std::filesystem::path dir_to_parse;
    std::filesystem::path out_path;

    bool has_extracted_maindata_path = false; // Was a fallback path with the extracted maindata passed as command line argument?
    std::filesystem::path extracted_maindata_path;

    bool disable_filenamefilters = false;
    bool disable_texture_blacklist = false;
    bool atlas_mode = false;
    bool save_non_mod_textures = false;

    bool flat_overwrites_steep = true;

    bool save_png = false;
    bool save_dds = true;
    bool save_renderings = false;

    bool no_prompt = false;

    bool display_help_message = false;
    bool display_licenses = false;
};
