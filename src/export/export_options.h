#pragma once
#include <xsi_string.h>

struct ExportTextureOptions
{
	bool use_relative_path;
	bool copy_files;
	std::string copy_folder;
};

struct ExportOptions
{
	std::string output_path;
	ExportTextureOptions textures;
};