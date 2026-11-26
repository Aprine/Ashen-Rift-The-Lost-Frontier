"""Place the editable route board and candidate nodes in Dungeon 3."""

import unreal


def configure():
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
    assert editor.save_all_dirty_levels()
    unreal.log('ASHEN_ROUTE_BOARD_CONFIGURED')


configure()
