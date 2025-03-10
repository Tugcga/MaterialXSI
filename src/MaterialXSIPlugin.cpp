// MaterialXSIPlugin
// Initial code generated by Softimage SDK Wizard
// Executed Tue Apr 30 20:28:17 UTC+0500 2024 by Shekn
// 
// Tip: You need to compile the generated code before you can load the plug-in.
// After you compile the plug-in, you can load it by clicking Update All in the Plugin Manager.
#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_pluginregistrar.h>
#include <xsi_status.h>
#include <xsi_command.h>
#include <xsi_argument.h>
#include <xsi_context.h>

#include "utilities/logging.h"
#include "utilities/string.h"
#include "export/export.h"
#include "parse/parse.h"
#include "export/export_generate.h"

SICALLBACK XSILoadPlugin(XSI::PluginRegistrar& in_reg) {
	in_reg.PutAuthor("Shekn");
	in_reg.PutName("MaterialXSIPlugin");
	in_reg.PutVersion(1, 0);
	//RegistrationInsertionPoint - do not remove this line
	in_reg.RegisterCommand("MaterialXSIExport", "MaterialXSIExport");
	in_reg.RegisterShaderLanguageParser("MaterialXSIParser");

	prepare_generators(in_reg.GetFilename());

	return XSI::CStatus::OK;
}

SICALLBACK XSIUnloadPlugin(const XSI::PluginRegistrar& in_reg) {
	XSI::CString strPluginName;
	strPluginName = in_reg.GetName();
	XSI::Application().LogMessage(strPluginName + " has been unloaded.", XSI::siVerboseMsg);
	return XSI::CStatus::OK;
}

SICALLBACK MaterialXSIExport_Init(XSI::CRef& in_ctxt) {
	XSI::Context ctxt(in_ctxt);
	XSI::Command cmd;
	cmd = ctxt.GetSource();
	cmd.PutDescription("Export array of items to *.mtlx");
	cmd.SetFlag(XSI::siNoLogging, false);

	XSI::ArgumentArray args;
	args = cmd.GetArguments();
	XSI::CValueArray empty_array(0);
	args.Add("object_ids", empty_array);
	args.Add("file_path", "");
	args.Add("insert_nodedefs", false);
	args.Add("textures_use_relative_path", true);
	args.Add("textures_copy", true);
	args.Add("textures_folder", "textures");
	args.Add("material_all_nodes", false);  // if true then export all shaders, not only connected to the root node
	args.Add("material_priority", true);  // if true, then export only material connection (if it contains MaterialX node), if false - export all connections
	args.Add("format", "mtlx");

	return XSI::CStatus::OK;
}

SICALLBACK MaterialXSIExport_Execute(XSI::CRef& in_ctxt) {
	XSI::Context ctxt(in_ctxt);
	XSI::CValueArray args = ctxt.GetAttribute("Arguments");

	//extract input arguments
	XSI::CValueArray in_objects = args[0];
	XSI::CString file_path = args[1];
	bool insert_nodedefs = args[2];
	bool textures_use_relative_path = args[3];
	bool textures_copy = args[4];
	XSI::CString textures_folder = args[5];
	bool material_all_nodes = args[6];
	bool material_priority = args[7];
	XSI::CString format = args[8];  // if format is differ from mtlx, then we should use shader generators

	ExportOptions export_options;
	export_options.output_path = file_path.GetAsciiString();
	// ensure that the directory is exist
	check_output_path(export_options.output_path);

	export_options.insert_nodedefs = insert_nodedefs;
	ExportTextureOptions export_textures;
	export_textures.use_relative_path = textures_use_relative_path;
	export_textures.copy_files = textures_copy;
	export_textures.copy_folder = textures_folder.GetAsciiString();
	ExportMaterialOptions export_materials;
	export_materials.all_shaders = material_all_nodes;
	export_materials.material_priority = material_priority;

	export_options.textures = export_textures;
	export_options.materials = export_materials;

	export_options.format = format == "osl" ? ExportFormat::OSL : 
						   (format == "glsl" ? ExportFormat::GLSL : 
						   (format == "mdl" ? ExportFormat::MDL : 
						   (format == "msl" ? ExportFormat::MSL : ExportFormat::MTLX)));

	if (in_objects.GetCount() > 0) {
		if (file_path.Length() > 0) {
			std::vector<int> object_ids;
			for (size_t i = 0; i < in_objects.GetCount(); i++) {
				XSI::CValue value = in_objects[i];
				XSI::CValue::DataType value_type = value.m_t;
				if (value_type == XSI::CValue::DataType::siInt4) {
					object_ids.push_back((int)value);
				}
				else {
					log_message("Try to export item " + XSI::CString(value) + ", but it's not an ID", XSI::siWarningMsg);
				}
			}

			export_mtlx(object_ids, export_options);
		}
		else {
			log_message("Invalid output file path", XSI::siWarningMsg);
		}
	}
	else {
		log_message("The array of items for export is empty, nothing to do", XSI::siWarningMsg);
	}

	return XSI::CStatus::OK;
}


SICALLBACK MaterialXSIParser_QueryParserSettings(XSI::CRef& in_ctxt) {
	XSI::Context context(in_ctxt);

	return on_query_parser_settings(context);
}

SICALLBACK MaterialXSIParser_ParseInfo(XSI::CRef& in_ctxt) {
	XSI::Context context(in_ctxt);

	return on_parse_info(context);
}

SICALLBACK MaterialXSIParser_Parse(XSI::CRef& in_ctxt) {
	XSI::Context context(in_ctxt);

	return on_parse(context);
}