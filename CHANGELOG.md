# Changelog

Version entries describe the implemented prototype and its recorded validation. Dates below follow the project development notes. New versions must update this file and both README language versions in the same change.

## Unreleased

### Documentation and repository preparation

- Recorded the requirement to publish each completed project update and its changelog together on GitHub and verify the remote result.

- Aligned the Chinese README with the English README as complete translations, including matching sections, tables, code examples, screenshots and links; recorded the bilingual parity requirement in `AGENTS.md`.
- Added combat and camp screenshots with Chinese captions to `README.zh-CN.md`.
- Added an English GitHub README and preserved the Chinese instructions as `README.zh-CN.md`.
- Added repository screenshots under `Docs/Images` so they are available without the ignored `Saved` directory.
- Added this changelog and project instructions requiring README and changelog updates for every version change.
- Added the canonical GitHub repository link and clone instructions for `Aprine/Ashen-Rift-The-Lost-Frontier`.
- Excluded generated build output, caches, player saves, installers, backups and local environment files from repository uploads.

## 0.17 — 2026-11-28

### Added

- Integrated byimy's Knight and Mage models with class-specific movement, jump, attack, spell, hit and death clips. Player attacks resolve at the relevant point in the clip.
- Added distinct attacks for the Raider, Hexer, Sentinel and Ashen Warden, including the Warden's sweep, elbow strike and ground slam. Enemy death animations play after drops become available.

### Fixed

- Room clearance, the guarded chest, the route board and the Boss HUD now respond to death immediately while the fall animation plays.

### Documentation

- Replaced both combat screenshots with 1920 × 1080 UE4 gameplay captures that show the full Warrior and Mage, their weapons, and the hit or spell in frame.

### Validation

- Passed all 15 regression tests. The player and enemy combat suites also passed with real rendering. All 36 imported Unreal packages were verified in the WindowsNoEditor cook. Development Editor and Game builds, Blueprint compilation and the seven-map cook passed. See the [integration notes and screenshots](Docs/23_Character_Combat_Integration.md).

## 0.16 — 2026-11-27

### Added

- Added a ninth generated room in Random Ruins with a guarded gold extraction portal. Defeating the elite unseals it, and the Ashen Warden can be bypassed en route. The blue portal remains a three-second extraction choice.
- An uninterrupted five-second gold channel banks 2, 3 or 4 bonus runes at tiers 1, 2 or 3. The route map shows both exits, their conditions and the potential payout. The results screen shows the bonus separately.

### Validation

- Passed all 15 regression tests. Checked 128 seeded layouts and ran both extraction routes in UE4 with seeds 9 and 28, including channel interruptions, a fatal hit and saved rune totals. Development Editor and Game builds, Blueprint compilation and the seven-map WindowsNoEditor cook passed. See the [deep extraction notes](Docs/22_Deep_Extraction.md) for a gameplay screenshot.

## 0.15 — 2026-11-26

### Added

- Added Random Ruins as a standalone mode from the town preparation panel, with eight seeded rooms, a merchant, a guarded storeroom and timed extraction.
- Added an optional Ashen Warden encounter off the elite room. The Warden drops a guaranteed Runic Charm, and the route map shows the encounter and surviving enemies.
- Random Ruins shares the three difficulty tiers and settlement rules with Random Expedition. Extracting at the highest unlocked tier unlocks the next one.

### Validation

- Passed all 15 regression tests. Checked 128 seeded layouts and verified the complete Random Ruins loop in UE4 with seeds 9 and 28. Development Editor and Game builds, Blueprint compilation and the seven-map WindowsNoEditor cook passed. The [Random Ruins notes](Docs/21_Random_Ruins.md) include a screenshot captured in UE4.27.2.

## 0.14 — 2026-11-26

### Added

- Added a guarded storeroom to the modular branch. Its doorway connects the elite room to either supplies or extraction, giving a playable loop and four seeded connection patterns.
- Added a single-claim chest that awards two healing potions or two runes based on the seed. The map previews the reward and the chest waits for its guard to be defeated and for room in the potion stack.
- The route map now shows the current room, previously visited rooms, cleared encounters, remaining enemies and whether the chest has been claimed.

### Validation

- Passed all 14 regression tests. Checked 128 seed plans and ran five representative seeds that cover all four connection patterns and both chest rewards. Verified guarded access, the insufficient-capacity case, repeat-interaction protection, retreat and extraction. Development builds, Blueprint compilation and the WindowsNoEditor map cook passed.

## 0.13 — 2026-11-26

### Added

- Added a seeded, six-room modular branch accessible from the route board in Dungeon 3. The shop or exit can branch off the guard room or the junction, giving three connection patterns with rotated and mirrored layouts.
- Added a clickable map that draws real room connections, marks the current room, previews the elite reward and passes the selected destination to the HUD.
- Assembled floors, doors, walls and room props from the existing KayKit assets. The branch shares the main dungeon's run state, including the merchant wallet, elite loot, timed extraction and difficulty progression.

