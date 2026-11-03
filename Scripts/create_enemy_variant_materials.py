"""Add two skeletal-compatible materials; never alter existing maps or assets."""
import unreal

root = '/Game/AshenRift/Materials'
lib = unreal.EditorAssetLibrary
for name, rgb in [('M_Hexer', (.035, .16, .36)), ('M_Sentinel', (.43, .23, .045))]:
    path = root + '/' + name
    if lib.does_asset_exist(path):
        mat = lib.load_asset(path)
        assert mat.get_editor_property('used_with_skeletal_mesh'), path + ' exists without skeletal support'
        continue
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', unreal.LinearColor(*rgb, 1))
    assert unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r', .6)
    assert unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert lib.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('ASHEN_RIFT_ENEMY_VARIANT_MATERIALS_SUCCESS')
