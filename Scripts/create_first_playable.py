"""UE4 editor setup: new M1 map and Blueprint children; preserves M0 assets."""
import unreal

ROOT = '/Game/AshenRift'
MAP = ROOT + '/Maps/L_FirstPlayable'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
if lib.does_asset_exist(MAP):
    raise RuntimeError('L_FirstPlayable already exists; will not overwrite your map.')

def material(name, rgb, glow=0):
    path = ROOT + '/Materials/' + name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant', unreal.LinearColor(*rgb, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if glow:
        unreal.MaterialEditingLibrary.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat

gold = material('M_Gold', (1, .55, .04), 1)
potion = material('M_Potion', (.1, 1, .35), 1)
portal_mat = material('M_Portal', (.04, .45, 1), 1)
enemy_mat = material('M_Enemy', (.45, .035, .045))
floor_mat = material('M_Floor', (.13, .15, .18))
wall_mat = material('M_Wall', (.055, .07, .09))

def blueprint(name, folder, parent):
    path = ROOT + '/' + folder + '/' + name
    if lib.does_asset_exist(path):
        return lib.load_blueprint_class(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT + '/' + folder, unreal.Blueprint, factory)
    lib.save_loaded_asset(asset)
    return lib.load_blueprint_class(path)

player_class = blueprint('BP_ARPlayableCharacter', 'Characters', unreal.ARPlayerCharacter)
enemy_class = blueprint('BP_ARGrunt', 'Enemies', unreal.AREnemy)
mode_class = blueprint('BP_ARPlayableGameMode', 'Core', unreal.ARPrototypeGameMode)
enemy_default = unreal.get_default_object(enemy_class)
for slot in range(enemy_default.get_editor_property('mesh').get_num_materials()):
    enemy_default.get_editor_property('mesh').set_material(slot, enemy_mat)
mode_default = unreal.get_default_object(mode_class)
mode_default.set_editor_property('default_pawn_class', player_class)
mode_default.set_editor_property('enemy_class', enemy_class)
lib.save_directory(ROOT, only_if_is_dirty=False, recursive=True)

if not level.load_level(ROOT + '/Maps/L_Prototype'):
    raise RuntimeError('Could not load original greybox')
if not unreal.EditorLoadingAndSavingUtils.save_map(level.get_editor_world(), MAP):
    raise RuntimeError('Could not duplicate greybox map')
if not level.load_level(MAP):
    raise RuntimeError('Could not switch to duplicated map')
level.get_editor_world().get_world_settings().set_editor_property('default_game_mode', mode_class)
for actor in level.get_all_level_actors():
    name = actor.get_actor_label()
    if name.startswith('Floor_'):
        actor.static_mesh_component.set_material(0, floor_mat)
    elif name.startswith(('SideWall', 'DoorWall', 'Hub_Back', 'EndWall', 'Cover')):
        actor.static_mesh_component.set_material(0, wall_mat)
    if isinstance(actor, unreal.TextRenderActor):
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=-90, roll=0), False)
        text = str(actor.text_render.get_editor_property('text'))
        if text.startswith('PORTAL PLACEHOLDER'):
            actor.text_render.set_text('BLUE PORTAL - PRESS E TO EXTRACT')
        elif text == 'SHOP AND EXTRACTION':
            actor.text_render.set_text('EXTRACTION CHAMBER')
        elif text == 'BOSS ARENA':
            actor.text_render.set_text('BOSS AREA - FUTURE MILESTONE')
        elif text.startswith('WASD MOVE'):
            actor.text_render.set_text('CAMP - PRESS E TO ENTER DUNGEON')
    if name == 'Portal_Placeholder':
        actor.static_mesh_component.set_visibility(False)
    if isinstance(actor, unreal.DirectionalLight):
        actor.light_component.set_editor_property('intensity', 5.0)

portal = level.spawn_actor_from_class(unreal.ARPortal, unreal.Vector(4800, -650, 12))
portal.set_actor_label('ExtractionPortal')
portal.mesh.set_material(0, portal_mat)
portal_light = level.spawn_actor_from_class(unreal.PointLight, unreal.Vector(4800, -650, 180))
portal_light.light_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
portal_light.light_component.set_editor_property('intensity', 9000)
portal_light.light_component.set_editor_property('attenuation_radius', 650)
portal_light.light_component.set_light_color(unreal.LinearColor(.1, .5, 1))
level.save_current_level()
lib.save_directory(ROOT, only_if_is_dirty=False, recursive=True)
unreal.log('ASHEN_RIFT_FIRST_PLAYABLE_ASSETS_SUCCESS')
