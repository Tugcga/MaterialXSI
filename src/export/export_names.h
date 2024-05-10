#pragma once
#include <string>

#include <xsi_string.h>
#include <xsi_shader.h>

std::string prog_id_to_render(const XSI::CString& prog_id);
std::string materialx_render();
std::string parameter_type_to_string(const XSI::ShaderParameter& xsi_parameter);
std::string colorspace_to_string(const XSI::CString& xsi_value);
std::string multioutput_name();
std::string get_normal_type(const XSI::CString& xsi_prog_id);