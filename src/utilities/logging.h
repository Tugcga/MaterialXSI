#pragma once
#include <vector>

#include <xsi_string.h>
#include <xsi_application.h>

void log_message(XSI::CString message, XSI::siSeverityType type = XSI::siSeverityType::siInfoMsg);

XSI::CString to_string(const std::vector<int> &array);