import unreal
import json
from pathlib import Path

project = Path(unreal.Paths.project_dir())
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []
for name in ['Container_Quaternius']:
    task = unreal.AssetImportTask()
    task.filename = str(project / 'Content' / 'PortSim' / 'Assets' / 'SourceFiles' / (name + '.glb'))
    task.destination_path = '/Game/PortSim/Assets/Models/' + name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    paths = unreal.EditorAssetLibrary.list_assets(task.destination_path, recursive=True)
    for path in paths:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.StaticMesh):
            b = asset.get_bounds()
            entry = dict(source=name, path=path, origin=[b.origin.x,b.origin.y,b.origin.z], extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z])
            report.append(entry)
            unreal.log('PORTSIM_IMPORTED ' + json.dumps(entry))
    if not any(r['source'] == name for r in report):
        raise RuntimeError('No static mesh imported for ' + name)

# Simple opaque palette, also used by the procedural moving crane and yard.
for name, color in [('CraneYellow',(0.95,0.48,0.035)),('Steel',(0.08,0.12,0.16)),('Quay',(0.17,0.23,0.28)),('Pickup',(0.1,0.3,0.5)),('Target',(0.08,0.5,0.3))]:
    path = '/Game/PortSim/Assets/Materials/M_' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_' + name, '/Game/PortSim/Assets/Materials', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    node = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    node.set_editor_property('constant', unreal.LinearColor(*color,1))
    unreal.MaterialEditingLibrary.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',0.7)
    unreal.MaterialEditingLibrary.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)

(project / 'Saved' / 'Reports').mkdir(parents=True, exist_ok=True)
(project / 'Saved' / 'Reports' / 'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('PORTSIM_IMPORT_PASS')
