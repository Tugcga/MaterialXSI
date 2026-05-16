#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_selection.h>
#include <xsi_kinematics.h>
#include <xsi_shader.h>
#include <xsi_material.h>
#include <xsi_project.h>

#include "../utilities/logging.h"
#include "../utilities/math.h"
#include "../export/export_generate.h"
#include "viewer.h"
#include "controll.h"
#include "materialxview.h"
#include "../extern/SimpleIni.h"

#define NOMINMAX
#include <Windows.h>
#include <windowsx.h>
#include <gl/gl.h>

extern HINSTANCE __gInstance;
extern char __gGameApplicationPath[MAX_PATH];
/* we create two windows:
*     - window which is the child of the parent
*     - display which contains OpenGL context and make the render
* this pattern follows to the example in SDK
*/

#define M_PI 3.14159265358979323846
#define TWO_PI 2.0f * M_PI
#define CAMERA_CLIP 0.001

// constants for camera controll
float ROTATE_LIGHT_SPEED = 0.025f;
float ROTATE_SPEED = 0.01f;
float CAMERA_ZOOM_FACTOR = 0.1f;
float CAMERA_PAN_FACTOR = 0.01f;

// hotkeys
char ht_frame = 'A';
char ht_lock_selection = 'L';
char ht_statistics = 'I';
char ht_shadowmap = 'S';
char ht_rebuild_animated = 'R';

// window hwnd and dsiplay we store in global variables
// it allows to destroy these windows when call terminate callback of the custom display
HWND window_hwnd = NULL;
HWND display_hwnd = NULL;
HDC	display_device_context = NULL;
HGLRC display_render_context = NULL;
Viewer* viewer = nullptr;
Controll* controll = nullptr;

// here we cal  the render command, which should be preces in the viewer
void render() {
	if (display_device_context && display_render_context && viewer && controll) {
		wglMakeCurrent(display_device_context, display_render_context);
		viewer->render_frame(controll->lock_selection);
		SwapBuffers(display_device_context);
	}
}

float framebb_distance(const std::array<double, 6> &box, const MaterialX::Vector3& center, const MaterialX::Vector3& direction, const MaterialX::Vector3& up, float fov_y, float aspect){
	std::array<MaterialX::Vector3, 8> corners;
	//fill the corners array, calculate it in local coordinates with respect to the center
	corners[0] = MaterialX::Vector3((float)box[0], (float)box[1], (float)box[2]) - center;
	corners[1] = MaterialX::Vector3((float)box[3], (float)box[4], (float)box[5]) - center;
	corners[2] = MaterialX::Vector3((float)box[0], (float)box[4], (float)box[5]) - center;
	corners[3] = MaterialX::Vector3((float)box[3], (float)box[1], (float)box[5]) - center;
	corners[4] = MaterialX::Vector3((float)box[3], (float)box[4], (float)box[2]) - center;
	corners[5] = MaterialX::Vector3((float)box[0], (float)box[1], (float)box[5]) - center;
	corners[6] = MaterialX::Vector3((float)box[0], (float)box[4], (float)box[2]) - center;
	corners[7] = MaterialX::Vector3((float)box[3], (float)box[1], (float)box[2]) - center;

	MaterialX::Vector3 front = -direction;
	MaterialX::Vector3 right = up.cross(front).getNormalized();
	MaterialX::Vector3 top = front.cross(right);

	float a = aspect * std::tan(fov_y * 0.5f);
	float b = std::tan(fov_y * 0.5f);

	float r_min = 0.0f;
	for (const MaterialX::Vector3& v : corners) {
		float proj_d = direction.dot(v);
		float proj_r = std::abs(right.dot(v));
		float proj_u = std::abs(top.dot(v));

		float required = proj_d + std::max(proj_r / a, proj_u / b);
		if (required > r_min) r_min = required;
	}
	r_min *= 1.05f;

	return r_min;
}

