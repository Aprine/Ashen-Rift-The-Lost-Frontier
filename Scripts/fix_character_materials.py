import unreal
for path in ['/Game/AshenRift/Materials/M_Enemy', '/Game/AshenRift/Materials/M_Warden']:
    mat = unreal.EditorAssetLibrary.load_asset(path)
    assert mat, path
    mat.set_editor_property('used_with_skeletal_mesh', True)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('ASHEN_RIFT_SKELETAL_MATERIALS_FIXED')
