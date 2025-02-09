#include <vector>
#include <string>

#include <xsi_string.h>
#include <xsi_application.h>

void log_message(XSI::CString message, XSI::siSeverityType type) {
	XSI::Application().LogMessage("[MaterialXSI] : " + message, type);
}

XSI::CString to_string(const std::vector<int>& array) {
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