void update_bounding_box(const XSI::X3DObject &xsi_object, std::array<double, 6>& selection_bb) {
	if (!xsi_object.IsValid()) {
		return;
	}
	double min_x, min_y, min_z, max_x, max_y, max_z;
	// use global transform
	XSI::MATH::CTransformation item_tfm = xsi_object.GetKinematics().GetGlobal().GetTransform();

	// update selection bounding box
	xsi_object.GetBoundingBox(min_x, min_y, min_z, max_x, max_y, max_z, item_tfm);
	// if bb is invalid, then each min value is greater than max value
	if (min_x < max_x) {
		if (selection_bb[0] > min_x) { selection_bb[0] = min_x; }
		if (selection_bb[1] > min_y) { selection_bb[1] = min_y; }
		if (selection_bb[2] > min_z) { selection_bb[2] = min_z; }
		if (selection_bb[3] < max_x) { selection_bb[3] = max_x; }
		if (selection_bb[4] < max_y) { selection_bb[4] = max_y; }
		if (selection_bb[5] < max_z) { selection_bb[5] = max_z; }
	}
}

std::map<ULONG, XSI::X3DObject> build_selection_from_app(std::array<double, 6> &selection_bb) {
	XSI::Selection app_selection = XSI::Application().GetSelection();

	LONG count = app_selection.GetCount();
	std::map<ULONG, XSI::X3DObject> selection;
	for (LONG i = 0; i < count; i++) {
		XSI::SIObject item = app_selection[i];
		// for now we support only selected meshes
		if (item.GetType() == "polymsh") {
			XSI::X3DObject item_object(item);
			ULONG item_id = item_object.GetObjectID();
			selection[item_id] = item_object;

			update_bounding_box(item_object, selection_bb);
		}
	}

	return selection;
}

std::map<ULONG, XSI::X3DObject> build_selection_from_viewer(std::array<double, 6>& selection_bb) {
	if (viewer) {
		std::map<ULONG, XSI::X3DObject> selection;
		std::vector<ULONG> viewer_selection = viewer->get_selection();
		for (size_t i = 0; i < viewer_selection.size(); i++) {
			ULONG xsi_id = viewer_selection[i];

			XSI::ProjectItem item = XSI::Application().GetObjectFromID(xsi_id);
			XSI::X3DObject xsi_object(item);
			if (xsi_object.IsValid()) {
				selection[xsi_id] = xsi_object;

				update_bounding_box(xsi_object, selection_bb);
			}
		}

		return selection;
	}
	else {
		return {};
	}
}

// we call this funciton when create the window, and also when selection is changed
// the last one is come from XSI callback
void notify_update_selection(bool force_frame, bool ignore_frame) {
	if (viewer && controll) {
		std::array<double, 6> selection_bb = { DBL_MAX, DBL_MAX, DBL_MAX, -DBL_MAX, -DBL_MAX, -DBL_MAX };
		std::map<ULONG, XSI::X3DObject> selection = controll->lock_selection ? build_selection_from_viewer(selection_bb) : build_selection_from_app(selection_bb);

		// calcualte the center
		MaterialX::Vector3 bb_center = MaterialX::Vector3((float)((selection_bb[0] + selection_bb[3]) / 2.0),
														  (float)((selection_bb[1] + selection_bb[4]) / 2.0),
														  (float)((selection_bb[2] + selection_bb[5]) / 2.0));
		// calculate current camera direction
		MaterialX::Vector3 direction = MaterialX::Vector3(-1.0f * sin(controll->camera_vertical_angle) * cos(controll ->camera_horizontal_angle),
														  -1.0f * cos(controll->camera_vertical_angle), 
														  -1.0f * sin(controll->camera_vertical_angle) * sin(controll->camera_horizontal_angle));

		viewer->update_selection(selection);
		// next we should update camera distance
		// keep angles the same, but change the distance to see the whole aabb of the selection
		if (selection.size() > 0 && (!controll->lock_selection || force_frame) && !ignore_frame) {
			float frame_distance = framebb_distance(selection_bb, bb_center, direction, MaterialX::Vector3(0.0f, 1.0f, 0.0f), viewer->get_camera_fov(false), viewer->get_camera_aspect());
			controll->camera_distance = frame_distance;
			controll->camera_distance = math_max(controll->camera_distance, CAMERA_CLIP);
			controll->camera_center_x = bb_center[0];
			controll->camera_center_y = bb_center[1];
			controll->camera_center_z = bb_center[2];

			viewer->update_camera_position(controll->camera_horizontal_angle, controll->camera_vertical_angle, controll->camera_distance,
										   controll->camera_center_x, controll->camera_center_y, controll->camera_center_z);
		}
		render();
	}
}

