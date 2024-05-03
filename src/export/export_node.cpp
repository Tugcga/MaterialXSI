#include <string>
#include <unordered_map>

#include <xsi_shader.h>
#include <xsi_shaderparameter.h>
#include <xsi_color.h>
#include <xsi_color4f.h>
#include <xsi_vector2f.h>
#include <xsi_vector3f.h>
#include <xsi_vector4f.h>
#include <xsi_quaternionf.h>
#include <xsi_matrix3f.h>
#include <xsi_matrix4f.h>
#include <xsi_fcurve.h>
#include <xsi_fcurvekey.h>
#include <xsi_imageclip2.h>

#include "MaterialXCore/Document.h"
#include "boost/filesystem.hpp"

#include "export_options.h"
#include "export_names.h"
#include "export_utilities.h"

MaterialX::NodePtr get_or_create_node(std::unordered_map<ULONG, MaterialX::NodePtr>& id_to_node,
									  ULONG node_id,
									  MaterialX::DocumentPtr& mx_doc,
									  const std::string& node_type,
									  const std::string& node_name,
									  const std::string& node_output,
									  bool& is_create) {
	auto search = id_to_node.find(node_id);
	if (search != id_to_node.end()) {
		is_create = false;
		return search->second;
	}

	MaterialX::NodePtr new_node = mx_doc->addNode(node_type, node_name, node_output);
	id_to_node.insert({ node_id, new_node });
	is_create = true;
	return new_node;
}

