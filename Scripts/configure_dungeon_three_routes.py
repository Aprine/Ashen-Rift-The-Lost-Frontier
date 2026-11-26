"""Place the editable route board and candidate nodes in Dungeon 3."""

import unreal


def configure_material():
    assets = unreal.EditorAssetLibrary
    material = assets.load_asset('/Game/Environments/KayKitDungeon/Materials/M_KayKitDungeon')
    assert material
    if not material.get_editor_property('used_with_instanced_static_meshes'):
        material.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(material)
        assert assets.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log('ASHEN_BRANCH_MATERIAL_CONFIGURED')


def configure():
    configure_material()
    editor = unreal.EditorLevelLibrary
    assert editor.load_level('/Game/AshenRift/Maps/Map_Dungeon3')
    actors = editor.get_all_level_actors()
    shop = next(a for a in actors if a.get_actor_label() == 'ExpeditionShopEntrance')
    portal = next(a for a in actors if a.get_actor_label() == 'ExpeditionExtraction')
    board = next((a for a in actors if isinstance(a, unreal.ARExpeditionRouteBoard)), None)
    origin = (640, 2620, 0)
    rotation = unreal.Rotator(yaw=-90)
    location = unreal.Vector(*origin)
    if board is None:
        board = editor.spawn_actor_from_class(unreal.ARExpeditionRouteBoard, location, rotation)
        assert board
        board.set_actor_label('ExpeditionRouteBoard')
    else:
        board.set_actor_location_and_rotation(location, rotation, False, True)

    def offset(x, y, z):
        # The board faces south. Its local X points along world -Y.
        return unreal.Vector(origin[1] - y, x - origin[0], z - origin[2])

    board.set_editor_property('shop_passage', shop)
    board.set_editor_property('entrance_portal', portal)
    board.set_editor_property('elite_offsets', [offset(-365, 3480, 110), offset(395, 4500, 110)])
    board.set_editor_property('extraction_offsets', [offset(1550, 2570, 15), offset(510, -850, 15)])

    def place(label, cls, position, yaw=0):
        actor = next((a for a in actors if a.get_actor_label() == label), None)
        at = unreal.Vector(*position)
        facing = unreal.Rotator(yaw=yaw)
        if actor is None:
            actor = editor.spawn_actor_from_class(cls, at, facing)
            assert actor, label
            actor.set_actor_label(label)
        else:
            actor.set_actor_location_and_rotation(at, facing, False, True)
        return actor

    # The authored dungeon and its two sublevels remain in their own areas.
    # Reserve one 20-metre grid with room for any of the four rotations.
    wing_origin = (-16200, -9200, 0)
    wing = place('ExpeditionBranchWing', unreal.ARExpeditionBranchWing, wing_origin)
    entrance = place('ExpeditionBranchEntrance', unreal.ARExpeditionPassage, (1150, 2650, 14), 180)
    entrance.set_editor_property('branch_wing', wing)
    entrance.set_editor_property('prompt', '进入随机支路  ·  [M] 预览路线图')
    entrance.set_actor_scale3d(unreal.Vector(2.55, 2.55, .08))
    gate_material = unreal.EditorAssetLibrary.load_asset('/Game/AshenRift/Materials/M_Potion')
    assert gate_material
    entrance.mesh.set_material(0, gate_material)
    wing.set_editor_property('entrance', entrance)
    board.set_editor_property('branch_wing', wing)
    volume = next((a for a in actors if a.get_actor_label() == 'BranchWingNavigation'), None)
    center = unreal.Vector(wing_origin[0], wing_origin[1], 180)
    extent = unreal.Vector(5250, 5250, 560)
    if volume is None:
        volume = unreal.ARDungeonEditorLibrary.create_navigation_bounds(editor.get_editor_world(), center, extent)
        assert volume
        volume.set_actor_label('BranchWingNavigation')
    else:
        assert unreal.ARDungeonEditorLibrary.resize_navigation_bounds(volume, center, extent)

    # Let the runtime build from the current bounds instead of serializing
    # the existing Recast tile capacity from before the reserved wing.
    for actor in editor.get_all_level_actors():
        if isinstance(actor, unreal.RecastNavMesh):
            assert editor.destroy_actor(actor)

    assert editor.save_all_dirty_levels()
    unreal.log('ASHEN_ROUTE_BOARD_CONFIGURED')


if __name__ == '__main__':
    configure()
