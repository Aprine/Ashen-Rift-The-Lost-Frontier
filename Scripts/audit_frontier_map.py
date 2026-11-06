"""Read-only audit of the integrated level, dependencies and authored layouts."""
import json
import os
import unreal

MAP = '/Game/AshenRift/Maps/L_FrontierDungeon'
level = unreal.EditorLevelLibrary
assert level.load_level(MAP)
def vector(value):
    return [value.x, value.y, value.z]

def bounds(values):
    return [vector(value) for value in values]

rows = []
for actor in level.get_all_level_actors():
    row = {'label': actor.get_actor_label(), 'class': actor.get_class().get_name(),
           'location': str(actor.get_actor_location())}
    if isinstance(actor, unreal.ARDungeonDirector):
        row['room_layouts'] = [str(c) for c in actor.get_editor_property('room_layouts')]
        row['camp_layout'] = str(actor.get_editor_property('camp_layout'))
    if isinstance(actor, unreal.StaticMeshActor):
        component = actor.static_mesh_component
        mesh = component.static_mesh
        assert mesh, 'Missing mesh on ' + row['label']
        row['mesh'] = mesh.get_path_name()
        row['collision'] = str(component.get_collision_enabled())
        row['bounds'] = bounds(actor.get_actor_bounds(False))
    rows.append(row)

layouts = []
for name in ('BP_Room_Pillars', 'BP_Room_Barricade', 'BP_Room_Cross', 'BP_Camp'):
    cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/AshenRift/Rooms/' + name)
    assert cls, 'Missing prefab ' + name
    actor = level.spawn_actor_from_class(cls, unreal.Vector(30000, 0, 0))
    components = actor.get_components_by_class(unreal.StaticMeshComponent)
    layout = {'name': name, 'components': []}
    for component in components:
        mesh = component.static_mesh
        assert mesh, 'Missing prefab mesh ' + name
        body = mesh.get_editor_property('body_setup')
        layout['components'].append({'name': component.get_name(), 'mesh': mesh.get_path_name(),
            'location': str(component.relative_location),
            'bounds': bounds(component.get_local_bounds()),
            'trace': str(body.get_editor_property('collision_trace_flag')) if body else 'none'})
    assert components, 'Empty prefab ' + name
    layouts.append(layout)
    level.destroy_actor(actor)

result = {'map': MAP, 'actors': rows, 'layouts': layouts}
destination = os.path.join(unreal.Paths.project_saved_dir(), 'frontier_asset_audit.json')
with open(destination, 'w', encoding='utf-8') as output:
    json.dump(result, output, indent=2, ensure_ascii=False)
unreal.log('ASHEN_RIFT_FRONTIER_AUDIT_SUCCESS actors=%d' % len(rows))
