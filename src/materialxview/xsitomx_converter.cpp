#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_selection.h>
#include <xsi_polygonmesh.h>
#include <xsi_geometryaccessor.h>
#include <xsi_x3dobject.h>
#include <xsi_primitive.h>
#include <xsi_vertex.h>
#include <xsi_polygonnode.h>
#include <xsi_material.h>
#include <xsi_kinematics.h>
#include <xsi_transformation.h>
#include <xsi_shaderarrayparameter.h>
#include <xsi_shader.h>
#include <xsi_clusterproperty.h>

#include "MaterialXRender/Mesh.h"
#include "MaterialXRender/ImageHandler.h"
#include "MaterialXRender/ShaderRenderer.h"
#include "MaterialXCore/Document.h"
#include "MaterialXFormat/File.h"
#include "MaterialXFormat/Util.h"
#include "MaterialXGenShader/GenContext.h"
#include "MaterialXRenderGlsl/GlslMaterial.h"

#include "../utilities/math.h"
#include "../utilities/logging.h"
#include "../utilities/array.h"
#include "../export/export.h"
#include "../export/export_options.h"
#include "../export/export_utilities.h"
#include "../extern/mikktspace.h"

#include <vector>
#include <cfloat>
#include <chrono>

struct MikkMeshData {
	LONG triangles_count;
	LONG* nodes;  // pointer to the array with triangle vertex (nodes) indices
	float* positions;
	float* normals;
	float* uvs;
	float* out_tangents;
	float* out_bitangents;
};

// get from Sycles
XSI::Property get_xsi_object_property(XSI::X3DObject& xsi_object, const XSI::CString& property_name) {
	XSI::CRefArray props = xsi_object.GetProperties();
	for (LONG i = 0; i < props.GetCount(); i++) {
		XSI::CRef prop(props[i]);
		if (prop.GetClassID() == XSI::siCustomPropertyID) {
			XSI::CustomProperty custom_prop(prop);
			XSI::CString custom_prop_type = custom_prop.GetType();
			if (custom_prop_type == property_name) {
				return custom_prop;
			}
		}
		else if (prop.GetClassID() == XSI::siPropertyID) {
			XSI::Property xsi_prop(prop);
			XSI::CString xsi_prop_type = xsi_prop.GetType();
			if (xsi_prop_type == property_name) {
				return xsi_prop;
			}
		}
	}

	XSI::Property empty_prop;
	return empty_prop;
}

// get from sitoa
void get_geo_accessor_normals(const XSI::CGeometryAccessor& in_geo_acc, LONG in_normal_indices_size, XSI::CFloatArray& out_node_normals) {
	XSI::CRefArray user_normals_refs = in_geo_acc.GetUserNormals();
	if (user_normals_refs.GetCount() <= 0) {
		in_geo_acc.GetNodeNormals(out_node_normals);
	}
	else {
		XSI::ClusterProperty cluster_prop(user_normals_refs[0]);
		XSI::CClusterPropertyElementArray cluster_prop_elements = cluster_prop.GetElements();

		const LONG cluster_element_count = cluster_prop_elements.GetCount();
		if (cluster_element_count <= in_normal_indices_size) {
			cluster_prop.GetValues(out_node_normals);
		}
		else {
			out_node_normals.Resize(in_normal_indices_size * 3);
			XSI::CDoubleArray tmp;
			float* nrm = (float*)out_node_normals.GetArray();
			for (LONG i = 0; i < in_normal_indices_size; i++, nrm += 3) {
				tmp = cluster_prop_elements.GetItem(i);
				nrm[0] = float(tmp[0]);
				nrm[1] = float(tmp[1]);
				nrm[2] = float(tmp[2]);
			}
		}
	}
}

/*
* utility function
* it create the map from the mesh node indices to vertex indices
* in XSI the same mesh vertex corresponds to different nodes (each node is an corner of the incident polygon)
* for valid normals and uvs- we should export each node
* but in the xsi-api we can obtain only vertex positions
* so, we required map to obtain vertex index for a given node
*/
std::vector<LONG> build_node_to_vertex_map(const XSI::CGeometryAccessor& geometry, size_t nodes_count) {
	XSI::CLongArray triangle_nodes;
	XSI::CLongArray triangle_vertices;
	geometry.GetTriangleNodeIndices(triangle_nodes);
	geometry.GetTriangleVertexIndices(triangle_vertices);

	LONG triangles_count = geometry.GetTriangleCount();
	LONG tri_indices_count = triangles_count * 3;

	std::vector<LONG> xsi_node_to_vertex(nodes_count, 0);
	LONG samples_count = triangle_nodes.GetCount();

	LONG* raw_tri_nodes = (LONG*)triangle_nodes.GetArray();
	LONG* raw_tri_verts = (LONG*)triangle_vertices.GetArray();

	for (LONG i = 0; i < samples_count; i++) {
		xsi_node_to_vertex[raw_tri_nodes[i]] = raw_tri_verts[i];
	}

	return xsi_node_to_vertex;
}

