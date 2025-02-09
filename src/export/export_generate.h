#pragma once

// call this method at the load plugin stage
void prepare_generators(const XSI::CString& plugin_path);

void generate_shader(const MaterialX::DocumentPtr& mx_doc, const std::string& output_path, ExportFormat format);