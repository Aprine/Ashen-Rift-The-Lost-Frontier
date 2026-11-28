"""Convert the official KayKit Skeletons 1.0 glTF files to UE4 FBX with Blender 4.2."""
import math
from pathlib import Path
import shutil
import sys

import bpy
from mathutils import Matrix, Vector


SOURCE, OUTPUT = (Path(value).resolve() for value in sys.argv[sys.argv.index("--") + 1:])
OUTPUT.mkdir(parents=True, exist_ok=True)
CHARACTERS = SOURCE / "Characters/gltf"
PROPS = SOURCE / "Assets/gltf"
SHARED_CLIPS = (
    "Idle_Combat", "Walking_A", "1H_Melee_Attack_Chop", "1H_Melee_Attack_Slice_Horizontal",
    "Block_Attack", "Spellcast_Shoot", "Hit_A", "Death_A", "Death_C_Skeletons",
    "2H_Melee_Attack_Chop", "2H_Melee_Attack_Slice", "Unarmed_Melee_Attack_Punch_B",
)


def import_gltf(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    return [obj for obj in bpy.data.objects if obj not in before]


def fbx(path, objects, animated=False):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(
        filepath=str(path), use_selection=True, object_types={"ARMATURE", "MESH"},
        axis_forward="-Y", axis_up="Z", add_leaf_bones=False, mesh_smooth_type="FACE",
        bake_anim=animated, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_simplify_factor=0, use_custom_props=False,
    )


def material(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def bind(obj, rig, bone_name, world_matrix):
    obj.parent = rig
    obj.parent_type = "OBJECT"
    obj.matrix_parent_inverse = rig.matrix_world.inverted()
    obj.matrix_world = world_matrix
    group = obj.vertex_groups.new(name=bone_name)
    group.add(range(len(obj.data.vertices)), 1.0, "REPLACE")
    obj.modifiers.new("KayKit skin", "ARMATURE").object = rig


def equip(rig, prop, bone_name, scale=1.0, angle=0.0):
    meshes = [obj for obj in import_gltf(PROPS / (prop + ".gltf")) if obj.type == "MESH"]
    assert meshes, prop
    bone = rig.data.bones[bone_name]
    pivot = rig.matrix_world @ bone.matrix_local
    for obj in meshes:
        original = obj.matrix_world.copy()
        placement = pivot @ Matrix.Rotation(math.radians(angle), 4, "Y") @ Matrix.Scale(scale, 4)
        bind(obj, rig, bone_name, placement @ original)
        obj.data.materials.clear()
        obj.data.materials.append(material("SkeletonAtlas"))
    return meshes


def crown(rig, body):
    head = rig.matrix_world @ rig.data.bones["head"].matrix_local
    head_pos = head.translation
    height = max((obj.matrix_world @ Vector(corner)).z for obj in body for corner in obj.bound_box)
    for bearing in (-55, 0, 55):
        radians = math.radians(bearing)
        # Make the dark crown read from the front and sides of the hood.
        center = Vector((head_pos.x + math.sin(radians) * .26, head_pos.y - math.cos(radians) * .24, height + .02))
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=.11, radius2=.035, depth=.35, location=center)
        shard = bpy.context.object
        shard.name = "WardenCrown"
        shard.rotation_euler.y = math.radians(bearing * .38)
        shard.data.materials.append(material("WardenCrown"))
        bpy.context.view_layer.update()
        bind(shard, rig, "head", shard.matrix_world.copy())
        body.append(shard)


def bake_centimeters(rig, meshes, actions):
    # UE4 strips Blender's Armature node. Bake meters into the rest pose and
    # animated translations so stripping it cannot discard a required scale.
    meters_to_cm = 100.0
    scale = Matrix.Scale(meters_to_cm, 4)
    rig.data.transform(scale)
    for mesh in meshes:
        world = mesh.matrix_world.copy()
        world.translation *= meters_to_cm
        mesh.data.transform(scale)
        mesh.matrix_world = world
    for action in set(actions.values()):
        for curve in action.fcurves:
            if not curve.data_path.endswith(".location"):
                continue
            for key in curve.keyframe_points:
                key.co.y *= meters_to_cm
                key.handle_left.y *= meters_to_cm
                key.handle_right.y *= meters_to_cm
            curve.update()
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = .01
    bpy.context.view_layer.update()


def role(mesh_name, source, weapons, warden=False, clips=False):
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.render.fps = 30
    bpy.context.scene.unit_settings.system = "NONE"
    bpy.context.scene.unit_settings.scale_length = 1.0
    objects = import_gltf(CHARACTERS / (source + ".glb"))
    rig = next(obj for obj in objects if obj.type == "ARMATURE")
    rig.name = "Armature"
    data = rig.animation_data
    actions = {track.name: track.strips[0].action for track in data.nla_tracks if track.strips}
    for track in list(data.nla_tracks):
        data.nla_tracks.remove(track)
    data.action = None
    for bone in rig.pose.bones:
        bone.matrix_basis.identity()
    bpy.context.scene.frame_set(0)
    body = [obj for obj in objects if obj.type == "MESH"]
    for obj in body:
        if obj.parent == rig and obj.parent_type == "BONE":
            bind(obj, rig, obj.parent_bone, obj.matrix_world.copy())
        if warden and obj.name.endswith(("_Hood", "_Cape")):
            slot = "WardenCloth"
        elif warden and obj.name.endswith("_Eyes"):
            slot = "WardenEyes"
        else:
            slot = "SkeletonAtlas"
        obj.data.materials.clear()
        obj.data.materials.append(material(slot))
    for prop, socket, scale, angle in weapons:
        body.extend(equip(rig, prop, socket, scale, angle))
    if warden:
        crown(rig, body)
    bake_centimeters(rig, body, actions)
    fbx(OUTPUT / (mesh_name + ".fbx"), [rig] + body)
    if clips:
        for name in SHARED_CLIPS:
            action = actions.get(name)
            assert action, (name, sorted(actions))
            data.action = action
            first_frame, last_frame = action.frame_range
            bpy.context.scene.frame_start = math.floor(first_frame)
            bpy.context.scene.frame_end = max(math.ceil(last_frame), bpy.context.scene.frame_start + 1)
            bpy.context.scene.frame_set(bpy.context.scene.frame_start)
            fbx(OUTPUT / ("A_" + name + ".fbx"), [rig], animated=True)


role("SK_Raider", "Skeleton_Minion", [("Skeleton_Axe", "handslot.r", 1.0, 0)], clips=True)
role("SK_Hexer", "Skeleton_Mage", [("Skeleton_Staff", "handslot.r", 1.0, 0)])
role("SK_Sentinel", "Skeleton_Warrior", [
    ("Skeleton_Blade", "handslot.r", 1.0, 0), ("Skeleton_Shield_Large_A", "handslot.l", 1.0, 0),
])
role("SK_Warden", "Skeleton_Rogue", [("Skeleton_Axe", "handslot.r", 1.28, 0)], warden=True)
shutil.copyfile(CHARACTERS / "skeleton_texture.png", OUTPUT / "T_SkeletonAtlas.png")
