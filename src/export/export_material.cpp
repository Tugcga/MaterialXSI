#include <unordered_map>

#include <xsi_material.h>
#include <xsi_shaderparameter.h>
#include <xsi_shader.h>

#include "MaterialXCore/Document.h"

#include "../utilities/logging.h"
#include "export_options.h"
#include "export_utilities.h"
#include "export.h"

void start_material_port(MaterialX::DocumentPtr& mx_doc, 
						 const XSI::CRef &xsi_surface_source, 
						 const MaterialX::NodePtr &material_node,
						 const std::string &input_name,
						 std::unordered_map<ULONG, MaterialX::NodePtr> &id_to_node, 
						 std::unordered_map<ULONG, MaterialX::NodeGraphPtr> &id_to_graph, 
						 const ExportOptions& export_options) {
	// get source parent node
	XSI::ShaderParameter xsi_source_parameter(xsi_surface_source);
	if (xsi_source_parameter.IsValid()) {
		XSI::Shader xsi_shader = xsi_source_parameter.GetParent();
		if (is_compound(xsi_shader)) {
			//MaterialX::NodeGraphPtr node_graph = compound_to_graph();
		}
		else {
			MaterialX::NodePtr node = shader_to_node(xsi_shader, mx_doc, id_to_node, export_options);
			// connect the node
			material_node->setConnectedNode(input_name, node);
		}
	}
}

void export_material(const XSI::Material &xsi_material, MaterialX::DocumentPtr &mx_doc, const ExportOptions& export_options) {
	std::string material_name = xsi_material.GetName().GetAsciiString();
	std::string material_volume_name = material_name + ".Volume";
	material_name += ".Surface";
	ULONG xsi_material_id = xsi_material.GetObjectID();

	std::unordered_map<ULONG, MaterialX::NodePtr> id_to_node;
	std::unordered_map<ULONG, MaterialX::NodeGraphPtr> id_to_graph;

	// get all material input ports
	XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
	XSI::ShaderParameter xsi_surface = xsi_material_parameters.GetItem("surface");
	XSI::ShaderParameter xsi_displacement = xsi_material_parameters.GetItem("displacement");
	XSI::ShaderParameter xsi_volume = xsi_material_parameters.GetItem("volume");

	XSI::CRef xsi_surface_source = xsi_surface.GetSource();
	if (xsi_surface_source.IsValid()) {
		bool is_new = false;
		MaterialX::NodePtr surface_node = get_or_create_node(id_to_node, xsi_material_id, mx_doc, "surfacematerial", material_name, "material", is_new);

		start_material_port(mx_doc, xsi_surface_source, surface_node, "surfaceshader", id_to_node, id_to_graph, export_options);
	}

	XSI::CRef xsi_displacement_source = xsi_displacement.GetSource();
	if (xsi_displacement_source.IsValid()) {
		bool is_new = false;
		MaterialX::NodePtr surface_node = get_or_create_node(id_to_node, xsi_material_id, mx_doc, "surfacematerial", material_name, "material", is_new);

		start_material_port(mx_doc, xsi_displacement_source, surface_node, "displacementshader", id_to_node, id_to_graph, export_options);
	}

	XSI::CRef xsi_volume_source = xsi_volume.GetSource();
	if (xsi_volume_source.IsValid()) {
		// should not store this node, because we use it only here
		MaterialX::NodePtr volume_node = mx_doc->addNode("volumematerial", material_volume_name, "material");

		start_material_port(mx_doc, xsi_volume_source, volume_node, "volumeshader", id_to_node, id_to_graph, export_options);
	}
}

void export_shaders(const std::vector<XSI::Shader> &xsi_shaders, MaterialX::DocumentPtr& mx_doc, const ExportOptions &export_options) {
	std::unordered_map<ULONG, MaterialX::NodePtr> id_to_node;
	std::unordered_map<ULONG, MaterialX::NodeGraphPtr> id_to_graph;

	for (size_t i = 0; i < xsi_shaders.size(); i++) {
		XSI::Shader xsi_shader = xsi_shaders[i];

		if (is_compound(xsi_shader)) {
			//MaterialX::NodeGraphPtr node_graph = compound_to_graph();
		}
		else {
			MaterialX::NodePtr node = shader_to_node(xsi_shader, mx_doc, id_to_node, export_options);
		}
	}
}