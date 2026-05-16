#pragma once
#include <xsi_application.h>
#include <xsi_x3dobject.h>
#include <xsi_shader.h>
#include <xsi_material.h>

#define NOMINMAX
#include <Windows.h>

XSI::CStatus init_materialxview(HWND parent_hwnd);
XSI::CStatus term_materialxview();
void notify_materialxview_window_focus(bool is_focus);
void notify_materialxview_window_paint();
void notify_update_selection(bool force_frame = false, bool ignore_frame = false);
void notify_update_object(const XSI::X3DObject &xsi_object);
void notify_object_remove(const XSI::CString &object_name);
void notify_update_material(const XSI::Shader &start_node, const XSI::Material& material);
void notify_change_frame(double time, LONG state);