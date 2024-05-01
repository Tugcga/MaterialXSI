#pragma once
#include <vector>

#include <xsi_material.h>
#include <xsi_shader.h>

#include "MaterialXCore/Document.h"

#include "export_options.h"

void export_mtlx(const std::vector<int> &object_ids, const XSI::CString &output_path, const ExportOptions &export_options);

void export_material(const XSI::Material& xsi_material, MaterialX::DocumentPtr &mx_doc);
void export_shader(const XSI::Shader& xsi_shader, MaterialX::DocumentPtr &mx_doc);