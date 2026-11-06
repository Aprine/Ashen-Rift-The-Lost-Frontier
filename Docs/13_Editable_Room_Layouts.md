# Editable Room Layouts (Actor Blueprints)

Room prefabs live at `Content/AshenRift/Rooms/`:

- `BP_Room_Pillars`
- `BP_Room_Barricade`
- `BP_Room_Cross`

**Parent class is `Actor`**, not `ARDungeonRoom_*`. Open any of them and you should see Viewport + Components. Drag the Static Mesh components (W/E/R) to edit layouts, then Compile & Save.

`RandomRoomDirector` on the integrated `L_FrontierDungeon` has **Room Layouts** pointing at these three BPs. Duplicate a BP in the Content Browser, edit it, add it to Room Layouts to expand the random pool. Three layouts are selected without repetition; an empty or short pool uses legacy geometry for missing entries. Seed repeatability depends on keeping pool order unchanged.

The delivered assets contain Kuangxin's hand-edited layouts; do not regenerate them. The historical starter generator (editor Python) is:

`Scripts/create_room_blueprints.py`

Uses `ARDungeonEditorLibrary.CreateEditableRoomBlueprint` which builds an Actor BP with SCS meshes. Both the script and native helper now refuse to replace an existing asset. The starter geometry differs from the delivered hand-edited layout.

Camp hub props: see `14_Camp_Room_Blueprint.md` (BP_Camp, Director **Camp Layout**).

Source and integration evidence: [version 0.9 integration notes](15_Team_Map_Integration.md).
