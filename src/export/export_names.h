#pragma once
#include <string>

#include <xsi_string.h>
#include <xsi_shader.h>

std::string parameter_type_to_string(XSI::siShaderParameterDataType xsi_type);
std::string colorspace_to_string(const XSI::CString& xsi_value);
std::string multioutput_name();
std::string prog_id_to_render(const XSI::CString& prog_id);
std::string materialx_render();