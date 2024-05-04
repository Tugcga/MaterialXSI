#pragma once
#include <vector>

#include <xsi_string.h>

bool is_array_contains(const std::vector<XSI::CString> &array, const XSI::CString &value);
bool is_values_contains(const std::unordered_map<ULONG, MaterialX::NodePtr> &id_to_node, const std::string &value);