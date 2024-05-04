#include <vector>

#include <xsi_string.h>

bool is_array_contains(const std::vector<XSI::CString>& array, const XSI::CString& value) {
	for (size_t i = 0; i < array.size(); i++) {
		if (array[i] == value) {
			return true;
		}
	}

	return false;
}