void notify_object_remove(const XSI::CString& object_name) {
	if (viewer) {
		viewer->fix_selected_ids();
		notify_update_selection();
	}
}

void switch_lock_selection() {
	if (controll && viewer) {
		if (!controll->lock_selection) {
			// if current lock is off, then check that viewer selection is not empty
			// if it empty, nothing to lock
			if (viewer->is_empty_selection()) {
				return;
			}
		}

		controll->lock_selection = !controll->lock_selection;

		// if the current lock is false, update selection from the app
		if (!controll->lock_selection) {
			notify_update_selection();
		}
		else {
			// redraw the frame to show that selection is locked
			render();
		}
	}
}

void switch_show_statistics() {
	if (controll && viewer) {
		controll->show_statistics = !controll->show_statistics;
		viewer->update_show_statistics(controll->show_statistics);

		render();
	}
}

void switch_shadowmaps() {
	if (controll && viewer) {
		controll->use_shadowmaps = !controll->use_shadowmaps;
		viewer->update_use_shadowmaps(controll->use_shadowmaps);

		notify_update_selection(false, true);
	}
}

void switch_rebuild_animated_mesh() {
	if (controll && viewer) {
		controll->rebuild_animated_mesh = !controll->rebuild_animated_mesh;
		viewer->update_rebuild_animated_mesh(controll->rebuild_animated_mesh);
		viewer->clear_cache(true, false);
		notify_update_selection(false, true);
	}
}

void display_resize(int in_width, int in_height) {
	if (display_hwnd) {
		SetWindowPos(display_hwnd, NULL, 0, 0, in_width, in_height, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOZORDER);
	}
	if (display_device_context && display_render_context) {
		wglMakeCurrent(display_device_context, display_render_context);
	}
	
	if (viewer) {
		viewer->update_size(in_width, in_height);
	}

	render();
}

void calculate_camera_rotation(int x, int y, float& out_horizontal, float& out_vertical) {
	int dx = x - controll->left_click_position_x;
	int dy = y - controll->left_click_position_y;
	float horizontal_delta = (float)dx * ROTATE_SPEED;
	float vertical_delta = -(float)dy * ROTATE_SPEED;

	out_horizontal = controll->camera_horizontal_angle + horizontal_delta;
	out_vertical = controll->camera_vertical_angle + vertical_delta;

	out_horizontal = fmodf(out_horizontal, TWO_PI);
	if (out_horizontal < 0) out_horizontal += TWO_PI;

	// also clamp vertical angle
	out_vertical = clamp(out_vertical, 0.01f, M_PI - 0.01f);

	// recalculate camera right and top vectors
	float view_x = -sin(out_vertical) * cos(out_horizontal);
	float view_y = -cos(out_vertical);
	float view_z = -sin(out_vertical) * sin(out_horizontal);

	// right = [view, up]
	// top = [view, right]
	float right_x = view_z;
	float right_y = 0;
	float right_z = -view_x;
	float right_length = sqrt(right_x * right_x + right_y * right_y + right_z * right_z);
	controll->camera_right_x = right_x / right_length;
	controll->camera_right_y = right_y / right_length;
	controll->camera_right_z = right_z / right_length;

	// we should not normalise top-vector, because view and right are orthogonal
	controll->camera_top_x = view_y * controll->camera_right_z - view_z * controll->camera_right_y;
	controll->camera_top_y = -view_x * controll->camera_right_z + view_z * controll->camera_right_x;
	controll->camera_top_z = view_x * controll->camera_right_y - view_y * controll->camera_right_x;
}

