#pragma once
#include <vector>

#include "export_options.h"

void export_mtlx(const std::vector<int> &object_ids, const XSI::CString &output_path, const ExportOptions &export_options);