import unreal
level = unreal.EditorLevelLibrary
assert level.load_level('/Game/AshenRift/Maps/L_RandomDungeon')
for actor in level.get_all_level_actors():
    if isinstance(actor, unreal.TextRenderActor) and actor.get_actor_location().y < 0:
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=90, roll=0), False)
assert level.save_current_level()
unreal.log('ASHEN_RIFT_CACHE_SIGN_FIXED')
