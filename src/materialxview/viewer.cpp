#include <xsi_selection.h>
#include <xsi_x3dobject.h>
#include <xsi_material.h>

#include "text_strings.h"
#include "viewer.h"
#include "../utilities/logging.h"
#include "../utilities/string.h"
#include "../utilities/array.h"
#include "../export/export_generate.h"
#include "../utilities/math.h"
#include "xsitomx_converter.h"

#include "MaterialXFormat/File.h"
#include "MaterialXFormat/XmlIo.h"
#include "MaterialXFormat/Util.h"
#include "MaterialXRender/Util.h"
#include "MaterialXRender/ImageHandler.h"
#include "MaterialXRender/StbImageLoader.h"
#include "MaterialXRender/TinyObjLoader.h"
#include "MaterialXRender/CgltfLoader.h"
#include "MaterialXRender/GeometryHandler.h"
#include "MaterialXRender/Harmonics.h"
#include "MaterialXRender/ShaderRenderer.h"
#include "MaterialXGenGlsl/GlslShaderGenerator.h"
#include "MaterialXRenderGlsl/GLTextureHandler.h"
#include "MaterialXRenderGlsl/GlslMaterial.h"
#include "MaterialXGenShader/ShaderStage.h"
#include "MaterialXGenShader/Shader.h"
#include "MaterialXGenShader/DefaultColorManagementSystem.h"
#include "MaterialXRenderGlsl/GLFramebuffer.h"

#define NOMINMAX
#include <Windows.h>
#include <gl/gl.h>

#include <sstream>
#include <map>
#include <string>
#include <chrono>

#ifndef GL_FRAMEBUFFER_SRGB
    #define GL_FRAMEBUFFER_SRGB 0x8DB9  // define the constant by hands, <gl/gl.h> contains only legacy opengl
#endif

const size_t FRAME_ACCUMULATOR_SIZE = 128;
const MaterialX::Vector3 DEFAULT_CAMERA_POSITION(0.0f, 0.0f, 5.0f);
const float PI = std::acos(-1.0f);

int TEXT_PADDING = 10;
int STATISTIC_ROW_HEIGHT = 18;
int SHADOW_MAP_SIZE = 1024;
std::string DIR_LIGHT_NODE_CATEGORY = "directional_light";
std::string IRRADIANCE_MAP_FOLDER = "irradiance";
int IRRADIANCE_MAP_WIDTH = 256;
int IRRADIANCE_MAP_HEIGHT = 128;
float DEFAULT_CAMERA_VIEW_ANGLE = 45.0f;

Viewer::Viewer(int in_width, int in_height, std::vector<MaterialX::FilePath> in_available_hdrs, ViewerSettings settings) : gen_context(MaterialX::GlslShaderGenerator::create()) {
    // define constants
    TEXT_PADDING = settings.text_padding;
    STATISTIC_ROW_HEIGHT = settings.row_height;
    SHADOW_MAP_SIZE = settings.shadowmap_size;
    DIR_LIGHT_NODE_CATEGORY = settings.light_type;
    IRRADIANCE_MAP_FOLDER = settings.irradiance_map_folder;
    IRRADIANCE_MAP_WIDTH = settings.irradiance_map_width;
    IRRADIANCE_MAP_HEIGHT = settings.irradiance_map_height;
    DEFAULT_CAMERA_VIEW_ANGLE = settings.view_angle;

    search_path = get_search_path();
    available_hdrs = in_available_hdrs;

    if (available_hdrs.size() == 0) {
        used_hdr_index = -1;
    }
    else {
        used_hdr_index = 0;
    }

    MaterialX::FilePath env_sphere("resources/Geometry/sphere.obj");

	MaterialX::Color3 screen_color(MaterialX::DEFAULT_SCREEN_COLOR_SRGB);

    std_lib = get_std_lib();

    update_map_filenames.clear();
    current_time = 0.0f;
    current_frame = 0;
    render_accumulator.reserve(FRAME_ACCUMULATOR_SIZE);
    render_accumulator.clear();
    use_shadowmap = false;
    rebuild_animated_mesh = false;
    show_statistics = false;
    ao_gain = 0.6f;
    shadow_softness = 1;
    light_handler = MaterialX::LightHandler::create();
    view_camera = MaterialX::Camera::create();
    env_camera = MaterialX::Camera::create();
    shadow_camera = MaterialX::Camera::create();
    shadow_map = nullptr;
    light_rotation = 0.0f;
    width = in_width;
    height = in_height;
    camera_position = DEFAULT_CAMERA_POSITION;
    camera_target = MaterialX::Vector3();
    camera_up = MaterialX::Vector3(0.0f, 1.0f, 0.0f);
    camera_view_angle = DEFAULT_CAMERA_VIEW_ANGLE;
    camera_near_dist = 0.05f;
    camera_far_dist = 5000.0f;

    std::string target = gen_context.getShaderGenerator().getTarget();
    gen_context.registerSourceCodeSearchPath(search_path);
    gen_context.registerSourceCodeSearchPath(MaterialX::FileSearchPath(std::string(search_path.asString() + "\\libraries")));

    MaterialX::DefaultColorManagementSystemPtr cms = MaterialX::DefaultColorManagementSystem::create(target);
    cms->loadLibrary(std_lib);
    gen_context.getShaderGenerator().setColorManagementSystem(cms);

    MaterialX::UnitSystemPtr unit_system = MaterialX::UnitSystem::create(target);
    unit_system->loadLibrary(std_lib);
    gen_context.getShaderGenerator().setUnitSystem(unit_system);
    gen_context.getOptions().targetDistanceUnit = "meter";
    gen_context.getOptions().targetColorSpaceOverride = "lin_rec709";
    gen_context.getOptions().fileTextureVerticalFlip = true;
    gen_context.getOptions().hwShadowMap = use_shadowmap;
    gen_context.getOptions().hwAmbientOcclusion = false;
    gen_context.getOptions().hwImplicitBitangents = true;

    // in this step original viewer class construciton is finish
    // next it setup several variables and call initialisation method
    // we will continue to setup
    // we already load std lib, simply store it in the variable
    MaterialX::StbImageLoaderPtr stb_loader = MaterialX::StbImageLoader::create();
    image_handler = MaterialX::GLTextureHandler::create(stb_loader);
    image_handler->setSearchPath(search_path);

    invalidate_shadow_map();

    MaterialX::TinyObjLoaderPtr obj_loader = MaterialX::TinyObjLoader::create();
    MaterialX::CgltfLoaderPtr gltf_loader = MaterialX::CgltfLoader::create();
    
    env_geometry_handler = MaterialX::GeometryHandler::create();
    env_geometry_handler->addLoader(obj_loader);
    env_geometry_handler->loadGeometry(search_path.find(env_sphere));

    // next load environment light
    select_hdr();

    env_material = nullptr;

    mesh_cache.clear();
    selection.clear();

    material_cache.clear();

    std::tie(empty_material, time_empty_material) = build_empty_glslmaterial(std_lib, search_path, image_handler, gen_context);

    //  next init the camera
    view_camera->setViewportSize(MaterialX::Vector2(static_cast<float>(width), static_cast<float>(height)));
    update_cameras();
}

