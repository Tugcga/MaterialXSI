#include <string>
#include <unordered_map>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>
#include <xsi_color.h>
#include <xsi_color4f.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>
#include <xsi_quaternionf.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_fcurve.h>
#include <xsi_fcurvekey.h>
#include <xsi_imageclip2.h>

#include "MaterialXCore/Document.h"
#include "boost/filesystem.hpp"

#include "export_options.h"
#include "export_names.h"
#include "export_utilities.h"
#include "../utilities/logging.h"
#include "../utilities/array.h"
#include "../utilities/string.h"
#include "../parse/parse.h"

MaterialX::NodePtr get_or_create_node(std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
									  ULONG node_id,
									  MaterialX::DocumentPtr& mx_doc,
									  MaterialX::NodeGraphPtr &mx_graph,
									  bool use_doc,
									  const std::string& node_type,
									  const std::string& node_name,
									  const std::string& node_output,
									  bool& is_create) {
	auto search = id_to_node.find(node_id);
	if (search != id_to_node.end()) {
		is_create = false;
		return search->second;
	}

	MaterialX::NodePtr new_node;
	if (use_doc) {
		new_node = mx_doc->addNode(node_type, node_name, node_output);
	}
	else {
		new_node = mx_graph->addNode(node_type, node_name, node_output);
	}
	id_to_node.insert({ node_id, new_node });
	is_create = true;
	return new_node;
}

MaterialX::NodeGraphPtr get_or_create_graph(std::unordered_map<ULONG, MaterialX::NodeGraphPtr> &id_to_graph,
											ULONG compound_id,
											MaterialX::DocumentPtr &mx_doc,
											const std::string &graph_name,
											bool &is_create) {
	auto search = id_to_graph.find(compound_id);
	if (search != id_to_graph.end()) {
		is_create = false;
		return search->second;
	}

	// Nested node graphs are not supported by MaterialX
	// so, node graph can be created only under the root document
	MaterialX::NodeGraphPtr new_graph = mx_doc->addNodeGraph(graph_name);;
	id_to_graph.insert({ compound_id, new_graph });
	is_create = true;
	return new_graph;
}

