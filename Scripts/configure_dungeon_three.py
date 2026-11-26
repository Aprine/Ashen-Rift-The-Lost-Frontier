"""Connect Kuangxin's Dungeon 3, shop and boss maps inside one expedition."""
from collections import defaultdict

import unreal


EDITOR = unreal.EditorLevelLibrary
ASSETS = unreal.EditorAssetLibrary
MAPS = '/Game/AshenRift/Maps/'
MATERIALS = '/Game/AshenRift/Materials/'
INTEGRATION_TAG = 'AshenRiftDungeonThree'
SHOP_ORIGIN = (18000, 0, 0)
BOSS_ORIGIN = (31000, 0, 0)


def at(origin, local):
    return tuple(a + b for a, b in zip(origin, local))


def relative_to(position, origin):
    return unreal.Vector(*(value - base for value, base in zip(position, origin)))


def spawn(cls, label, position, yaw=0):
    actor = EDITOR.spawn_actor_from_class(cls, unreal.Vector(*position), unreal.Rotator(yaw=yaw))
    assert actor, label
    actor.set_actor_label(label)
    actor.tags = [INTEGRATION_TAG, label]
    return actor


def persistent_actors():
    prefix = EDITOR.get_editor_world().get_path_name() + ':'
    return [actor for actor in EDITOR.get_all_level_actors() if actor.get_path_name().startswith(prefix)]


def clean_actors():
    for actor in persistent_actors():
        if INTEGRATION_TAG in list(map(str, actor.tags)):
            EDITOR.destroy_actor(actor)
    return persistent_actors()


def add_bounds(world, label, center, extent):
    bounds = unreal.ARDungeonEditorLibrary.create_navigation_bounds(
        world, unreal.Vector(*center), unreal.Vector(*extent))
    assert bounds, label
    bounds.set_actor_label(label)
    bounds.tags = [INTEGRATION_TAG]


def collision_box(label, center, size):
    actor = spawn(unreal.StaticMeshActor, label, center)
    comp = actor.static_mesh_component
    comp.set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    comp.set_static_mesh(ASSETS.load_asset('/Engine/BasicShapes/Cube'))
    actor.set_actor_scale3d(unreal.Vector(*(distance / 100.0 for distance in size)))
    comp.set_collision_profile_name('BlockAll')
    actor.set_actor_hidden_in_game(True)
    return actor


def set_no_collision(component):
    component.set_collision_profile_name('NoCollision')
    component.set_editor_property('can_ever_affect_navigation', False)


def support_walkable_tiles(actors):
    # Level floors are rectangles on the same horizontal plane. Join adjacent
    # tiles by row to close the navigation seams and keep the outer footprint.
    rows = defaultdict(list)
    for actor in actors:
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        comp = actor.static_mesh_component
        mesh = comp.static_mesh
        if not mesh:
            continue
        if actor.get_actor_label() == 'Dungeon2_Floor':
            set_no_collision(comp)
            continue
        if '/Floors/' not in mesh.get_path_name() or abs(actor.get_actor_location().z) > 5:
            continue
        center, ext = actor.get_actor_bounds(False)
        if min(ext.x, ext.y) < 45 or ext.z > 45:
            continue
        rows[(round(center.y - ext.y, 1), round(center.y + ext.y, 1))].append(
            (round(center.x - ext.x, 1), round(center.x + ext.x, 1)))
        set_no_collision(comp)
    assert rows, 'Map has no horizontal floor tiles'
    number = 0
    for (bottom, top), segments in sorted(rows.items()):
        ranges = []
        for start, end in sorted(segments):
            if ranges and start <= ranges[-1][1] + 2:
                ranges[-1] = ranges[-1][0], max(end, ranges[-1][1])
            else:
                ranges.append((start, end))
        for start, end in ranges:
            number += 1
            collision_box('FloorSupport_%03d' % number, ((start + end) / 2, (bottom + top) / 2, -20),
                          (end - start, top - bottom, 40))