void Viewer::select_hdr() {
    if (used_hdr_index >= 0) {
        env_radiance_filename = available_hdrs[used_hdr_index];

        MaterialX::ImagePtr env_radiance_map = image_handler->acquireImage(env_radiance_filename);
        if (!env_radiance_map) {
            log_message("Failed to load hdr-image environment light", XSI::siWarningMsg);
        }
        else {
            MaterialX::FilePath env_irradiance_path = env_radiance_filename.getParentPath() / IRRADIANCE_MAP_FOLDER / env_radiance_filename.getBaseName();
            MaterialX::ImagePtr env_irradiance_map = image_handler->acquireImage(env_irradiance_path);

            // If not found, then generate an irradiance map via spherical harmonics.
            if (!env_irradiance_map || env_irradiance_map->getWidth() == 1) {
                MaterialX::Sh3ColorCoeffs sh_irradiance = MaterialX::projectEnvironment(env_radiance_map, true);
                env_irradiance_map = MaterialX::renderEnvironment(sh_irradiance, IRRADIANCE_MAP_WIDTH, IRRADIANCE_MAP_HEIGHT);
            }

            image_handler->releaseRenderResources(light_handler->getEnvRadianceMap());
            image_handler->releaseRenderResources(light_handler->getEnvIrradianceMap());
            light_handler->setEnvRadianceMap(env_radiance_map);
            light_handler->setEnvIrradianceMap(env_irradiance_map);
        }

        light_rig_filename = env_radiance_filename;
        light_rig_filename.removeExtension();
        light_rig_filename.addExtension(MaterialX::MTLX_EXTENSION);
        if (light_rig_filename.exists()) {
            light_rig_doc = MaterialX::createDocument();
            MaterialX::readFromXmlFile(light_rig_doc, light_rig_filename, search_path);
            light_rig_doc->importLibrary(std_lib);

            try {
                std::vector<MaterialX::NodePtr> lights;
                light_handler->findLights(light_rig_doc, lights);
                gen_context.clearUserData();
                light_handler->registerLights(light_rig_doc, lights, gen_context);
                light_handler->setLightSources(lights);
            }
            catch (std::exception& e) {
                log_message("Failed to set up lighting: " + XSI::CString(e.what()), XSI::siWarningMsg);
            }
        }
        else {
            light_rig_doc = nullptr;
            log_message("No light rig " + XSI::CString(light_rig_filename.asString().c_str()), XSI::siWarningMsg);
        }

        light_handler->setDirectLighting(true);
    }
    else {
        log_message("No available HDRs for lighting", XSI::siWarningMsg);

    }
}

void Viewer::init_viewer_font(HDC display_device_context, int bold_font_size, int normal_font_size) {
    HFONT font_bold = CreateFont(bold_font_size, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, "Bold");
    SelectObject(display_device_context, font_bold);

    gl_font_bold = glGenLists(256);
    wglUseFontBitmaps(display_device_context, 0, 256, gl_font_bold);

    SIZE row_size;
    GetTextExtentPoint32A(display_device_context, text_empty_selection, (int)strlen(text_empty_selection), &row_size);
    text_select_length = row_size.cx;
    text_select_height = row_size.cy;

    GetTextExtentPoint32A(display_device_context, text_active_shadowmap, (int)strlen(text_active_shadowmap), &row_size);
    text_shadowmap_length = row_size.cx;

    GetTextExtentPoint32A(display_device_context, text_active_rebuild_anim_mesh, (int)strlen(text_active_rebuild_anim_mesh), &row_size);
    text_rebuild_animated_mesh_length = row_size.cx;

    DeleteObject(font_bold);

    HFONT font_normal = CreateFont(normal_font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, "Normal");
    SelectObject(display_device_context, font_normal);

    gl_font_normal = glGenLists(256);
    wglUseFontBitmaps(display_device_context, 0, 256, gl_font_normal);
    DeleteObject(font_normal);
}

