#include <vector>

#include <xsi_string.h>
#include <xsi_projectitem.h>

#include "MaterialXFormat/XmlIo.h"

#include "export_options.h"
#include "export.h"
#include "../utilities/logging.h"

void export_mtlx(const std::vector<int>& object_ids, const ExportOptions& export_options) {
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
				export_material((XSI::Material)xsi_item, mx_doc, export_options);
			}
			else if (xsi_type == "Shader") {
				// TODO: we should export not only selected nodes, but also all nodes connected with it
				// it may happens that already exported node (and the corresponding chain) connected to the current one
				// in this case we should connect in MX these nodes
				// so, store the map from xsi id to mx node and use it when we find already known node
				export_shader((XSI::Shader)xsi_item, mx_doc);
			}
			// all other types are not supported
		}
	}

	// output document
	MaterialX::writeToXmlFile(mx_doc, export_options.output_path);
}