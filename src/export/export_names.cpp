#include <string>

#include <xsi_shader.h>

// convert xsi data types to string
// some of them are not match with data types from MaterialX
std::string parameter_type_to_string(XSI::siShaderParameterDataType xsi_type) {
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
	else if (xsi_type == XSI::siShaderDataTypeCustom) { return "custom"; }
	else if (xsi_type == XSI::siShaderDataTypeStructure) { return "structure"; }
	else if (xsi_type == XSI::siShaderDataTypeArray) { return "array"; }

	return "";
}

std::string colorspace_to_string(const XSI::CString& xsi_value) {
	if (xsi_value == "Automatic") {
		return "auto";
	}
	else if (xsi_value == "Linear") {
		return "raw";
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
		else {
			return plugin.GetAsciiString();
		}
	}

	return "";
}

std::string materialx_render() {
	return "MaterialX";
}