void calculate_camera_center(int x, int y, float& out_camera_center_x, float& out_camera_center_y, float& out_camera_center_z) {
	int dx = x - controll->middle_click_position_x;
	int dy = y - controll->middle_click_position_y;
	float right_delta = (float)dx * CAMERA_PAN_FACTOR;
	float top_delta = (float)dy * CAMERA_PAN_FACTOR;

	out_camera_center_x = controll->camera_center_x + (right_delta * controll->camera_right_x + top_delta * controll->camera_top_x) * controll->camera_distance;
	out_camera_center_y = controll->camera_center_y + (right_delta * controll->camera_right_y + top_delta * controll->camera_top_y) * controll->camera_distance;
	out_camera_center_z = controll->camera_center_z + (right_delta * controll->camera_right_z + top_delta * controll->camera_top_z) * controll->camera_distance;
}

void calculate_light_angle(int x, int y, float &out_angle) {
	// we use only horizontal angle - x-dirction of the mouse move
	int dx = x - controll->right_click_position_x;
	float angle_delta = (float)dx * ROTATE_LIGHT_SPEED;
	out_angle = controll->light_angle + angle_delta;
	// make module 2pi
	out_angle = fmodf(out_angle, TWO_PI);
	if (out_angle < 0) out_angle += TWO_PI;
}

LRESULT CALLBACK display_process(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (controll && viewer) {
		if (message == WM_INITDIALOG) {
			return TRUE;
		}
		else if (message == WM_KEYDOWN) {
			if (w_param == ht_frame) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					notify_update_selection(true);
				}
			}
			else if (w_param == ht_lock_selection) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					switch_lock_selection();
				}
			}
			else if (w_param == ht_statistics) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					switch_show_statistics();
				}
			}
			else if (w_param == ht_shadowmap) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					switch_shadowmaps();
				}
			}
			else if (w_param == VK_LEFT) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					if (viewer) {
						viewer->switch_hdr(false);
						render();
					}
				}
			}
			else if (w_param == VK_RIGHT) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					viewer->switch_hdr(true);
					render();
				}
			}
			else if (w_param == ht_rebuild_animated) {
				bool is_repeat = (l_param & (1 << 30)) != 0;
				if (!is_repeat) {
					switch_rebuild_animated_mesh();
				}
			}
		}
		else if (message == WM_MOUSEWHEEL) {
			if (controll->is_view_focus) {
				int wheel_delta = GET_WHEEL_DELTA_WPARAM(w_param);
				if (wheel_delta > 0) {
					controll->camera_distance *= (1.0f + CAMERA_ZOOM_FACTOR);
					controll->camera_distance = math_max(controll->camera_distance, CAMERA_CLIP);
					viewer->update_camera_position(controll->camera_horizontal_angle, controll->camera_vertical_angle, controll->camera_distance,
												   controll->camera_center_x, controll->camera_center_y, controll->camera_center_z);
				}
				else if (wheel_delta < 0) {
					controll->camera_distance *= (1.0f - CAMERA_ZOOM_FACTOR);
					controll->camera_distance = math_max(controll->camera_distance, CAMERA_CLIP);
					viewer->update_camera_position(controll->camera_horizontal_angle, controll->camera_vertical_angle, controll->camera_distance,
												   controll->camera_center_x, controll->camera_center_y, controll->camera_center_z);
				}
				render();
			}
		}
		else if (message == WM_LBUTTONDOWN) {
			if (controll->is_view_focus) {
				controll->is_left_click = true;
				controll->left_click_position_x = GET_X_LPARAM(l_param);
				controll->left_click_position_y = GET_Y_LPARAM(l_param);

				SetCapture(hwnd);
			}
		}
		else if (message == WM_LBUTTONUP) {
			if (controll->is_left_click) {
				ReleaseCapture();
				controll->is_left_click = false;
			}

			float next_horizontal;
			float next_vertical;
			calculate_camera_rotation(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param), next_horizontal, next_vertical);

			controll->camera_horizontal_angle = next_horizontal;
			controll->camera_vertical_angle = next_vertical;
		}
		else if (message == WM_MBUTTONDOWN) {
			if (controll->is_view_focus) {
				controll->is_middle_click = true;
				controll->middle_click_position_x = GET_X_LPARAM(l_param);
				controll->middle_click_position_y = GET_Y_LPARAM(l_param);

				SetCapture(hwnd);
			}
		}
		else if (message == WM_MBUTTONUP) {
			if (controll->is_middle_click) {
				ReleaseCapture();
				controll->is_middle_click = false;
			}

			float next_center_x;
			float next_center_y;
			float next_center_z;
			calculate_camera_center(GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param), next_center_x, next_center_y, next_center_z);

			controll->camera_center_x = next_center_x;
			controll->camera_center_y = next_center_y;
			controll->camera_center_z = next_center_z;
		}
		else if (message == WM_RBUTTONDOWN) {
			if (controll->is_view_focus) {
				controll->is_right_click = true;
				controll->right_click_position_x = GET_X_LPARAM(l_param);
				controll->right_click_position_y = GET_Y_LPARAM(l_param);

				SetCapture(hwnd);
			}
		}
		else if (message == WM_RBUTTONUP) {
			if (controll->is_right_click) {
				ReleaseCapture();
				controll->is_right_click = false;
			}

			int x = GET_X_LPARAM(l_param);
			int y = GET_Y_LPARAM(l_param);

			float next_light_angle = 0.0f;
			calculate_light_angle(x, y, next_light_angle);
			controll->light_angle = next_light_angle;
		}
		else if (message == WM_MOUSEMOVE) {
			if (controll->is_left_click) {
				int x = GET_X_LPARAM(l_param);
				int y = GET_Y_LPARAM(l_param);

				float next_horizontal;
				float next_vertical;
				calculate_camera_rotation(x, y, next_horizontal, next_vertical);

				viewer->update_camera_position(next_horizontal, next_vertical, controll->camera_distance,
											   controll->camera_center_x, controll->camera_center_y, controll->camera_center_z);

				render();
			}
			if (controll->is_middle_click) {
				int x = GET_X_LPARAM(l_param);
				int y = GET_Y_LPARAM(l_param);

				float next_camera_center_x;
				float next_camera_center_y;
				float next_camera_center_z;
				calculate_camera_center(x, y, next_camera_center_x, next_camera_center_y, next_camera_center_z);

				viewer->update_camera_position(controll->camera_horizontal_angle, controll->camera_vertical_angle, controll->camera_distance,
					next_camera_center_x, next_camera_center_y, next_camera_center_z);

				render();
			}
			if (controll->is_right_click) {
				int x = GET_X_LPARAM(l_param);
				int y = GET_Y_LPARAM(l_param);

				float next_light_angle = 0.0f;
				calculate_light_angle(x, y, next_light_angle);
				viewer->update_light_angle(next_light_angle);
				render();
			}
		}
		else if (message == WM_CAPTURECHANGED) {
			if (controll->is_left_click) {
				controll->is_left_click = false;
			}
			if (controll->is_right_click) {
				controll->is_right_click = false;
			}
		}		
	}

	return DefWindowProc(hwnd, message, w_param, l_param);
}