void multiply_vector_to_rotation(const XSI::MATH::CVector3 &in_vector,
								 const XSI::MATH::CMatrix3 &in_matrix,
								 float &out_x, float &out_y, float &out_z) {
	out_x = (float)(in_vector.GetX() * in_matrix.GetValue(0, 0) +
		in_vector.GetY() * in_matrix.GetValue(1, 0) +
		in_vector.GetZ() * in_matrix.GetValue(2, 0));

	out_y = (float)(in_vector.GetX() * in_matrix.GetValue(0, 1) +
		in_vector.GetY() * in_matrix.GetValue(1, 1) +
		in_vector.GetZ() * in_matrix.GetValue(2, 1));

	out_z = (float)(in_vector.GetX() * in_matrix.GetValue(0, 2) +
		in_vector.GetY() * in_matrix.GetValue(1, 2) +
		in_vector.GetZ() * in_matrix.GetValue(2, 2));
}



/*
* this function convert input XSI polygon mesh object to format acceptable by MaterialX
* we retrun MaterialX mesh and array of material IDs
* these IDs are materials, used by mesh clusters
* if there are no separate clusters, then this array contains only one value - ID of the obejct material
* the number of partitions in the MaterialX mesh coincide with the number of different materials
* it follows in the same order
*/
std::tuple<MaterialX::MeshPtr, std::vector<ULONG>, long long> xsipolymesh_to_mxmesh(XSI::X3DObject& xsi_object) {
	if (!xsi_object.IsValid()) {
		return { nullptr, {}, 0 };
	}

	XSI::CString xsi_type = xsi_object.GetType();
	if (xsi_type != "polymsh") {
		return { nullptr, {}, 0 };
	}

	auto time_start = std::chrono::steady_clock::now();

	XSI::CString object_name = xsi_object.GetFullName();
	XSI::MATH::CTransformation object_tfm = xsi_object.GetKinematics().GetGlobal().GetTransform();
	XSI::MATH::CRotation object_rotation = object_tfm.GetRotation();
	XSI::MATH::CMatrix3 object_rotation_matrix = object_rotation.GetMatrix();

	// create the MaterialX mesh
	MaterialX::MeshPtr mx_mesh = MaterialX::Mesh::create(object_name.GetAsciiString());
	// and required geomerty streams
	MaterialX::MeshStreamPtr mx_position_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::POSITION_ATTRIBUTE, MaterialX::MeshStream::POSITION_ATTRIBUTE, 0);
	MaterialX::MeshStreamPtr mx_normal_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::NORMAL_ATTRIBUTE, MaterialX::MeshStream::NORMAL_ATTRIBUTE, 0);
	
	MaterialX::Vector3 box_min = { FLT_MAX, FLT_MAX, FLT_MAX };
	MaterialX::Vector3 box_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

	XSI::Property geo_property = get_xsi_object_property(xsi_object, "geomapprox");
	bool is_geo_prop = geo_property.IsValid();
	LONG geo_subdivs = 0;
	float geo_angle = 60.0;
	bool geo_use_angle = true;
	if (is_geo_prop)
	{
		geo_subdivs = geo_property.GetParameterValue("gapproxmordrsl", DBL_MAX);
		geo_angle = geo_property.GetParameterValue("gapproxmoan", DBL_MAX);
		geo_use_angle = geo_property.GetParameterValue("gapproxmoad", DBL_MAX);
	}
	XSI::PolygonMesh polymesh = xsi_object.GetActivePrimitive().GetGeometry(DBL_MAX, XSI::siConstructionModeSecondaryShape);
	XSI::CGeometryAccessor geometry = polymesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, geo_subdivs, false, geo_use_angle, geo_angle);
	XSI::CRefArray uv_refs = geometry.GetUVs();
	LONG uv_count = uv_refs.GetCount();

	// materials
	XSI::CRefArray geometry_materials = geometry.GetMaterials();
	LONG materials_count = geometry_materials.GetCount();
	std::vector<ULONG> material_ids;
	material_ids.reserve(materials_count);
	for (LONG i = 0; i < geometry_materials.GetCount(); i++) {
		XSI::Material material = geometry_materials[i];
		ULONG material_id = material.GetObjectID();

		material_ids.push_back(material_id);
	}

	// create as many partitions as the number of used materials
	std::vector<MaterialX::MeshPartitionPtr> partitions;
	// each partition corresponds to the some material, so, the number of partitions coincide with the number of materials
	partitions.reserve(materials_count);
	// join with previous loop
	for (size_t i = 0; i < geometry_materials.GetCount(); i++) {
		partitions.push_back(MaterialX::MeshPartition::create());
	}

	// now actually get mesh data
	XSI::CLongArray triangle_nodes;
	XSI::CDoubleArray vertex_positions;
	XSI::CLongArray polygon_materials;
	XSI::CLongArray triangle_polygons;
	XSI::CFloatArray node_normals;
	LONG triangles_count = geometry.GetTriangleCount();
	LONG nodes_count = geometry.GetNodeCount();
	geometry.GetTriangleNodeIndices(triangle_nodes);
	geometry.GetVertexPositions(vertex_positions);
	geometry.GetPolygonMaterialIndices(polygon_materials);
	geometry.GetPolygonTriangleIndices(triangle_polygons);
	geometry.GetNodeNormals(node_normals);
	get_geo_accessor_normals(geometry, nodes_count, node_normals);

	std::vector<LONG> xsi_node_to_vertex = build_node_to_vertex_map(geometry, nodes_count);

	std::vector<float>& mx_position_array = mx_position_stream->getData();
	std::vector<float>& mx_normal_array = mx_normal_stream->getData();
	mx_position_array.resize(nodes_count * 3);
	mx_normal_array.resize(nodes_count * 3);

	XSI::MATH::CVector3 point; // we will use this point to transform local coordinates to global coordinates
	XSI::MATH::CVector3 vector; // this value we will use for normals

	float* mx_position_raw = mx_position_array.data();
	float* mx_normal_raw = mx_normal_array.data();
	float* xsi_normal_raw = (float*)node_normals.GetArray();
	for (LONG i = 0; i < nodes_count; i++) {
		LONG v_index = xsi_node_to_vertex[i];

		LONG v_index_coord = v_index * 3;
		LONG n_index_coord = i * 3;

		double x_raw = vertex_positions[v_index_coord];
		double y_raw = vertex_positions[v_index_coord + 1];
		double z_raw = vertex_positions[v_index_coord + 2];
		point.Set(x_raw, y_raw, z_raw);
		XSI::MATH::CVector3 global_pos = XSI::MATH::MapObjectPositionToWorldSpace(object_tfm, point);
		float x = (float)global_pos.GetX();
		float y = (float)global_pos.GetY();
		float z = (float)global_pos.GetZ();

		mx_position_raw[n_index_coord] = x;
		mx_position_raw[n_index_coord + 1] = y;
		mx_position_raw[n_index_coord + 2] = z;

		// also transform local normals
		// we simply multiplay local normals to rotation matrix
		float n_x = xsi_normal_raw[n_index_coord];
		float n_y = xsi_normal_raw[n_index_coord + 1];
		float n_z = xsi_normal_raw[n_index_coord + 2];
		vector.Set(n_x, n_y, n_z);
		float out_n_x = 0.0f;
		float out_n_y = 0.0f;
		float out_n_z = 0.0f;
		multiply_vector_to_rotation(vector, object_rotation_matrix, out_n_x, out_n_y, out_n_z);

		mx_normal_raw[n_index_coord] = out_n_x;
		mx_normal_raw[n_index_coord + 1] = out_n_y;
		mx_normal_raw[n_index_coord + 2] = out_n_z;

		// and also update bounding box
		box_min[0] = math_min(x, box_min[0]); box_max[0] = math_max(x, box_max[0]);
		box_min[1] = math_min(y, box_min[1]); box_max[1] = math_max(y, box_max[1]);
		box_min[2] = math_min(z, box_min[2]); box_max[2] = math_max(z, box_max[2]);
	}

	// reserve indices arrays for each partition
	LONG default_indices_count = (triangles_count * 3) / (materials_count > 0 ? materials_count : 1);
	for (auto& part : partitions) {
		part->getIndices().reserve(default_indices_count);
	}

	// fill indices data
	LONG* triangle_nodes_raw = (LONG*)triangle_nodes.GetArray();
	LONG* triangle_polygons_raw = (LONG*)triangle_polygons.GetArray();
	LONG* polygon_materials_raw = (LONG*)polygon_materials.GetArray();

	for (LONG i = 0; i < triangles_count; i++) {
		LONG material_index = polygon_materials_raw[triangle_polygons_raw[i]];
		auto& indices = partitions[material_index]->getIndices();

		LONG triangle_offset = i * 3;
		indices.push_back(triangle_nodes_raw[triangle_offset]);
		indices.push_back(triangle_nodes_raw[triangle_offset + 1]);
		indices.push_back(triangle_nodes_raw[triangle_offset + 2]);
	}

	// define required mesh components
	mx_mesh->addStream(mx_position_stream);
	mx_mesh->addStream(mx_normal_stream);

	// now create and fill data for uv streams
	std::vector<MaterialX::MeshStreamPtr> mx_uv_streams(uv_count);
	for (LONG uv_index = 0; uv_index < uv_count; uv_index++) {
		XSI::ClusterProperty uv_prop(uv_refs[uv_index]);
		XSI::CFloatArray uv_values;
		uv_prop.GetValues(uv_values);

		MaterialX::MeshStreamPtr mx_texcoord_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::TEXCOORD_ATTRIBUTE + "_" + std::to_string(uv_index), MaterialX::MeshStream::TEXCOORD_ATTRIBUTE, uv_index);
		mx_texcoord_stream->setStride(MaterialX::MeshStream::STRIDE_2D);
		std::vector<float>& mx_texcoord_array = mx_texcoord_stream->getData();
		mx_texcoord_array.resize(nodes_count * 2);
		float* mx_texcoord_raw = mx_texcoord_array.data();

		for (LONG i = 0; i < nodes_count; i++) {
			mx_texcoord_raw[2 * i] = uv_values[3 * i];
			mx_texcoord_raw[2 * i + 1] = uv_values[3 * i + 1];
		}

		mx_mesh->addStream(mx_texcoord_stream);
		mx_uv_streams[uv_index] = mx_texcoord_stream;
	}

	// vertex colors
	// WARNING: we export all available vertex colors
	// but when it compile the shader, it use only colors, required by the shader
	// for example, if there is a nodes with different vertex color indices, but one of them connected with 0-weight
	// then it optimise the shader and include only one vertex color
	// after change the weight in the node, it simply update uniform values, but dies not inlude the other vertex color attribute
	// so, the shader recompile is required
	// simply change the topology and it will recompile shader and inlude both vertex colors
	XSI::CRefArray vertex_colors_array = geometry.GetVertexColors();
	size_t vertex_colors_array_count = vertex_colors_array.GetCount();
	for (LONG vc_index = 0; vc_index < vertex_colors_array_count; vc_index++) {
		XSI::ClusterProperty vertex_color_prop(vertex_colors_array[vc_index]);
		XSI::CFloatArray color_values;
		vertex_color_prop.GetValues(color_values);

		MaterialX::MeshStreamPtr mx_vertex_color_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::COLOR_ATTRIBUTE + "_" + std::to_string(vc_index), MaterialX::MeshStream::COLOR_ATTRIBUTE, vc_index);
		mx_vertex_color_stream->setStride(MaterialX::MeshStream::STRIDE_4D);
		std::vector<float>& mx_color_array = mx_vertex_color_stream->getData();
		mx_color_array.resize(nodes_count * 4);

		for (LONG i = 0; i < nodes_count; i++) {
			for (LONG c = 0; c < 4; c++) {
				mx_color_array[4 * i + c] = color_values[4 * i + c];
			}
		}
		mx_mesh->addStream(mx_vertex_color_stream);
	}

	for (size_t i = 0; i < partitions.size(); i++) {
		MaterialX::MeshPartitionPtr part = partitions[i];
		XSI::Material material = geometry_materials[(LONG)i];
		part->setName(material.GetFullName().GetAsciiString());
		part->setFaceCount(partitions[i]->getIndices().size() / 3);
		mx_mesh->addPartition(partitions[i]);
	}

	mx_mesh->setVertexCount(nodes_count);
	mx_mesh->setMinimumBounds(box_min);
	mx_mesh->setMaximumBounds(box_max);
	MaterialX::Vector3 sphere_center = (box_max + box_min) * 0.5;
	mx_mesh->setSphereCenter(sphere_center);
	mx_mesh->setSphereRadius((sphere_center - box_min).getMagnitude());

	if (mx_uv_streams.size() == 0) {
		MaterialX::MeshStreamPtr mx_texcoord_stream = mx_mesh->generateTextureCoordinates(mx_position_stream);
		if (mx_texcoord_stream) { mx_mesh->addStream(mx_texcoord_stream); }
		mx_uv_streams.push_back(mx_texcoord_stream);
	}

	// create tangent and bitangent streams
	if (mx_uv_streams.size() > 0) {
		MaterialX::MeshStreamPtr mx_tangent_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::TANGENT_ATTRIBUTE, MaterialX::MeshStream::TANGENT_ATTRIBUTE, 0);
		MaterialX::MeshStreamPtr mx_bitangent_stream = MaterialX::MeshStream::create("i_" + MaterialX::MeshStream::BITANGENT_ATTRIBUTE, MaterialX::MeshStream::BITANGENT_ATTRIBUTE, 0);;

		// get data pointeres and reserve arrays
		std::vector<float>& mx_tangent_array = mx_tangent_stream->getData();
		mx_tangent_array.resize((size_t)nodes_count * 3, 0.0f);
		std::vector<float>& mx_bitangent_array = mx_bitangent_stream->getData();
		mx_bitangent_array.resize((size_t)nodes_count * 3, 0.0f);

		// next fill mikk context
		MikkMeshData mikk_mesh;
		mikk_mesh.triangles_count = triangles_count;
		mikk_mesh.nodes = triangle_nodes_raw;
		mikk_mesh.positions = mx_position_stream->getData().data();
		mikk_mesh.normals = mx_normal_stream->getData().data();
		mikk_mesh.uvs = mx_uv_streams[0]->getData().data();
		mikk_mesh.out_tangents = mx_tangent_array.data();
		mikk_mesh.out_bitangents = mx_bitangent_array.data();

		// create interfaces
		SMikkTSpaceInterface mikk_interface = {};
		mikk_interface.m_getNumFaces = [](const SMikkTSpaceContext* pContext) -> int { return static_cast<MikkMeshData*>(pContext->m_pUserData)->triangles_count; };
		mikk_interface.m_getNumVerticesOfFace = [](const SMikkTSpaceContext*, const int) -> int { return 3; };
		mikk_interface.m_getPosition = [](const SMikkTSpaceContext* pContext, float fPosOut[], const int iFace, const int iVert) {
			MikkMeshData* mikk_mesh = static_cast<MikkMeshData*>(pContext->m_pUserData);
			LONG node_index = mikk_mesh->nodes[iFace * 3 + iVert];
			fPosOut[0] = mikk_mesh->positions[node_index * 3 + 0];
			fPosOut[1] = mikk_mesh->positions[node_index * 3 + 1];
			fPosOut[2] = mikk_mesh->positions[node_index * 3 + 2];
		};
		mikk_interface.m_getNormal = [](const SMikkTSpaceContext* pContext, float fNormOut[], const int iFace, const int iVert) {
			MikkMeshData* mikk_mesh = static_cast<MikkMeshData*>(pContext->m_pUserData);
			LONG node_index = mikk_mesh->nodes[iFace * 3 + iVert];
			fNormOut[0] = mikk_mesh->normals[node_index * 3 + 0];
			fNormOut[1] = mikk_mesh->normals[node_index * 3 + 1];
			fNormOut[2] = mikk_mesh->normals[node_index * 3 + 2];
		};
		mikk_interface.m_getTexCoord = [](const SMikkTSpaceContext* pContext, float fTexcOut[], const int iFace, const int iVert) {
			MikkMeshData* mikk_mesh = static_cast<MikkMeshData*>(pContext->m_pUserData);
			LONG node_index = mikk_mesh->nodes[iFace * 3 + iVert];
			fTexcOut[0] = mikk_mesh->uvs[node_index * 2 + 0];
			fTexcOut[1] = mikk_mesh->uvs[node_index * 2 + 1];
		};
		mikk_interface.m_setTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fTangent[], const float fSign, const int iFace, const int iVert) {
			MikkMeshData* mikk_mesh = static_cast<MikkMeshData*>(pContext->m_pUserData);
			LONG node_index = mikk_mesh->nodes[iFace * 3 + iVert];
			mikk_mesh->out_tangents[node_index * 3 + 0] = fTangent[0];
			mikk_mesh->out_tangents[node_index * 3 + 1] = fTangent[1];
			mikk_mesh->out_tangents[node_index * 3 + 2] = fTangent[2];
			float normal_x = mikk_mesh->normals[node_index * 3 + 0];
			float normal_y = mikk_mesh->normals[node_index * 3 + 1];
			float normal_z = mikk_mesh->normals[node_index * 3 + 2];
			float tangent_x = fTangent[0];
			float tangent_y = fTangent[1];
			float tangent_z = fTangent[2];
			float bitangent_x = (normal_y * tangent_z - normal_z * tangent_y) * fSign;
			float bitangent_y = (normal_z * tangent_x - normal_x * tangent_z) * fSign;
			float bitangent_z = (normal_x * tangent_y - normal_y * tangent_x) * fSign;
			mikk_mesh->out_bitangents[node_index * 3 + 0] = bitangent_x;
			mikk_mesh->out_bitangents[node_index * 3 + 1] = bitangent_y;
			mikk_mesh->out_bitangents[node_index * 3 + 2] = bitangent_z;
		};

		SMikkTSpaceContext context;
		context.m_pInterface = &mikk_interface;
		context.m_pUserData = &mikk_mesh;

		// make computations
		if (genTangSpaceDefault(&context)) {
			mx_mesh->addStream(mx_tangent_stream);
			mx_mesh->addStream(mx_bitangent_stream);
		}
		else {
			log_message("Fail to calculate tangents and bitangents for the mesh " + XSI::CString(xsi_object.GetName()), XSI::siWarningMsg);
		}
	}
	
	auto time_end = std::chrono::steady_clock::now();
	auto time_duration = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);

	return std::make_tuple(mx_mesh, material_ids, time_duration.count());
}