bool add_input_value_to_node(MaterialX::NodePtr& node,
						     MaterialX::NodeGraphPtr& graph,
						     MaterialX::NodeDefPtr def,
						     SubjectMode mode,
						     const std::string& name, 
						     const XSI::CValue &xsi_value,
						     const XSI::ShaderParameter& xsi_parameter,
						     const XSI::siShaderParameterDataType xsi_type, 
						     const ExportOptions& export_options) {
	std::string type_string = parameter_type_to_string(xsi_parameter);
	if (xsi_type == XSI::siShaderDataTypeBoolean) {
		if (mode == SubjectMode::NODE) { node->setInputValue(name, (bool)xsi_value); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, (bool)xsi_value);  }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, (bool)xsi_value); }
		else { return false;  }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeInteger) {
		if (mode == SubjectMode::NODE) { node->setInputValue(name, (int)xsi_value); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, (int)xsi_value); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, (int)xsi_value); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeScalar) {
		if (mode == SubjectMode::NODE) { node->setInputValue(name, (float)xsi_value); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, (float)xsi_value); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, (float)xsi_value); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector2) {
		MaterialX::Vector2 mx_vector = MaterialX::Vector2(xsi_parameter.GetParameterValue("x"),
														  xsi_parameter.GetParameterValue("y"));
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_vector); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector3) {
		MaterialX::Vector3 mx_vector = MaterialX::Vector3(xsi_parameter.GetParameterValue("x"),
														  xsi_parameter.GetParameterValue("y"),
														  xsi_parameter.GetParameterValue("z"));
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_vector); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeVector4) {
		MaterialX::Vector4 mx_vector = MaterialX::Vector4(xsi_parameter.GetParameterValue("x"),
														  xsi_parameter.GetParameterValue("y"),
														  xsi_parameter.GetParameterValue("z"),
														  xsi_parameter.GetParameterValue("w"));
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_vector); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_vector); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeQuaternion) {
		XSI::MATH::CQuaternionf value_quaternion(xsi_value);
		// write as custom quaternion data
		std::string data_string = std::to_string(value_quaternion.GetX()) + "," + std::to_string(value_quaternion.GetY()) + "," + std::to_string(value_quaternion.GetZ()) + "," + std::to_string(value_quaternion.GetW());
		if (mode == SubjectMode::NODE) { node->setInputValue(name, data_string, "quaternion"); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, data_string, "quaternion"); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, data_string, "quaternion"); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix33) {
		MaterialX::Matrix33 mx_matrix = MaterialX::Matrix33(xsi_parameter.GetParameterValue("_00"), xsi_parameter.GetParameterValue("_01"), xsi_parameter.GetParameterValue("_02"),
															xsi_parameter.GetParameterValue("_10"), xsi_parameter.GetParameterValue("_11"), xsi_parameter.GetParameterValue("_12"),
															xsi_parameter.GetParameterValue("_20"), xsi_parameter.GetParameterValue("_21"), xsi_parameter.GetParameterValue("_22"));
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_matrix); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_matrix); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_matrix); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix44) {
		MaterialX::Matrix44 mx_matrix = MaterialX::Matrix44(xsi_parameter.GetParameterValue("_00"), xsi_parameter.GetParameterValue("_01"), xsi_parameter.GetParameterValue("_02"), xsi_parameter.GetParameterValue("_03"),
															xsi_parameter.GetParameterValue("_10"), xsi_parameter.GetParameterValue("_11"), xsi_parameter.GetParameterValue("_12"), xsi_parameter.GetParameterValue("_13"),
															xsi_parameter.GetParameterValue("_20"), xsi_parameter.GetParameterValue("_21"), xsi_parameter.GetParameterValue("_22"), xsi_parameter.GetParameterValue("_23"),
															xsi_parameter.GetParameterValue("_30"), xsi_parameter.GetParameterValue("_31"), xsi_parameter.GetParameterValue("_32"), xsi_parameter.GetParameterValue("_33"));
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_matrix); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_matrix); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_matrix); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeColor3) {
		XSI::MATH::CColor4f value_color(xsi_value);
		MaterialX::Color3 mx_color = MaterialX::Color3(value_color.GetR(), value_color.GetG(), value_color.GetB());
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_color); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_color); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_color); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeColor4) {
		XSI::MATH::CColor4f value_color(xsi_value);
		MaterialX::Color4 mx_color = MaterialX::Color4(value_color.GetR(), value_color.GetG(), value_color.GetB(), value_color.GetA());
		if (mode == SubjectMode::NODE) { node->setInputValue(name, mx_color); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, mx_color); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, mx_color); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeString) {
		XSI::CString value_sting(xsi_value);
		if (mode == SubjectMode::NODE) { node->setInputValue(name, std::string(value_sting.GetAsciiString())); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, std::string(value_sting.GetAsciiString())); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, std::string(value_sting.GetAsciiString())); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeProfileCurve) {
		// write data in the format ltx, lty: t, v: rtx, rty;...
		std::string data_string = "";
		XSI::FCurve curve(xsi_value);
		XSI::CFCurveKeyRefArray curve_keys = curve.GetKeys();
		for (size_t i = 0; i < curve_keys.GetCount(); i++) {
			XSI::FCurveKey key(curve_keys[i]);
			data_string += std::to_string(key.GetLeftTanX()) + "," + std::to_string(key.GetLeftTanY()) + ":" +
				std::to_string(key.GetTime().GetTime()) + "," + std::to_string((float)key.GetValue()) + ":" +  // suppose that value is a float
				std::to_string(key.GetRightTanX()) + "," + std::to_string(key.GetRightTanY());

			if (i < curve_keys.GetCount() - 1) {
				data_string += ";";
			}
		}

		if (mode == SubjectMode::NODE) { node->setInputValue(name, data_string, "fcurve"); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, data_string, "fcurve"); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, data_string, "fcurve"); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeGradient) {
		XSI::CParameterRefArray gradient_parameters = xsi_parameter.GetParameters();
		XSI::Parameter markers_parameter = gradient_parameters.GetItem("markers");
		XSI::CParameterRefArray markers = markers_parameter.GetParameters();
		ULONG markers_count = markers.GetCount();

		// store keys as pos, r, g, b, a
		std::vector<std::tuple<float, float, float, float, float>> markers_array;
		for (size_t i = 0; i < markers_count; i++) {
			XSI::ShaderParameter p = markers[i];
			float pos = p.GetParameterValue("pos");
			float mid = p.GetParameterValue("mid");
			float r = p.GetParameterValue("red");
			float g = p.GetParameterValue("green");
			float b = p.GetParameterValue("blue");
			float a = p.GetParameterValue("alpha");
			if (pos > -1) {
				markers_array.push_back(std::make_tuple(pos, r, g, b, a));
			}
		}

		// sort markers array by position
		std::sort(markers_array.begin(), markers_array.end());

		// output as string in the form: pos: r, g, b, a; pos: r, g, b, a
		std::string data_string = "";
		for (size_t i = 0; i < markers_array.size(); i++) {
			auto m = markers_array[i];

			data_string += std::to_string(std::get<0>(m)) + ":" + std::to_string(std::get<1>(m)) + "," + std::to_string(std::get<2>(m)) + "," + std::to_string(std::get<3>(m)) + "," + std::to_string(std::get<4>(m));
			if (i < markers_array.size() - 1) {
				data_string += ";";
			}
		}

		// write data string
		if (mode == SubjectMode::NODE) { node->setInputValue(name, data_string, "gradient"); }
		else if (mode == SubjectMode::GRAPH) { graph->setInputValue(name, data_string, "gradient"); }
		else if (mode == SubjectMode::DEFINITION) { def->setInputValue(name, data_string, "gradient"); }
		else { return false; }

		return true;
	}
	else if (xsi_type == XSI::siShaderDataTypeImage) {
		XSI::ShaderParameter image_parameter = get_finall_parameter(xsi_parameter);
		// we create node/graph input in any case
		// if there are no valid connected clip, then the input is empty
		MaterialX::InputPtr input;
		if (mode == SubjectMode::NODE) { input = node->addInput(name, "filename"); }
		else if (mode == SubjectMode::GRAPH) { input = graph->addInput(name, "filename"); }
		else if (mode == SubjectMode::DEFINITION) { input = def->addInput(name, "filename"); }
		else { return false; }
		
		if (image_parameter.IsValid() && mode != SubjectMode::DEFINITION) {
			XSI::CRef image_source = image_parameter.GetSource();
			if (image_source.IsValid()) {
				XSI::ImageClip2 image_clip(image_source);

				if (image_clip.IsValid()) {
					XSI::CString xsi_image_path = image_clip.GetFileName();
					XSI::CValue color_profile = image_clip.GetParameterValue("RenderColorProfile");

					boost::filesystem::path image_path = xsi_image_path.GetAsciiString();

					// extract folder with output *.mtlx file
					boost::filesystem::path output_path = export_options.output_path;
					boost::filesystem::path folder_path = output_path.parent_path();

					// copy image to the separate folder
					if (export_options.textures.copy_files) {

						// create new folder
						boost::filesystem::path textures_path = folder_path / export_options.textures.copy_folder;
						boost::filesystem::create_directories(textures_path);

						// copy the file
						boost::filesystem::copy_file(image_path, textures_path / image_path.filename(), boost::filesystem::copy_options::overwrite_existing);

						// override the path to the file
						image_path = textures_path / image_path.filename();
					}

					// convert image_path to relative, if it required
					if (export_options.textures.use_relative_path) {
						image_path = boost::filesystem::relative(image_path, folder_path);
					}
					std::string image_path_string = image_path.string();
					input->setValueString(image_path_string);
					input->setColorSpace(colorspace_to_string(color_profile));
				}
			}
		}

		return true;
	}
	// does not support the following properties
	else if (xsi_type == XSI::siShaderDataTypeProperty) {}
	else if (xsi_type == XSI::siShaderDataTypeLightProfile) {}
	else if (xsi_type == XSI::siShaderDataTypeReference) {}
	else if (xsi_type == XSI::siShaderDataTypeCustom) {}
	else if (xsi_type == XSI::siShaderDataTypeStructure) {}
	else if (xsi_type == XSI::siShaderDataTypeArray) {}

	return false;
}

