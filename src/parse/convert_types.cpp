#include <string>

#include <xsi_shaderdef.h>
#include <xsi_color4f.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>

#include "MaterialXCore/Document.h"

// convert type from MaterialX to built-in data type in Softimage
XSI::siShaderParameterDataType mx_type_to_xsi(const std::string& mx_type) {
	if (mx_type == "boolean") { return XSI::siShaderParameterDataType::siShaderDataTypeBoolean; }
	else if (mx_type == "integer") { return XSI::siShaderParameterDataType::siShaderDataTypeInteger; }
	else if (mx_type == "float") { return XSI::siShaderParameterDataType::siShaderDataTypeScalar; }
	else if (mx_type == "color3") { return XSI::siShaderParameterDataType::siShaderDataTypeColor3; }
	else if (mx_type == "color4") { return XSI::siShaderParameterDataType::siShaderDataTypeColor4; }
	else if (mx_type == "vector2") { return XSI::siShaderParameterDataType::siShaderDataTypeVector2; }
	else if (mx_type == "vector3") { return XSI::siShaderParameterDataType::siShaderDataTypeVector3; }
	else if (mx_type == "vector4") { return XSI::siShaderParameterDataType::siShaderDataTypeVector4; }
	else if (mx_type == "matrix33") { return XSI::siShaderParameterDataType::siShaderDataTypeMatrix33; }
	else if (mx_type == "matrix44") { return XSI::siShaderParameterDataType::siShaderDataTypeMatrix44; }
	else if (mx_type == "string") { return XSI::siShaderParameterDataType::siShaderDataTypeString; }
	else if (mx_type == "filename") { return XSI::siShaderParameterDataType::siShaderDataTypeImage; }
	else if (mx_type == "geomname") { return XSI::siShaderParameterDataType::siShaderDataTypeString; }  // <-- does not used in MaterialX library
	else if (mx_type == "surfaceshader") { return XSI::siShaderParameterDataType::siShaderDataTypeColor4; }
	else if (mx_type == "displacementshader") { return XSI::siShaderParameterDataType::siShaderDataTypeScalar; }
	else if (mx_type == "volumeshader") { return XSI::siShaderParameterDataType::siShaderDataTypeColor4; }
	else if (mx_type == "lightshader") { return XSI::siShaderParameterDataType::siShaderDataTypeColor4; }
	else if (mx_type == "material") { return XSI::siShaderParameterDataType::siShaderDataTypeColor4; }  // also does not used
	else { return XSI::siShaderParameterDataType::siShaderDataTypeUnknown; }  // all other types are custom
}

bool is_visible_in_ppg(const std::string& mx_type) {
	if (mx_type == "boolean" || mx_type == "integer" || mx_type == "float" || mx_type == "color3" || mx_type == "color4" || mx_type == "vector2" ||
		mx_type == "vector3" || mx_type == "vector4" || mx_type == "matrix33" || mx_type == "matrix44" || mx_type == "string" || mx_type == "filename") {
		return true;
	}

	return false;
}

bool is_type_boolean(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeBoolean) {
		return true;
	}
	return false;
}

bool is_type_numeric(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeInteger || xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeScalar) {
		return true;
	}
	return false;
}

bool is_type_color(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeColor3 || xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeColor4) {
		return true;
	}
	return false;
}

bool is_type_vector(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeVector2 || xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeVector3 || xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeVector4) {
		return true;
	}
	return false;
}

bool is_type_matrix(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeMatrix33 || xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeMatrix44) {
		return true;
	}
	return false;
}

bool is_type_string(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeString) {
		return true;
	}
	return false;
}

bool is_type_image(XSI::siShaderParameterDataType xsi_type) {
	if (xsi_type == XSI::siShaderParameterDataType::siShaderDataTypeImage) {
		return true;
	}
	return false;
}

