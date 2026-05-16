#include <MaterialXCore/Document.h>
#include <MaterialXFormat/Util.h>
#include <MaterialXGenOsl/OslShaderGenerator.h>
#include <MaterialXGenGlsl/GlslShaderGenerator.h>
#include <MaterialXGenMdl/MdlShaderGenerator.h>
#include <MaterialXGenMsl/MslShaderGenerator.h>
#include <MaterialXGenShader/DefaultColorManagementSystem.h>
#include <MaterialXGenShader/Util.h>
#include <MaterialXRenderGlsl/GlslMaterial.h>
#include <MaterialXRender/ShaderRenderer.h>

#include "../utilities/logging.h"
#include "export_format.h"
#include "export_options.h"
#include "../utilities/string.h"

// global variables for shader generators
MaterialX::FileSearchPath search_path;
MaterialX::DocumentPtr std_lib;
MaterialX::LightHandlerPtr light_handler;
MaterialX::DocumentPtr light_doc;

MaterialX::GenContext osl_context = MaterialX::OslShaderGenerator::create(); bool osl_init = false;
MaterialX::GenContext glsl_context = MaterialX::GlslShaderGenerator::create(); bool glsl_init = false;
MaterialX::GenContext mdl_context = MaterialX::MdlShaderGenerator::create(); bool mdl_init = false;
MaterialX::GenContext msl_context = MaterialX::MslShaderGenerator::create(); bool msl_init = false;

void set_search_path(const XSI::CString& plugin_path) {
    // input plugin_path is a full path to the plugin dll: ...\MaterialXSI\Application\Plugins\MaterialXSIPlugin.dll
    // search path is just the folder of this file
    // it contains libraries folder with MaterialX actual library
    ULONG slash_pos = plugin_path.ReverseFindString("\\");
    XSI::CString folder = plugin_path.GetSubString(0, slash_pos);
    search_path = std::string(folder.GetAsciiString());
}

MaterialX::FileSearchPath get_search_path() {
    return search_path;
}

void load_std_lib() {
    std_lib = MaterialX::createDocument();
    MaterialX::FilePathVec library_folders;
    library_folders.push_back("libraries");

    try {
        MaterialX::StringSet loaded = MaterialX::loadLibraries(library_folders, search_path, std_lib);
        if (loaded.empty()) {
            log_message(XSI::CString(("Could not find standard data libraries on the given search path: " + search_path.asString()).c_str()), XSI::siWarningMsg);
        }
    }
    catch (std::exception& e)
    {
        log_message(XSI::CString(("Failed to load standard data libraries: " + std::string(e.what())).c_str()), XSI::siWarningMsg);
    }

    light_handler = MaterialX::LightHandler::create();
    light_doc = MaterialX::createDocument();
    light_doc->addNode("point_light", "point", "lightshader");
    light_doc->addNode("directional_light", "direction", "lightshader");
    light_doc->addNode("spot_light", "spot", "lightshader");
    light_doc->addNode("light", "light", "lightshader");

    light_doc->importLibrary(std_lib);
}

MaterialX::DocumentPtr get_std_lib() {
    return std_lib;
}

void initialize_context(MaterialX::GenContext& context, ExportFeaturesOptions features) {
    std::string target = context.getShaderGenerator().getTarget();
    context.registerSourceCodeSearchPath(search_path);
    context.registerSourceCodeSearchPath(MaterialX::FileSearchPath(std::string(search_path.asString() + "\\libraries")));

    MaterialX::DefaultColorManagementSystemPtr cms = MaterialX::DefaultColorManagementSystem::create(target);
    cms->loadLibrary(std_lib);
    context.getShaderGenerator().setColorManagementSystem(cms);

    MaterialX::UnitSystemPtr unit_system = MaterialX::UnitSystem::create(target);
    unit_system->loadLibrary(std_lib);
    context.getShaderGenerator().setUnitSystem(unit_system);
    context.getOptions().targetDistanceUnit = "meter";

    context.getOptions().targetColorSpaceOverride = "lin_rec709";
    context.getOptions().fileTextureVerticalFlip = true;
    context.getOptions().hwShadowMap = features.shadowmap;
    context.getOptions().hwAmbientOcclusion = features.ao;
    context.getOptions().hwImplicitBitangents = false;
    context.getOptions().hwSpecularEnvironmentMethod = MaterialX::SPECULAR_ENVIRONMENT_FIS;
    context.getOptions().hwTransmissionRenderMethod = MaterialX::TRANSMISSION_REFRACTION;
    context.getOptions().hwDirectionalAlbedoMethod = MaterialX::HwDirectionalAlbedoMethod::DIRECTIONAL_ALBEDO_ANALYTIC;
}

void prepare_generators(const XSI::CString& plugin_path) {
    set_search_path(plugin_path);
    load_std_lib();
}

void try_init_generators(ExportFormat format, ExportFeaturesOptions features) {
    if (format == ExportFormat::OSL && !osl_init) {
        osl_context = MaterialX::OslShaderGenerator::create();
        initialize_context(osl_context, features);
        osl_init = true;
    }
    else if (format == ExportFormat::GLSL && !glsl_init) {
        glsl_context = MaterialX::GlslShaderGenerator::create();
        initialize_context(glsl_context, features);
        glsl_init = true;
    }
    else if (format == ExportFormat::MDL && !mdl_init) {
        mdl_context = MaterialX::MdlShaderGenerator::create();
        initialize_context(mdl_context, features);
        mdl_init = true;
    }
    else if (format == ExportFormat::MSL && !msl_init) {
        msl_context = MaterialX::MslShaderGenerator::create();
        initialize_context(msl_context, features);
        msl_init = true;
    }
}

