"""Create a new 0.3 map without changing existing M0/M1 maps."""
import unreal
ROOT = '/Game/AshenRift'
MAP = ROOT + '/Maps/L_Progression'
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
assert not lib.does_asset_exist(MAP), 'Progression map already exists; refusing to overwrite.'

def material(name, rgb, glow=False):
    path = ROOT + '/Materials/' + name
    if lib.does_asset_exist(path):
        return lib.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    expression = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    expression.set_editor_property('constant', unreal.LinearColor(*rgb, 1))
    assert unreal.MaterialEditingLibrary.connect_material_property(expression, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if glow:
        assert unreal.MaterialEditingLibrary.connect_material_property(expression, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    lib.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat

material('M_Arcane', (.05, .5, 1), True)
material('M_Rune', (.7, .15, 1), True)
material('M_Threat', (1, .05, .015), True)
boss_material = material('M_Warden', (.3, .025, .015))
boss_material.set_editor_property('used_with_skeletal_mesh', True)
unreal.MaterialEditingLibrary.recompile_material(boss_material)
lib.save_loaded_asset(boss_material, only_if_is_dirty=False)
shop_material = material('M_Trader', (.3, .14, .045))
floor_material = material('M_DungeonFloor', (.075, .085, .11))

def blueprint(name, folder, parent):
    path = ROOT + '/' + folder + '/' + name
    if not lib.does_asset_exist(path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT + '/' + folder, unreal.Blueprint, factory)
        lib.save_loaded_asset(asset)
    return lib.load_blueprint_class(path)

boss_class = blueprint('BP_ARAshenWarden', 'Enemies', unreal.ARBoss)
mode_class = blueprint('BP_ARProgressionGameMode', 'Core', unreal.ARPrototypeGameMode)
boss_default = unreal.get_default_object(boss_class)
for slot in range(boss_default.mesh.get_num_materials()):
    boss_default.mesh.set_material(slot, boss_material)
mode_default = unreal.get_default_object(mode_class)
mode_default.set_editor_property('default_pawn_class', lib.load_blueprint_class(ROOT + '/Characters/BP_ARPlayableCharacter'))
mode_default.set_editor_property('enemy_class', lib.load_blueprint_class(ROOT + '/Enemies/BP_ARGrunt'))
mode_default.set_editor_property('boss_class', boss_class)
lib.save_directory(ROOT + '/Core', only_if_is_dirty=False)
lib.save_directory(ROOT + '/Enemies', only_if_is_dirty=False)

assert level.load_level(ROOT + '/Maps/L_FirstPlayable')
assert unreal.EditorLoadingAndSavingUtils.save_map(level.get_editor_world(), MAP)
assert level.load_level(MAP)  # save_map alone does not switch the edited world.
level.get_editor_world().get_world_settings().set_editor_property('default_game_mode', mode_class)
for actor in level.get_all_level_actors():
    if isinstance(actor, unreal.TextRenderActor):
        text = str(actor.text_render.get_editor_property('text'))
        if 'BOSS AREA' in text:
            actor.text_render.set_text('ASHEN WARDEN - OPTIONAL BOSS')
        if text == 'HUB - START':
            actor.text_render.set_text('CAMP - 1 WARRIOR / 2 MAGE / E ENTER')
    if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label().startswith('Floor_'):
        actor.static_mesh_component.set_material(0, floor_material)
    if isinstance(actor, unreal.DirectionalLight):
        actor.light_component.set_editor_property('intensity', 2.0)
    if isinstance(actor, unreal.PointLight):
        actor.light_component.set_editor_property('intensity', 3500)

shop = level.spawn_actor_from_class(unreal.ARShop, unreal.Vector(5050, 650, 55))
shop.set_actor_label('DungeonTrader')
shop.get_editor_property('mesh').set_material(0, shop_material)
sign = level.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(5050, 850, 220), unreal.Rotator(pitch=0, yaw=-90, roll=0))
sign.set_actor_label('TraderSign')
sign.text_render.set_text('TRADER - E TO TRADE / 1 POTION / 2 POWER')
sign.text_render.set_world_size(38)
fog = level.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_actor_label('DungeonFog')
fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_component.set_editor_property('fog_density', .012)
fog_component.set_editor_property('fog_inscattering_color', unreal.LinearColor(.045, .065, .1, 1))
level.save_current_level()
lib.save_directory(ROOT + '/Materials', only_if_is_dirty=False)
unreal.log('ASHEN_RIFT_PROGRESSION_ASSETS_SUCCESS')