void display_shutdown() {
	// switch to empy context
	wglMakeCurrent(NULL, NULL);

	if (viewer) {
		delete viewer;
		viewer = nullptr;
	}

	if (controll) {
		delete controll;
		controll = nullptr;
	}

	// destory render and device context
	if (display_render_context) {
		wglDeleteContext(display_render_context);
	}
	if (display_hwnd && display_device_context) {
		ReleaseDC(display_hwnd, display_device_context);
	}

	if (display_hwnd) {
		DestroyWindow(display_hwnd);
	}
	
	display_render_context = NULL;
	display_device_context = NULL;
	display_hwnd = NULL;
}

// find all names with the same [name].mtlx and [name].hdr
// these files can be used for lighting
std::vector<MaterialX::FilePath> find_available_hdrs() {
	std::vector<MaterialX::FilePath> to_return;
	MaterialX::FileSearchPath search_path = get_search_path();
	MaterialX::FilePath lights_dir_relative("resources/Lights");
	MaterialX::FilePath lights_dir_absolute = search_path.find(lights_dir_relative);

	if (!lights_dir_absolute.isEmpty() && lights_dir_absolute.exists() && lights_dir_absolute.isDirectory()) {
		MaterialX::FilePathVec hdr_files = lights_dir_absolute.getFilesInDirectory("hdr");
		MaterialX::FilePathVec mtlx_files = lights_dir_absolute.getFilesInDirectory("mtlx");
		for (size_t i = 0; i < mtlx_files.size(); i++) {
			MaterialX::FilePath mtlx_name = mtlx_files[i];
			// check is it exists in the hdr list
			MaterialX::FilePath hdr_name(mtlx_name);
			hdr_name.removeExtension();
			hdr_name.addExtension("hdr");
			MaterialX::FilePath hdr_full_path = lights_dir_absolute / hdr_name;
			if (hdr_full_path.exists()) {
				to_return.push_back(hdr_full_path);
			}
		}
	}
	return to_return;
}

