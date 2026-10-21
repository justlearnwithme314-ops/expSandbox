# Godot → C++ port coverage

Updated 2026-10-20. The supplied Godot project was inspected read-only at `C:/Users/Admin/Downloads/powered-by-go-vw-modular-steve-tools/powered-by-go-vw-modular`. Its 48 mod manifests, 228 mod scripts and approximately 29,212 mod-script lines are inventoried in [GODOT_MODULES.md](generated/GODOT_MODULES.md). Those counts exclude core scripts, native code, third-party assets and addons.

This is a playable native foundation, **not a completed recreation of every Godot module**. New implementations reproduce selected behaviors rather than executing the original GDScript. The old project and saves remain unchanged.

## Implemented in the native build

| Area | Native behavior | Limit relative to the original |
|---|---|---|
| Terrain and LOD | Seeded hills/caves, smooth density field, official Transvoxel regular/transition tables, 1:2:4 spacing, worker meshing | Fixed vertical column extent; no hierarchical octree; edits currently invalidate all resident columns |
| Construction | Separate cube mesh, tree blocks, placement clearance, mining, wide sculpt brush, line/wall/cube/sphere brushes | No microvoxels, stairs/slabs orientation, mechanisms or texture atlas parity |
| World persistence | SQLite atomic save for density/block edits, player/inventory, time, creatures, stations, crops and gates; backup export | New versioned generator; original Godot/Wayfinder world importer not implemented; snapshots are synchronous |
| Player and survival | Walk, jump, sprint, creative flight, swimming, health, hunger, stamina, oxygen, fall damage, retained-inventory respawn | No skins, third-person model, armor/equipment slots or exact original movement hooks |
| Inventory | 40 slots, ten-slot hotbar, merge/swap/split, stack limits, wear, creative item browser | No drag cursor item ownership, equipment panel, dropped-item recovery ledger or network inventory |
| Crafting/progression | Editable recipes, ingredient checks, transactional input/output, nearby station requirements, tool tiers | Recipe browser replaces shaped-grid UI; no professions, repair/enchanting/quality system |
| Stations/industry | Chest storage, furnace smelting, coal generation, adjacent energy transfer, battery and crusher | No pipes, drills, turrets, pumps, fluid tanks or recoverable multi-service operation journal |
| Creatures/combat | Pig/sheep cuboid animals, hostile pursuit/melee, bow ray attacks/ammo, damage/loot and persistent creature state | Simple collision-aware steering; no pathfinding/ecology caps/variants/ability composition, skeleton ranged behavior or creeper fuse |
| Farming | Seeds, saved crop age, maturity, harvest and bread recipe | No irrigation or soil moisture |
| Techniques | XP unlocks, double jump, dash, parry and slam | Simple native implementations; no wall kick, roll, backstep, movement talent UI or host arbitration |
| Dungeons | Command-created nine-room structure, corridors, monsters, chest loot, paired persistent gates | No three-floor layout, altar regeneration, encounter cycles, rarity modifiers or crash recovery journal |
| Atmosphere | Day/night tint, fog, flat lake surface, underwater overlay | Water does not flow; no lava/fire propagation, weather or propagated light |
| Tools/modding | Console commands, field guide, diagnostics, wireframe, JSON content packs, typed C++ event bus, C ABI, Python ctypes example, headless CLI | No executable Godot mod compatibility, dependency graph for code mods, scripting runtime or hot reload |
| Assets | Selected CC0 Kenney props/audio, native creature and terrain meshes | Minecraft-derived atlases and restricted mod artwork are not imported; animation and original item/character visuals remain work |

## Original modules still requiring substantial port work

- `adventure_interactions`: grabbing, traps, loose props and escape systems.
- `block_mechanisms`, `micro_building`, `transparent_blocks`: doors/pistons/relays, piece geometry and actual transparent material behavior. The current glass-colored block is opaque.
- `body_cosmetics`, `player_skins`, `galaxy_characters`, `visual_unification`: character rigs, skins, cosmetics and presentation.
- `creature_variants`, `naturalist`, `world_ecology`, `entity_framework`: species-specific AI, breeding, spawning controls and composable behaviors.
- `galaxy_content`, much of `gameplay_expansion`: vehicles, boats, robotics, surgery, fabrication, brewing, bosses and dimension travel.
- `loot_expansion`, `equipment_condition`, `workshop`: rarity/instance modifiers, repair and professions beyond basic durability.
- `luanti_nodes`, `nether_fire`, `terrain_fluids`, `world_effects`: generic timers/metadata, reactive nodes, fire, liquid simulation and lighting. Only basic sand/gravel falling is native.
- `projectiles`: physical projectile simulation; the native bow currently uses ray damage.
- `shaped_crafting`: shaped layouts and connected workbench patterns.
- `sandbox_commands`: region fill/copy/paste, command blocks and the original command catalog.
- `world_expansion`, `world_landforms`, `frontier_survival`: complete biome/ore/material distribution, landmarks and original progression content.
- `user_menu`: settings persistence, searchable mod menu routing and input rebinding.
- Core ENet multiplayer, dedicated server authority, authentication/content signatures, late joins, inventory replication and conflict handling are **not implemented**. `voxel-cli` is an offline tool, not a multiplayer server.

## Verification

The Windows Release build, core tests and actual OpenGL rendering smoke are run locally. Tests cover all 256 regular and 512 transition case indices for valid finite geometry, seam coverage across all six face orientations, negative addressing, recipe conservation/capacity, SQLite round trips, consistent backup, foreign-format refusal, station processing, crop growth, command behavior, brush cost, skill unlocks and dungeon/gate persistence.

The seam coverage fixture splits collinear segments before comparison because a straight contour may have two fine segments and one coarse segment. It compares geometric coverage rather than requiring identical triangulation. This is not an exhaustive proof of arbitrary topology or distant floating-point rasterization.

GPU smoke renders terrain with actual transition triangles and loads the Kenney models. It is a rendering/asset/save check, not a substitute for a full manual survival playthrough. Cross-platform CI is supplied but not executed locally. There is no multiplayer validation because multiplayer is not implemented.
