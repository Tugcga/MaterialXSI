#include <xsi_material.h>

#include "MaterialXCore/Document.h"

#include "../utilities/logging.h"

void export_material(const XSI::Material &xsi_material, MaterialX::DocumentPtr &mx_doc) {
	log_message("export material " + xsi_material.GetFullName());
}