def fill(label, pos, intensity=160, radius=700):
    actor = spawn(unreal.PointLight, label, pos)
    comp = actor.light_component
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('intensity', intensity)
    comp.set_editor_property('attenuation_radius', radius)
    comp.set_editor_property('light_color', unreal.Color(209, 222, 247))
    comp.set_editor_property('cast_shadows', False)


def use_dynamic_lighting(world, actors):
    world.get_world_settings().set_editor_property('force_no_precomputed_lighting', True)
    assert unreal.ARDungeonEditorLibrary.clear_inherited_map_lighting(world)
    for actor in actors:
        if isinstance(actor, (unreal.PointLight, unreal.SkyLight)):
            comp = actor.light_component
            comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)


def prepare_room(name):
    assert EDITOR.load_level(MAPS + name), name
    actors = clean_actors()
    for actor in actors:
        # The persistent map owns its spawn, fog and skylight. Local torch
        # lights travel with each room.
        if isinstance(actor, (unreal.PlayerStart, unreal.ExponentialHeightFog, unreal.SkyLight)):
            EDITOR.destroy_actor(actor)
    actors = persistent_actors()
    unreal.log('Configuring light and floor for ' + name)
    use_dynamic_lighting(EDITOR.get_editor_world(), actors)
    support_walkable_tiles(actors)
    unreal.log('Adding gameplay to ' + name)
    if name == 'Map_Shop':
        shop = spawn(unreal.ARShop, 'ExpeditionTrader', (790, 558, 62))
        shop.set_actor_scale3d(unreal.Vector(.42, .42, .22))
        mesh = shop.get_editor_property('mesh')
        mesh.set_material(0, ASSETS.load_asset(MATERIALS + 'M_Trader'))
        set_no_collision(mesh)
        fill('ShopEntryFill', (0, 280, 315))
        fill('ShopCounterFill', (800, 640, 325))
    else:
        # These three composite frames use solid collision along the opening.
        # Retain the visible arches. Restore blocking on both outer jambs.
        for actor in actors:
            label = actor.get_actor_label()
            if label not in ('wall_doorway_Tsplit', 'wall_doorway_Tsplit2', 'wall_doorway_sides2'):
                continue
            set_no_collision(actor.static_mesh_component)
            center, ext = actor.get_actor_bounds(False)
            for side in (-1, 1):
                width = ext.x - 125
                pos = (center.x + side * (125 + width / 2), center.y, 175)
                collision_box(label + ('_West' if side < 0 else '_East'), pos, (width, 76, 350))
        fill('BossEntranceFill', (2810, 6320, 320))
        fill('BossNorthDoorFill', (2400, 4980, 330))
        fill('BossArenaFill', (2400, 3575, 360), 205, 950)
    unreal.log('Saving ' + name)
    assert EDITOR.save_current_level(), name


for room in ('Map_Shop', 'Map_Boss'):
    prepare_room(room)

assert EDITOR.load_level(MAPS + 'Map_Dungeon3')
world = EDITOR.get_editor_world()
actors = clean_actors()
for actor in actors:
    if isinstance(actor, (unreal.NavMeshBoundsVolume, unreal.RecastNavMesh)):
        EDITOR.destroy_actor(actor)
actors = persistent_actors()
mode = ASSETS.load_blueprint_class('/Game/AshenRift/Core/BP_ARRandomDungeonMode')
assert mode
world.get_world_settings().set_editor_property('default_game_mode', mode)
use_dynamic_lighting(world, actors)
support_walkable_tiles(actors)

