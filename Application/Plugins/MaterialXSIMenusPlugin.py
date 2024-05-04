import win32com.client
from win32com.client import constants
import os

null = None
false = 0
true = 1
app = Application

prev_export_params = None


def XSILoadPlugin(in_reg):
    in_reg.Author = "blairs"
    in_reg.Name = "MaterialXSIMenusPlugin"
    in_reg.Major = 1
    in_reg.Minor = 0

    in_reg.RegisterMenu(constants.siMenuMaterialManagerTopLevelID, "MaterialX Export", false, false)
    in_reg.RegisterMenu(constants.siMenuRTNodeContextID, "MaterialX Compounds", true, false)
    # RegistrationInsertionPoint - do not remove this line

    return true


def XSIUnloadPlugin(in_reg):
    strPluginName = in_reg.Name
    app.LogMessage(str(strPluginName) + str(" has been unloaded."), constants.siVerbose)
    return true


def MaterialXExport_Init(in_ctxt):
    menu = in_ctxt.Source
    menu.AddCallbackItem("Selected Materials", "export_materials")
    return true


def MaterialXCompounds_Init(in_ctxt):
    menu = in_ctxt.Source
    menu.AddCallbackItem("Export Compounds", "export_compounds")
    return true


def export_window(export_array):
    scene_root = app.ActiveProject2.ActiveScene.Root
    prop = scene_root.AddProperty("CustomProperty", False, "MaterialX Export")

    prop.AddParameter3("file_path", constants.siString, "", "", "", False, False)

    param = prop.AddParameter3("textures_use_relative_path", constants.siBool, True, False, False)
    param.Animatable = False

    param = prop.AddParameter3("textures_copy", constants.siBool, True, False, False)
    param.Animatable = False

    param = prop.AddParameter3("textures_folder", constants.siString, "textures", False, False)
    param.Animatable = False

    layout = prop.PPGLayout
    layout.Clear()
    layout.AddGroup("Export")
    item = layout.AddItem("file_path", "File", constants.siControlFilePath)
    filterstring = "MaterialX files (*.mtlx)|*.mtlx|"
    item.SetAttribute(constants.siUIFileFilter, filterstring)
    layout.EndGroup()

    layout.AddGroup("Textures")
    layout.AddItem("textures_use_relative_path", "Use Relative Path")
    layout.AddItem("textures_copy", "Copy Sources")
    layout.AddItem("textures_folder", "Folder")
    layout.EndGroup()

    layout.Language = "Python"
    layout.Logic = '''
def update(prop):
    textures_copy = prop.Parameters("textures_copy").Value
    if textures_copy:
        prop.Parameters("textures_folder").ReadOnly = False
    else:
        prop.Parameters("textures_folder").ReadOnly = True

def OnInit():
    prop = PPG.Inspected(0)
    update(prop)

def textures_copy_OnChanged():
    prop = PPG.Inspected(0)
    update(prop)
'''

    property_keys = [
        "textures_use_relative_path",
        "textures_copy",
        "textures_folder"]

    global prev_export_params
    if prev_export_params is not None:
        for k in property_keys:
            prop.Parameters(k).Value = prev_export_params[k]

    rtn = app.InspectObj(prop, "", "Export MaterialX file...", constants.siModal, False)
    if rtn is False:
        file_path = prop.Parameters("file_path").Value
        if len(file_path) > 0:
            file_path_reduce, extension = os.path.splitext(file_path)
            app.MaterialXSIExport(export_array, file_path_reduce + ".mtlx",
                                  prop.Parameters("textures_use_relative_path").Value,
                                  prop.Parameters("textures_copy").Value,
                                  prop.Parameters("textures_folder").Value)
        else:
            app.LogMessage("Define non-empty export path")

    if prev_export_params is None:
        prev_export_params = {}
    for k in property_keys:
        prev_export_params[k] = prop.Parameters(k).Value

    app.DeleteObj(prop)


def export_materials(in_ctxt):
    view = in_ctxt.GetAttribute("Target")
    selection_str = view.GetAttributeValue("selection")
    material_ids = []
    if len(selection_str) > 0:
        parts = selection_str.split(",")
        for part in parts:
            material = app.Dictionary.GetObject(part)
            material_id = material.ObjectID
            material_ids.append(material_id)
    export_window(material_ids)


def export_compounds(in_ctxt):
    nodes_collection = in_ctxt.GetAttribute("Target")
    node_ids = []
    for node in nodes_collection:
        node_id = node.ObjectID
        node_ids.append(node_id)
    export_window(node_ids)