void Viewer::try_update_environment_material() {
    if (!env_material)
    {
        MaterialX::FilePath env_filename = search_path.find(MaterialX::FilePath("resources/Lights/envmap_shader.mtlx"));
        try {
            env_material = MaterialX::GlslMaterial::create();
            env_material->generateEnvironmentShader(gen_context, env_filename, std_lib, env_radiance_filename);
        }
        catch (std::exception& e) {
            log_message("Failed to generate environment shader: " + XSI::CString(e.what()), XSI::siWarningMsg);
            env_material = nullptr;
        }
    }
}

std::tuple<MaterialX::Vector3, MaterialX::Vector3> Viewer::get_selection_bb() {
    if (selection.size() == 0) {
        // for empty selection return default BB
        return { MaterialX::Vector3(-1.0, -1.0, -1.0), MaterialX::Vector3(1.0, 1.0, 1.0) };
    }

    MaterialX::Vector3 min_corner(FLT_MAX, FLT_MAX, FLT_MAX);
    MaterialX::Vector3 max_corner(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (size_t i = 0; i < selection.size(); i++) {
        ULONG id = selection[i];
        auto it = mesh_cache.find(id);
        if (it != mesh_cache.end()) {
            MaterialX::MeshPtr mx_mesh = std::get<0>(it->second);
            MaterialX::Vector3 mx_mesh_min = mx_mesh->getMinimumBounds();
            MaterialX::Vector3 mx_mesh_max = mx_mesh->getMaximumBounds();

            if (mx_mesh_min[0] < min_corner[0]) { min_corner[0] = mx_mesh_min[0]; }
            if (mx_mesh_min[1] < min_corner[1]) { min_corner[1] = mx_mesh_min[1]; }
            if (mx_mesh_min[2] < min_corner[2]) { min_corner[2] = mx_mesh_min[2]; }

            if (mx_mesh_max[0] > max_corner[0]) { max_corner[0] = mx_mesh_max[0]; }
            if (mx_mesh_max[1] > max_corner[1]) { max_corner[1] = mx_mesh_max[1]; }
            if (mx_mesh_max[2] > max_corner[2]) { max_corner[2] = mx_mesh_max[2]; }
        }
    }

    return { min_corner, max_corner };
}

void Viewer::update_cameras() {
    auto& create_perspective_matrix = MaterialX::Camera::createPerspectiveMatrix;
    auto& create_orthographic_matrix = MaterialX::Camera::createOrthographicMatrix;

    MaterialX::Matrix44 view_matrix, projection_matrix;
    float aspect_ratio = (float)width / (float)height;
    view_matrix = MaterialX::Camera::createViewMatrix(camera_position, camera_target, camera_up);
    float f_h = std::tan(camera_view_angle / 360.0f * PI) * camera_near_dist;
    float f_w = f_h * aspect_ratio;
    projection_matrix = create_perspective_matrix(-f_w, f_w, -f_h, f_h, camera_near_dist, camera_far_dist);
    
    view_camera->setWorldMatrix(MaterialX::Matrix44::IDENTITY);
    view_camera->setViewMatrix(view_matrix);
    view_camera->setProjectionMatrix(projection_matrix);

    env_camera->setWorldMatrix(MaterialX::Matrix44::createScale(MaterialX::Vector3(300.0f)));
    env_camera->setViewMatrix(view_camera->getViewMatrix());
    env_camera->setProjectionMatrix(view_camera->getProjectionMatrix());

    MaterialX::NodePtr dir_light = light_handler->getFirstLightOfCategory(DIR_LIGHT_NODE_CATEGORY);
    if (dir_light) {
        auto& selection_bb = get_selection_bb();
        MaterialX::Vector3 sphere_center = (std::get<0>(selection_bb) + std::get<1>(selection_bb)) * 0.5f;

        float r = (sphere_center - std::get<0>(selection_bb)).getMagnitude();
        shadow_camera->setWorldMatrix(MaterialX::Matrix44::createTranslation(-sphere_center));
        shadow_camera->setProjectionMatrix(MaterialX::Camera::createOrthographicMatrixZP(-r, r, -r, r, 0.0f, r * 2.0f));
        MaterialX::ValuePtr value = dir_light->getInputValue("direction");
        if (value->isA<MaterialX::Vector3>()) {
            MaterialX::Vector3 dir = MaterialX::Matrix44::createRotationY(light_rotation).transformVector(value->asA<MaterialX::Vector3>());
            shadow_camera->setViewMatrix(MaterialX::Camera::createViewMatrix(dir * -r, MaterialX::Vector3(0.0f), camera_up));
        }
    }
}

void Viewer::update_size(int in_width, int in_height) {
    if (in_width <= 1 || in_height <= 1) {
        return;
    }

    width = in_width;
    height = in_height;

    glViewport(0, 0, in_width, in_height);

    view_camera->setViewportSize(MaterialX::Vector2(static_cast<float>(width), static_cast<float>(height)));
    update_cameras();
}

void Viewer::update_camera_position(float horisontal_angle, float vertical_angle, float distance, float center_x, float center_y, float center_z) {
    distance = clamp(distance, 0.01f, 1024.0f);

    camera_target = MaterialX::Vector3(center_x, center_y, center_z);
    camera_position = MaterialX::Vector3(distance * sin(vertical_angle) * cos(horisontal_angle), distance * cos(vertical_angle), distance * sin(vertical_angle) * sin(horisontal_angle)) + camera_target;

    update_cameras();
}

void Viewer::update_light_angle(float in_angle) {
    light_rotation = in_angle;
    update_cameras();
    invalidate_shadow_map();
}

void Viewer::begin_text() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(0.85f, 0.85f, 0.85f);
}

