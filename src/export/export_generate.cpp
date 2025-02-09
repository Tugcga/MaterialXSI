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
#include "../utilities/string.h"

// global variables for shader generators
MaterialX::FileSearchPath search_path;
MaterialX::DocumentPtr std_lib;

MaterialX::GenContext osl_context = MaterialX::OslShaderGenerator::create(); bool osl_init = false;
MaterialX::GenContext glsl_context = MaterialX::GlslShaderGenerator::create(); bool glsl_init = false;
MaterialX::GenContext mdl_context = MaterialX::MdlShaderGenerator::create(); bool mdl_init = false;
MaterialX::GenContext msl_context = MaterialX::MslShaderGenerator::create(); bool msl_init = false;

void set_search_path(const XSI::CString& plugin_path) {
    // input plugin_path is a full path to the plugon dll: ...\MaterialXSI\Application\Plugins\MaterialXSIPlugin.dll
    // search path is just the folder of this file
    // it contains libraries folder with MaterialX actual library
    ULONG slash_pos = plugin_path.ReverseFindString("\\");
    XSI::CString folder = plugin_path.GetSubString(0, slash_pos);
    search_path = std::string(folder.GetAsciiString());
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
}

void initialize_context(MaterialX::GenContext& context) {
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
}

void prepare_generators(const XSI::CString& plugin_path) {
    set_search_path(plugin_path);
    load_std_lib();
}

void try_init_generators(ExportFormat format) {
    if (format == ExportFormat::OSL && !osl_init) {
        osl_context = MaterialX::OslShaderGenerator::create();
        initialize_context(osl_context);
        osl_init = true;
    }
    else if (format == ExportFormat::GLSL && !glsl_init) {
        glsl_context = MaterialX::GlslShaderGenerator::create();
        initialize_context(glsl_context);
        glsl_init = true;
    }
    else if (format == ExportFormat::MDL && !mdl_init) {
        mdl_context = MaterialX::MdlShaderGenerator::create();
        initialize_context(mdl_context);
        mdl_init = true;
    }
    else if (format == ExportFormat::MSL && !msl_init) {
        msl_context = MaterialX::MslShaderGenerator::create();
        initialize_context(msl_context);
        msl_init = true;
    }
}

void generate_shader_code(const MaterialX::DocumentPtr& mx_doc, MaterialX::GenContext& context, MaterialX::GlslMaterialPtr& material, ExportFormat format, const std::string& output_path) {
    MaterialX::TypedElementPtr element = material->getElement();
    
    try {
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
            write_text_file(pixel_shader, add_suffics_to_path(output_path, "frag"));
            write_text_file(vertex_shader, add_suffics_to_path(output_path, "vert"));
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

void generate_shader(const MaterialX::DocumentPtr& mx_doc, const std::string& output_path, ExportFormat format) {
    try_init_generators(format);

    // create a blank mx doc
    MaterialX::DocumentPtr doc = MaterialX::createDocument();
    // import input materials
    doc->importLibrary(mx_doc);

    // define elements to shader generator
    std::vector<MaterialX::TypedElementPtr> elements = MaterialX::findRenderableElements(doc);
    size_t elements_count = elements.size();
    if (elements_count == 0) {
        // show a warning message
        log_message("Nothing to generate. Material does not contains valid material node.", XSI::siWarningMsg);
    }
    else {
        // import std lib to the doc
        doc->importLibrary(std_lib);
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
            if (format == ExportFormat::OSL) { generate_shader_code(doc, osl_context, material, format, use_output); }
            else if (format == ExportFormat::GLSL) { generate_shader_code(doc, glsl_context, material, format, use_output); }
            else if (format == ExportFormat::MDL) { generate_shader_code(doc, mdl_context, material, format, use_output); }
            else if (format == ExportFormat::MSL) { generate_shader_code(doc, msl_context, material, format, use_output); }
        }
    }
}