// only headers, implementations are later
MaterialX::NodePtr shader_to_node(const XSI::Shader& xsi_shader,
	MaterialX::DocumentPtr& mx_doc,
	MaterialX::NodeGraphPtr &mx_graph,
	bool use_doc,
	std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
	std::unordered_map<ULONG, MaterialX::NodeGraphPtr>& id_to_graph,
	std::vector<XSI::CString>& stop_names,
	const ExportOptions& export_options);
MaterialX::NodeGraphPtr compound_to_graph(const XSI::Shader& xsi_compound,
	MaterialX::DocumentPtr& mx_doc,
	std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
	std::unordered_map<ULONG, MaterialX::NodeGraphPtr>& id_to_graph,
	std::vector<XSI::CString>& stop_names,
	const ExportOptions& export_options);

void propagate_connection(const XSI::ShaderParameter &xsi_paramater,
						  MaterialX::DocumentPtr &mx_doc,
						  MaterialX::NodeGraphPtr &mx_graph,
						  bool use_doc,
						  MaterialX::InputPtr mx_input,
						  MaterialX::OutputPtr mx_output,
						  bool use_input,
						  std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
						  std::unordered_map<ULONG, MaterialX::NodeGraphPtr>& id_to_graph,
						  std::vector<XSI::CString> &stop_names,
						  const ExportOptions& export_options) {
	XSI::CRef input_source = xsi_paramater.GetSource();
	if (input_source.IsValid()) {
		XSI::siClassID source_class = input_source.GetClassID();
		if (source_class == XSI::siShaderParameterID) {
			XSI::ShaderParameter source_parameter(input_source);
			// check is this source parameter in the stop list
			XSI::CString source_parameter_unique_name = source_parameter.GetUniqueName();
			if (is_array_contains(stop_names, source_parameter_unique_name)) {
				// we find parameter from the stop names list
				// it's possible only when we propagate inside the compound, because stop names are names of input ports
				// in this case we should connect input/output port to this compound input
				// instead nodename use interfacename
				// as interface name use ordinally parameter name
				std::string interface_name = source_parameter.GetName().GetAsciiString();
				if (use_input) { mx_input->setInterfaceName(interface_name); }
				else { mx_output->setInterfaceName(interface_name); }  // <-- impossible in Softimage, because compound output can not be connected to compound input directly
			}
			else {
				XSI::Shader source_shader = source_parameter.GetParent();
				if (source_shader.IsValid()) {
					if (is_compound(source_shader)) {
						if (use_doc) {
							MaterialX::NodeGraphPtr source_graph = compound_to_graph(source_shader, mx_doc, id_to_node, id_to_graph, stop_names, export_options);
							MaterialX::OutputPtr source_graph_output = source_graph->getOutput(source_parameter.GetName().GetAsciiString());
							if (use_input) { mx_input->setConnectedOutput(source_graph_output); }
							else { mx_output->setConnectedOutput(source_graph_output); }
						}
						else {
							// compounds inside compounds are not supported, skip all feather connections
							// if use_doc is false, then we make propagation inside the compound
							log_message("Nested compounds are not supported in MaterialX. Skip compound " + source_shader.GetFullName(), XSI::siSeverityType::siWarningMsg);
						}
					}
					else {
						MaterialX::NodePtr source_node = shader_to_node(source_shader, mx_doc, mx_graph, use_doc, id_to_node, id_to_graph, stop_names, export_options);
						if (use_input) { mx_input->setConnectedNode(source_node); }
						else { mx_output->setConnectedNode(source_node); }

						// if source node has only one output, then use siple connection
						// // if there are several outputs, then use one of them
						std::string source_last_output_name = "";
						size_t source_outputs = get_shader_outputs_count(source_shader, source_last_output_name);
						if (source_outputs > 1) {
							std::string out_name = source_parameter.GetName().GetAsciiString();
							if (use_input) { mx_input->setOutputString(out_name); }
							else { mx_output->setOutputString(out_name); }
						}
					}
				}
			}
		}
	}
}