void Viewer::print_bold_text(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    glListBase(gl_font_bold);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

void Viewer::print_normal_text(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    glListBase(gl_font_normal);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
}

void Viewer::end_text() {
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
}

void Viewer::render_one_partition(MaterialX::MeshPtr mesh, 
                                  MaterialX::MeshPartitionPtr partition, 
                                  MaterialX::GlslMaterialPtr material, 
                                  MaterialX::ShadowState &shadow_state,
                                  bool is_transparent) {
    try {
        material->bindMesh(mesh);
    }
    catch (MaterialX::ExceptionRenderError& e) {
        log_message("Fail to bind the mesh " + XSI::CString(mesh->getName().c_str()) + " to the material " + XSI::CString(material->getMaterialNode()->getName().c_str()) + ". It will be skipped from the frame render.", XSI::siWarningMsg);
        log_message("Error: " + XSI::CString(e.what()), XSI::siWarningMsg);
        for (const std::string& error : e.errorLog()) {
            log_message(XSI::CString(error.c_str()), XSI::siWarningMsg);
        }
        return;
    }
    
    if (is_transparent) {
        if (material->getProgram()->hasUniform(MaterialX::HW::ALPHA_THRESHOLD)) {
            material->getProgram()->bindUniform(MaterialX::HW::ALPHA_THRESHOLD, MaterialX::Value::createValue(0.001f));
        }
    }
    else {
        if (material->getProgram()->hasUniform(MaterialX::HW::ALPHA_THRESHOLD)) {
            material->getProgram()->bindUniform(MaterialX::HW::ALPHA_THRESHOLD, MaterialX::Value::createValue(0.99f));
        }
    }

    material->getProgram()->bindTimeAndFrame((float)current_time, 0.0f);
    material->bindViewInformation(view_camera);
    material->bindLighting(light_handler, image_handler, shadow_state);
    material->bindImages(image_handler, search_path);
    if (is_transparent) {
        glCullFace(GL_FRONT);
        material->drawPartition(partition);
        glCullFace(GL_BACK);
        material->drawPartition(partition);
    }
    else {
        material->drawPartition(partition);
    }
    material->unbindImages(image_handler);
}

void Viewer::render_frame_task(bool is_transparent, MaterialX::ShadowState &shadow_state) {
    for (size_t i = 0; i < selection.size(); i++) {
        ULONG id = selection[i];
        auto it = mesh_cache.find(id);
        if (it != mesh_cache.end()) {
            auto& mesh_data = it->second;
            auto& mx_mesh = std::get<0>(mesh_data);
            auto& material_ids = std::get<1>(mesh_data);

            if (is_transparent) {
                glEnable(GL_CULL_FACE);
            }
            size_t partitions_count = mx_mesh->getPartitionCount();
            for (size_t partition_index = 0; partition_index < partitions_count; partition_index++) {
                if (partition_index >= material_ids.size()) {
                    break;
                }
                ULONG partition_material_id = material_ids[partition_index];
                MaterialX::MeshPartitionPtr partition = mx_mesh->getPartition(partition_index);
                // try to find corresponding material
                auto mat_it = material_cache.find(partition_material_id);
                if (mat_it == material_cache.end()) {
                    // no material in the cache with this id
                    // use empty material
                    if (is_transparent) {
                        continue;
                    }
                    empty_material->bindShader();
                    render_one_partition(mx_mesh, partition, empty_material, shadow_state, is_transparent);
                }
                else {
                    MaterialX::GlslMaterialPtr partition_material = std::get<0>(mat_it->second);

                    /*if (is_transparent && !partition_material->hasTransparency()) {
                        continue;
                    }

                    if (!is_transparent && partition_material->hasTransparency()) {
                        continue;
                    }*/
                    render_one_partition(mx_mesh, partition, partition_material, shadow_state, is_transparent);
                }
            }

            if (is_transparent) {
                glDisable(GL_CULL_FACE);
            }
        }
    }
}

void Viewer::render_screen_space_quad(MaterialX::MaterialPtr material) {
    if (!quad_mesh) {
        quad_mesh = MaterialX::GeometryHandler::createQuadMesh();
    }

    material->bindMesh(quad_mesh);
    material->drawPartition(quad_mesh->getPartition(0));
}

void Viewer::try_update_shadow_map() {
    if (!shadow_map) {
        auto time_start = std::chrono::steady_clock::now();
        // shadow map is null, we shold generate it
        if (!shadow_material) {
            // no valid shadow material
            try {
                MaterialX::ShaderPtr hw_shader = MaterialX::createDepthShader(gen_context, std_lib, "__SHADOW_SHADER__");
                shadow_material = MaterialX::GlslMaterial::create();
                shadow_material->generateShader(hw_shader);
            }
            catch (std::exception& e) {
                log_message(XSI::CString("Failed to generate shadow shader: ") + XSI::CString(e.what()), XSI::siWarningMsg);
                shadow_material = nullptr;
            }
        }

        if (!shadow_blur_material) {
            // also construct blur material
            try {
                MaterialX::ShaderPtr hw_shader = MaterialX::createBlurShader(gen_context, std_lib, "__SHADOW_BLUR_SHADER__", "gaussian", 1.0f);
                shadow_blur_material = MaterialX::GlslMaterial::create();
                shadow_blur_material->generateShader(hw_shader);
            }
            catch (std::exception& e) {
                log_message(XSI::CString("Failed to generate shadow blur shader: ") + e.what());
                shadow_blur_material = nullptr;
            }
        }

        if (shadow_material && shadow_blur_material) {
            MaterialX::GLFramebufferPtr framebuffer = MaterialX::GLFramebuffer::create(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 2, MaterialX::Image::BaseType::FLOAT);
            framebuffer->bind();
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            shadow_material->bindShader();
            for (size_t i = 0; i < selection.size(); i++) {
                ULONG id = selection[i];
                auto mesh_it = mesh_cache.find(id);
                if (mesh_it != mesh_cache.end()) {
                    auto& mesh_data = mesh_it->second;
                    auto& mx_mesh = std::get<0>(mesh_data);
                    shadow_material->bindMesh(mx_mesh);
                    shadow_material->bindViewInformation(shadow_camera);

                    for (size_t j = 0; j < mx_mesh->getPartitionCount(); j++) {
                        MaterialX::MeshPartitionPtr geom = mx_mesh->getPartition(j);
                        shadow_material->drawPartition(geom);
                    }
                }
            }

            shadow_map = framebuffer->getColorImage();

            // blur shadow map
            MaterialX::ImageSamplingProperties blur_sampling_properties;
            blur_sampling_properties.uaddressMode = MaterialX::ImageSamplingProperties::AddressMode::CLAMP;
            blur_sampling_properties.vaddressMode = MaterialX::ImageSamplingProperties::AddressMode::CLAMP;
            blur_sampling_properties.filterType = MaterialX::ImageSamplingProperties::FilterType::CLOSEST;
            for (unsigned int i = 0; i < shadow_softness; i++) {
                framebuffer->bind();
                shadow_blur_material->bindShader();
                if (image_handler->bindImage(shadow_map, blur_sampling_properties)) {
                    MaterialX::GLTextureHandlerPtr texture_handler = std::static_pointer_cast<MaterialX::GLTextureHandler>(image_handler);
                    int texture_location = texture_handler->getBoundTextureLocation(shadow_map->getResourceId());
                    if (texture_location >= 0) {
                        std::static_pointer_cast<MaterialX::GlslMaterial>(shadow_blur_material)->getProgram()->bindUniform("image_file", MaterialX::Value::createValue(texture_location));
                    }
                }
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                render_screen_space_quad(shadow_blur_material);
                image_handler->releaseRenderResources(shadow_map);
                shadow_map = framebuffer->getColorImage();
            }

            glViewport(0, 0, width, height);
            glDrawBuffer(GL_BACK);

            auto time_end = std::chrono::steady_clock::now();
            auto time_duration = std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start);
            time_shadowmap = time_duration.count();
        }
    }
}

