#pragma once
#include <xsi_x3dobject.h>

#include "MaterialXFormat/File.h"
#include "MaterialXGenGlsl/GlslShaderGenerator.h"
#include "MaterialXRenderGlsl/GlslMaterial.h"
#include "MaterialXRender/ImageHandler.h"
#include "MaterialXRender/LightHandler.h"
#include "MaterialXRender/GeometryHandler.h"
#include "MaterialXRender/ShaderMaterial.h"
#include "MaterialXRender/Camera.h"

#define NOMINMAX
#include <Windows.h>
#include <gl/GL.h>
#include <map>

struct ViewerSettings {
	float view_angle;
	int irradiance_map_width;
	int irradiance_map_height;
	std::string irradiance_map_folder;
	std::string light_type;
	int shadowmap_size;
	int text_padding;
	int bold_font_size;
	int normal_font_size;
	int row_height;
};

class Viewer {
	public:
		Viewer(int in_width, int in_height, std::vector<MaterialX::FilePath> in_available_hdrs, ViewerSettings settings);
		~Viewer();

		void init_viewer_font(HDC display_device_context, int bold_font_size, int normal_font_size);
		void update_size(int in_width, int in_height);
		void update_camera_position(float horisontal_angle, float vertical_angle, float distance, float center_x, float center_y, float center_z);
		void update_light_angle(float in_angle);
		void render_frame(bool lock_selection);
		void update_selection(const std::map<ULONG, XSI::X3DObject> &xsi_selection);
		void update_material(ULONG xsi_material_id);
		void remove_from_mesh_cache(ULONG xsi_id);
		float get_camera_fov(bool in_degrees);
		float get_camera_aspect();
		void fix_selected_ids();
		std::vector<ULONG> get_selection();
		bool is_empty_selection();  // return true if current selection is empty
		void update_show_statistics(bool in_show);
		void update_use_shadowmaps(bool in_use_shadowmaps);
		void update_rebuild_animated_mesh(bool in_rebuild);
		void switch_hdr(bool is_next);
		void define_time(double in_time, int in_frame);
		void clear_cache(bool clear_mesh = false, bool clear_material = false);

	private:
		void select_hdr();
		void begin_text();
		void print_bold_text(float x, float y, const char* text);
		void print_normal_text(float x, float y, const char* text);
		void end_text();
		void try_update_environment_material();
		std::tuple<MaterialX::Vector3, MaterialX::Vector3> get_selection_bb();
		void update_cameras();
		void render_one_partition(MaterialX::MeshPtr mesh, MaterialX::MeshPartitionPtr partition, MaterialX::GlslMaterialPtr material, MaterialX::ShadowState& shadow_state, bool is_transparent);
		void render_frame_task(bool is_transparent, MaterialX::ShadowState &shadow_state);
		void render_screen_space_quad(MaterialX::MaterialPtr material);
		void try_update_shadow_map();
		void invalidate_shadow_map();
		void get_total_convert_time(long long &material_out, long long &mesh_out);

		// timing variables
		long long time_empty_material;
		long long time_shadowmap;
		std::vector<long long> render_accumulator;  // here we store values
		size_t render_accumulator_ptr;  // this is pointer to the accumulator array, where we should rewrite value

		MaterialX::FilePath env_radiance_filename;
		MaterialX::FileSearchPath search_path;

		double current_time;
		int current_frame;
		int used_hdr_index;  // use -1 if there are no available hdrs
		std::vector<MaterialX::FilePath> available_hdrs;
		bool rebuild_animated_mesh;
		bool use_shadowmap;
		float ao_gain;
		float light_rotation;
		int width;
		int height;
		MaterialX::Vector3 camera_position;
		MaterialX::Vector3 camera_target;
		MaterialX::Vector3 camera_up;
		float camera_view_angle;
		float camera_near_dist;
		float camera_far_dist;
		unsigned int shadow_softness;
		bool show_statistics;

		MaterialX::GenContext gen_context;
		MaterialX::DocumentPtr std_lib;
		MaterialX::ImageHandlerPtr image_handler;
		MaterialX::LightHandlerPtr light_handler;
		MaterialX::FilePath light_rig_filename;
		MaterialX::DocumentPtr light_rig_doc;
		MaterialX::GeometryHandlerPtr env_geometry_handler;
		MaterialX::MaterialPtr env_material;
		MaterialX::CameraPtr view_camera;
		MaterialX::CameraPtr env_camera;
		MaterialX::CameraPtr shadow_camera;
		MaterialX::ImagePtr shadow_map;
		MaterialX::MaterialPtr shadow_material;
		MaterialX::MaterialPtr shadow_blur_material;
		MaterialX::MeshPtr quad_mesh;

		// std::vector<MaterialX::MeshPartitionPtr> geometry_list;
		// std::vector<MaterialX::MaterialPtr> material_list;
		std::map<ULONG, std::tuple<MaterialX::MeshPtr, std::vector<ULONG>, long long>> mesh_cache;  // key - object ID from XSI
		std::vector<ULONG> selection;  // store XSI object IDs of the current selection, we should draw these objects
		std::map<ULONG, std::tuple<MaterialX::GlslMaterialPtr, long long>> material_cache;
		MaterialX::GlslMaterialPtr empty_material;
		std::map<std::string, MaterialX::ShaderPort*> update_map_filenames;  // use this map inside update material method

		GLuint gl_font_bold;
		GLuint gl_font_normal;
		int text_select_length;
		int text_select_height;
		int text_shadowmap_length;
		int text_rebuild_animated_mesh_length;
};