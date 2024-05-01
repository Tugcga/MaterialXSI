#include <vector>

#include <xsi_string.h>
#include <xsi_projectitem.h>

#include "MaterialXFormat/XmlIo.h"

#include "export_options.h"
#include "export.h"
#include "../utilities/logging.h"

void export_mtlx(const std::vector<int>& object_ids, const XSI::CString& output_path, const ExportOptions& export_options) {
	log_message("items to export: " + to_string(object_ids));
	log_message("path: " + output_path);
	log_message("textures relative: " + XSI::CString(export_options.textures.use_relative_path));
	log_message("textures should copy: " + XSI::CString(export_options.textures.copy_files));
	log_message("textures folder: " + XSI::CString(export_options.textures.copy_folder));

	// create MaterialX document
	MaterialX::DocumentPtr mx_doc = MaterialX::createDocument();

	for (size_t i = 0; i < object_ids.size(); i++) {
		size_t object_id = object_ids[i];

		// get object from it id
		XSI::ProjectItem xsi_item = XSI::Application().GetObjectFromID(object_id);
		if (xsi_item.IsValid()) {
			// get type of the object
			XSI::CString xsi_type = xsi_item.GetType();
			if (xsi_type == "material") {
				export_material((XSI::Material)xsi_item, mx_doc);
			}
			else if (xsi_type == "Shader") {
				export_shader((XSI::Shader)xsi_item, mx_doc);
			}
			// all other types are not supported
		}
	}

	// output document
	MaterialX::writeToXmlFile(mx_doc, output_path.GetAsciiString());
}