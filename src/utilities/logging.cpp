#include <vector>
#include <string>

#include <xsi_string.h>
#include <xsi_application.h>
#include <xsi_longarray.h>

#include "MaterialXCore/Types.h"

void log_message(XSI::CString message, XSI::siSeverityType type) {
	XSI::Application().LogMessage("[MaterialXSI] : " + message, type);
}

XSI::CString to_string(const XSI::CLongArray& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.GetCount(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.GetCount() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<int>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<float>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<long long>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<ULONG>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<LONG>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i]) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<std::string>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		to_return += XSI::CString(array[i].c_str()) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const float* data, size_t size) {
	XSI::CString to_return = "[";
	for (size_t i = 0; i < size; i++) {
		float v = data[i];
		to_return += XSI::CString(v) + (i == size - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const std::vector<uint32_t>& array) {
	XSI::CString to_return = "[";

	for (size_t i = 0; i < array.size(); i++) {
		int v = array[i];
		to_return += XSI::CString(v) + (i == array.size() - 1 ? "]" : ", ");
	}
	return to_return;
}

XSI::CString to_string(const MaterialX::Matrix44& matrix) {
	XSI::CString to_return = "";

	to_return += XSI::CString(matrix[0][0]) + ", " + XSI::CString(matrix[0][1]) + ", " + XSI::CString(matrix[0][2]) + ", " + XSI::CString(matrix[0][3]);
	to_return += "|" + XSI::CString(matrix[1][0]) + ", " + XSI::CString(matrix[1][1]) + ", " + XSI::CString(matrix[1][2]) + ", " + XSI::CString(matrix[1][3]);
	to_return += "|" + XSI::CString(matrix[2][0]) + ", " + XSI::CString(matrix[2][1]) + ", " + XSI::CString(matrix[2][2]) + ", " + XSI::CString(matrix[2][3]);
	to_return += "|" + XSI::CString(matrix[3][0]) + ", " + XSI::CString(matrix[3][1]) + ", " + XSI::CString(matrix[3][2]) + ", " + XSI::CString(matrix[3][3]);

	return to_return;
}

XSI::CString to_string(const MaterialX::Vector3& vector) {
	return "(" + XSI::CString(vector[0]) + ", " + XSI::CString(vector[1]) + ", " + XSI::CString(vector[2]) + ")";
}