// get from Sycles
XSI::ShaderParameter get_source_parameter(const XSI::ShaderParameter& parameter, bool return_output) {
	XSI::CRef source = parameter.GetSource();
	if (source.IsValid()) {
		if (source.GetClassID() == XSI::siShaderParameterID) {
			XSI::ShaderParameter source_param(source);
			XSI::Shader source_node = source_param.GetParent();
			XSI::CString source_prog_id = source_node.GetProgID();
			if (source_prog_id.GetSubString(0, 13) == "XSIRTCOMPOUND") {
				return get_source_parameter(source_param, return_output);
			}
			else {
				XSI::CStringArray name_parts = source_prog_id.Split(".");
				if (name_parts[0] == "SIUtilityShaders") {
					if (name_parts[1].ReverseFindString("Passthrough") < UINT_MAX) {
						XSI::ShaderParameter p(source_node.GetParameter("input"));
						return get_source_parameter(p, return_output);
					}
					else {
						return return_output ? source_param : parameter;
					}
				}
				else {
					return return_output ? source_param : parameter;
				}
			}
		}
		else {
			return parameter;
		}
	}
	else {
		return parameter;
	}
}

std::tuple<MaterialX::GlslMaterialPtr, long long> mx_to_glsl(MaterialX::DocumentPtr mx_doc,
														     MaterialX::DocumentPtr std_lib, 
														     const MaterialX::FileSearchPath& search_path,
														     MaterialX::ImageHandlerPtr image_handler,
														     MaterialX::GenContext& gen_context,
														     const XSI::CString &xsi_name) {
	auto time_start = std::chrono::steady_clock::now();

	std::vector<MaterialX::TypedElementPtr> elements = MaterialX::findRenderableElements(mx_doc);
	if (elements.size() == 0) {
		std::vector<MaterialX::NodePtr> material_nodes = mx_doc->getMaterialNodes();
		for (size_t i = 0; i < material_nodes.size(); i++) {
			MaterialX::NodePtr node = material_nodes[0];
			if (node->getCategory() == "LamaSurface") {
				MaterialX::TypedElementPtr element(node);
				elements.push_back(element);
			}
		}
	}
	
	size_t elements_count = elements.size();
	if (elements_count > 0) {
		mx_doc->setDataLibrary(std_lib);
		gen_context.getShaderGenerator().registerTypeDefs(mx_doc);
		// create material from the first element
		MaterialX::TypedElementPtr renderable_element = elements[0];
		MaterialX::NodePtr node = elements[0]->asA<MaterialX::Node>();
		MaterialX::GlslMaterialPtr mx_material = MaterialX::GlslMaterial::create();
		mx_material->setDocument(mx_doc);
		mx_material->setElement(renderable_element);
		mx_material->setMaterialNode(node);

		MaterialX::FileSearchPath extended_search_path = search_path;
		MaterialX::FileSearchPath material_search_path = MaterialX::getSourceSearchPath(mx_doc);
		extended_search_path.append(material_search_path);
		image_handler->setSearchPath(extended_search_path);

		try {
			bool is_generate = mx_material->generateShader(gen_context);

			if (is_generate) {
				auto time_end = std::chrono::steady_clock::now();
				auto time_duration = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);
				return { mx_material, time_duration.count() };
			}
			else {
				log_message(("Fail to generate shader for the element " + renderable_element->getName()).c_str(), XSI::siWarningMsg);
				return { nullptr, 0 };
			}
		}
		catch (MaterialX::ExceptionRenderError& e) {
			for (const std::string& error : e.errorLog()) {
				log_message(("Fail to generate shader for the element " + renderable_element->getName()).c_str() + XSI::CString(". Error: ") + error.c_str(), XSI::siWarningMsg);
			}
			return { nullptr, 0 };
		}
	}
	else {
		return { nullptr, 0 };
	}
}

