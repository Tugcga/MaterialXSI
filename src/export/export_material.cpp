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
						 std::vector<XSI::CString>& stop_names,
						 const ExportOptions& export_options) {
	// get source parent node
	XSI::ShaderParameter xsi_source_parameter(xsi_surface_source);
	if (xsi_source_parameter.IsValid()) {
		std::string output_name = xsi_source_parameter.GetName().GetAsciiString();
		XSI::Shader xsi_shader = xsi_source_parameter.GetParent();
		if (is_compound(xsi_shader)) {
			MaterialX::NodeGraphPtr node_graph = compound_to_graph(xsi_shader, mx_doc, id_to_node, id_to_graph, stop_names, export_options);
			MaterialX::OutputPtr graph_output = node_graph->getOutput(output_name);
			material_node->setConnectedOutput(input_name, graph_output);
		}
		else {
			MaterialX::NodeGraphPtr temp_graph;
			MaterialX::NodePtr node = shader_to_node(xsi_shader, mx_doc, temp_graph, true, id_to_node, id_to_graph, stop_names, export_options);
			// connect the node
			material_node->setConnectedNode(input_name, node);

			std::string shader_last_output_name = "";
			size_t shader_outputs = get_shader_outputs_count(xsi_shader, shader_last_output_name);
			if (shader_outputs > 1) {
				MaterialX::InputPtr input = material_node->getInput(input_name);
				input->setOutputString(output_name);
			}
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
	std::vector<XSI::CString> stop_names;
	MaterialX::NodeGraphPtr temp_graph;

	if (export_options.materials.all_shaders) {
		// export all nodes at the root level of the material
		XSI::CRefArray material_shaders = xsi_material.GetAllShaders();
		for (size_t i = 0; i < material_shaders.GetCount(); i++) {
			XSI::CRef material_shader = material_shaders[i];
			XSI::Shader xsi_shader(material_shader);
			if (xsi_shader.IsValid()) {
				if (is_compound(xsi_shader)) {
					MaterialX::NodeGraphPtr graph = compound_to_graph(xsi_shader, mx_doc, id_to_node, id_to_graph, stop_names, export_options);
				}
				else {
					MaterialX::NodePtr node = shader_to_node(xsi_shader, mx_doc, temp_graph, true, id_to_node, id_to_graph, stop_names, export_options);
				}
			}
		}
	}

	// get all material input ports
	XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
	XSI::ShaderParameter xsi_surface = xsi_material_parameters.GetItem("surface");
	XSI::ShaderParameter xsi_displacement = xsi_material_parameters.GetItem("displacement");
	XSI::ShaderParameter xsi_volume = xsi_material_parameters.GetItem("volume");

	XSI::CRef xsi_surface_source = xsi_surface.GetSource();
	if (xsi_surface_source.IsValid()) {
		bool is_new = false;
		MaterialX::NodePtr surface_node = get_or_create_node(id_to_node, xsi_material_id, mx_doc, temp_graph, true, "surfacematerial", material_name, "material", is_new);

		start_material_port(mx_doc, xsi_surface_source, surface_node, "surfaceshader", id_to_node, id_to_graph, stop_names, export_options);
	}

	XSI::CRef xsi_displacement_source = xsi_displacement.GetSource();
	if (xsi_displacement_source.IsValid()) {
		bool is_new = false;
		MaterialX::NodePtr surface_node = get_or_create_node(id_to_node, xsi_material_id, mx_doc, temp_graph, true, "surfacematerial", material_name, "material", is_new);

		start_material_port(mx_doc, xsi_displacement_source, surface_node, "displacementshader", id_to_node, id_to_graph, stop_names, export_options);
	}

	XSI::CRef xsi_volume_source = xsi_volume.GetSource();
	if (xsi_volume_source.IsValid()) {
		// should not store this node, because we use it only here
		MaterialX::NodePtr volume_node = mx_doc->addNode("volumematerial", material_volume_name, "material");

		start_material_port(mx_doc, xsi_volume_source, volume_node, "volumeshader", id_to_node, id_to_graph, stop_names, export_options);
	}
}

void export_shaders(const std::vector<XSI::Shader> &xsi_shaders, MaterialX::DocumentPtr& mx_doc, const ExportOptions &export_options) {
	std::unordered_map<ULONG, MaterialX::NodePtr> id_to_node;
	std::unordered_map<ULONG, MaterialX::NodeGraphPtr> id_to_graph;
	std::vector<XSI::CString> stop_names;

	for (size_t i = 0; i < xsi_shaders.size(); i++) {
		XSI::Shader xsi_shader = xsi_shaders[i];

		if (is_compound(xsi_shader)) {
			MaterialX::NodeGraphPtr node_graph = compound_to_graph(xsi_shader, mx_doc, id_to_node, id_to_graph, stop_names, export_options);
		}
		else {
			MaterialX::NodeGraphPtr temp_graph;
			MaterialX::NodePtr node = shader_to_node(xsi_shader, mx_doc, temp_graph, true, id_to_node, id_to_graph, stop_names, export_options);
		}
	}
}