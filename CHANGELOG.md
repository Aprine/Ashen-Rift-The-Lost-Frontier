# Changelog

Version entries describe the implemented prototype and its recorded validation. Dates below follow the project development notes. New versions must update this file and both README language versions in the same change.

## Unreleased

### Documentation and repository preparation

- Added an English GitHub README and preserved the Chinese instructions as `README.zh-CN.md`.
- Added repository screenshots under `Docs/Images` so they are available without the ignored `Saved` directory.
- Added this changelog and project instructions requiring README and changelog updates for every version change.
- Excluded generated build output, caches, player saves, installers, backups and local environment files from repository uploads.

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
