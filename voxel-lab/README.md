# Voxel Lab

A native C++20 voxel sandbox with smooth, editable **Transvoxel terrain**, grid-aligned construction, SQLite worlds and a portable world/gameplay library.

This is the first playable native port of the supplied GO.VW Godot project. Its original files and worlds are untouched. **Full feature parity is unfinished**: see [the port coverage](docs/PORT_STATUS.md), including multiplayer and the advanced mod systems still to implement.

## Play on Windows

Double-click **`launch.cmd`**, or run `build/native/bin/Release/voxel-lab.exe`. The launcher builds the game if the executable is missing. The Windows build requires Visual Studio 2022 C++ tools and CMake.

Enter a world name and seed, choose Creative or Survival, then enter the world. Reusing a name resumes that SQLite world, including its saved game mode. Initial terrain columns stream in over a few seconds. Worlds save every 30 seconds, on F5, on menu exit and on closing the window. A terrain/mesh worker runs on immutable world snapshots; GPU uploads stay on the main thread.

| Control | Action |
|---|---|
| WASD / mouse | Move / look |
| Space / Shift | Jump / sprint; Space swims upward |
| F, Space, Ctrl | Toggle creative flight, fly up/down |
| Left mouse | Mine terrain/blocks or attack a creature |
| Right mouse | Use a station, gate, bed or TNT; otherwise place/eat/plant |
| Shift + right mouse | Place instead of interacting |
| 1–0 / wheel | Select a hotbar slot |
| E | Inventory, recipe browser, creative catalog |
| Tab / Esc | Field guide / pause |
| F6 | Wide terrain sculpting brush |
| F7 / Shift F7 | Cycle line/wall/cube/sphere build brush / change size |
| F3 / F4 / F5 | Diagnostics / wireframe / save |
| / or T | Command console |
| Q | Discard one selected item |

Click inventory slots to swap or merge stacks; right-click a stack to split it. Crafting conserves inputs and refuses when output cannot fit. Some recipes need a nearby workbench.

Place a chest and right-click with a selected stack to deposit it. Select an empty hotbar slot and right-click to withdraw. Furnaces use iron ore + coal and produce iron ingots; sand produces glass. Adjacent generators supply batteries and crushers. Plant seeds on soil, wait one minute, harvest wheat and craft bread.

Use `/dungeon` to create a bounded nine-room sanctum nearby. Right-click the violet gate it places beside you. A matching return gate and loot chest persist in SQLite. This is a new, simpler C++ dungeon system; it does not implement the Godot altar renewal/encounter ledger.

Useful commands:

```text
/give core:log 64
/give survival:chest 2
/gamemode survival
/time night
/tp 10 35 10
/summon zombie
/dungeon
/learn double_jump
/learn dash
/learn parry
/learn slam
/save
/export
/heal
```

Techniques cost 20 XP each in Survival. Double jump uses Space, dash uses K, parry H, and slam J. Creative mode can learn them freely. Death returns you to spawn and retains inventory in this build.

## Build and test

```powershell
.\build.ps1 -Test
.\build.ps1 -Run
```

Or use CMake directly:

```text
cmake --preset windows
cmake --build --preset windows --parallel
ctest --preset windows
```

Linux/macOS have a Ninja desktop preset; raylib requires its platform development dependencies. Core-only builds avoid window/audio dependencies:

```text
cmake -S . -B build/core -DVOXEL_BUILD_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/core --config Release --parallel
ctest --test-dir build/core -C Release --output-on-failure
```

Windows is the locally tested platform. The core CI matrix is supplied for Windows/Linux/macOS; it has not been run by this local session. Desktop rendering on other platforms still needs validation. Mobile, web and consoles are not configured targets.

## Experimenting

- Edit `content/base.json` or add JSON packs to `content/mods/` with namespaced IDs, recipes, colors and item/tool properties. Restart to reload. Pack files load in sorted order and duplicate IDs are rejected. This is a data-pack loader, not a GDScript mod interpreter.
- `voxel-cli DATA_ROOT WORLD.sqlite "/command"` runs the same gameplay core without graphics. Without commands, it reads commands from standard input.
- `voxel-api.dll` / `libvoxel-api.so` / `libvoxel-api.dylib` exposes a versioned C ABI in `src/c_api.h`. Python, C#, Rust and other languages can bind that API. `tools/python_world_example.py` demonstrates Python ctypes and creates a separate test world.
- `references/` retains upstream source and exact revisions. `tools/fetch_references.py` restores missing checkouts from `references/manifest.json` without overwriting local modifications.
- `tools/audit_godot.py PATH` inventories the original project's mod manifests without editing it.

Core code has no raylib types. The client owns window/input/audio/rendering. Only `world.cpp` knows SQLite internals. `mesher.cpp` uses Eric Lengyel's official regular and transition lookup tables. The column layout supports 1:2:4 sample spacing with transition faces at neighboring resolutions; it currently covers Y = -32 to 64 for natural terrain, not an unbounded vertical octree.

## Saves and portable package

Development worlds are in `Worlds/Native/`. `--save-dir PATH` changes that location; `--data-root PATH` selects a content/assets directory. Native worlds carry the `voxel-lab-native-v1` generator marker. Existing Godot worlds and the older `Worlds/Wayfinder.sqlite` use other schemas/generators and are deliberately refused rather than silently reinterpreted. An importer is still needed.

`/export` writes a consistent SQLite backup, including committed WAL changes. Do not copy just the main file while a world is open; use the export command or close the game cleanly.

Run `tools/package.ps1` after building to produce `build/package/VoxelLab/`. That folder includes the executable, content, curated Kenney assets and notices. It can be moved as a unit; it does not include the reference repositories, old saves or unverified Minecraft/mod artwork. It writes its own `Worlds/Native/` directory, so put it somewhere writable or pass `--save-dir`.