### Fixed

- Enabled instanced-mesh usage on the reused KayKit material and set up movable lights in each generated room. Rebuilt Dungeon 3's navigation from the current bounds, including the area reserved for the branch.

### Validation

- Passed all 14 regression tests. Validated deterministic layouts over 128 seeds and checked physical doors, navigation, shopping, elite loot, walking back and extraction with seeds 9, 15 and 28. Seed 9 was verified with real rendering. Development Editor and Game builds, Blueprint compilation and the WindowsNoEditor cook passed.

## 0.12 — 2026-11-26

### Added

- Added random departure from town. The run seed selects one of three authored dungeons, assigns combatants to authored encounter points and rolls equipment drops.
- Added three saved expedition tiers. A successful extraction at the highest unlocked tier opens the next one and selects it for the next departure. Higher tiers increase elite counts, enemy stats and rewards.
- Added a route board in Dungeon 3 with the live player status, carried equipment, walking distances and previews for supplies, an elite fight and extraction. The seed chooses the elite encounter site, named equipment reward and active extraction destination.

### Validation

- All 13 regression tests passed, including a rendered run of the new route and tier progression test. Three earlier test saves loaded with tier 1. Development Editor and Game builds, Blueprint compilation and the WindowsNoEditor map cook passed.

## 0.11 — 2026-11-26

### Added

- Integrated Kuangxin's larger `Map_Dungeon3` and its dedicated `Map_Shop` and `Map_Boss` rooms. Dungeon 3 is selectable from town preparation.
- Added paired passages to the merchant and Boss areas. The player keeps the same health, inventory, currency and purchased upgrades while moving between them. Extraction still uses the blue portal near the dungeon entrance.
- Added five encounters to Dungeon 3, placed the Ashen Warden in the dedicated arena, and added the three maps to the Windows cook list.

### Fixed

- Added continuous floor collision inside verified tile rows, made the connecting doorways traversable, and configured navigation for the main dungeon, shop and arena. The imported rooms use local dynamic lights.

### Validation

- All 12 regression tests passed, including the expanded merchant, Boss and extraction flow with an additional rendered run. Development Editor and Game builds, Blueprint compilation and the WindowsNoEditor cook for all six current maps passed.

## 0.10 — 2026-11-25

### Added

- Integrated Kuangxin's `Map_Hub`, `Map_Dungeon1` and `Map_Dungeon2` with their environment packages. Town is the new startup map. Both dungeons have selectable departures, encounters, a Boss, trading, loot and timed extraction.
- Preserve the selected class through travel and debit prepared equipment when the destination pawn is ready. The result screen returns the player to town after extraction or death.
- Added `Integration.Expeditions` for both departures and the cross-map run loop. `PlayPrototype.ps1 -Classic` starts the earlier shuffled route.

### Fixed

- Rebound the new maps to Ashen Rift gameplay and made authored doorway openings and floor seams traversable by player and navigation agents.
- Added fill lights for character and room readability, and kept enemy labels from overlapping the HUD and other enemy labels.
- Retook the town and dungeon screenshots at 1920 × 1080 with the HUD hidden. Updated the English and Chinese READMEs and the expedition notes to use the new views of the town, the Dungeon 1 trader and extraction portal, and the Ashen Warden in Dungeon 2.

### Validation

- Passed the 12-test default regression suite, the rendered expedition flow, Development Editor and Game builds, project Blueprint compilation, and a WindowsNoEditor cook of all three new maps.

See the [town and expedition integration notes](Docs/16_Town_Expeditions.md) for the contribution sources and validation.

## 0.9 — 2026-11-06

### Added

- Integrated Kuangxin's environment from source commit `e2dfc0b83847e912f477b9bc0ecd9c0046c2f729`: 215 KayKit asset packages, three editable combat-room Blueprints and a furnished camp Blueprint.
- Added `L_FrontierDungeon` through UE4 Save Map As and made it the default play map; preserved the existing greybox and historical levels.
- Connected authored room pools to seeded generation and reused the camp prefab across departures, extraction and death returns.
- Added an isolated `Integration.TeamMap` regression for prefab dependencies, spawn/door clearance, all six layout orders, route connectivity and settlement.
- Added source attribution, prefab preservation hashes, editing notes and current environment screenshots in both README languages.

### Fixed

- Replaced the camp exit's closed-door mesh with an open frame; provided arch clearance for existing navigation agents and rebuilt obsolete simple navigation collision.
- Disabled blocking collision on the already-open decorative wooden door.
- Removed editor-only Actor-label dependencies from runtime room display; empty or short layout pools retain three combat slots with legacy fallback geometry.
- Protected hand-edited room and camp assets from starter-generator overwrites.
- Moved the ranged-enemy test's dodge position away from new shop cover so it continues to exercise evasion rather than line-of-sight cancellation. Gameplay AI rules are unchanged.
- Moved the UI pause test's temporary caster and projectile away from the contributed counter and columns.