void add_input_to_node(MaterialX::NodePtr& node, 
					   const std::string& name, 
					   const XSI::ShaderParameter& xsi_parameter, 
					   const XSI::siShaderParameterDataType xsi_type, 
					   const ExportOptions& export_options) {
	XSI::CValue xsi_value = xsi_parameter.GetValue();
	std::string type_string = parameter_type_to_string(xsi_type);
	if (xsi_type == XSI::siShaderDataTypeBoolean) {
		node->setInputValue(name, (bool)xsi_value);
	}
	else if (xsi_type == XSI::siShaderDataTypeInteger) {
		node->setInputValue(name, (int)xsi_value);
	}
	else if (xsi_type == XSI::siShaderDataTypeScalar) {
		node->setInputValue(name, (float)xsi_value);
	}
	else if (xsi_type == XSI::siShaderDataTypeVector2) {
		XSI::MATH::CVector2f value_vector(xsi_value);
		node->setInputValue(name, MaterialX::Vector2(value_vector.GetX(), value_vector.GetY()));
	}
	else if (xsi_type == XSI::siShaderDataTypeVector3) {
		XSI::MATH::CVector3f value_vector(xsi_value);
		node->setInputValue(name, MaterialX::Vector3(value_vector.GetX(), value_vector.GetY(), value_vector.GetZ()));
	}
	else if (xsi_type == XSI::siShaderDataTypeVector4) {
		XSI::MATH::CVector4f value_vector(xsi_value);
		node->setInputValue(name, MaterialX::Vector4(value_vector.GetX(), value_vector.GetY(), value_vector.GetZ(), value_vector.GetW()));
	}
	else if (xsi_type == XSI::siShaderDataTypeQuaternion) {
		XSI::MATH::CQuaternionf value_quaternion(xsi_value);
		// node->setInputValue(name, MaterialX::Vector4(value_quaternion.GetX(), value_quaternion.GetY(), value_quaternion.GetZ(), value_quaternion.GetW()));
		// write as custom quaternion data
		std::string data_string = std::to_string(value_quaternion.GetX()) + "," + std::to_string(value_quaternion.GetY()) + "," + std::to_string(value_quaternion.GetZ()) + "," + std::to_string(value_quaternion.GetW());
		node->setInputValue(name, data_string, "quaternion");
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix33) {
		XSI::MATH::CMatrix3f value_matrix(xsi_value);
		node->setInputValue(name, MaterialX::Matrix33(value_matrix.GetValue(0, 0), value_matrix.GetValue(0, 1), value_matrix.GetValue(0, 2),
			value_matrix.GetValue(1, 0), value_matrix.GetValue(1, 1), value_matrix.GetValue(1, 2),
			value_matrix.GetValue(2, 0), value_matrix.GetValue(2, 1), value_matrix.GetValue(2, 2)));
	}
	else if (xsi_type == XSI::siShaderDataTypeMatrix44) {
		XSI::MATH::CMatrix4f value_matrix(xsi_value);
		node->setInputValue(name, MaterialX::Matrix44(value_matrix.GetValue(0, 0), value_matrix.GetValue(0, 1), value_matrix.GetValue(0, 2), value_matrix.GetValue(0, 3),
			value_matrix.GetValue(1, 0), value_matrix.GetValue(1, 1), value_matrix.GetValue(1, 2), value_matrix.GetValue(1, 3),
			value_matrix.GetValue(2, 0), value_matrix.GetValue(2, 1), value_matrix.GetValue(2, 2), value_matrix.GetValue(2, 3),
			value_matrix.GetValue(3, 0), value_matrix.GetValue(3, 1), value_matrix.GetValue(3, 2), value_matrix.GetValue(3, 3)));
	}
	else if (xsi_type == XSI::siShaderDataTypeColor3) {
		XSI::MATH::CColor4f value_color(xsi_value);
		node->setInputValue(name, MaterialX::Color3(value_color.GetR(), value_color.GetG(), value_color.GetB()));
	}
	else if (xsi_type == XSI::siShaderDataTypeColor4) {
		XSI::MATH::CColor4f value_color(xsi_value);
		node->setInputValue(name, MaterialX::Color4(value_color.GetR(), value_color.GetG(), value_color.GetB(), value_color.GetA()));
	}
	else if (xsi_type == XSI::siShaderDataTypeString) {
		XSI::CString value_sting(xsi_value);
		node->setInputValue(name, std::string(value_sting.GetAsciiString()));
	}
	else if (xsi_type == XSI::siShaderDataTypeProfileCurve) {
		// write data in the format ltx, lty: t, v: rtx, rty;...
		std::string data_string = "";
		XSI::FCurve curve(xsi_value);
		XSI::CFCurveKeyRefArray curve_keys = curve.GetKeys();
		for (size_t i = 0; i < curve_keys.GetCount(); i++) {
			XSI::FCurveKey key(curve_keys[i]);
			data_string += std::to_string(key.GetLeftTanX()) + "," + std::to_string(key.GetLeftTanY()) + ":" +
				std::to_string(key.GetTime().GetTime()) + "," + std::to_string((float)key.GetValue()) + ":" +  // suppose that value is a float
				std::to_string(key.GetRightTanX()) + "," + std::to_string(key.GetRightTanY());

			if (i < curve_keys.GetCount() - 1) {
				data_string += ";";
			}
		}

		node->setInputValue(name, data_string, "fcurve");
	}
	else if (xsi_type == XSI::siShaderDataTypeGradient) {
		XSI::CParameterRefArray gradient_parameters = xsi_parameter.GetParameters();
		XSI::Parameter markers_parameter = gradient_parameters.GetItem("markers");
		XSI::CParameterRefArray markers = markers_parameter.GetParameters();
		ULONG markers_count = markers.GetCount();

		// store keys as pos, r, g, b, a
		std::vector<std::tuple<float, float, float, float, float>> markers_array;
		for (size_t i = 0; i < markers_count; i++) {
			XSI::ShaderParameter p = markers[i];
			float pos = p.GetParameterValue("pos");
			float mid = p.GetParameterValue("mid");
			float r = p.GetParameterValue("red");
			float g = p.GetParameterValue("green");
			float b = p.GetParameterValue("blue");
			float a = p.GetParameterValue("alpha");
			if (pos > -1) {
				markers_array.push_back(std::make_tuple(pos, r, g, b, a));
			}
		}

		// sort markers array by position
		std::sort(markers_array.begin(), markers_array.end());

		// output as string in the form: pos: r, g, b, a; pos: r, g, b, a
		std::string data_string = "";
		for (size_t i = 0; i < markers_array.size(); i++) {
			auto m = markers_array[i];

			data_string += std::to_string(std::get<0>(m)) + ":" + std::to_string(std::get<1>(m)) + "," + std::to_string(std::get<2>(m)) + "," + std::to_string(std::get<3>(m)) + "," + std::to_string(std::get<4>(m));
			if (i < markers_array.size() - 1) {
				data_string += ";";
			}
		}

		// write data string
		node->setInputValue(name, data_string, "gradient");
	}
	else if (xsi_type == XSI::siShaderDataTypeImage) {
		XSI::ShaderParameter image_parameter = get_finall_parameter(xsi_parameter);
		if (image_parameter.IsValid()) {
			XSI::CRef image_source = image_parameter.GetSource();
			if (image_source.IsValid()) {
				XSI::ImageClip2 image_clip(image_source);

				if (image_clip.IsValid()) {
					XSI::CString xsi_image_path = image_clip.GetFileName();
					XSI::CValue color_profile = image_clip.GetParameterValue("RenderColorProfile");

					boost::filesystem::path image_path = xsi_image_path.GetAsciiString();

					// extract folder with output *.mtlx file
					boost::filesystem::path output_path = export_options.output_path;
					boost::filesystem::path folder_path = output_path.parent_path();

					// copy image to the separate folder
					if (export_options.textures.copy_files) {

						// create new folder
						boost::filesystem::path textures_path = folder_path / export_options.textures.copy_folder;
						boost::filesystem::create_directories(textures_path);

						// copy the file
						boost::filesystem::copy_file(image_path, textures_path / image_path.filename(), boost::filesystem::copy_options::overwrite_existing);

						// override the path to the file
						image_path = textures_path / image_path.filename();
					}

					// convert image_path to relative, if it required
					if (export_options.textures.use_relative_path) {
						image_path = boost::filesystem::relative(image_path, folder_path);
					}
					std::string image_path_string = image_path.string();
					MaterialX::InputPtr input = node->setInputValue(name, image_path_string, "filename");
					input->setColorSpace(colorspace_to_string(color_profile));
				}
			}
		}
	}
	// does not support the following properties
	else if (xsi_type == XSI::siShaderDataTypeProperty) {}
	else if (xsi_type == XSI::siShaderDataTypeLightProfile) {}
	else if (xsi_type == XSI::siShaderDataTypeReference) {}
	else if (xsi_type == XSI::siShaderDataTypeCustom) {}
	else if (xsi_type == XSI::siShaderDataTypeStructure) {}
	else if (xsi_type == XSI::siShaderDataTypeArray) {}
}

