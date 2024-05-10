#include <unordered_map>

#include <xsi_material.h>
#include <xsi_shaderparameter.h>
#include <xsi_shader.h>

#include "MaterialXCore/Document.h"

#include "../utilities/logging.h"
#include "export_options.h"
#include "export_utilities.h"
#include "export.h"
#include "export_names.h"

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
	std::string material_name = xsi_material.GetName().GetAsciiString() + std::string(".Root");
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

	bool material_priority = export_options.materials.material_priority;
	XSI::ShaderParameter xsi_material_port = xsi_material_parameters.GetItem("material");
	XSI::CRef xsi_material_port_source = xsi_material_port.GetSource();
	bool export_materialx = false;
	if (material_priority && xsi_material_port_source.IsValid()) {
		XSI::ShaderParameter xsi_material_source_param = xsi_material_port_source;;
		if (xsi_material_source_param.IsValid()) {
			XSI::Shader xsi_material_root_shader = xsi_material_source_param.GetParent();
			if (xsi_material_root_shader.IsValid()) {
				XSI::CString xsi_material_root_progid = xsi_material_root_shader.GetProgID();
				std::string render = prog_id_to_render(xsi_material_root_progid);
				if (render == materialx_render()) {
					// start export from this node
					// we are not inside compound
					// if the root is compound, or not MaterialX node, then export as in common case
					MaterialX::NodeGraphPtr temp_graph;
					MaterialX::NodePtr node = shader_to_node(xsi_material_root_shader, mx_doc, temp_graph, true, id_to_node, id_to_graph, stop_names, export_options);
					export_materialx = true;
				}
			}
		}
	}

	size_t parameters_count = xsi_material_parameters.GetCount();
	for (size_t i = 0; i < parameters_count; i++) {
		XSI::ShaderParameter xsi_parameter = xsi_material_parameters[i];
		if (xsi_parameter.IsValid()) {
			XSI::CString parameter_name = xsi_parameter.GetName();
			if (export_materialx == false &&
				(parameter_name == "surface" ||
				parameter_name == "volume" || 
				parameter_name == "environment" || 
				parameter_name == "contour" || 
				parameter_name == "displacement" || 
				parameter_name == "shadow" || 
				parameter_name == "photon" || 
				parameter_name == "photonvolume" || 
				parameter_name == "normal" || 
				parameter_name == "lightmap" || 
				parameter_name == "realtime" ||
				parameter_name == "material")) {
				XSI::CRef xsi_parameter_source = xsi_parameter.GetSource();
				if (xsi_parameter_source.IsValid()) {
					bool is_new = false;
					MaterialX::NodePtr root_node = get_or_create_node(id_to_node, xsi_material_id, mx_doc, temp_graph, true, "root", material_name, "material", is_new);
					if (is_new && export_options.insert_nodedefs) {
						// create node def
						MaterialX::NodeDefPtr node_def = mx_doc->addNodeDef("ND_Root", "", "root");
						node_def->removeOutput("out");
						node_def->addInput("surface", "color4");
						node_def->addInput("volume", "color4");
						node_def->addInput("environment", "color4");
						node_def->addInput("contour", "color4");
						node_def->addInput("displacement", "float");
						node_def->addInput("shadow", "color4");
						node_def->addInput("photon", "color4");
						node_def->addInput("photonvolume", "color4");
						node_def->addInput("normal", "vector3");
						node_def->addInput("lightmap", "mrLmap");
						node_def->addInput("realtime", "xgsRTCtx");
						node_def->addInput("material", "mrPhenMat");
					}

					start_material_port(mx_doc, xsi_parameter_source, root_node, parameter_name.GetAsciiString(), id_to_node, id_to_graph, stop_names, export_options);
				}
			}
		}
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