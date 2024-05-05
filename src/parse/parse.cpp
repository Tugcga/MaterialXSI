#include <unordered_map>

#include <xsi_status.h>
#include <xsi_context.h>
#include <xsi_shaderdef.h>
#include <xsi_shaderparamdefcontainer.h>
#include <xsi_shaderparamdef.h>
#include <xsi_shaderparamdefoptions.h>
#include <xsi_factory.h>
#include <xsi_color4f.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>
#include <xsi_ppglayout.h>
#include <xsi_floatarray.h>

#include "MaterialXCore/Document.h"
#include <MaterialXFormat/XmlIo.h>

#include "../utilities/logging.h";
#include "../utilities/string.h"
#include "convert_types.h"

// store two maps: from full name to short name and from short name to array of full names
std::unordered_map<std::string, std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>, std::vector<std::tuple<std::string, std::string>>>> fullname_to_data;
std::unordered_map<std::string, std::vector<std::string>> short_to_fullname;

std::unordered_map<std::string, std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>, std::vector<std::tuple<std::string, std::string>>>> get_fullname_to_data() {
	return fullname_to_data;
}

XSI::CStatus on_query_parser_settings(XSI::Context& context) {
	context.PutAttribute("FileTypes", ".mtlx");
	context.PutAttribute("Folders", "material_x");

	return XSI::CStatus::OK;
}

XSI::CStatus on_parse_info(XSI::Context& context) {
	XSI::CString filepath = context.GetAttribute("Filename");
	
	MaterialX::DocumentPtr doc = MaterialX::createDocument();
	MaterialX::readFromXmlFile(doc, filepath.GetAsciiString());

	// at start we should parse the whole file and find all nodes with the same name (but different inputs and outputs)
	// this will be used to properly define the node categories
	std::vector<MaterialX::NodeDefPtr> definitions = doc->getNodeDefs();
	// if the file does not contains node definitions, nothing to do
	if (definitions.size() == 0) {
		return XSI::CStatus::OK;
	}

	std::string library = get_library_name(filepath.GetAsciiString());

	for (size_t i = 0; i < definitions.size(); i++) {
		MaterialX::NodeDefPtr node_def = definitions[i];

		std::string node_full_name = node_def->getName();
		std::string node_name = node_def->getNodeString();

		std::vector<std::tuple<std::string, std::string>> inputs_data;
		std::vector<MaterialX::InputPtr> node_inputs = node_def->getInputs();
		for (size_t j = 0; j < node_inputs.size(); j++) {
			MaterialX::InputPtr input = node_inputs[j];
			std::string input_name = input->getName();
			std::string input_type = input->getType();
			inputs_data.push_back({ input_name, input_type });
		}
		std::vector<std::tuple<std::string, std::string>> outputs_data;
		std::vector<MaterialX::OutputPtr> node_outputs = node_def->getOutputs();
		for (size_t j = 0; j < node_outputs.size(); j++) {
			MaterialX::OutputPtr output = node_outputs[j];
			std::string output_name = output->getName();
			std::string output_type = output->getType();
			outputs_data.push_back({ output_name, output_type });
		}
		fullname_to_data.insert({ node_full_name, {node_name, inputs_data, outputs_data } });

		auto search = short_to_fullname.find(node_name);
		if (search != short_to_fullname.end()) {
			search->second.push_back(node_full_name);
		}
		else {
			short_to_fullname.insert({ node_name, {node_full_name} });
		}
	}

	// we should set the following attributes:
	// ClassName - this is the name of the shader in prog id
	// MajorVersion and MinorVersion - by default set 1 and 0
	// Category - where the shader placed in the hierarhy
	// DisplayName - readable name of the class name
	
	// here we use the for-loop, but it properly works only for one definition
	// the last definition reqrite data about all others
	for (size_t i = 0; i < definitions.size(); i++) {
		MaterialX::NodeDefPtr node_def = definitions[i];

		// we skip nodes with material output
		// in standart there are only two such nodes: surfacematerial and volumematerial
		// we use these nodes implicitly when export material

		std::string node_full_name = node_def->getName();
		if (node_full_name == "ND_surfacematerial" || node_full_name == "ND_volumematerial") {
			continue;
		}

		std::string node_name = node_def->getNodeString();
		std::string node_group = node_def->getNodeGroup();

		context.PutAttribute("ClassName", node_full_name.c_str());
		context.PutAttribute("MajorVersion", 1);
		context.PutAttribute("MinorVersion", 0);
		// for the category we use MaterialX node category and also add it short name as subcategory if there are several nodes with different full names
		// category has the form: MaterialX/Library/NodeGroup/...
		// where ... is either empty or short name of the node with different full names
		XSI::CString category_str = "MaterialX";
		if (library.size() > 0) {
			category_str += "/" + XSI::CString(snake_to_space_cammel(library).c_str());
		}
		if (node_group.size() > 0) {
			category_str += "/" + XSI::CString(snake_to_space_cammel(node_group).c_str());
		}

		auto short_to_full_search = short_to_fullname.find(node_name);
		if (short_to_full_search != short_to_fullname.end()) {
			std::vector<std::string> full_names = short_to_full_search->second;
			if (full_names.size() > 1) {
				category_str += "/" + XSI::CString(snake_to_space_cammel(node_name).c_str());
			}
		}
		context.PutAttribute("Category", category_str);
		// finally we should set display name
		// erase ND_ if it presented in full name
		std::string display_name = node_full_name;
		if (display_name.size() > 3) {
			std::string part = display_name.substr(0, 3);
			if (part == "ND_") {
				display_name = display_name.substr(3);
			}
		}

		// add MX prefix and convert to spaced cammel
		display_name = "MX " + snake_to_space_cammel(display_name);
		context.PutAttribute("DisplayName", XSI::CString(display_name.c_str()));
	}

	return XSI::CStatus::OK;
}