MaterialX::NodePtr shader_to_node(const XSI::Shader& xsi_shader,
								  MaterialX::DocumentPtr& mx_doc, 
								  std::unordered_map<ULONG, 
								  MaterialX::NodePtr>& id_to_node, 
								  const ExportOptions& export_options) {
	XSI::CString prog_id = xsi_shader.GetProgID();
	ULONG xsi_id = xsi_shader.GetObjectID();
	XSI::CString xsi_name = xsi_shader.GetName();

	std::string node_type = prog_id_to_name(prog_id);
	std::string node_output_type = "";
	size_t outputs_count = get_shader_outputs_count(xsi_shader, node_output_type);
	if (outputs_count > 1) {
		node_output_type = multioutput_name();
	}
	std::string render_name = prog_id_to_render(prog_id);

	bool is_new = false;
	MaterialX::NodePtr node = get_or_create_node(id_to_node, xsi_id, mx_doc, node_type, xsi_name.GetAsciiString(), node_output_type, is_new);
	if (is_new) {
		node->setTarget(render_name);
		// get shader parameters
		XSI::CParameterRefArray shader_parameters = xsi_shader.GetParameters();
		for (size_t i = 0; i < shader_parameters.GetCount(); i++) {
			XSI::ShaderParameter param = shader_parameters[i];
			XSI::ShaderParamDef param_def = param.GetDefinition();

			XSI::siShaderParameterDataType xsi_type = param_def.GetDataType();
			std::string input_name = param.GetName().GetAsciiString();

			bool is_input = param_def.IsInput();
			bool is_output = param_def.IsOutput();

			if (is_input) {
				XSI::CValue value = param.GetValue();
				add_input_to_node(node, input_name, param, xsi_type, export_options);

				// check is this input connected
				XSI::CRef input_source = param.GetSource();
				if (input_source.IsValid()) {
					XSI::siClassID source_class = input_source.GetClassID();
					if (source_class == XSI::siShaderParameterID) {
						XSI::ShaderParameter source_parameter(input_source);
						XSI::Shader source_shader = source_parameter.GetParent();
						if (source_shader.IsValid()) {
							if (!is_compound(source_shader)) {
								MaterialX::NodePtr source_node = shader_to_node(source_shader, mx_doc, id_to_node, export_options);
								// if source node has only one output, then use siple connection
								// // if there are several outputs, then use one of them
								std::string source_last_output_name = "";
								size_t source_outputs = get_shader_outputs_count(source_shader, source_last_output_name);
								if (source_outputs == 1) {
									node->setConnectedNode(input_name, source_node);
								}
								else {
									MaterialX::InputPtr input = node->getInput(input_name);

									std::string out_name = source_parameter.GetName().GetAsciiString();
									input->setConnectedNode(source_node);
									input->setOutputString(out_name);
								}
							}
						}
					}
				}
			}
		}
	}

	return node;
}