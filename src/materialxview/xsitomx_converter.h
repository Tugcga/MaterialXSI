#pragma once
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_shaderarrayparameter.h>

#include "MaterialXRenderGlsl/GlslMaterial.h"
#include "MaterialXRender/Mesh.h"
#include "MaterialXRender/ImageHandler.h"
#include "MaterialXCore/Document.h"
#include "MaterialXFormat/File.h"
#include "MaterialXGenShader/GenContext.h"

#include <vector>
#include <unordered_map>

std::tuple<MaterialX::MeshPtr, std::vector<ULONG>, long long> xsipolymesh_to_mxmesh(XSI::X3DObject &xsi_object);
XSI::ShaderParameter get_source_parameter(const XSI::ShaderParameter& parameter, bool return_output);
MaterialX::DocumentPtr xsimaterial_to_doc(ULONG xsi_material_id);
std::tuple<MaterialX::GlslMaterialPtr, long long> xsimaterial_to_glslmaterial(ULONG xsi_material_id, MaterialX::DocumentPtr std_lib,
																		      const MaterialX::FileSearchPath &search_path,
																		      MaterialX::ImageHandlerPtr image_handler,
																		      MaterialX::GenContext &gen_context);
std::tuple<MaterialX::GlslMaterialPtr, long long> build_empty_glslmaterial(MaterialX::DocumentPtr std_lib,
																		   const MaterialX::FileSearchPath& search_path,
																		   MaterialX::ImageHandlerPtr image_handler,
																		   MaterialX::GenContext& gen_context);
bool are_material_changed(const XSI::X3DObject &xsi_object, const std::vector<ULONG>& material_ids);
bool is_material_changed(MaterialX::GlslMaterialPtr original_material, MaterialX::DocumentPtr new_material_doc);