// we should reset shadow map in the following cases:
/*
* - init viewer
* - change light direction
* - reload geometry (in particular, change selection)
* - change material
*/
void Viewer::invalidate_shadow_map() {
    if (shadow_map) {
        image_handler->releaseRenderResources(shadow_map);
        shadow_map = nullptr;
    }
}

void Viewer::get_total_convert_time(long long &material_out, long long &mesh_out) {
    for (size_t i = 0; i < selection.size(); i++) {
        ULONG xsi_id = selection[i];
        auto mesh_it = mesh_cache.find(xsi_id);
        if (mesh_it != mesh_cache.end()) {
            MaterialX::MeshPtr mx_mesh;
            std::vector<ULONG> material_ids;
            long long mesh_time;
            std::tie(mx_mesh, material_ids, mesh_time) = mesh_it->second;
            // add time to the mesh timer
            mesh_out += mesh_time;

            // next geather time for materials
            for (size_t j = 0; j < material_ids.size(); j++) {
                ULONG material_id = material_ids[j];

                auto material_it = material_cache.find(material_id);
                if (material_it != material_cache.end()) {
                    material_out += std::get<1>(material_it->second);
                }
                else {
                    // no material in the cache, use empty material
                    material_out += time_empty_material;
                }
            }
        }
    }
}

