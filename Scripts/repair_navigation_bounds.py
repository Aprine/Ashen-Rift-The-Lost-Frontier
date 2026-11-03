import unreal
level = unreal.EditorLevelLibrary
assert level.load_level('/Game/AshenRift/Maps/L_RandomDungeon')
volumes = [a for a in level.get_all_level_actors() if isinstance(a, unreal.NavMeshBoundsVolume)]
assert len(volumes) == 1, 'Unexpected navigation volume count; stop.'
assert unreal.ARDungeonEditorLibrary.resize_navigation_bounds(volumes[0], unreal.Vector(6000, 0, 200), unreal.Vector(7500, 1200, 500))
center, extent = volumes[0].get_actor_bounds(False)
unreal.log('NAV_REPAIRED_EXTENT %s %s %s' % (extent.x, extent.y, extent.z))
for actor in level.get_all_level_actors():
    if isinstance(actor, unreal.RecastNavMesh):
        actor.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
        actor.set_editor_property('agent_radius', 72.0)
        actor.set_editor_property('agent_height', 300.0)
assert level.save_current_level()
unreal.log('ASHEN_RIFT_NAV_BOUNDS_REPAIRED')
