#pragma once
#include <string>

#include <xsi_shaderdef.h>
#include <xsi_color4f.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>
#include <xsi_shaderstructparamdef.h>

#include "MaterialXCore/Document.h"

XSI::siShaderParameterDataType mx_type_to_xsi(const std::string& mx_type);
bool is_visible_in_ppg(const std::string& mx_type);
bool is_type_boolean(XSI::siShaderParameterDataType xsi_type);
bool is_type_numeric(XSI::siShaderParameterDataType xsi_type);
bool is_type_color(XSI::siShaderParameterDataType xsi_type);
bool is_type_vector(XSI::siShaderParameterDataType xsi_type);
bool is_type_matrix(XSI::siShaderParameterDataType xsi_type);
bool is_type_string(XSI::siShaderParameterDataType xsi_type);
bool is_type_image(XSI::siShaderParameterDataType xsi_type);
float mx_color3_to_xsi_color(MaterialX::Color3 mx_color);
float mx_color4_to_xsi_color(MaterialX::Color4 mx_color);
XSI::MATH::CVector2f mx_vector2_to_xsi_vector2(MaterialX::Vector2 mx_vector);
XSI::MATH::CVector3f mx_vector3_to_xsi_vector3(MaterialX::Vector3 mx_vector);
XSI::MATH::CVector4f mx_vector4_to_xsi_vector4(MaterialX::Vector4 mx_vector);
XSI::MATH::CMatrix3f mx_matrix3_to_xsi_matrix3(MaterialX::Matrix33 mx_matrix);
XSI::MATH::CMatrix4f mx_matrix4_to_xsi_matrix4(MaterialX::Matrix44 mx_matrix);
XSI::CValue build_value(const std::string& mx_type, MaterialX::ValuePtr mx_value);
void set_struct_default_value(const std::string& mx_type, const XSI::CValue& xsi_value, XSI::ShaderParamDefContainer& xsi_container);