XSI::CStatus on_parse(XSI::Context& context) {
	XSI::CString file_path = context.GetAttribute("Filename");
	XSI::ShaderDef shader_def = context.GetAttribute("Definition");
	XSI::CString shader_prog_id = shader_def.GetProgID();

	// create the doc
	MaterialX::DocumentPtr doc = MaterialX::createDocument();
	MaterialX::readFromXmlFile(doc, file_path.GetAsciiString());

	// extract class name from prog id
	std::string class_name = prog_id_to_name(shader_prog_id);
	MaterialX::NodeDefPtr mx_def = doc->getNodeDef(class_name);

	// for the shader we should define:
	// shader family
	// inputs
	// outputs
	// ppg
	// render def with symbol name

	// shader family
	// we define the family as follows
	// if the node has output with surfaceshader type, then it's main material node, set it siShaderFamilySurfaceMat
	// with output volumeshader - siShaderFamilyVolume
	// all other are siShaderFamilyTexture
	std::vector<MaterialX::OutputPtr> mx_outputs = mx_def->getOutputs();
	bool is_define_family = false;
	for (size_t i = 0; i < mx_outputs.size(); i++) {
		MaterialX::OutputPtr mx_output = mx_outputs[i];
		std::string mx_output_type = mx_output->getType();
		if (mx_output_type == "surfaceshader") {
			shader_def.AddShaderFamily(XSI::siShaderFamilySurfaceMat);
			is_define_family = true;
			break;
		}
		else if (mx_output_type == "volumeshader") {
			shader_def.AddShaderFamily(XSI::siShaderFamilyVolume);
			is_define_family = true;
			break;
		}
	}
	if (!is_define_family) {
		shader_def.AddShaderFamily(XSI::siShaderFamilyTexture);
	}

	XSI::Factory xsi_factory = XSI::Application().GetFactory();
	XSI::PPGLayout shader_ppg = shader_def.GetPPGLayout();
	shader_ppg.Clear();
	// start parameters group
	shader_ppg.AddGroup("Parameters");

	// input ports
	XSI::ShaderParamDefContainer shader_inputs = shader_def.GetInputParamDefs();
	std::vector<MaterialX::InputPtr> mx_inputs = mx_def->getInputs();
	for (size_t i = 0; i < mx_inputs.size(); i++) {
		MaterialX::InputPtr mx_input = mx_inputs[i];

		std::string input_name = mx_input->getName();
		std::string input_type = mx_input->getType();
		MaterialX::ValuePtr input_value = mx_input->getValue();
		std::string input_value_string = mx_input->getValueString();

		// additional attributes
		std::string input_ui_name = mx_input->getAttribute("uiname");
		std::string input_ui_min = mx_input->getAttribute("uimin");
		std::string input_ui_max = mx_input->getAttribute("uimax");
		std::string input_enum = mx_input->getAttribute("enum");
		std::string input_enum_values = mx_input->getAttribute("enumvalues");

		XSI::siShaderParameterDataType xsi_type = mx_type_to_xsi(input_type);
		bool is_numeric = is_type_numeric(xsi_type);

		// create parameter options
		XSI::ShaderParamDefOptions options = xsi_factory.CreateShaderParamDefOptions();
		options.SetTexturable(true);
		options.SetAnimatable(true);
		options.SetReadOnly(false);
		options.SetInspectable(true);
		
		// parse default value
		XSI::CValue xsi_value;
		if (input_value_string.size() > 0) {
			xsi_value = build_value(input_type, input_value);
			if (!xsi_value.IsEmpty()) {
				options.SetDefaultValue(xsi_value);
			}
		}

		// define min/max ui values
		if (is_numeric) {
			if (input_ui_min.size() > 0 && input_ui_max.size() > 0) {
				if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeInteger) {
					options.SetSoftLimit(std::stoi(input_ui_min), std::stoi(input_ui_max));
				}
				else if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeScalar) {
					options.SetSoftLimit(std::stof(input_ui_min), std::stof(input_ui_max));
				}
			}
			else if (!xsi_value.IsEmpty()) {
				if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeInteger) {
					int xsi_value_int = xsi_value;
					if (xsi_value_int != 0) {
						options.SetSoftLimit(xsi_value_int / 4, xsi_value_int * 4);
					}
				}
				else if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeScalar) {
					float xsi_value_float = xsi_value;
					if (std::abs(xsi_value_float) > 0.1) {
						options.SetSoftLimit(xsi_value_float / 4.0f, xsi_value_float * 4.0f);
					}
				}
			}
		}

		// define ui name
		XSI::CString xsi_ui_name = input_name.c_str();
		if (input_ui_name.size() > 0) {
			xsi_ui_name = input_ui_name.c_str();
		}
		options.SetLongName(xsi_ui_name);

		// define input parameter
		XSI::ShaderParamDef shader_param_def = shader_inputs.AddParamDef(input_name.c_str(), xsi_type, options);

		// next add this input to ppg
		if (is_visible_in_ppg(input_type)) {
			if (is_type_color(xsi_type)) {
				// color
				shader_ppg.AddColor(input_name.c_str(), xsi_ui_name);
			}
			else if (input_enum.size() > 0) {
				// enum
				XSI::CValueArray enums;
				std::vector<std::string> enum_parts = split_string(input_enum, ',', true);
				std::vector<std::string> enum_values_parts = split_string(input_enum_values, ',', true);
				for (size_t e = 0; e < enum_parts.size(); e++) {
					enums.Add(XSI::CString(enum_parts[e].c_str()));
					if (e < enum_values_parts.size()) {
						std::string ev = enum_values_parts[e];
						if (input_type == "integer") {
							enums.Add(std::stoi(ev));
						}
						else {
							enums.Add(XSI::CString(ev.c_str()));
						}
					}
					else {
						enums.Add(XSI::CString(enum_parts[e].c_str()));
					}
				}
				shader_ppg.AddEnumControl(input_name.c_str(), enums, xsi_ui_name);
			}
			else {
				// all other items
				shader_ppg.AddItem(input_name.c_str(), xsi_ui_name);
			}
		}
	}

	// finish parameters group
	shader_ppg.EndGroup();

	// outputs
	XSI::ShaderParamDefContainer shader_outputs = shader_def.GetOutputParamDefs();
	for (size_t i = 0; i < mx_outputs.size(); i++) {
		MaterialX::OutputPtr mx_output = mx_outputs[i];

		std::string output_name = mx_output->getName();
		std::string output_type = mx_output->getType();

		shader_outputs.AddParamDef(output_name.c_str(), mx_type_to_xsi(output_type));
	}

	// render target
	XSI::MetaShaderRendererDef render_def = shader_def.AddRendererDef("MaterialX");
	render_def.PutSymbolName(class_name.c_str());

	return XSI::CStatus::OK;
}