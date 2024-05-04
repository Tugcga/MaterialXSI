#include <vector>
#include <unordered_map>

#include <xsi_string.h>

#include "MaterialXCore/Node.h"

bool is_array_contains(const std::vector<XSI::CString>& array, const XSI::CString& value) {
	for (size_t i = 0; i < array.size(); i++) {
		if (array[i] == value) {
			return true;
		}
	}

	return false;
}

bool is_values_contains(const std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node, const std::string& value) {
	for (auto& iterator : id_to_node) {
		MaterialX::NodePtr node = iterator.second;
		std::string node_type = node->getCategory();
		if (node_type == value) {
			return true;
		}
	}

	return false;
}