"""Connect the preserved teammate layouts to the integrated level only."""
import unreal

MAP = '/Game/AshenRift/Maps/L_FrontierDungeon'
level = unreal.EditorLevelLibrary
lib = unreal.EditorAssetLibrary
assert level.load_level(MAP)
directors = [actor for actor in level.get_all_level_actors() if isinstance(actor, unreal.ARDungeonDirector)]
assert len(directors) == 1
director = directors[0]
layouts = [lib.load_blueprint_class('/Game/AshenRift/Rooms/' + name) for name in
           ('BP_Room_Pillars', 'BP_Room_Barricade', 'BP_Room_Cross')]
camp = lib.load_blueprint_class('/Game/AshenRift/Rooms/BP_Camp')
assert all(layouts) and camp
director.set_editor_property('room_layouts', layouts)
director.set_editor_property('camp_layout', camp)
director.set_editor_property('camp_origin', unreal.Vector())
for actor in level.get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    component = actor.static_mesh_component
    mesh = component.static_mesh
    if not mesh:
        continue
    name = mesh.get_name()
    if (name.startswith('wall_doorway') and not name.endswith('_door')) or name == 'wall_open_scaffold':
        actor.modify()
        component.modify()
        if name in ('wall_doorway_scaffold', 'wall_doorway'):
            # Both variants contain a closed wooden door in the same mesh.
            # The sides variant is an actual open frame.
            mesh = unreal.load_asset('/Game/AshenRift/KayKit/Architecture/Walls/wall_doorway_sides')
            assert mesh
            component.set_static_mesh(mesh)
        scale = actor.get_actor_scale3d()
        # The shared NavMesh accommodates the 288 cm Boss capsule. Original
        # arches were too low for the configured 300 cm clearance.
        if name == 'wall_open_scaffold':
            actor.set_actor_scale3d(unreal.Vector(scale.x, scale.y, max(scale.z, 1.2)))
        else:
            # Recast erodes the opening by the agent radius and grid cells;
            # provide clearance under the sides of the arch as well as its apex.
            actor.set_actor_scale3d(unreal.Vector(max(scale.x, 1.4), scale.y, max(scale.z, 1.5)))
        body = mesh.get_editor_property('body_setup')
        unreal.log('DOOR_FRAME %s trace=%s scale=%s' % (actor.get_actor_label(),
                   body.get_editor_property('collision_trace_flag') if body else None,
                   actor.get_actor_scale3d()))
        if body:
            # Rebuild even when the flag was already set in the delivered pack:
            # its cached navigation geometry may still contain a solid hull.
            unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            lib.save_loaded_asset(mesh, only_if_is_dirty=False)
    elif name.startswith('wall_doorway') and name.endswith('_door'):
        # Open wooden leaves remain visible without encroaching on the passage.
        actor.modify()
        component.modify()
        component.set_collision_profile_name('NoCollision')
        component.set_editor_property('can_ever_affect_navigation', False)
assert level.save_current_level()
unreal.log('ASHEN_RIFT_FRONTIER_CONFIGURED')
