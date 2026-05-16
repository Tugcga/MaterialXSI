#include "controll.h"

Controll::Controll()
{
	is_view_focus = false;
	is_left_click = false;
	left_click_position_x = 0;
	left_click_position_y = 0;
	is_right_click = false;
	right_click_position_x = 0;
	right_click_position_y = 0;
	is_middle_click = false;
	middle_click_position_x = 0;
	middle_click_position_y = 0;
	camera_horizontal_angle = 3.0f * 3.1415f / 4.0f;
	camera_vertical_angle = 3.1415f / 2.0f;
	camera_distance = 5.0f;
	camera_center_x = 0.0f;
	camera_center_y = 0.0f;
	camera_center_z = 0.0f;
	camera_right_x = -1.41421356237f / 2.0f;
	camera_right_y = 0.0f;
	camera_right_z = -1.41421356237f / 2.0f;
	camera_top_x = 0.0f;
	camera_top_y = 1.0f;
	camera_top_z = 0.0f;
	lock_selection = false;
	light_angle = 0.0f;
	show_statistics = false;
	use_shadowmaps = false;
	rebuild_animated_mesh = false;
}

Controll::~Controll()
{

}