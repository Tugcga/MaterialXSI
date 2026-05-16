#pragma once
#include "MaterialXFormat/File.h"
#include "MaterialXCore/Document.h"

#include "export_format.h"
#include "export_options.h"

// call this method at the load plugin stage
void prepare_generators(const XSI::CString& plugin_path);

MaterialX::FileSearchPath get_search_path();
MaterialX::DocumentPtr get_std_lib();
void generate_shader(const MaterialX::DocumentPtr& mx_doc, const std::string& output_path, ExportFormat format, ExportFeaturesOptions features);