frame = ASSETS.load_asset('/Game/Environments/KayKitDungeon/Meshes/Walls/SM_wall_open_scaffold')
assert frame
for actor in actors:
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    comp = actor.static_mesh_component
    mesh = comp.static_mesh
    if not mesh:
        continue
    name = mesh.get_name()
    if name in ('SM_wall_doorway_door', 'SM_wall_doorway_scaffold_door'):
        set_no_collision(comp)
    elif name in ('SM_wall_doorway', 'SM_wall_doorway_scaffold', 'SM_wall_open_scaffold'):
        comp.set_static_mesh(frame)
        scale = actor.get_actor_scale3d()
        actor.set_actor_scale3d(unreal.Vector(max(1.25, scale.x), max(1.25, scale.y), scale.z))

start = (-1180, 0, 120)
starts = [a for a in actors if isinstance(a, unreal.PlayerStart)]
assert len(starts) == 1
starts[0].set_actor_location_and_rotation(unreal.Vector(*start), unreal.Rotator(yaw=0), False, True)
site = spawn(unreal.ARExpeditionSite, 'ExpeditionSite', start)
encounters = [(135, -450, 110), (1000, 1230, 110), (-880, 2640, 110),
              (-260, 4750, 110), (-2140, -1960, 110)]
site.set_editor_property('encounter_offsets', [relative_to(p, start) for p in encounters])
site.set_editor_property('supply_offset', unreal.Vector(135, 80, -68))
site.set_editor_property('boss_offset', relative_to(at(BOSS_ORIGIN, (2400, 3600, 155)), start))

portal = spawn(unreal.ARPortal, 'ExpeditionExtraction', (-1410, 0, 15))
portal.mesh.set_material(0, ASSETS.load_asset(MATERIALS + 'M_Portal'))

links = (
    ('ExpeditionShopEntrance', (-1765, 2720, 13), -5, '进入商店  ·  在补给区消费本局金币',
     'ExpeditionShopReturn', at(SHOP_ORIGIN, (0, 0, 13)), 90, '返回地牢三', 'M_Trader'),
    ('ExpeditionBossEntrance', (-510, 5240, 13), -90, '进入首领区  ·  挑战灰烬守望者',
     'ExpeditionBossReturn', at(BOSS_ORIGIN, (3130, 6390, 13)), 180, '返回地牢三', 'M_Rune'),
)
for name, pos, yaw, prompt, return_name, return_pos, return_yaw, return_prompt, material in links:
    forward = spawn(unreal.ARExpeditionPassage, name, pos, yaw)
    reverse = spawn(unreal.ARExpeditionPassage, return_name, return_pos, return_yaw)
    for actor, target, text in ((forward, reverse, prompt), (reverse, forward, return_prompt)):
        actor.set_actor_scale3d(unreal.Vector(2.55, 2.55, .08))
        actor.set_editor_property('destination', target)
        actor.set_editor_property('prompt', text)
        actor.mesh.set_material(0, ASSETS.load_asset(MATERIALS + material))

add_bounds(world, 'DungeonThreeNavigation', (-2600, 550, 180), (5500, 5550, 850))
add_bounds(world, 'ShopNavigation', at(SHOP_ORIGIN, (825, 660, 180)), (1175, 1020, 600))
add_bounds(world, 'BossNavigation', at(BOSS_ORIGIN, (1800, 3950, 180)), (2400, 4300, 650))
for i, pos in enumerate(((-1180, 0, 330), (-1765, 2720, 325), (-510, 5240, 325))):
    fill('DungeonGuideFill_%d' % i, pos)

for room, origin in (('Map_Shop', SHOP_ORIGIN), ('Map_Boss', BOSS_ORIGIN)):
    if unreal.GameplayStatics.get_streaming_level(world, room):
        continue
    transform = unreal.Transform()
    transform.translation = unreal.Vector(*origin)
    stream = unreal.EditorLevelUtils.add_level_to_world_with_transform(
        world, MAPS + room, unreal.LevelStreamingAlwaysLoaded, transform)
    assert stream, room

# Adding a streaming level makes it current in the Levels panel. Save
# the dirty persistent map as well as both room packages.
assert EDITOR.save_all_dirty_levels()
unreal.log('ASHEN_DUNGEON_THREE_CONFIGURED')
