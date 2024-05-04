#include <string>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>

#include "export_names.h"

bool is_compound(const XSI::Shader& xsi_shader) {
	XSI::CString prog_id = xsi_shader.GetProgID();
	if (prog_id.GetSubString(0, 13) == "XSIRTCOMPOUND") {
		return true;
	}
	return false;
}


XSI::ShaderParameter get_finall_parameter(const XSI::ShaderParameter& parameter) {
	XSI::CRef source = parameter.GetSource();
	if (source.IsValid()) {
		XSI::CString source_class_name = source.GetClassIDName();
		XSI::siClassID source_class = source.GetClassID();
		if (source_class == XSI::siShaderParameterID) {
			XSI::ShaderParameter source_param(source);
			XSI::Shader source_node = source_param.GetParent();
			XSI::CString source_prog_id = source_node.GetProgID();
			if (source_prog_id.GetSubString(0, 13) == "XSIRTCOMPOUND") {
				return get_finall_parameter(source_param);
			}
			else {
				return parameter;
			}
		}
		else {
			return parameter;
		}
	}
	else {
		return parameter;
	}
}

std::string prog_id_to_name(const XSI::CString& prog_id) {
	// split by point and extract the second part
	// prog id has the form: Plugin.Name.Version.MinVersion
	XSI::CStringArray parts = prog_id.Split(".");
	if (parts.GetCount() > 1) {
		return parts[1].GetAsciiString();
	}

	return "";
}

size_t get_shader_outputs_count(const XSI::Shader& xsi_shader, std::string& out_last_output) {
	size_t to_return = 0;
	XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();
	ULONG params_count = shader_parameters.GetCount();
	size_t outputs_count = 0;
	for (ULONG i = 0; i < params_count; i++) {
		XSI::ShaderParameter param = shader_parameters[i];
		XSI::ShaderParamDef param_def = param.GetDefinition();

		if (param_def.IsOutput()) {
			XSI::siShaderParameterDataType data_type = param_def.GetDataType();
			std::string data_type_string = parameter_type_to_string(data_type);
			if (data_type_string != "") {
				to_return++;
				out_last_output = data_type_string;
			}
		}
	}

	return to_return;
}