void Viewer::render_frame(bool lock_selection) {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);

    glClearColor(30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);

    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    light_handler->setLightTransform(MaterialX::Matrix44::createRotationY(light_rotation));

    MaterialX::ShadowState shadow_state;
    shadow_state.ambientOcclusionGain = ao_gain;
    MaterialX::NodePtr dir_light = light_handler->getFirstLightOfCategory(DIR_LIGHT_NODE_CATEGORY);
    if (gen_context.getOptions().hwShadowMap && dir_light) {
        try_update_shadow_map();
        if (shadow_map) {
            shadow_state.shadowMap = shadow_map;
            shadow_state.shadowMatrix = view_camera->getWorldMatrix().getInverse() * shadow_camera->getWorldViewProjMatrix();
        }
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    // start measure time
    auto time_start = std::chrono::steady_clock::now();

    // Opaque pass
    render_frame_task(false, shadow_state);

    // Transparent pass
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    render_frame_task(true, shadow_state);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    auto time_end = std::chrono::steady_clock::now();
    auto time_duration = std::chrono::duration_cast<std::chrono::microseconds>(time_end - time_start);
    long long render_time = time_duration.count();

    // add to the accumulator
    if (render_accumulator.size() < FRAME_ACCUMULATOR_SIZE) {
        render_accumulator.push_back(render_time);
    }
    else {
        render_accumulator[render_accumulator_ptr] = render_time;
        render_accumulator_ptr = (render_accumulator_ptr + 1) % FRAME_ACCUMULATOR_SIZE;
    }

    // check, should we output some text
    // for now we output a text only when the selection is empty, or it locked, or we should show statistics
    if (selection.size() == 0 || lock_selection || show_statistics || use_shadowmap || rebuild_animated_mesh) {
        // after all rendering, disable all programs
        for (const auto& pair : material_cache) {
            MaterialX::GlslMaterialPtr material = std::get<0>(pair.second);
            if (!material) { continue; }

            MaterialX::GlslProgramPtr program = material->getProgram();
            if (!program) { continue; }

            program->unbind();
        }

        // and also for empty material
        empty_material->getProgram()->unbind();

        // next draw text on the screen
        begin_text();
        if (selection.size() == 0) {
            // warning: there is a problem here
            // when we change selection on the app, it at first drop it (selection is empty) and then make the actual
            // it sends two callbacks - for empty, and then for non-empty
            // that's why with change selection the frame for empty selection drawn once, and then removed
            // but it's visible and looks like a glitch
            print_bold_text((float)((width - text_select_length) / 2), float((height - text_select_height) / 2), text_empty_selection);
        }
        if (lock_selection) {
            print_bold_text((float)TEXT_PADDING, (float)TEXT_PADDING, text_selection_locked);
        }

        if (show_statistics) {
            print_normal_text((float)TEXT_PADDING, (float)(height - TEXT_PADDING - STATISTIC_ROW_HEIGHT / 2), text_stat_info);
            // for now we can output the total time of the materials compile, and mesh converts
            long long material_time = 0;
            long long geometry_time = 0;
            get_total_convert_time(material_time, geometry_time);

            // print it
            print_normal_text((float)TEXT_PADDING, (float)(height - TEXT_PADDING - 2 * STATISTIC_ROW_HEIGHT), (text_mesh_export + std::to_string(geometry_time) + text_ms).c_str());
            print_normal_text((float)TEXT_PADDING, (float)(height - TEXT_PADDING - 3 * STATISTIC_ROW_HEIGHT), (text_material_compile + std::to_string(material_time) + text_ms).c_str());

            // then frame render time
            long long frame_time = 0;
            for (size_t i = 0; i < render_accumulator.size(); i++) {
                frame_time += render_accumulator[i];
            }
            print_normal_text((float)TEXT_PADDING, (float)(height - TEXT_PADDING - 4 * STATISTIC_ROW_HEIGHT), (text_render_time + time_to_string(frame_time / render_accumulator.size()) + text_mcs).c_str());
            if (use_shadowmap) {
                print_normal_text((float)TEXT_PADDING, (float)(height - TEXT_PADDING - 5 * STATISTIC_ROW_HEIGHT), (text_shadowmap_time + std::to_string(time_shadowmap) + text_ms).c_str());
            }
        }

        if (use_shadowmap) {
            print_bold_text((float)(width - TEXT_PADDING - text_shadowmap_length), (float)TEXT_PADDING, text_active_shadowmap);
        }

        if (rebuild_animated_mesh) {
            print_bold_text((float)(width - text_rebuild_animated_mesh_length) / 2, (float)TEXT_PADDING, text_active_rebuild_anim_mesh);
        }

        end_text();
    }

    glDisable(GL_FRAMEBUFFER_SRGB);
}

