# Editable Camp Layout (BP_Camp)

Camp props live at `Content/AshenRift/Rooms/BP_Camp`.

**Parent class is `Actor`** (same as combat room BPs). Open it for Viewport + Components, drag KayKit meshes (W/E/R), Compile & Save.

## When it spawns

`RandomRoomDirector` (`AARDungeonDirector`) on `L_FrontierDungeon`:

- **Camp Layout** — Actor class for `BP_Camp` (assignable in Details)
- **Camp Origin** — world transform of the camp cell (default `(0,0,0)`, slot 0 / same cell as `Camp_Back` + `CampSpawn`)

Spawn hooks:

1. **Game start** — `AARDungeonDirector::BeginPlay` → `EnsureCampLayout()`
2. **Evacuate / return to hub** — after `ReturnToHub` + `ClearRunActors`, `EnsureCampLayout()` again, then player teleports to `CampSpawn` `(-700,0,100)`

Combat rooms (`Room Layouts`) are unchanged. Camp is **not** in the random combat pool.

## Edit / assign

1. Content Browser → `AshenRift/Rooms/BP_Camp`
2. Add Static Mesh components from `AshenRift/KayKit/Props` (Furniture / Containers / Lights / Decor)
3. Compile & Save
4. Select **RandomRoomDirector** in `L_FrontierDungeon` → keep **Camp Layout** assigned to `BP_Camp` → Save level

## Regenerate starter BP

The historical starter generator is `Scripts/create_camp_blueprint.py`. The delivered `BP_Camp` contains 20 hand-edited mesh components. Open it in the Blueprint viewport to continue editing; the starter generator does not reproduce those edits.

`ARDungeonEditorLibrary.CreateEditableCampBlueprint` can create a starter under a new asset name (table, chair, chest, barrel, crate, torch, banner). Both the script and helper refuse to replace an existing asset. Camp generation is opt-in on each map; historical maps with no assigned Camp Layout retain their previous behaviour.

The integrated map explicitly binds the camp prefab. It is reused across runs; returning to camp does not duplicate furnishings. See [version 0.9 integration notes](15_Team_Map_Integration.md).
