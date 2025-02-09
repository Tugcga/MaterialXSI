#pragma once
#include <vector>
#include <string>

#include <xsi_string.h>
#include <xsi_application.h>

void log_message(XSI::CString message, XSI::siSeverityType type = XSI::siSeverityType::siInfoMsg);

XSI::CString to_string(const std::vector<int> &array);
XSI::CString to_string(const std::vector<std::string>& array);