/*
* input - the map, where key - XSI object ID, value - XSI object
* all of these objects currently selected and should be drawn on the view
*/
void Viewer::update_selection(const std::map<ULONG, XSI::X3DObject>& xsi_selection) {
    // we should reset shadowmap if selection is changed
    if (xsi_selection.size() != selection.size()) {
        invalidate_shadow_map();
    }
    else {
        for (const auto& pair : xsi_selection) {
            ULONG xsi_object_id = pair.first;
            if (!is_array_contains(selection, xsi_object_id)) {
                invalidate_shadow_map();
                break;
            }
        }
    }
    
    // now rebuild the viewer selection
    selection.clear();
    render_accumulator.clear();
    render_accumulator_ptr = 0;
    for (const auto& pair : xsi_selection) {
        ULONG xsi_object_id = pair.first;
        XSI::X3DObject xsi_object = pair.second;
        selection.push_back(xsi_object_id);

        bool is_fresh_mesh = false;
        
        auto mesh_it = mesh_cache.find(xsi_object_id);
        if (mesh_it == mesh_cache.end()) {
            // no object in the cache
            std::tuple<MaterialX::MeshPtr, std::vector<ULONG>, long long> cache_item = xsipolymesh_to_mxmesh(xsi_object);
            mesh_cache[xsi_object_id] = cache_item;

            is_fresh_mesh = true;
        }

        // now mesh_cache contains xsi_object_id key, so, get it
        mesh_it = mesh_cache.find(xsi_object_id);
        bool need_rebuild = false;
        if (!is_fresh_mesh && mesh_it != mesh_cache.end()) {
            // only for non-fresh meshes
            // obtain remembered mesh materials
            auto& cache_item = mesh_it->second;
            std::vector<ULONG>& material_ids = std::get<1>(cache_item);

            if (are_material_changed(xsi_object, material_ids)) {
                need_rebuild = true;
            }
        }

        if (need_rebuild) {
            mesh_cache.erase(xsi_object_id);

            std::tuple<MaterialX::MeshPtr, std::vector<ULONG>, long long> cache_item = xsipolymesh_to_mxmesh(xsi_object);
            mesh_cache[xsi_object_id] = cache_item;
        }

        mesh_it = mesh_cache.find(xsi_object_id);
        if (mesh_it != mesh_cache.end()) {
            auto& cache_item = mesh_it->second;
            // second value - the array of material ids
            // we should check is it id in the cache, if no - create it
            std::vector<ULONG>& material_ids = std::get<1>(cache_item);
            for (size_t i = 0; i < material_ids.size(); i++) {
                ULONG material_id = material_ids[i];

                auto mat_it = material_cache.find(material_id);
                if (mat_it == material_cache.end()) {
                    // no material in the cache, create it
                    std::tuple<MaterialX::GlslMaterialPtr, long long> mx_material = xsimaterial_to_glslmaterial(material_id, std_lib, search_path, image_handler, gen_context);
                    if (std::get<0>(mx_material)) {
                        material_cache[material_id] = mx_material;
                    }
                    else {
                        // if material export fail, nothing to do
                        // we will use empty material in the render frame function
                    }
                }
            }
        }
    }
}

void Viewer::update_material(ULONG xsi_material_id) {
    // here we should either reassigna material parameters, or recreate material
    // for simplicity, simply remove the material
    auto it_mat = material_cache.find(xsi_material_id);
    if (it_mat == material_cache.end()) {
        // no material in the cache with this id
        // noting to do
    }
    else {
        // here we should check is the material is change the topology
        // if yes, remove it and recreate
        // if no - update only parameters
        MaterialX::GlslMaterialPtr mx_material = std::get<0>(it_mat->second);
        // also we should convert xsi-material no mx-material
        MaterialX::DocumentPtr new_mx_doc = xsimaterial_to_doc(xsi_material_id);
        MaterialX::GlslProgramPtr mx_program = mx_material->getProgram();

        if (!mx_program) {
            material_cache.erase(it_mat);
            invalidate_shadow_map();
        }
        else {
            // if new doc is null, it means that material is changed and become non-valid mx-material
            if (!new_mx_doc) {
                // delete it fom the cache
                material_cache.erase(it_mat);
                invalidate_shadow_map();
            }
            else {
                // compare with existing one
                if (is_material_changed(mx_material, new_mx_doc)) {
                    // topology changed, erease it
                    material_cache.erase(it_mat);
                    invalidate_shadow_map();
                }
                else {
                    // topology is the same, update parameters
                    bool break_update = false;
                    try {
                        // before we copy values from new mx-doc to the existing material
                        // we should get names of filepath uniforms
                        // because these values should be set in another way
                        // numeric values defined by modifyUniform method
                        // path to the texture - by uniform->setVariable
                        // so, we should store the map from name to shader port pointer
                        // and then, when copy values - use it for the proper path
                        update_map_filenames.clear();
                        const MaterialX::VariableBlock* mx_public_uniforms = mx_material->getPublicUniforms();
                        for (const auto& uniform : mx_public_uniforms->getVariableOrder()) {
                            // consider only filenames
                            if (uniform->getType() != MaterialX::Type::FILENAME) {
                                continue;
                            }
                            // const std::string& uniform_variable = uniform->getVariable();
                            update_map_filenames[uniform->getName()] = uniform;
                        }

                        // now iterate throw uniforms and update it values
                        const MaterialX::GlslProgram::InputMap& uniforms = mx_program->getUniformsList();

                        for (const auto& uniform : uniforms) {
                            MaterialX::GlslProgram::InputPtr uniform_input = uniform.second;
                            const std::string& uniform_input_type = uniform_input->typeString;
                            const std::string& path = uniform_input->path;
                            if (path.empty()) {
                                continue;
                            }
                            MaterialX::ElementPtr new_path_element = new_mx_doc->getDescendant(path);
                            MaterialX::InputPtr new_input = new_path_element ? new_path_element->asA<MaterialX::Input>() : nullptr;
                            if (!new_input) {
                                continue;
                            }

                            const std::string& new_input_type = new_input->getType();  // this type is the type of tha attribute of the node
                            MaterialX::ValuePtr new_value = new_input->getValue();
                            std::string new_value_type_str = new_value->getTypeString();

                            // we can update uniform only if it has supported type
                            // for example, filename attribute has type string and this uniform can not be modify
                            if (uniform_input_type == new_input_type && new_value_type_str != "string") {
                                mx_material->modifyUniform(path, new_value);
                            }
                            if (new_value_type_str == "string" && new_input_type == "filename") {
                                // modify path
                                std::string uniform_name = path;
                                auto pos = uniform_name.rfind('/');
                                if (pos != std::string::npos) { uniform_name[pos] = '_'; }

                                // try to find it
                                auto search_it = update_map_filenames.find(uniform_name);
                                if (search_it != update_map_filenames.end()) {
                                    // define new texture path
                                    search_it->second->setValue(search_it->second->getType().createValueFromStrings(new_value->getValueString()));
                                }

                                // check that the color space is the same
                                // if not - then we should recompile the shader, because color space convertation - is a code in the glsl-code
                                MaterialX::ElementPtr path_element = mx_material->getDocument()->getDescendant(path);
                                if (path_element && path_element->getColorSpace() != new_input->getColorSpace()) {
                                    break_update = true;
                                    break;
                                }
                            }
                        }
                    }
                    catch (MaterialX::ExceptionRenderError& e) {
                        log_message("Fail to get uniforms for update on the material " + XSI::CString((mx_material->getDocument()->getName() + ": " + e.what()).c_str()), XSI::siWarningMsg);
                        break_update = true;
                    }

                    if (break_update) {
                        material_cache.erase(it_mat);
                        invalidate_shadow_map();
                    }
                }
            }
        }
    }
}

