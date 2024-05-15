#include <string>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>

#include "export_names.h"
#include "../parse/parse.h"
#include "../utilities/string.h"
#include "../utilities/logging.h"

bool is_compound(const XSI::Shader& xsi_shader) {
	return xsi_shader.GetShaderType() == XSI::siShaderCompound;
	/*XSI::CString prog_id = xsi_shader.GetProgID();
	if (prog_id.GetSubString(0, 13) == "XSIRTCOMPOUND") {
		return true;
	}
	return false;*/
}


XSI::ShaderParameter get_finall_parameter(const XSI::ShaderParameter& parameter) {
	XSI::CRef source = parameter.GetSource();
	if (source.IsValid()) {
		XSI::CString source_class_name = source.GetClassIDName();
		XSI::siClassID source_class = source.GetClassID();
		if (source_class == XSI::siShaderParameterID) {
			XSI::ShaderParameter source_param(source);
			XSI::Shader source_node = source_param.GetParent();
			if (is_compound(source_node)) {
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

size_t get_shader_outputs_count(const XSI::Shader& xsi_shader, std::string& out_last_output) {
	size_t to_return = 0;

	XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();
	ULONG params_count = shader_parameters.GetCount();
	size_t outputs_count = 0;
	for (ULONG i = 0; i < params_count; i++) {
		XSI::ShaderParameter param = shader_parameters[i];
		XSI::ShaderParamDef param_def = param.GetDefinition();

		if (param_def.IsOutput()) {
			std::string data_type_string = parameter_type_to_string(param);
			if (data_type_string != "") {
				to_return++;
				out_last_output = data_type_string;
			}
		}
	}

	return to_return;
}

std::string value_to_string(const XSI::CValue& value) {
	XSI::CValue::DataType type = value.m_t;
	if (type == XSI::CValue::DataType::siInt1 || type == XSI::CValue::DataType::siInt2 || type == XSI::CValue::DataType::siInt8) {
		return std::to_string((int)value);
	}
	else if (type == XSI::CValue::DataType::siFloat) {
		return std::to_string((float)value);
	}
	else if (type == XSI::CValue::DataType::siDouble) {
		return std::to_string((double)value);
	}
	else if (type == XSI::CValue::DataType::siUInt1 || type == XSI::CValue::DataType::siUInt2 || type == XSI::CValue::DataType::siUInt4 || type == XSI::CValue::DataType::siUInt8) {
		return std::to_string((int)value);
	}

	return "";
}