### Validation

- Development Editor and Development Game builds passed; project Blueprint compilation reported zero errors, zero warnings and zero load failures.
- The default regression configuration passed eleven tests; the preserved greybox configuration passed ten tests. The new map test also passed with real GPU rendering.
- README structure, commands and relative documentation/image links passed checks; original greybox and all four hand-authored prefab hashes were preserved.

Details are recorded in [map integration notes](Docs/15_Team_Map_Integration.md). Windows packaged-build acceptance remains pending.

## 0.8 — 2026-11-03

### Added

- Five basic skeletal overlay animations: Slash, Cast, Heavy, Hurt and Whirlwind.
- Hit flashes, sparks, actual damage numbers and light player camera feedback.
- Eleven originally synthesised sound effects for combat, healing, pickups, extraction, death and interface actions.
- Session-level sound toggles in the main and pause menus.

### Fixed

- Projectiles fired by the same caster no longer collide with one another when two spells are launched in the same frame.

### Recorded validation

- Development Editor and Development Game builds and project Blueprint compilation passed.
- Ten automated tests passed: three core tests and seven integration tests.
- GPU feedback checks passed, and recorded AudioMixer output was non-silent without clipping in the measured sample.

### Remaining work

- Refined animation and hit timing, final visual assets, sound mixing, background music, performance benchmarks and packaged Windows acceptance.

See [combat feedback notes](Docs/12_Combat_Feedback.md).

## 0.7 — 2026-11-03

### Added

- Clickable Chinese UMG screens for the main menu, camp, inventory, trader, pause and settlement.
- Chinese HUD, enemy labels, attack warnings and item names.
- Panel-aware mouse and gameplay input handling.
- World pause and resume, while inventory and trading continue to leave combat active.

### Recorded validation

- Nine automated tests passed, including real button actions and pause/resume integration coverage.
- GPU interface checks and a visible standalone-window interaction check were recorded.

See [Chinese UMG notes](Docs/11_Chinese_UMG.md).

## 0.6 — 2026-11-03

### Added

- Hexer ranged enemies with aiming warnings, dodgeable projectiles and wall blocking.
- Sentinel elites with delayed, forward-facing heavy attacks and gold/rune rewards.
- A mixed six-enemy dungeon population: two Raiders, three Hexers and one Sentinel, plus the existing Boss.
- Enemy projectile cleanup on caster death and run settlement.

### Recorded validation

- Eight automated tests passed, including enemy behaviour, counterplay, drops and settlement coverage.
- GPU enemy checks and Blueprint compilation passed.

See [enemy variant notes](Docs/10_Enemy_Variants.md).

## 0.5 — 2026-11-03

### Added

- Shared Vitality and Might permanent talents with three ranks and banked-rune costs.
- Camp stash preparation for class weapons, armour, charms and potions.
- Saved stash deductions before departure, automatic equipment on entry and carried-item loss on death.
- Validation for class restrictions, item ownership, inventory capacity and profile data.

### Recorded validation

- Seven automated tests passed, including camp purchases, departure, death and extraction accounting.
- Editor/Game builds, Blueprint compilation and GPU camp checks passed.
- Save-failure handling was reviewed; simulated disk-failure injection and packaged-build acceptance were not performed.

See [camp and stash notes](Docs/09_Camp_Talents_Stash.md).

## 0.4 — 2026-10-29

### Added

- Three predefined combat layouts shuffled without repetition along a fixed connected route.
- Repeatable room-order seeds and six connected greybox rooms.
- Dynamic navigation and enemy/Boss paths around obstacles.
- Weapon, armour and charm equipment slots with ownership and class checks.

### Recorded validation

- Six automated tests passed, including seed repeatability, connectivity, equipment effects, navigation and settlement.
- GPU dungeon checks and Editor/Game builds passed.

See [random dungeon and equipment notes](Docs/08_Random_Dungeon_Equipment.md).

## 0.3 — 2026-10-28

### Added

- Warrior and Mage class selection, mana, projectiles and active skills.
- Run-gold trading, limited potion stock and temporary damage upgrades.
- The Ashen Warden Boss, ground warnings, gold/rune drops and Boss outcome reporting.
- Separate accounting for earnings, spending, extracted gold and death losses.

### Recorded validation

- Four automated tests passed, covering core systems, the run loop and progression.
- Editor/Game builds, Blueprint compilation and GPU progression checks passed.

See [progression prototype notes](Docs/07_Progression_Prototype.md).

## 0.2 — 2026-10-28

### Added

- The first playable camp-to-dungeon loop with combat, pickups, potions and inventory display.
- Three-second extraction with damage and movement cancellation.
- Death loss, extraction saving, settlement summaries and return to camp.
- Save retry and duplicate-settlement protection.

### Recorded validation

- Editor/Game builds and run-loop automation passed.
- Visible editor play confirmed movement through the run flow, damage, death and returning to camp.

See [first playable notes](Docs/06_First_Playable.md).
