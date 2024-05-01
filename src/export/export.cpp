#include <vector>

#include <xsi_string.h>

#include "export_options.h"
#include "../utilities/logging.h"

void export_mtlx(const std::vector<int>& object_ids, const XSI::CString& output_path, const ExportOptions& export_options) {
	log_message("items to export: " + to_string(object_ids));
	log_message("path: " + output_path);
	log_message("textures relative: " + XSI::CString(export_options.textures.use_relative_path));
	log_message("textures should copy: " + XSI::CString(export_options.textures.copy_files));
	log_message("textures folder: " + XSI::CString(export_options.textures.copy_folder));
}