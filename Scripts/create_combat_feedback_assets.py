"""Import original audio, add two materials, and build additive combat animations."""
from pathlib import Path
import unreal

lib = unreal.EditorAssetLibrary
root = '/Game/AshenRift'
source = Path(unreal.Paths.project_dir()) / 'Data' / 'AudioSource'
tasks = []
for wav in sorted(source.glob('S_AR_*.wav')):
    if lib.does_asset_exist(root + '/Audio/' + wav.stem):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', str(wav))
    task.set_editor_property('destination_path', root + '/Audio')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', False)
    tasks.append(task)
assert len(list(source.glob('S_AR_*.wav'))) == 11, 'Missing generated WAV sources'
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for wav in source.glob('S_AR_*.wav'):
    assert lib.does_asset_exist(root + '/Audio/' + wav.stem), wav.name

for name, parameter, color in [('M_ARHitFlash', False, (1, .65, .16)), ('M_ARImpact', True, (1, .7, .15))]:
    if lib.does_asset_exist(root + '/Materials/' + name):
        continue
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    expression = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter if parameter else unreal.MaterialExpressionConstant3Vector)
    if parameter:
        expression.set_editor_property('parameter_name', 'Color')
        expression.set_editor_property('default_value', unreal.LinearColor(*color, 1))
    else:
        expression.set_editor_property('constant', unreal.LinearColor(*color, 1))
    assert unreal.MaterialEditingLibrary.connect_material_property(expression, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert unreal.MaterialEditingLibrary.connect_material_property(expression, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert lib.save_loaded_asset(mat, only_if_is_dirty=False)

assert unreal.ARCombatAssets.create_combat_animations(), 'Combat animation creation failed'
lib.save_directory(root + '/Animations', only_if_is_dirty=False)
unreal.log('ASHEN_RIFT_COMBAT_FEEDBACK_ASSETS_SUCCESS')
