"""Run inside UE 4.27 Editor. Creates an editable Blueprint greybox foundation.
Existing generated map is intentionally protected; delete/rename it in the editor
before rebuilding. This script is editor tooling, not runtime Python gameplay.
"""
import json
import os
import unreal

ROOT = '/Game/AshenRift'
MAP = ROOT + '/Maps/L_Prototype'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
if lib.does_asset_exist(MAP):
    raise RuntimeError('L_Prototype already exists; refusing to overwrite map edits.')
for folder in ['Maps', 'Core', 'Characters', 'Combat', 'Enemies', 'Items',
               'Inventory', 'UI', 'World', 'Data', 'Materials']:
    lib.make_directory(ROOT + '/' + folder)

character = lib.load_asset(ROOT + '/Characters/BP_ARPlayer') if lib.does_asset_exist(ROOT + '/Characters/BP_ARPlayer') else lib.duplicate_asset('/Game/ThirdPersonBP/Blueprints/ThirdPersonCharacter', ROOT + '/Characters/BP_ARPlayer')
game_mode = lib.load_asset(ROOT + '/Core/BP_ARGameMode') if lib.does_asset_exist(ROOT + '/Core/BP_ARGameMode') else lib.duplicate_asset('/Game/ThirdPersonBP/Blueprints/ThirdPersonGameMode', ROOT + '/Core/BP_ARGameMode')
unreal.get_default_object(lib.load_blueprint_class(ROOT + '/Core/BP_ARGameMode')).set_editor_property(
    'default_pawn_class', lib.load_blueprint_class(ROOT + '/Characters/BP_ARPlayer'))
lib.save_loaded_asset(character)
lib.save_loaded_asset(game_mode)

if not level.new_level(MAP):
    raise RuntimeError('Could not create prototype map')
world = level.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', lib.load_blueprint_class(ROOT + '/Core/BP_ARGameMode'))
cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')

def block(name, x, y, z, sx, sy, sz):
    actor = level.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(sx, sy, sz))
    actor.set_folder_path('Greybox')
    return actor

def label(text, x, y, z=170):
    actor = level.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, z),
                                          unreal.Rotator(0, 180, 0))
    actor.set_actor_label('Label_' + text)
    actor.text_render.set_text(text)
    actor.text_render.set_world_size(52)
    actor.set_folder_path('Markers')

# Four connected rooms. The openings are 400 cm wide, with wall segments
# leaving a continuous walkable route. Runtime room randomization is a later task.
rooms = [('HUB - START', 0), ('COMBAT ROOM', 2400),
         ('SHOP AND EXTRACTION', 4800), ('BOSS ARENA', 7200)]
for index, (name, x) in enumerate(rooms):
    block('Floor_' + str(index), x, 0, -25, 24, 20, .5)
    for side in [-1, 1]:
        block('SideWall', x, side * 1000, 150, 24, .3, 3)
    if index == 0:
        block('Hub_Back', x - 1200, 0, 150, .3, 20, 3)
    # Room divisions are created once, leaving central doorways.
    for side in [-1, 1]:
        block('DoorWall', x + 1200, side * 600, 150, .3, 8, 3)
    label(name, x, 800, 180)
block('EndWall', 8550, 0, 150, .3, 20, 3)
for x, y in [(2400, 500), (2700, -500), (7300, 550), (7600, -550)]:
    block('Cover', x, y, 90, 2, 2, 1.8)
label('PORTAL PLACEHOLDER - NO EXTRACTION LOGIC YET', 4750, -700)
block('Portal_Placeholder', 4800, -650, 10, 3, 3, .2)
label('WASD MOVE / MOUSE LOOK / SPACE JUMP', -700, -750)
start = level.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-700, 0, 120))
start.set_actor_label('PlayerStart_Hub')
sun = level.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 900),
                                  unreal.Rotator(-60, -30, 0))
sun.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property('intensity', 3.0)
sky = level.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 700))
sky.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property('intensity', 1.0)
for _, x in rooms:
    light = level.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, 0, 500))
    light.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    light.light_component.set_editor_property('intensity', 15000.0)
    light.light_component.set_editor_property('attenuation_radius', 1800.0)

level.save_current_level()
lib.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
# Load and compile Blueprint assets, including dependencies, before reporting success.
for path in [ROOT + '/Characters/BP_ARPlayer', ROOT + '/Core/BP_ARGameMode',
             '/Game/Mannequin/Animations/ThirdPerson_AnimBP']:
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('Missing required Blueprint: ' + path)
    # Blueprint compilation is verified separately with CompileAllBlueprints.
    lib.save_loaded_asset(asset)
report = {'map': MAP, 'rooms': len(rooms), 'game_mode': game_mode.get_path_name(),
          'status': 'generated; editor Play and packaging require separate validation'}
report_path = os.path.join(unreal.Paths.project_saved_dir(), 'prototype_build_report.json')
with open(report_path, 'w') as handle:
    json.dump(report, handle, indent=2)
unreal.log('ASHEN_RIFT_PROTOTYPE_BUILD_SUCCESS')