MaterialX::DocumentPtr xsimaterial_to_doc(ULONG xsi_material_id) {
	// get XSI material
	XSI::ProjectItem xsi_item = XSI::Application().GetObjectFromID(xsi_material_id);
	if (xsi_item.GetClassID() != XSI::siMaterialID) {
		return nullptr;
	}

	XSI::Material xsi_material(xsi_item);
	if (!xsi_material.IsValid()) {
		return nullptr;
	}

	// find the first node and check that this is supported MaterialX output node
	XSI::CParameterRefArray xsi_material_parameters = xsi_material.GetParameters();
	XSI::ShaderParameter xsi_material_port = xsi_material_parameters.GetItem("material");
	XSI::ShaderParameter xsi_to_material_port = get_source_parameter(xsi_material_port, true);
	// XSI::ShaderParameter xsi_to_material_port = get_finall_parameter(xsi_material_port);
	XSI::Shader xsi_first_node = xsi_to_material_port.GetParent();
	if (!xsi_first_node.IsValid()) {
		return nullptr;
	}

	XSI::CString xsi_first_node_id = xsi_first_node.GetProgID();
	if (xsi_first_node_id != "MaterialXSIParser.ND_surfacematerial.1.0" &&
		xsi_first_node_id != "MaterialXSIParser.ND_volumematerial.1.0" &&
		xsi_first_node_id != "MaterialXSIParser.ND_lama_surface.1.0") {
		return nullptr;
	}

	// ok, now we find the valid start node
	// we should export this node and all connections to MaterialX doc
	MaterialX::DocumentPtr mx_doc = MaterialX::createDocument();
	ExportOptions export_options;
	export_options.output_path = "";
	export_options.insert_nodedefs = false;
	export_options.use_unique_names = true;
	ExportTextureOptions export_textures;
	export_textures.use_relative_path = false;
	export_textures.copy_files = false;
	export_textures.copy_folder = "";
	ExportMaterialOptions export_materials;
	export_materials.all_shaders = false;
	export_materials.material_priority = true;

	export_options.textures = export_textures;
	export_options.materials = export_materials;
	export_options.format = ExportFormat::GLSL;

	// make export
	std::vector<XSI::Shader> shaders = { xsi_first_node };
	export_shaders(shaders, mx_doc, export_options);

	// flatten the tree
	mx_doc->flattenSubgraphs();
	for (MaterialX::NodeGraphPtr graph : mx_doc->getNodeGraphs()) {
		if (graph->getActiveSourceUri() == mx_doc->getSourceUri()) {
			graph->flattenSubgraphs();
		}
	}

	return mx_doc;
}

