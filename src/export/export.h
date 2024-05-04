#pragma once
#include <vector>

#include <xsi_material.h>
#include <xsi_shader.h>

#include "MaterialXCore/Document.h"

#include "export_options.h"

// export.cpp
void export_mtlx(const std::vector<int> &object_ids, const ExportOptions &export_options);

// export_material.cpp
void export_material(const XSI::Material& xsi_material, MaterialX::DocumentPtr &mx_doc, const ExportOptions& export_options);
void export_shaders(const std::vector<XSI::Shader>& xsi_shaders, MaterialX::DocumentPtr& mx_doc, const ExportOptions& export_options);

// export_node.cpp
MaterialX::NodePtr shader_to_node(const XSI::Shader& xsi_shader, MaterialX::DocumentPtr& mx_doc, std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node, const ExportOptions& export_options);
MaterialX::NodePtr get_or_create_node(std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node, ULONG node_id, MaterialX::DocumentPtr& mx_doc, const std::string& node_type, const std::string& node_name, const std::string& node_output, bool& is_create);