ViewerSettings load_ini_settings() {
	ViewerSettings settings;

	MaterialX::FileSearchPath search_path = get_search_path();
	std::string search_path_str = search_path.asString();

	// load ini
	CSimpleIniA ini;
	ini.SetUnicode();

	SI_Error rc = ini.LoadFile((search_path_str + "\\settings.ini").c_str());
	if (rc < 0) { 
		log_message("Fail to load ini-file with settings from " + XSI::CString(search_path_str.c_str()), XSI::siWarningMsg);

		settings.view_angle = 45.0f;
		settings.irradiance_map_width = 256;
		settings.irradiance_map_height = 128;
		settings.irradiance_map_folder = "irradiance";
		settings.light_type = "directional_light";
		settings.shadowmap_size = 1024;
		settings.text_padding = 10;
		settings.bold_font_size = 16;
		settings.normal_font_size = 16;
		settings.row_height = 18;
	}
	else {
		ROTATE_LIGHT_SPEED = (float)ini.GetDoubleValue("Viewer Control", "RotateLightSpeed", 0.025);
		ROTATE_SPEED = (float)ini.GetDoubleValue("Viewer Control", "RotateCameraSpeed", 0.01);
		CAMERA_ZOOM_FACTOR = (float)ini.GetDoubleValue("Viewer Control", "ZoomCameraFactor", 0.1);
		CAMERA_PAN_FACTOR = (float)ini.GetDoubleValue("Viewer Control", "PanCameraFactor", 0.01);

		settings.view_angle = (float)ini.GetDoubleValue("Viewer Camera", "ViewAngle", 45.0);
		settings.irradiance_map_width = (int)ini.GetLongValue("Viewer Light", "IrradianceMapWidth", 256);
		settings.irradiance_map_height = (int)ini.GetLongValue("Viewer Light", "IrradianceMapHeight", 128);
		settings.irradiance_map_folder = ini.GetValue("Viewer Light", "IrradianceMapFolder", "irradiance");
		settings.light_type = ini.GetValue("Viewer Light", "LightType", "directional_light");
		settings.shadowmap_size = (int)ini.GetLongValue("Viewer Generator", "ShadowmapSize", 1024);
		settings.text_padding = (int)ini.GetLongValue("Viewer UI", "TextPadding", 10);
		settings.bold_font_size = (int)ini.GetLongValue("Viewer UI", "BoldFontSize", 16);
		settings.normal_font_size = (int)ini.GetLongValue("Viewer UI", "NormalFontSize", 16);
		settings.row_height = (int)ini.GetLongValue("Viewer UI", "RowHeight", 18);

		const char* frame_str = ini.GetValue("Hotkeys", "Frame", "A");
		if (frame_str != nullptr && frame_str[0] != '\0') { ht_frame = frame_str[0]; }

		const char* lock_str = ini.GetValue("Hotkeys", "LockSelection", "L");
		if (lock_str != nullptr && lock_str[0] != '\0') { ht_lock_selection = lock_str[0]; }

		const char* statistics_str = ini.GetValue("Hotkeys", "Statistics", "I");
		if (statistics_str != nullptr && statistics_str[0] != '\0') { ht_statistics = statistics_str[0]; }

		const char* shadowmap_str = ini.GetValue("Hotkeys", "ShadowMap", "S");
		if (shadowmap_str != nullptr && shadowmap_str[0] != '\0') { ht_shadowmap = shadowmap_str[0]; }

		const char* rebuild_str = ini.GetValue("Hotkeys", "RebuildAnimatedMesh", "A");
		if (rebuild_str != nullptr && rebuild_str[0] != '\0') { ht_rebuild_animated = rebuild_str[0]; }
	}

	return settings;
}

