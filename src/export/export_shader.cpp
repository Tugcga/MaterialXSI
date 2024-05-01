#include <xsi_shader.h>

#include "MaterialXCore/Document.h"

#include "../utilities/logging.h"

void export_shader(const XSI::Shader &xsi_shader, MaterialX::DocumentPtr &mx_doc) {
	log_message("export shader node " + xsi_shader.GetFullName());
}