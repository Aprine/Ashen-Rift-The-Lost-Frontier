"""Create the loadable shell for the standalone Random Ruins expedition."""

import unreal

ROOT = '/Game/AshenRift'
MAP = ROOT + '/Maps/Map_RandomRuins'
editor = unreal.EditorLevelLibrary
assets = unreal.EditorAssetLibrary

if assets.does_asset_exist(MAP):
    assert editor.load_level(MAP)
else:
    assert editor.new_level(MAP)

world = editor.get_editor_world()
mode = assets.load_blueprint_class(ROOT + '/Core/BP_ARRandomDungeonMode')
assert mode
world.get_world_settings().set_editor_property('default_game_mode', mode)
world.get_world_settings().set_editor_property('force_no_precomputed_lighting', True)


def place(cls, label, position):
    existing = next((a for a in editor.get_all_level_actors() if a.get_actor_label() == label), None)
    if existing:
        return existing
    actor = editor.spawn_actor_from_class(cls, unreal.Vector(*position))
    assert actor, label
    actor.set_actor_label(label)
    return actor


place(unreal.PlayerStart, 'RuinsSpawnBeforeGeneration', (0, 0, 115))
wing = place(unreal.ARExpeditionBranchWing, 'ExpeditionRuinsGenerator', (0, 0, 0))
wing.set_editor_property('standalone', True)
site = place(unreal.ARExpeditionSite, 'ExpeditionSite', (0, 0, 115))
site.set_editor_property('standalone_wing', wing)
if not any(isinstance(a, unreal.NavMeshBoundsVolume) for a in editor.get_all_level_actors()):
    bounds = unreal.ARDungeonEditorLibrary.create_navigation_bounds(
        world, unreal.Vector(0, 0, 180), unreal.Vector(5250, 5250, 560))
    assert bounds
    bounds.set_actor_label('RandomRuinsNavigation')

for actor in editor.get_all_level_actors():
    if isinstance(actor, unreal.RecastNavMesh):
        assert editor.destroy_actor(actor)
assert editor.save_all_dirty_levels()
unreal.log('ASHEN_RANDOM_RUINS_CONFIGURED')
