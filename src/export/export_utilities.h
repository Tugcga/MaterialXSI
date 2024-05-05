#pragma once
#include <string>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>

enum SubjectMode
{
	NODE,
	GRAPH,
	DEFINITION
};

bool is_compound(const XSI::Shader& xsi_shader);
XSI::ShaderParameter get_finall_parameter(const XSI::ShaderParameter& parameter);
std::string prog_id_to_render(const XSI::CString& prog_id);
size_t get_shader_outputs_count(const XSI::Shader& xsi_shader, std::string& out_last_output);
std::string value_to_string(const XSI::CValue &value);