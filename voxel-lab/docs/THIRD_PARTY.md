# Third-party code and runtime assets

The native build uses the following sources. Exact Git revisions are recorded in `references/manifest.json`; retain their notices in redistributed builds.

| Source | Use | License evidence |
|---|---|---|
| Eric Lengyel / Transvoxel | Regular/transition lookup data included by `src/mesher.cpp` | `references/Transvoxel/LICENSE` (MIT) |
| Auburn / FastNoiseLite | Portable noise generation | `references/FastNoiseLite/LICENSE` (MIT) |
| SQLite | Bundled amalgamation from the SQLiteCpp reference | Public-domain dedication in `references/SQLiteCpp/sqlite3/sqlite3.c` header |
| nlohmann/json | Content and profile serialization | `references/json/LICENSE.MIT`; supplemental files in `LICENSES/` |
| raylib | Native window, input, rendering, model loading and audio | `references/raylib/LICENSE` (zlib); bundled third-party source files retain their individual notices |
| Kenney Survival Kit | `campfire-pit.glb`, `barrel.glb` | `assets/kenney_survival-kit/License.txt` (CC0) |
| Kenney Impact Sounds | `footstep_wood_000.ogg` / wood impact prototype | `assets/kenney_impact-sounds/License.txt` (CC0) |

Zylann's Voxel Tools and voxelgame were read as architecture/algorithm integration references. Their implementation is not linked into the standalone client. Other downloaded references are not runtime dependencies. The new mesher uses the official Eric Lengyel tables directly; it does not compile Godot engine code.

The old Godot project was inspected as the user's functional reference; its scripts and restricted third-party artwork are not bundled into this executable. Terrain colors, UI, gameplay code and creature geometry are implemented in the new source. No Minecraft-derived texture atlas is used.
