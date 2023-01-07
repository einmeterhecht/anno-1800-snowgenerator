#include "cli_options.h"

CliOptions::CliOptions(int argc, char* argv[], fs::path containing_dir) {
	string dir_to_parse = containing_dir.string();
	fs::path out_path = containing_dir.append("[Winter] Snow/");

	bool has_extracted_maindata_path = false; // Was a fallback path with the extracted maindata passed as command line argument?
	fs::path extracted_maindata_path;

	bool disable_filenamefilters = false;
	bool atlas_mode = false;
    bool save_non_mod_textures = false;

	bool save_png = false;
	bool save_dds = true;

    // cout << "Command line arguments passed:" << endl;
    string last_word = "";
    for (int i = 1; i < argc; i++) {
        // cout << i << ": " << argv[i] << endl;
        string arg = string(argv[i]);
        if ((arg == "--no_ff") || (arg == "--no_filename_filters")) {
            disable_filenamefilters = true;
        }
        else if ((arg == "--no_ff") || (arg == "--no_filename_filters")) {
            disable_filenamefilters = true;
        }
        else if ((arg == "--png") || (arg == "--save_png")) {
            save_png = true;
        }
        else if (arg == "--no_dds") {
            save_dds = false;
        }
        else if (arg == "--only_png") {
            save_png = true;
            save_dds = false;
        }
        else if ((arg == "--save_non_mod_textures") || (arg == "--save_vanilla")) {
            save_non_mod_textures = true;
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
            if (last_word == "-i") dir_to_parse = argv[i];
            else if (last_word == "-o") out_path = fs::path(argv[i]);
            else if (last_word == "-d") {
                has_extracted_maindata_path = true;
                extracted_maindata_path = fs::path(argv[i]);
            }
        }
    }

    cout << "-i " << dir_to_parse << endl;
    cout << "-o " << out_path.string() << endl;
    if (has_extracted_maindata_path) cout << "-d " << extracted_maindata_path.string() << endl;
    if (disable_filenamefilters) cout << "--no_ff" << endl;
    if (atlas_mode) cout << "--atlas_mode" << endl;
}