MaterialX::NodePtr shader_to_node(const XSI::Shader& xsi_shader,
								  MaterialX::DocumentPtr& mx_doc, 
								  MaterialX::NodeGraphPtr &mx_graph,
								  bool use_doc,
								  std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node, 
								  std::unordered_map<ULONG, MaterialX::NodeGraphPtr>& id_to_graph,
								  std::vector<XSI::CString> &stop_names,
								  const ExportOptions& export_options) {
	XSI::CString prog_id = xsi_shader.GetProgID();
	ULONG xsi_id = xsi_shader.GetObjectID();
	XSI::CString xsi_name = export_options.use_unique_names ? xsi_shader.GetUniqueName() : xsi_shader.GetName();
	std::string render_name = prog_id_to_render(prog_id);

	std::string node_type = get_normal_type(prog_id);
	
	std::string node_output_type = "";
	size_t outputs_count = get_shader_outputs_count(xsi_shader, node_output_type);
	if (outputs_count > 1) {
		node_output_type = multioutput_name();
	}

	if (export_options.insert_nodedefs && render_name != materialx_render()) {
		// for non-default shaders we can export also all node defs
		// alwasy add it to the root doc
		// check that this node does not have definition in doc
		if (!is_values_contains(id_to_node, node_type)) {
			MaterialX::NodeDefPtr node_def = mx_doc->addNodeDef("ND_" + node_type, node_output_type, node_type);
			// remove default output
			node_def->removeOutput("out");
			// for inputs and outputs we should define only names and types
			XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();

			MaterialX::NodePtr temp_node;
			MaterialX::NodeGraphPtr temp_graph;
			for (size_t i = 0; i < shader_parameters.GetCount(); i++) {
				XSI::ShaderParameter param = shader_parameters[i];
				XSI::ShaderParamDef param_def = param.GetDefinition();

				XSI::siShaderParameterDataType xsi_type = param_def.GetDataType();
				std::string parameter_name = param.GetName().GetAsciiString();

				bool is_input = param_def.IsInput();
				bool is_output = param_def.IsOutput();
				if (is_input) {
					bool is_add = add_input_value_to_node(temp_node, temp_graph, node_def, SubjectMode::DEFINITION, parameter_name, param_def.GetDefaultValue(), param, xsi_type, export_options);
					if (is_add) {
						MaterialX::InputPtr input = node_def->getInput(parameter_name);
						XSI::CValue xsi_sugg_min = param.GetSuggestedMin();
						XSI::CValue xsi_sugg_max = param.GetSuggestedMax();
						if (!xsi_sugg_min.IsEmpty()) { input->setAttribute("uimin", value_to_string(xsi_sugg_min)); }
						if (!xsi_sugg_max.IsEmpty()) { input->setAttribute("uimax", value_to_string(xsi_sugg_max)); }
					}
					else {
						node_def->addInput(parameter_name, parameter_type_to_string(param));
					}
				}
				if (is_output) {
					node_def->addOutput(parameter_name, parameter_type_to_string(param));
				}
			}
		}
	}

	bool is_new = false;
	MaterialX::NodePtr node = get_or_create_node(id_to_node, xsi_id, mx_doc, mx_graph, use_doc, node_type, xsi_name.GetAsciiString(), node_output_type, is_new);

	MaterialX::NodeGraphPtr temp_graph;
	MaterialX::OutputPtr temp_output;
	MaterialX::NodeDefPtr temp_def;
	if (is_new) {
		node->setTarget(render_name);
		// get shader parameters
		XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();
		for (size_t i = 0; i < shader_parameters.GetCount(); i++) {
			XSI::ShaderParameter param = shader_parameters[i];
			XSI::ShaderParamDef param_def = param.GetDefinition();

			XSI::siShaderParameterDataType xsi_type = param_def.GetDataType();
			std::string parameter_name = param.GetName().GetAsciiString();

			bool is_input = param_def.IsInput();
			bool is_output = param_def.IsOutput();

			if (is_input) {
				bool is_add = add_input_value_to_node(node, temp_graph, temp_def, SubjectMode::NODE, parameter_name, param.GetValue(), param, xsi_type, export_options);
				if (!is_add) {
					std::string parameter_type = parameter_type_to_string(param);
					// non-empty paramter connected to something, then add the input port
					if (parameter_type.size() > 0 && param.GetSource().IsValid()) {
						// add simple input by name and type, without setting value
						node->addInput(parameter_name, parameter_type);
						is_add = true;
					}
				}

				if (is_add) {
					propagate_connection(param, mx_doc, mx_graph, use_doc, node->getInput(parameter_name), temp_output, true, id_to_node, id_to_graph, stop_names, export_options);
				}
			}
		}
	}

	return node;
}

