"""Create a six-slot dungeon with runtime room interiors and real NavMesh bounds."""
import unreal
ROOT = '/Game/AshenRift'
MAP = ROOT + '/Maps/L_RandomDungeon'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
assert not lib.does_asset_exist(MAP), 'Random dungeon already exists; refusing to overwrite.'

factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.ARPrototypeGameMode)
mode_path = ROOT + '/Core/BP_ARRandomDungeonMode'
if not lib.does_asset_exist(mode_path):
    mode = unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_ARRandomDungeonMode', ROOT + '/Core', unreal.Blueprint, factory)
    lib.save_loaded_asset(mode)
mode_class = lib.load_blueprint_class(mode_path)
defaults = unreal.get_default_object(mode_class)
defaults.set_editor_property('default_pawn_class', lib.load_blueprint_class(ROOT + '/Characters/BP_ARPlayableCharacter'))
defaults.set_editor_property('enemy_class', lib.load_blueprint_class(ROOT + '/Enemies/BP_ARGrunt'))
defaults.set_editor_property('boss_class', lib.load_blueprint_class(ROOT + '/Enemies/BP_ARAshenWarden'))
defaults.set_editor_property('use_navigation', True)
lib.save_asset(mode_path, only_if_is_dirty=False)

assert level.new_level(MAP)
world = level.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', mode_class)
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
floor_material = lib.load_asset(ROOT + '/Materials/M_DungeonFloor')
wall_material = lib.load_asset(ROOT + '/Materials/M_Wall')

def block(name, location, size, mat):
    actor = level.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*(value / 100 for value in size)))
    actor.static_mesh_component.set_material(0, mat)
    return actor

def sign(text, x, y=850, z=220, size=38):
    actor = level.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, z), unreal.Rotator(pitch=0, yaw=-90 if y > 0 else 90, roll=0))
    actor.text_render.set_text(text)
    actor.text_render.set_world_size(size)

names = ['CAMP - 1 WARRIOR / 2 MAGE / E ENTER', 'RANDOM CHAMBER 1', 'RANDOM CHAMBER 2', 'SUPPLY / EXTRACTION', 'RANDOM CHAMBER 3', 'ASHEN WARDEN']
for index, name in enumerate(names):
    x = index * 2400
    block('Floor_' + str(index), (x, 0, -25), (2400, 2000, 50), floor_material)
    for side in [-1, 1]:
        block('SideWall', (x, side * 1000, 200), (2400, 30, 400), wall_material)
        if index < 5:
            block('DoorWall', (x + 1200, side * 600, 200), (30, 800, 400), wall_material)
    sign(name, x)
    light = level.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, 0, 550))
    light.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property('intensity', 2800)
    light.light_component.set_editor_property('attenuation_radius', 1700)
block('Camp_Back', (-1200, 0, 200), (30, 2000, 400), wall_material)
block('Dungeon_End', (13200, 0, 200), (30, 2000, 400), wall_material)
start = level.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-700, 0, 120))
start.set_actor_label('CampSpawn')
director = level.spawn_actor_from_class(unreal.ARDungeonDirector, unreal.Vector())
director.set_actor_label('RandomRoomDirector')
shop = level.spawn_actor_from_class(unreal.ARShop, unreal.Vector(7200, 650, 55))
shop.get_editor_property('mesh').set_material(0, lib.load_asset(ROOT + '/Materials/M_Trader'))
sign('TRADER - E / 1 POTION / 2 POWER', 7200, 850, 200, 32)
portal = level.spawn_actor_from_class(unreal.ARPortal, unreal.Vector(7200, -650, 12))
portal.mesh.set_material(0, lib.load_asset(ROOT + '/Materials/M_Portal'))
sign('EQUIPMENT CACHE - PICK UP / TAB / Z X C', 1750, -850, 170, 30)
sun = level.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(pitch=-60, yaw=-30, roll=0))
sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity', 2)
sky = level.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 700))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', .7)
fog = level.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector())
fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_component.set_editor_property('fog_density', .012)
fog_component.set_editor_property('fog_inscattering_color', unreal.LinearColor(.045, .065, .1, 1))
post = level.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector())
post.set_editor_property('unbound', True)
settings = post.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_min_brightness', True)
settings.set_editor_property('override_auto_exposure_max_brightness', True)
settings.set_editor_property('auto_exposure_min_brightness', 1.0)
settings.set_editor_property('auto_exposure_max_brightness', 1.0)
settings.set_editor_property('override_auto_exposure_bias', True)
settings.set_editor_property('auto_exposure_bias', -1.5)
post.set_editor_property('settings', settings)
bounds = unreal.ARDungeonEditorLibrary.create_navigation_bounds(world, unreal.Vector(6000, 0, 200), unreal.Vector(7500, 1200, 500))
assert bounds, 'Could not create navigation brush'
bounds.set_actor_label('DungeonNavigationBounds')
_, nav_extent = bounds.get_actor_bounds(False)
assert min(nav_extent.x, nav_extent.y, nav_extent.z) > 0, 'Navigation brush has zero bounds'
unreal.log('DUNGEON_NAV_BOUNDS %s %s %s' % (nav_extent.x, nav_extent.y, nav_extent.z))
assert level.save_current_level()
unreal.log('ASHEN_RIFT_RANDOM_DUNGEON_CREATED')