void generate_shader_code(const MaterialX::DocumentPtr& mx_doc, MaterialX::GenContext& context, MaterialX::GlslMaterialPtr& material, ExportFormat format, const std::string& output_path, ExportFeaturesOptions features) {
    MaterialX::TypedElementPtr element = material->getElement();

    try {
        // set context features
        context.getOptions().hwShadowMap = features.shadowmap;
        context.getOptions().hwAmbientOcclusion = features.ao;

        context.clearUserData();
        if (features.lights) {
            std::vector<MaterialX::NodePtr> lights;
            light_handler->findLights(light_doc, lights);
            light_handler->registerLights(light_doc, lights, context);
        }
        context.getShaderGenerator().registerTypeDefs(mx_doc);

        if (format == ExportFormat::OSL || format == ExportFormat::MDL || format == ExportFormat::MSL) {
            MaterialX::ShaderPtr shader = createShader(element->getNamePath(), context, element);
            const std::string& shader_code = shader->getSourceCode(MaterialX::Stage::PIXEL);

            // write the file
            write_text_file(shader_code, output_path);
        } else if (format == ExportFormat::GLSL) {
            bool is_generate = material->generateShader(context);
            MaterialX::ShaderPtr shader = material->getShader();
            const std::string& pixel_shader = shader->getSourceCode(MaterialX::Stage::PIXEL);
            const std::string& vertex_shader = shader->getSourceCode(MaterialX::Stage::VERTEX);

            // for glsl we should store two files: with vert and frag shaders
            // so, add suffixes _frag and _vert before extension
            write_text_file(pixel_shader, add_suffix_to_path(output_path, "_frag"));
            write_text_file(vertex_shader, add_suffix_to_path(output_path, "_vert"));
        }
    }
    catch (MaterialX::ExceptionRenderError& e) {
        for (const std::string& error : e.errorLog()) {
            log_message(("Fail to generate shader for the element " + element->getName()).c_str() + XSI::CString(". Error: ") + error.c_str(), XSI::siWarningMsg);
        }
    }
    catch (std::exception& e) {
        log_message(("Fail to generate shader for the element " + element->getName()).c_str() + XSI::CString(". Error: ") + std::string(e.what()).c_str(), XSI::siWarningMsg);
    }
}

void generate_shader(const MaterialX::DocumentPtr& mx_doc, const std::string& output_path, ExportFormat format, ExportFeaturesOptions features) {
    try_init_generators(format, features);

    // create a blank mx doc
    MaterialX::DocumentPtr doc = MaterialX::createDocument();
    // import input materials
    doc->importLibrary(mx_doc);

    // define elements to shader generator
    std::vector<MaterialX::TypedElementPtr> elements = MaterialX::findRenderableElements(doc);
    if (elements.size() == 0) {
        // when we use Lama Surface root shader node, then it failt to find something relevant
        // yes, it find this node as materia node, but it fail to find dependency nodes, think that no connections and then skip it
        // but in fact there are connections
        // so, we simply export shaders for all material nodes in the graph
        std::vector<MaterialX::NodePtr> material_nodes = doc->getMaterialNodes();
        for (size_t i = 0; i < material_nodes.size(); i++) {
            MaterialX::NodePtr node = material_nodes[0];
            if (node->getCategory() == "LamaSurface") {
                MaterialX::TypedElementPtr element(node);
                elements.push_back(element);
            }
        }
    }

    size_t elements_count = elements.size();
    if (elements_count == 0) {
        // show a warning message
        log_message("Nothing to generate. Material does not contains valid material node.", XSI::siWarningMsg);
    }
    else {
        // import std lib to the doc
        doc->setDataLibrary(std_lib);
        // export shader for each element
        for (size_t i = 0; i < elements.size(); i++) {
            MaterialX::TypedElementPtr element = elements[i];
            std::string name = element->getName();
            MaterialX::NodePtr node = element->asA<MaterialX::Node>();

            // create material
            MaterialX::GlslMaterialPtr material = MaterialX::GlslMaterial::create();

            material->setDocument(doc);
            material->setElement(element);
            material->setMaterialNode(node);

            // we should combine the output path
            // if there is only one element, then use the same path as in function argument
            // but if there are several elements, then we should change the name of the output file, by adding _Name at the end
            std::string use_output = tranfsorm_output_path(output_path, name, elements.size());
            
            // now call common function for code generator
            if (format == ExportFormat::OSL) { generate_shader_code(doc, osl_context, material, format, use_output, features); }
            else if (format == ExportFormat::GLSL) { generate_shader_code(doc, glsl_context, material, format, use_output, features); }
            else if (format == ExportFormat::MDL) { generate_shader_code(doc, mdl_context, material, format, use_output, features); }
            else if (format == ExportFormat::MSL) { generate_shader_code(doc, msl_context, material, format, use_output, features); }
        }
    }
}