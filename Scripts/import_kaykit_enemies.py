"""Import the converted KayKit enemy assets into Unreal Engine 4.27."""
from pathlib import Path
import unreal


SOURCE = Path(unreal.Paths.project_saved_dir()).resolve() / "KayKitSkeletons020/FBX"
ROOT = "/Game/AshenRift/Enemies/KayKit"
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT_MATERIAL = unreal.MaterialEditingLibrary


def import_asset(filename, folder, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / filename)
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options:
        task.options = options
    TOOLS.import_asset_tasks([task])
    objects = [ASSETS.load_asset(path) for path in task.get_editor_property("imported_object_paths")]
    assert objects, filename
    return objects


def make_material(name, color=None, texture=None, glow=None):
    path = ROOT + "/" + name
    material = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else TOOLS.create_asset(
        name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
    assert material, name
    EDIT_MATERIAL.delete_all_material_expressions(material)
    material.set_editor_property("used_with_skeletal_mesh", True)
    if texture:
        base = EDIT_MATERIAL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -350, 0)
        base.set_editor_property("texture", texture)
        assert EDIT_MATERIAL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        base = EDIT_MATERIAL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -350, 0)
        base.set_editor_property("constant", unreal.LinearColor(*color, 1))
        assert EDIT_MATERIAL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = EDIT_MATERIAL.create_material_expression(material, unreal.MaterialExpressionConstant, -350, 180)
    roughness.set_editor_property("r", 0.67)
    assert EDIT_MATERIAL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if glow:
        emissive = EDIT_MATERIAL.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -350, 340)
        emissive.set_editor_property("constant", unreal.LinearColor(*glow, 1))
        assert EDIT_MATERIAL.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT_MATERIAL.recompile_material(material)
    assert ASSETS.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def options_for(skeleton=None, animation=False):
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION if animation else unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_as_skeletal = True
    options.import_mesh = not animation
    options.import_animations = animation
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    if skeleton:
        options.skeleton = skeleton
    return options


assert SOURCE.is_dir(), SOURCE
texture = next(obj for obj in import_asset("T_SkeletonAtlas.png", ROOT) if isinstance(obj, unreal.Texture2D))
assert ASSETS.save_loaded_asset(texture, only_if_is_dirty=False)
materials = {
    "SkeletonAtlas": make_material("M_SkeletonAtlas", texture=texture),
    "WardenCloth": make_material("M_WardenCloth", color=(.026, .018, .065)),
    "WardenCrown": make_material("M_WardenCrown", color=(.26, .102, .025)),
    "WardenEyes": make_material("M_WardenEyes", color=(.65, .19, .045), glow=(2.5, .48, .08)),
}

skeleton = None
for name in ("SK_Raider", "SK_Hexer", "SK_Sentinel", "SK_Warden"):
    mesh = next(obj for obj in import_asset(name + ".fbx", ROOT, options_for(skeleton)) if isinstance(obj, unreal.SkeletalMesh))
    skeleton = skeleton or mesh.get_editor_property("skeleton")
    assert mesh.get_editor_property("skeleton") == skeleton, name
    slots = mesh.get_editor_property("materials")
    actual_slots = [str(slot.material_slot_name) for slot in slots]
    assert len(slots) == (4 if name == "SK_Warden" else 1), (name, actual_slots)
    materials_for_mesh = [
        unreal.SkeletalMaterial(material_interface=materials[str(slot.material_slot_name)],
                                material_slot_name=slot.material_slot_name)
        for slot in slots
    ]
    mesh.set_editor_property("materials", materials_for_mesh)
    saved_materials = [slot.material_interface for slot in mesh.get_editor_property("materials")]
    assert saved_materials == [materials[name] for name in actual_slots], (name, saved_materials)
    assert ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False), name
    unreal.log("ASHEN_ENEMY_MESH {} {}".format(name, actual_slots))
assert ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=False)

for source in sorted(SOURCE.glob("A_*.fbx")):
    anim = next(obj for obj in import_asset(source.name, ROOT + "/Animations", options_for(skeleton, True))
                if isinstance(obj, unreal.AnimSequence))
    assert anim.get_editor_property("skeleton") == skeleton, source.name
    assert ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
    unreal.log("ASHEN_ENEMY_CLIP {}".format(anim.get_name()))
unreal.log("ASHEN_KAYKIT_ENEMIES_IMPORTED")
