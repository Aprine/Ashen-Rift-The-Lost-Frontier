"""Run against the downloaded teammate project, before copying the new map.

Uses UE package duplication so neither source map is renamed on disk. This is
an integration utility, not a startup or recurring generation step.
"""
import unreal

SOURCE = '/Game/AshenRift/Maps/L_RandomDungeon'
DESTINATION = '/Game/AshenRift/Maps/L_FrontierDungeon'
lib = unreal.EditorAssetLibrary
assert lib.does_asset_exist(SOURCE), 'Contributed source map is missing'
assert not lib.does_asset_exist(DESTINATION), 'Refusing to overwrite an existing map'
assert unreal.EditorLevelLibrary.load_level(SOURCE)
world = unreal.EditorLevelLibrary.get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world, DESTINATION), 'UE Save Map As failed'
assert unreal.EditorLevelLibrary.load_level(DESTINATION), 'Saved map cannot be reopened'
unreal.log('ASHEN_RIFT_TEAM_MAP_DUPLICATED')
