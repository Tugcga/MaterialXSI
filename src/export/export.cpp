#include <vector>

#include <xsi_string.h>
#include <xsi_projectitem.h>

#include "MaterialXFormat/XmlIo.h"

#include "export_options.h"
#include "export.h"
#include "../utilities/logging.h"

void export_mtlx(const std::vector<int>& object_ids, ExportOptions& export_options) {
	// create MaterialX document
	MaterialX::DocumentPtr mx_doc = MaterialX::createDocument();

	std::vector<XSI::Material> materials;
	std::vector<XSI::Shader> shaders;

	// recognize materials and shader nodes
	for (size_t i = 0; i < object_ids.size(); i++) {
		size_t object_id = object_ids[i];

		// get object from it id
		XSI::ProjectItem xsi_item = XSI::Application().GetObjectFromID(object_id);

		if (xsi_item.IsValid()) {
			// get type of the object
			XSI::CString xsi_type = xsi_item.GetType();
			if (xsi_type == "material") {
				materials.push_back((XSI::Material)xsi_item);
			}
			else if (xsi_type == "Shader") {
				shaders.push_back((XSI::Shader)xsi_item);
			}
			// all other types are not supported
		}
	}

	// before start export, check is we export only one matrial or several ones
	// if several ones, then use unique names of nodes
	// for one material use simple node names, because it always unique in the scope of one material
	if (materials.size() > 1) {
		export_options.use_unique_names = true;
	}
	else {
		export_options.use_unique_names = false;
	}

	// separatly export materails
	for (size_t i = 0; i < materials.size(); i++) {
		export_material(materials[i], mx_doc, export_options);
	}

	// and all shaders
	export_shaders(shaders, mx_doc, export_options);

	// output document
	MaterialX::writeToXmlFile(mx_doc, export_options.output_path);
}