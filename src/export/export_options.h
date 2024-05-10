#pragma once
#include <xsi_string.h>

struct ExportTextureOptions {
	bool use_relative_path;
	bool copy_files;
	std::string copy_folder;
};

struct ExportMaterialOptions {
	bool all_shaders;
	bool material_priority;
};

struct ExportOptions {
	std::string output_path;
	bool insert_nodedefs;
	ExportTextureOptions textures;
	ExportMaterialOptions materials;
};