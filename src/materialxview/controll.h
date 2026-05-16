#pragma once
class Controll
{
public:
	Controll();
	~Controll();

	// true when the mouse over the viewer window
	bool is_view_focus;

	// activate when we click over the window
	// store positions of ht click
	bool is_left_click;
	int left_click_position_x;
	int left_click_position_y;

	bool is_right_click;
	int right_click_position_x;
	int right_click_position_y;

	bool is_middle_click;
	int middle_click_position_x;
	int middle_click_position_y;

	float camera_horizontal_angle;
	float camera_vertical_angle;
	float camera_distance;
	float camera_center_x;
	float camera_center_y;
	float camera_center_z;

	float camera_right_x;
	float camera_right_y;
	float camera_right_z;
	float camera_top_x;
	float camera_top_y;
	float camera_top_z;

	float light_angle;

	bool lock_selection;

	bool show_statistics;
	bool use_shadowmaps;

	bool rebuild_animated_mesh;
};