MaterialX::NodeGraphPtr compound_to_graph(const XSI::Shader& xsi_compound,
										  MaterialX::DocumentPtr& mx_doc,
										  std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
										  std::unordered_map<ULONG, MaterialX::NodeGraphPtr>& id_to_graph,
										  std::vector<XSI::CString> &stop_names,
										  const ExportOptions& export_options) {
	XSI::CString xsi_prog_id = xsi_compound.GetProgID();
	ULONG xsi_id = xsi_compound.GetObjectID();
	XSI::CString xsi_name = export_options.use_unique_names ? xsi_compound.GetUniqueName() : xsi_compound.GetName();

	bool is_new = false;
	MaterialX::NodeGraphPtr graph = get_or_create_graph(id_to_graph, xsi_id, mx_doc, xsi_name.GetAsciiString(), is_new);
	MaterialX::NodeGraphPtr temp_graph;
	if (is_new) {
		// for compound we should define input and output ports
		// we should store all compound inputs
		// this will allows to stop propagation for connections
		// store inputs by unique names
		// for inputs also make outside propagation
		XSI::CParameterRefArray compound_parameters = xsi_compound.GetParameters();
		MaterialX::NodePtr temp_node;
		MaterialX::OutputPtr temp_output;
		MaterialX::NodeDefPtr temp_def;
		for (size_t i = 0; i < compound_parameters.GetCount(); i++) {
			XSI::ShaderParameter param = compound_parameters[i];
			XSI::ShaderParamDef param_def = param.GetDefinition();

			XSI::siShaderParameterDataType xsi_type = param_def.GetDataType();
			std::string parameter_name = param.GetName().GetAsciiString();

			bool is_input = param_def.IsInput();
			bool is_output = param_def.IsOutput();

			if (is_input) {
				bool is_add = add_input_value_to_node(temp_node, graph, temp_def, SubjectMode::GRAPH, parameter_name, param.GetValue(), param, xsi_type, export_options);
				if (!is_add) {
					std::string parameter_type = parameter_type_to_string(param);
					if (parameter_type.size() > 0) {
						graph->addInput(parameter_name, parameter_type);
						is_add = true;
					}
				}
				if (is_add) {
					propagate_connection(param, mx_doc, temp_graph, true, graph->getInput(parameter_name), temp_output, true, id_to_node, id_to_graph, stop_names, export_options);
				}

				// add port names to the list, but use it only later when connect compound output ports
				stop_names.push_back(param.GetUniqueName());
			}

			if (is_output) {
				// get source of the output parameter
				XSI::CRef param_source_ref = param.GetSource();
				if (param_source_ref.IsValid() && param_source_ref.GetClassID() == XSI::siShaderParameterID) {
					XSI::ShaderParameter param_source = param_source_ref;
					if (param_source.IsValid()) {
						graph->addOutput(parameter_name, parameter_type_to_string(param_source));
					}
				}
			}
		}

		// then iterate again and consider only connections
		// for outputs we should add nodes inside the graph
		MaterialX::InputPtr temp_input;
		for (size_t i = 0; i < compound_parameters.GetCount(); i++) {
			XSI::ShaderParameter param = compound_parameters[i];
			XSI::ShaderParamDef param_def = param.GetDefinition();
			std::string parameter_name = param.GetName().GetAsciiString();

			if (param_def.IsOutput()) {
				MaterialX::OutputPtr output = graph->getOutput(parameter_name);

				// here we should create nodes inside graph, instead of doc
				propagate_connection(param, mx_doc, graph, false, temp_input, output, false, id_to_node, id_to_graph, stop_names, export_options);
			}
		}
	}

	return graph;
}