// TODO: it's not clear how to properly set default color for input port
// if set by using SetDefaultValue and pass CColor4 object, then in the shader the color is actual black
// in Python side if pass an array [r, g, b], then allworks correctly
// but in c++ it's not clear why it is not working
// the fix: instead of color set one value, then it will be initialized by black-white value
// this will create Color3 shader parameter, but in some cases it should be Color4
float mx_color3_to_xsi_color(MaterialX::Color3 mx_color) {
	// return XSI::MATH::CColor4f(mx_color[0], mx_color[1], mx_color[2], 0.0);
	return (mx_color[0] + mx_color[1] + mx_color[2]) / 3.0f;
}

float mx_color4_to_xsi_color(MaterialX::Color4 mx_color) {
	// return XSI::MATH::CColor4f(mx_color[0], mx_color[1], mx_color[2], mx_color[3]);
	return (mx_color[0] + mx_color[1] + mx_color[2]) / 3.0f;
}

XSI::MATH::CVector2f mx_vector2_to_xsi_vector2(MaterialX::Vector2 mx_vector) {
	return XSI::MATH::CVector2f(mx_vector[0], mx_vector[1]);
}

XSI::MATH::CVector3f mx_vector3_to_xsi_vector3(MaterialX::Vector3 mx_vector) {
	return XSI::MATH::CVector3f(mx_vector[0], mx_vector[1], mx_vector[2]);
}

XSI::MATH::CVector4f mx_vector4_to_xsi_vector4(MaterialX::Vector4 mx_vector) {
	return XSI::MATH::CVector4f(mx_vector[0], mx_vector[1], mx_vector[2], mx_vector[3]);
}

XSI::MATH::CMatrix3f mx_matrix3_to_xsi_matrix3(MaterialX::Matrix33 mx_matrix) {
	return XSI::MATH::CMatrix3f(mx_matrix[0][0], mx_matrix[0][1], mx_matrix[0][2],
		mx_matrix[1][0], mx_matrix[1][1], mx_matrix[1][2],
		mx_matrix[2][0], mx_matrix[2][1], mx_matrix[2][2]);
}

XSI::MATH::CMatrix4f mx_matrix4_to_xsi_matrix4(MaterialX::Matrix44 mx_matrix) {
	return XSI::MATH::CMatrix4f(mx_matrix[0][0], mx_matrix[0][1], mx_matrix[0][2], mx_matrix[0][3],
		mx_matrix[1][0], mx_matrix[1][1], mx_matrix[1][2], mx_matrix[1][3],
		mx_matrix[2][0], mx_matrix[2][1], mx_matrix[2][2], mx_matrix[2][3],
		mx_matrix[3][0], mx_matrix[3][1], mx_matrix[3][2], mx_matrix[3][3]);
}

// these are default values of the node inputs
XSI::CValue build_value(const std::string& mx_type, MaterialX::ValuePtr mx_value) {
	if (mx_type == "boolean") { return mx_value->asA<bool>(); }
	else if (mx_type == "integer") { return mx_value->asA<int>(); }
	else if (mx_type == "float") { return mx_value->asA<float>(); }
	else if (mx_type == "color3") { return mx_color3_to_xsi_color(mx_value->asA<MaterialX::Color3>()); }
	else if (mx_type == "color4") { return mx_color4_to_xsi_color(mx_value->asA<MaterialX::Color4>()); }
	else if (mx_type == "vector2") { return mx_vector2_to_xsi_vector2(mx_value->asA<MaterialX::Vector2>()); }
	else if (mx_type == "vector3") { return mx_vector3_to_xsi_vector3(mx_value->asA<MaterialX::Vector3>()); }
	else if (mx_type == "vector4") { return mx_vector4_to_xsi_vector4(mx_value->asA<MaterialX::Vector4>()); }
	else if (mx_type == "matrix33") { return mx_matrix3_to_xsi_matrix3(mx_value->asA<MaterialX::Matrix33>()); }
	else if (mx_type == "matrix44") { return mx_matrix4_to_xsi_matrix4(mx_value->asA<MaterialX::Matrix44>()); }
	else if (mx_type == "string") { return XSI::CString(mx_value->asA<std::string>().c_str()); }
	else if (mx_type == "filename") { return ""; }
	else if (mx_type == "geomname") { return XSI::CString(mx_value->asA<std::string>().c_str()); }

	return XSI::CValue();
}