std::tuple<MaterialX::GlslMaterialPtr, long long> xsimaterial_to_glslmaterial(ULONG xsi_material_id,
																		      MaterialX::DocumentPtr std_lib,
																		      const MaterialX::FileSearchPath& search_path,
																		      MaterialX::ImageHandlerPtr image_handler,
																		      MaterialX::GenContext& gen_context) {
	MaterialX::DocumentPtr mx_doc = xsimaterial_to_doc(xsi_material_id);
	if (!mx_doc) {
		return { nullptr, 0 };
	}
	return mx_to_glsl(mx_doc, std_lib, search_path, image_handler, gen_context, XSI::CString(xsi_material_id));
}

std::tuple<MaterialX::GlslMaterialPtr, long long> build_empty_glslmaterial(MaterialX::DocumentPtr std_lib,
	const MaterialX::FileSearchPath& search_path,
	MaterialX::ImageHandlerPtr image_handler,
	MaterialX::GenContext& gen_context) {
	MaterialX::DocumentPtr mx_doc = MaterialX::createDocument();
	MaterialX::NodePtr mat_node = mx_doc->addNode("surfacematerial", "MX_Surfacematerial", "material");
	mat_node->setTarget("MaterialX");
	MaterialX::NodePtr unlit_node = mx_doc->addNode("surface_unlit", "MX_Surface_Unlit", "surfaceshader");
	unlit_node->setTarget("MaterialX");
	unlit_node->setInputValue("emission", 1.0f);
	unlit_node->setInputValue("emission_color",  MaterialX::Color3(0.25f, 0.25f, 0.25f));
	mat_node->setConnectedNode("surfaceshader", unlit_node);

	return mx_to_glsl(mx_doc, std_lib, search_path, image_handler, gen_context, XSI::CString("empty_material"));
}

