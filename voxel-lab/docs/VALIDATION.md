# Native validation record

Local platform: Windows x64, MSVC Release, OpenGL 3.3. This records the checks run during the initial native implementation, not full Godot feature parity.

- CMake Windows configure and Release build: passed for `voxel-lab`, `voxel-cli`, `voxel-api` and `voxel-tests`.
- Core tests: passed. These cover inventory/crafting conservation, failed-harvest capacity, negative coordinates, immutable snapshots, ray hits, SQLite round trips, export, foreign-format refusal, furnace/crop simulation, commands, XP unlocks, atomic construction brushes, gates/dungeons, underground water rules, all regular/transition case indices for finite geometry, and seam coverage in six orientations.
- Python ctypes example: calls the C++ API, reads density, edits terrain, modifies inventory, serializes a player profile and saves SQLite; error-boundary and UTF-8 filename checks are included.
- Offline CLI: `/give`, `/learn` and `/save` executed against a separate test world.
- Actual graphical smoke: 121 loaded terrain columns, 279,313 triangles including 5,980 transition triangles in the surface scenario. Kenney campfire/barrel, their external texture, wood audio and terrain shader load.
- Crafting UI screenshot visually inspected: readable controls, ingredient rows and output buttons fit within the panel.
- Dungeon screenshot visually inspected: room walls/floor/corridor and creature render; sealed underground rooms are dry.
- Packaged executable was run using its own content/assets rather than the source asset collection. The package omits original worlds and unverified Minecraft/mod artwork.

Screenshots are local development outputs:

- [Surface terrain](../test-output/native-game.png)
- [Crafting](../test-output/native-inventory.png)
- [Dungeon](../test-output/native-dungeon-final.png)

Cross-platform CI is provided but was not executed by this Windows session. Multiplayer, the original Godot world's import, and advanced unported systems have no native acceptance results. See [port coverage](PORT_STATUS.md).
