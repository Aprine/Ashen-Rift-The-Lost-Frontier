"""Apply Ashen Rift gameplay placement to Kuangxin's three UE4 worlds."""
import math
import unreal

level = unreal.EditorLevelLibrary
assets = unreal.EditorAssetLibrary
root = '/Game/AshenRift'
mode = assets.load_blueprint_class(root + '/Core/BP_ARRandomDungeonMode')
open_frame = assets.load_asset('/Game/Environments/KayKitDungeon/Meshes/Walls/SM_wall_open_scaffold')
portal_material = assets.load_asset(root + '/Materials/M_Portal')
shop_material = assets.load_asset(root + '/Materials/M_Trader')
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
assert mode and open_frame and portal_material and shop_material


def actor_at(cls, name, location, yaw=0):
    actor = level.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(yaw=yaw))
    assert actor, name
    actor.set_actor_label(name)
    return actor


def marker_offset(point, origin, yaw):
    x, y, z = (point[i] - origin[i] for i in range(3))
    angle = math.radians(yaw)
    return unreal.Vector(x * math.cos(angle) + y * math.sin(angle),
                         -x * math.sin(angle) + y * math.cos(angle), z)


def fill_light(number, location):
    actor = actor_at(unreal.PointLight, 'ExpeditionFill_' + str(number), location)
    actor.tags = ['AshenRiftExpedition']
    light = actor.light_component
    light.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity', 145)
    light.set_editor_property('attenuation_radius', 630)
    light.set_editor_property('light_color', unreal.Color(187, 208, 237))
    light.set_editor_property('cast_shadows', False)


def configure_doors(actors):
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh
        if not mesh:
            continue
        name = mesh.get_name()
        if name in ('SM_wall_doorway', 'SM_wall_doorway_scaffold'):
            actor.modify()
            component.modify()
            component.set_static_mesh(open_frame)
            name = open_frame.get_name()
        if name == open_frame.get_name():
            actor.modify()
            # These imports are laid upright with a 90-degree roll. Local Y
            # is height, and local X spans the doorway.
            scale = actor.get_actor_scale3d()
            actor.set_actor_scale3d(unreal.Vector(max(1.25, scale.x), max(1.25, scale.y), scale.z))
        if name in ('SM_wall_doorway_door', 'SM_wall_doorway_scaffold_door'):
            actor.modify()
            component.modify()
            component.set_collision_profile_name('NoCollision')
            component.set_editor_property('can_ever_affect_navigation', False)


def configure_walkable_floor(actors, source_label):
    foundation = next(a for a in actors if a.get_actor_label() == source_label)
    center, extent = foundation.get_actor_bounds(False)
    # Keep the visible authored plane and tiles, with a solid continuous base
    # for walking and navigation across tile seams.
    support = actor_at(unreal.StaticMeshActor, 'ExpeditionFloorCollision', (center.x, center.y, center.z - 20))
    support.tags = ['AshenRiftExpedition']
    support.static_mesh_component.set_static_mesh(cube)
    support.set_actor_scale3d(unreal.Vector(extent.x / 50, extent.y / 50, .4))
    support.static_mesh_component.set_collision_profile_name('BlockAll')
    support.set_actor_hidden_in_game(True)
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh
        if actor != foundation and not (mesh and mesh.get_name().startswith('SM_floor_') and abs(actor.get_actor_location().z) < 25):
            continue
        actor.modify()
        component.modify()
        component.set_collision_profile_name('NoCollision')
        component.set_editor_property('can_ever_affect_navigation', False)


body = open_frame.get_editor_property('body_setup')
assert body
unreal.EditorStaticMeshLibrary.remove_collisions(open_frame)
body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
assert assets.save_loaded_asset(open_frame, only_if_is_dirty=False)

# World coordinates in the artist's original layout. The marker stores spawn
# offsets in its own frame, and can be adjusted from the editor Details panel.
layouts = {
    'Map_Hub': dict(start=(0, 340, 120), yaw=90),
    'Map_Dungeon1': dict(start=(-970, -30, 140), yaw=0,
        encounters=[(130, -270, 120), (810, -350, 120), (520, 1020, 120)],
        boss=(960, 460, 160), supply=(-720, 15, 52), trader=(220, 480, 65),
        portal=(-1210, -30, 17), fill=[(-780, -30, 330), (240, 80, 350), (880, 410, 340), (520, 1050, 355)]),
    'Map_Dungeon2': dict(start=(-1010, 0, 140), yaw=0,
        encounters=[(-260, -135, 120), (520, 470, 120), (1080, -900, 120)],
        boss=(1160, 1090, 160), supply=(-950, 80, 52), trader=(-530, 570, 65),
        portal=(-1330, 0, 17), fill=[(-850, 0, 330), (-430, 490, 330), (510, 520, 350), (1130, 960, 335), (1110, -900, 340)]),
}

for name, layout in layouts.items():
    assert level.load_level(root + '/Maps/' + name)
    world = level.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', mode)
    actors = level.get_all_level_actors()
    starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
    assert len(starts) == 1, name
    origin, yaw = layout['start'], layout['yaw']
    starts[0].set_actor_location_and_rotation(unreal.Vector(*origin), unreal.Rotator(yaw=yaw), False, True)
    for actor in actors:
        if 'AshenRiftExpedition' in list(map(str, actor.tags)):
            level.destroy_actor(actor)

    site = actor_at(unreal.ARExpeditionSite, 'ExpeditionSite', origin, yaw)
    site.tags = ['AshenRiftExpedition']
    site.set_editor_property('town', name == 'Map_Hub')
    if name != 'Map_Hub':
        configure_doors(actors)
        configure_walkable_floor(actors, 'Dungeon_Floor' if name == 'Map_Dungeon1' else 'Dungeon2_Floor')
        # Imported Recast actors contain tiles generated with another project's
        # tile-pool settings. Preserve the bounds and let this project build its data.
        for actor in actors:
            if isinstance(actor, unreal.RecastNavMesh):
                level.destroy_actor(actor)
        bounds = [a for a in actors if isinstance(a, unreal.NavMeshBoundsVolume)]
        assert len(bounds) == 1
        assert unreal.ARDungeonEditorLibrary.resize_navigation_bounds(bounds[0], unreal.Vector(0, 100, 200), unreal.Vector(2200, 2200, 600))
        site.set_editor_property('encounter_offsets', [marker_offset(p, origin, yaw) for p in layout['encounters']])
        site.set_editor_property('boss_offset', marker_offset(layout['boss'], origin, yaw))
        site.set_editor_property('supply_offset', marker_offset(layout['supply'], origin, yaw))
        shop = actor_at(unreal.ARShop, 'ExpeditionTrader', layout['trader'])
        shop.tags = ['AshenRiftExpedition']
        shop.get_editor_property('mesh').set_material(0, shop_material)
        portal = actor_at(unreal.ARPortal, 'ExpeditionExtraction', layout['portal'])
        portal.tags = ['AshenRiftExpedition']
        portal.mesh.set_material(0, portal_material)
        for index, position in enumerate(layout['fill'], 1):
            fill_light(index, position)
    else:
        # Use runtime lighting for the copied town rather than relying on
        # precomputed lighting from the Third Person template map.
        world.get_world_settings().set_editor_property('force_no_precomputed_lighting', True)
        assert unreal.ARDungeonEditorLibrary.clear_inherited_map_lighting(world)
        for actor in actors:
            if isinstance(actor, (unreal.DirectionalLight, unreal.SkyLight)):
                actor.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    assert level.save_current_level(), name
    unreal.log('ASHEN_EXPEDITION_CONFIGURED ' + name)
