#include <string>
#include <vector>
#include <unordered_map>
#include <cctype>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>
#include <xsi_shaderparamdef.h>

#include "../parse/parse.h"
#include "../utilities/string.h"

std::string prog_id_to_render(const XSI::CString& prog_id) {
	XSI::CStringArray parts = prog_id.Split(".");
	if (parts.GetCount() > 0) {
		XSI::CString plugin = parts[0];

		if (plugin == "MaterialXSIParser") {
			return "MaterialX";
		}
		else if (plugin == "CyclesShadersPlugin") {
			return "Cycles";
		}
		else if(plugin == "OSPShadersPlugin") {
			return "OSPRay";
		}
		else if (plugin == "LUXShadersPlugin") {
			return "LuxRender";
		}
		else {
			return plugin.GetAsciiString();
		}
	}

	return "";
}

std::string materialx_render() {
	return "MaterialX";
}

// convert xsi data types to string
std::string parameter_type_to_string(const XSI::ShaderParameter &xsi_parameter) {
	// get parent node
	XSI::Shader xsi_node = xsi_parameter.GetParent();
	if (xsi_node.IsValid()) {
		XSI::CString xsi_shader_prog_id = xsi_node.GetProgID();

		std::string param_name = xsi_parameter.GetName().GetAsciiString();
		XSI::ShaderParamDef xsi_def = xsi_parameter.GetDefinition();
		bool is_input = xsi_def.IsInput();
		bool is_output = xsi_def.IsOutput();
		// check is parameter corresponds to MaterialX node
		std::string render = prog_id_to_render(xsi_shader_prog_id);
		if (render == materialx_render()) {
			// node is MaterialX
			// use global dictionary to obtain type of the parameter
			std::unordered_map<std::string, std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>, std::vector<std::tuple<std::string, std::string>>>> fullname_to_data = get_fullname_to_data();
			std::string node_type = prog_id_to_name(xsi_shader_prog_id);
			std::vector<std::tuple<std::string, std::string>> data = is_input ? get_fullname_to_inputs(node_type) : get_fullname_to_outputs(node_type);
			for (size_t i = 0; i < data.size(); i++) {
				std::tuple<std::string, std::string> one_parameter = data[i];
				std::string name = std::get<0>(one_parameter);
				std::string type = std::get<1>(one_parameter);
				if (name == param_name) {
					return type;
				}
			}
			return "";
		}
		else {
			XSI::siShaderParameterDataType xsi_type = xsi_def.GetDataType();

			if (xsi_type == XSI::siShaderDataTypeUnknown) { return ""; }
			else if (xsi_type == XSI::siShaderDataTypeBoolean) { return "boolean"; }
			else if (xsi_type == XSI::siShaderDataTypeInteger) { return "integer"; }
			else if (xsi_type == XSI::siShaderDataTypeScalar) { return "float"; }
			else if (xsi_type == XSI::siShaderDataTypeVector2) { return "vector2"; }
			else if (xsi_type == XSI::siShaderDataTypeVector3) { return "vector3"; }
			else if (xsi_type == XSI::siShaderDataTypeVector4) { return "vector4"; }
			else if (xsi_type == XSI::siShaderDataTypeQuaternion) { return "quaternion"; }  // <-- does not supported by MaterialX
			else if (xsi_type == XSI::siShaderDataTypeMatrix33) { return "matrix33"; }
			else if (xsi_type == XSI::siShaderDataTypeMatrix44) { return "matrix44"; }
			else if (xsi_type == XSI::siShaderDataTypeColor3) { return "color3"; }
			else if (xsi_type == XSI::siShaderDataTypeColor4) { return "color4"; }
			else if (xsi_type == XSI::siShaderDataTypeString) { return "string"; }
			// next are custom data types
			else if (xsi_type == XSI::siShaderDataTypeProfileCurve) { return "fcurve"; }
			else if (xsi_type == XSI::siShaderDataTypeGradient) { return "gradient"; }
			else if (xsi_type == XSI::siShaderDataTypeImage) { return "filename"; }  // <-- use the same name as in MaterialX
			else if (xsi_type == XSI::siShaderDataTypeProperty) { return "property"; }
			else if (xsi_type == XSI::siShaderDataTypeLightProfile) { return "profile"; }
			else if (xsi_type == XSI::siShaderDataTypeReference) { return "reference"; }
			else if (xsi_type == XSI::siShaderDataTypeStructure) { return "structure"; }
			else if (xsi_type == XSI::siShaderDataTypeArray) { return "array"; }
			else if (xsi_type == XSI::siShaderDataTypeCustom) {
				XSI::ValueMap attributes = xsi_def.GetAttributes();
				XSI::CString custom_name = attributes.Get("customtypename");
				if (!custom_name.IsEmpty()) {
					return custom_name.GetAsciiString();
				}
			}
		}
	}

	return "";
}

std::string colorspace_to_string(const XSI::CString& xsi_value) {
	if (xsi_value == "Automatic") {
		return "auto";
	}
	else if (xsi_value == "Linear") {
		return "lin_rec709";
	}
	else if (xsi_value == "sRGB") {
		return "srgb_texture";
	}
	else {
		return "user";
	}
}

std::string multioutput_name() { 
	return "multioutput"; 
}

std::string get_normal_type(const XSI::CString &xsi_prog_id) {
	std::string node_type = prog_id_to_name(xsi_prog_id);
	std::string render_name = prog_id_to_render(xsi_prog_id);

	if (render_name == materialx_render()) {
		node_type = get_fullname_to_type(node_type);
		// if fail to use short name, use long from the node prog id
		if (node_type.size() == 0) {
			node_type = prog_id_to_name(xsi_prog_id);
		}
	}
	else if (render_name == "Cycles") {
		// for Cycles shaders we remove the first part Cycles...
		// and then convert cammel case to snake case
		if (node_type.size() > 6) {
			std::string start_part = node_type.substr(0, 6);
			if (start_part == "Cycles") {
				std::string remain_part = node_type.substr(6);

				if (remain_part == "UVMap") { return "uv_map"; }
				else if (remain_part == "RGBCurves") { return "rgb_curves"; }
				else if (remain_part == "RGBToBW") { return "rgb_to_bw"; }
				else if (remain_part == "IESTexture") { return "ies_texture"; }

				std::string new_name = "";
				bool up_underscore = false;
				for (size_t i = 0; i < remain_part.size(); i++) {
					char c = remain_part[i];
					if ((bool)std::isupper(static_cast<unsigned char>(c))) {
						if (up_underscore) {
							new_name += "_";
						}
						new_name += std::tolower(c);
						up_underscore = false;
					}
					else {
						up_underscore = true;
						new_name += c;
					}
				}
				return new_name;
			}
		}
	}

	return node_type;
}