XSI::CStatus display_initialize(HINSTANCE instance, HWND parent_hwnd, DWORD in_style, DWORD in_ext_style) {
	GLuint pixel_format;
	WNDCLASS window_class;
	DWORD ext_style;
	DWORD style;
	RECT window_rect;

	// get the size of the parent window
	// the size does not match the actual window size, it's default values, used at creation stage (640 x 480)
	// later it will be changed on the callback
	GetClientRect(parent_hwnd, &window_rect);

	int width = window_rect.right - window_rect.left;
	int height = window_rect.bottom - window_rect.top;

	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = (WNDPROC)display_process;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = NULL;
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = "OpenGLDisplay";

	RegisterClass(&window_class);

	ext_style = in_ext_style;
	style = in_style;

	AdjustWindowRectEx(&window_rect, style, FALSE, ext_style);

	if (!(display_hwnd = CreateWindowEx(ext_style,
										"OpenGLDisplay",
										"",
										style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
										0, 0,
										width, height,
										parent_hwnd,
										NULL,
										instance,
										NULL))) {
		log_message("Fail to create OpenGL display", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	static PIXELFORMATDESCRIPTOR pixel_def = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
		32,
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		16,
		1,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	if (!(display_device_context = GetDC(display_hwnd))) {
		log_message("Fail to create device context for the OpenGL display", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	if (!(pixel_format = ChoosePixelFormat(display_device_context, &pixel_def))) {
		log_message("Fail to choose pixel format for the OpenGL display", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	if (!SetPixelFormat(display_device_context, pixel_format, &pixel_def)) {
		log_message("Fail to define pixel format for the OpenGL display", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	if (!(display_render_context = wglCreateContext(display_device_context))) {
		log_message("Fail to create render context for the OpenGL display", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	if (!wglMakeCurrent(display_device_context, display_render_context)) {
		log_message("Fail to activate the OpenGL display context", XSI::siWarningMsg);
		display_shutdown();
		return XSI::CStatus::Fail;
	}

	const char* versionString = (const char*)glGetString(GL_VERSION);
	log_message("Create MaterialX viewer with OpenGL " + XSI::CString(versionString));

	ShowWindow(display_hwnd, SW_SHOW);
	display_resize(width, height);

	// load ini with viewer settings
	ViewerSettings settings = load_ini_settings();

	// before, parse all available hdr-images and pass it to the viewer constructor
	// it will allow to switch do different hdrs
	std::vector<MaterialX::FilePath> available_hdrs = find_available_hdrs();

	// at the end, if all ok, create the viewer
	viewer = new Viewer(width, height, available_hdrs, settings);
	viewer->init_viewer_font(display_device_context, settings.bold_font_size, settings.normal_font_size);
	// here we have already switched context

	// and also crate controller
	controll = new Controll();

	// fill the viewer by selected geometry
	notify_update_selection();
	// render the first frame
	render();

	return XSI::CStatus::OK;
}

/* these are process callbacks for the window
* here we shold procaess only three events:
*     - change size (in this case we call mathod from display)
*     - close window
*     - show window
*/
LRESULT CALLBACK window_process(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
	if (message == WM_INITDIALOG) {
		return TRUE;
	}
	else if (message == WM_SIZE) {
		RECT window_rect;
		GetClientRect(hwnd, &window_rect);
		display_resize(window_rect.right, window_rect.bottom);
	}
	else if (message == WM_SHOWWINDOW) {
		ShowWindow(hwnd, static_cast<int>(w_param));
	}
	else if (message == WM_CLOSE) {
		EndDialog(hwnd, 0);
	}

	return DefWindowProc(hwnd, message, w_param, l_param);
}

HWND create_window(HINSTANCE instance, HWND parent_hwnd, int width, int height) {
	HWND return_hwnd;
	WNDCLASS window_class;
	DWORD extend_style;
	DWORD style;
	RECT window_rect;

	// define window size
	window_rect.left = (LONG)0;
	window_rect.right = (LONG)width;
	window_rect.top = (LONG)0;
	window_rect.bottom = (LONG)height;

	// parameters
	window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	window_class.lpfnWndProc = (WNDPROC)window_process;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
	window_class.hbrBackground = NULL;
	window_class.lpszMenuName = NULL;
	window_class.lpszClassName = "Subparent";

	RegisterClass(&window_class);

	extend_style = 0;
	style = WS_CHILD;

	AdjustWindowRectEx(&window_rect, style, FALSE, extend_style);

	// actual create the window inside the parent
	if (!(return_hwnd = CreateWindowEx(extend_style,
									   "Subparent",
									   "",
									   style | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
									   0, 0,
									   window_rect.right - window_rect.left,
									   window_rect.bottom - window_rect.top,
									   parent_hwnd,
									   NULL,
									   instance,
									   NULL))) {
		// if fail, return null
		log_message("Fail to create parent window", XSI::siWarningMsg);
		return NULL;
	}

	ShowWindow(return_hwnd, SW_SHOWNORMAL);
	UpdateWindow(return_hwnd);
	return return_hwnd;

}

// we call this function when close the custom display host
// here we should destroy the display and the window
XSI::CStatus term_materialxview() {
	display_shutdown();
	
	if (window_hwnd) {
		DestroyWindow(window_hwnd);
		window_hwnd = NULL;
	}

	return XSI::CStatus::OK;
}

// this function called when we open custom display host
XSI::CStatus init_materialxview(HWND parent_hwnd) {
	// potentially we can open several custom dislpay hosts
	// for simplicity we destroy the previous one and recreate the new one
	if (window_hwnd) {
		term_materialxview();
	}

	// now create new window
	SetCurrentDirectory(__gGameApplicationPath);
	// here we can use any width and height of the window
	// after create process Softimage automatticaly fire event with define the size
	window_hwnd = create_window(__gInstance, parent_hwnd, 640, 480);

	if (!window_hwnd) {
		log_message("Fail to create the window for custom display host", XSI::siErrorMsg);
		return XSI::CStatus::Fail;
	}

	// and inicialize OpenGL display
	XSI::CStatus is_init = display_initialize(__gInstance, window_hwnd, WS_CHILD, 0);
	if (is_init == XSI::CStatus::Fail) {
		log_message("Fail to create OpenGL display inside the window for custom display host", XSI::siErrorMsg);
		term_materialxview();
		return XSI::CStatus::Fail;
	}

	return XSI::CStatus::OK;
}

void notify_materialxview_window_focus(bool is_focus) {
	if (controll) {
		controll->is_view_focus = is_focus;
		if (is_focus && display_hwnd) {
			SetFocus(display_hwnd);
		}
	}
}

void notify_materialxview_window_paint() {
	render();
}

void notify_update_object(const XSI::X3DObject& xsi_object) {
	if (xsi_object.IsValid() && viewer) {
		// when we update any object from the scene, we should remove it from the viewer cache
		// and recreate again
		viewer->remove_from_mesh_cache(xsi_object.GetObjectID());
		notify_update_selection();
	}
}

void notify_update_material(const XSI::Shader& start_node, const XSI::Material& material) {
	if (viewer) {
		ULONG material_id = material.GetObjectID();
		// before update material activate curent context
		// because it require using GL-functions
		wglMakeCurrent(display_device_context, display_render_context);
		viewer->update_material(material_id);

		notify_update_selection(false, true);
	}
}

void notify_change_frame(double time, LONG state) {
	if (viewer && controll) {
		// the time comes as frame * fps
		// for example, if the current framerate is 30 fps, then frame 0 - 0 sec, 15 - 0.5 sec, 30 - 1 sec and so on
		XSI::Project prj = XSI::Application().GetActiveProject();
		XSI::CRefArray proplist = prj.GetProperties();
		XSI::Property playctrl(proplist.GetItem("Play Control"));
		int pc_current = playctrl.GetParameterValue("Current");

		viewer->define_time(time, pc_current);
		// we should reset meshes only when this option is activated in controll
		viewer->clear_cache(controll->rebuild_animated_mesh, false);
		notify_update_selection(false, true);
	}
}