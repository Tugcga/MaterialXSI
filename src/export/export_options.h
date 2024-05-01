#pragma once
#include <xsi_string.h>

struct ExportTextureOptions
{
	bool use_relative_path;
	bool copy_files;
	XSI::CString copy_folder;
};

struct ExportOptions
{
	ExportTextureOptions textures;
};