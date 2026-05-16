#pragma once
#include <vector>
#include <string>

#include <xsi_string.h>
#include <xsi_application.h>
#include <xsi_longarray.h>

#include "MaterialXCore/Types.h"

void log_message(XSI::CString message, XSI::siSeverityType type = XSI::siSeverityType::siInfoMsg);

XSI::CString to_string(const XSI::CLongArray& array);
XSI::CString to_string(const std::vector<int> &array);
XSI::CString to_string(const std::vector<float>& array);
XSI::CString to_string(const std::vector<long long>& array);
XSI::CString to_string(const std::vector<ULONG>& array);
XSI::CString to_string(const std::vector<LONG>& array);
XSI::CString to_string(const std::vector<std::string>& array);
XSI::CString to_string(const float* data, size_t size);
XSI::CString to_string(const std::vector<uint32_t>& array);
XSI::CString to_string(const MaterialX::Matrix44 &matrix);
XSI::CString to_string(const MaterialX::Vector3 &vector);