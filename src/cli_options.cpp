#include "cli_options.h"

#include <filesystem>
#include <string>
#include <iostream>

namespace fs = std::filesystem;
using namespace std;

CliOptions::CliOptions(int argc, char* argv[], fs::path containing_dir) {
    dir_to_parse = containing_dir;
	out_path = fs::path(containing_dir).append("[Winter] Snow/");

	has_extracted_maindata_path = false; // Was a fallback path with the extracted maindata passed as command line argument?
	extracted_maindata_path;

	disable_filenamefilters = false;
    disable_texture_blacklist = false;
	atlas_mode = false;
    save_non_mod_textures = false;

    flat_overwrites_steep = true;

	save_png = false;
	save_dds = true;
    save_renderings = false;

    no_prompt = false;

    display_help_message = false;
    display_licenses = false;

    // cout << "Command line arguments passed:" << endl;
    string last_word = "";
    for (int i = 1; i < argc; i++) {
        // cout << i << ": " << argv[i] << endl;
        string arg = string(argv[i]);
        if ((arg == "--no_ff") || (arg == "--no_filename_filters")) {
            disable_filenamefilters = true;
        }
        else if (
            (arg == "--no_texture_ff") || (arg == "--no_tex_ff") || 
            (arg == "--no_texture_blacklist") ||
            (arg == "--no_texture_filename_filters") || (arg == "--no_texture_filters")) {
            disable_texture_blacklist = true;
        }
        else if ((arg == "--png") || (arg == "--save_png")) {
            save_png = true;
        }
        else if ((arg == "--no_dds") || (arg == "--nodds")) {
            save_dds = false;
        }
        else if ((arg == "--only_png") || (arg == "--onlypng")) {
            save_png = true;
            save_dds = false;
        }
        else if (arg == "--save_renderings") {
            save_renderings = true;
        }
        else if ((arg == "--save_non_mod_textures") || (arg == "--save_vanilla")) {
            save_non_mod_textures = true;
        }
		else if ((arg == "--atlas_mode") || (arg == "--atlasses")) {
            atlas_mode = true;
        }
        else if ((arg == "--flat_overwrites_steep") || (arg == "--maximal_snow_per_fragment")) {
            flat_overwrites_steep = true;
        }
        else if ((arg == "--steep_overwrites_flat") || (arg == "--minimal_snow_per_fragment")) {
            flat_overwrites_steep = false;
        }
        else if ((arg == "--no_prompt") || (arg == "--noprompt")) {
            no_prompt = true;
        }
        else if ((arg == "--license") || (arg == "--licenses")
			  || (arg == "--licence") || (arg == "--licences")) {
            display_licenses = true;
        }
        else if ((arg == "-h") || (arg == "--h")
              || (arg == "-help") || (arg == "--help") 
              || (arg == "--manual")) {
            display_help_message = true;
        }
        else if ((arg == "-o") || (arg == "-out_path") || (arg == "-output")) {
            last_word = "-o";
        }
        else if ((arg == "-i") || (arg == "-in_path") || (arg == "-input")) {
            last_word = "-i";
        }
        else if ((arg == "-d") || (arg == "-data_path") || (arg == "-data")) {
            last_word = "-d";
        }
        else {
            if (last_word == "-i") dir_to_parse = fs::path(arg);
            else if (last_word == "-o") out_path = fs::path(arg);
            else if (last_word == "-d") {
                has_extracted_maindata_path = true;
                extracted_maindata_path = fs::path(arg);
            }
            else {
                cout << "Unknown argument: " << arg << endl;
            }
            last_word = "";
        }
    }

    cout << "-i " << dir_to_parse << endl;
    cout << "-o " << out_path.string() << endl;
    if (has_extracted_maindata_path) cout << "-d " << extracted_maindata_path.string() << endl;
    /*
    if (disable_filenamefilters) cout << "--no_ff" << endl;
    if (atlas_mode) cout << "--atlas_mode" << endl;
    if (no_prompt) cout << "--noprompt" << endl;
    */
}