bool are_material_changed(const XSI::X3DObject& xsi_object, const std::vector<ULONG>& material_ids) {

	// we should check that all remembered in the viewer materials are the same as actual ones
	// here at first we should obtain all materials for the xsi-mesh
	XSI::CRefArray xsi_object_materials = xsi_object.GetMaterials();
	LONG materials_count = xsi_object_materials.GetCount();
	// construct ids array
	std::vector<ULONG> xsi_object_material_ids;
	xsi_object_material_ids.resize(materials_count);
	for (LONG i = 0; i < materials_count; i++) {
		XSI::CRef xsi_material_ref = xsi_object_materials[i];
		XSI::Material xsi_material(xsi_material_ref);
		if (xsi_material.IsValid()) {
			xsi_object_material_ids[i] = xsi_material.GetObjectID();
		}
	}

	// check remmbered material
	// if it does not conteinted in new array, simply remove it from the cache
	for (size_t i = 0; i < material_ids.size(); i++) {
		ULONG material_id_value = material_ids[i];

		if (!is_array_contains(xsi_object_material_ids, material_id_value)) {
			return true;
		}
	}

	return false;
}

bool are_nodes_coincide(MaterialX::NodePtr node_a, MaterialX::NodePtr node_b, std::set<std::string>& visited_nodes) {
	if (node_a->getType() != node_b->getType() ||
		node_a->getCategory() != node_b->getCategory() ||
		node_a->getName() != node_b->getName() ||
		node_a->getInputCount() != node_b->getInputCount()) {
		return false;
	}

	if (visited_nodes.find(node_a->getName()) != visited_nodes.end()) {
		// these nodes already checked, they coincide
		return true;
	}

	std::vector<MaterialX::InputPtr> a_inputs = node_a->getInputs();
	std::vector<MaterialX::InputPtr> b_inputs = node_b->getInputs();

	if (a_inputs.size() != b_inputs.size()) {
		return false;
	}

	for (size_t i = 0; i < a_inputs.size(); i++) {
		MaterialX::InputPtr a_input = a_inputs[i];
		MaterialX::InputPtr b_input = b_inputs[i];

		// inputs have different types
		if (a_input->getName() != b_input->getName() ||
			a_input->getType() != b_input->getType()) {
			return false;
		}

		MaterialX::NodePtr a_source = a_input->getConnectedNode();
		MaterialX::NodePtr b_source = b_input->getConnectedNode();
		// if some of inputs are not connected to something
		// the topogy is different
		if (a_source && !b_source ||
			!a_source && b_source) {
			return false;
		}

		// if both conneceted to something
		if (a_source && b_source) {
			// recursive call
			if (!are_nodes_coincide(a_source, b_source, visited_nodes)) {
				return false;
			}
		}

		// if no connections for both ports, skip it and check others
	}

	// check may be we shange the index of the geometry attribute
	// in this case we should recompile the shader, because previous version can contains only another component
	// change geometry attribute is equivalent to change the topology
	// for now we support only uv-coordinates and vertex colors (and does not support general geompropvalue or USDPrimvar)
	// so, check what value was for texcoord and geomcolor nodes
	// WARNING: for now aocnsider only durect value change
	// (potentially we can connect somethong to the index port and change it procedurally, don't consider this behaviour)
	if (node_a->getCategory() == "texcoord" || node_a->getCategory() == "geomcolor") {
		MaterialX::InputPtr index_input_a = node_a->getInput("index");
		MaterialX::InputPtr index_input_b = node_b->getInput("index");
		if (index_input_a && index_input_b) {
			if (index_input_a->getValueString() != index_input_b->getValueString()) {
				return false;
			}
		}
		else {
			log_message(XSI::CString(node_a->getCategory().c_str()) + " node does not contains index input. It's wrong.", XSI::siWarningMsg);
		}
	}

	visited_nodes.insert(node_a->getName());
	return true;
}

// this function should return TRUE if new material is the same in topological sence as the origianl one
// it return False if the topology is changed
// when construct all materials we flatten it
bool is_material_changed(MaterialX::GlslMaterialPtr original_material, MaterialX::DocumentPtr new_material_doc) {
	if (!new_material_doc || !original_material) {
		return true;
	}
	MaterialX::DocumentPtr original_material_doc = original_material->getDocument();

	// at first check the number of nodes and connections
	std::vector<MaterialX::NodePtr> original_nodes = original_material_doc->getNodes();
	std::vector<MaterialX::NodePtr> new_nodes = new_material_doc->getNodes();
	if (original_nodes.size() != new_nodes.size()) {
		return true;
	}

	if (original_nodes.size() == 0 || new_nodes.size() == 0) {
		return true;
	}

	std::set<std::string> visited_nodes;  // store here already considered nodes
	return !are_nodes_coincide(original_nodes[0], new_nodes[0], visited_nodes);
}