// simply remove the object, if it was in the cache
// later the host will call update selection and rebuild the mesh, if it needed
void Viewer::remove_from_mesh_cache(ULONG xsi_id) {
    auto it = mesh_cache.find(xsi_id);
    if (it != mesh_cache.end()) {
        mesh_cache.erase(it);
    }
}

bool Viewer::is_empty_selection() {
    return selection.size() == 0;
}

// in this method we should enumerate all id-s from selection list and check are corresponding objects exists
// if no - remove it from selection and also from the mesh cache
void Viewer::fix_selected_ids() {
    for (size_t i = 0; i < selection.size(); ) {
        ULONG xsi_id = selection[i];
        XSI::ProjectItem item = XSI::Application().GetObjectFromID(xsi_id);
        bool should_delete = false;
        if (item.IsValid()) {
            XSI::X3DObject xsi_object(item);
            if (!xsi_object.IsValid()) {
                should_delete = true;
            }
        }
        else {
            should_delete = true;
        }

        if (should_delete) {
            remove_from_mesh_cache(xsi_id);
            selection[i] = selection.back();
            selection.pop_back();
        }
        else {
            ++i;
        }
    }
}

std::vector<ULONG> Viewer::get_selection() {
    return selection;
}

float Viewer::get_camera_fov(bool in_degrees) {
    if (in_degrees) {
        return camera_view_angle;
    }
    else {
        return camera_view_angle * PI / 180.0f;
    }
}

float Viewer::get_camera_aspect() {
    return (float)width / (float)height;
}

void Viewer::update_show_statistics(bool in_show) {
    show_statistics = in_show;
}

void Viewer::update_use_shadowmaps(bool in_use_shadowmaps) {
    use_shadowmap = in_use_shadowmaps;

    gen_context.getOptions().hwShadowMap = in_use_shadowmaps;
    // clear all cashed materials
    material_cache.clear();
}

void Viewer::switch_hdr(bool is_next) {
    if (available_hdrs.empty()) {
        return;
    }

    int hdrs_size = (int)available_hdrs.size();
    if (used_hdr_index != -1) {
        if (is_next) {
            used_hdr_index = (used_hdr_index + 1) % hdrs_size;
        }
        else {
            used_hdr_index = (used_hdr_index - 1 + hdrs_size) % hdrs_size;
        }

        select_hdr();
    }
}

void Viewer::define_time(double in_time, int in_frame) {
    current_time = in_time;
    current_frame = in_frame;
}

void Viewer::clear_cache(bool clear_mesh, bool clear_material) {
    if (clear_mesh) {
        mesh_cache.clear();
    }
    
    if (clear_material) {
        material_cache.clear();
    }
}

void Viewer::update_rebuild_animated_mesh(bool in_rebuild) {
    rebuild_animated_mesh = in_rebuild;
}

Viewer::~Viewer() {
    if (gl_font_bold) { glDeleteLists(gl_font_bold, 256); }
    if (gl_font_normal) { glDeleteLists(gl_font_normal, 256); }
    invalidate_shadow_map();
    if (image_handler) {
        image_handler->releaseRenderResources();
    }

    update_map_filenames.clear();
    render_accumulator.clear();
    available_hdrs.clear();
    mesh_cache.clear();
    selection.clear();
    material_cache.clear();
}