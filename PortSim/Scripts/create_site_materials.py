"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
import unreal

palette = {
    'SiteAsphalt': (0.055, 0.065, 0.075),
    'SiteRoad': (0.025, 0.032, 0.04),
    'SiteWhite': (0.75, 0.78, 0.72),
    'SiteBuilding': (0.35, 0.41, 0.43),
    'SiteBlue': (0.025, 0.18, 0.48),
    'SiteOrange': (0.95, 0.32, 0.025),
    'SiteRed': (0.42, 0.06, 0.025),
    'SiteGreen': (0.025, 0.24, 0.17),
    'SiteWater': (0.018, 0.11, 0.18),
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
folder = '/Game/PortSim/Assets/Materials'
for name, color in palette.items():
    path = folder + '/M_' + name
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_' + name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('used_with_instanced_static_meshes', True)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError('Could not save ' + path)
unreal.log('PORTSIM_SITE_MATERIALS_PASS')
