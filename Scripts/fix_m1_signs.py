"""Correct label orientation only in the new M1 map; do not change M0 assets."""
import unreal
lib = unreal.EditorAssetLibrary
level = unreal.EditorLevelLibrary
assert level.load_level('/Game/AshenRift/Maps/L_FirstPlayable')
for actor in level.get_all_level_actors():
    if isinstance(actor, unreal.TextRenderActor):
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=-90, roll=0), False)
    if isinstance(actor, unreal.StaticMeshActor) and actor.get_actor_label().startswith('Floor_'):
        mat = actor.static_mesh_component.get_material(0)
        unreal.log('M1_FLOOR_MATERIAL ' + actor.get_actor_label() + ' ' + (mat.get_path_name() if mat else 'None'))
level.save_current_level()
unreal.log('ASHEN_RIFT_M1_SIGNS_FIXED')
