# Woodland dusk menu

The launch menu uses `res://assets/menu/woodland_dusk.png`, generated with the built-in `imagegen` tool on 2026-10-13. The landscape is an original menu backdrop; the game draws all typography, icons and controls separately. There is no terrain generation or 3D scene behind the menu. Fourteen lightweight, slowly drifting motes provide subtle movement.

Saved asset: `C:/Users/Admin/Downloads/powered-by-go-vw-modular-steve-tools/powered-by-go-vw-modular/assets/menu/woodland_dusk.png`

Final generation prompt:

> Use case: stylized-concept. Asset type: actual background art for an open-source voxel sandbox game's main menu, not a mockup. Primary request: an atmospheric, beautiful original voxel landscape at dusk. Wide 16:9 landscape composition. Stepped grass and moss blocks on rugged cliffs, clustered angular pine and birch trees, a calm river reflecting warm sunset, a few small floating rocky islands far in the hazy sky, distant mountains with softened layers. An intimate adventurous world, quiet and inviting. Style: crafted blocky 3D diorama with rich painterly lighting, crisp cuboid silhouettes and textured voxel materials, not photorealism and not a specific game's screenshot. Deep evergreen, teal, slate blue, warm amber sky and occasional golden firefly-like points. Composition: main landscape detail, glowing river and sunset on the RIGHT two-thirds; LEFT third shaded by quiet forest silhouettes with very low detail and darker values, deliberately reserved for readable menu text and buttons to be drawn by the game. No text, no letters, no logo, no user interface, no border, no characters, no watermark. The art must fill the entire wide frame. Carefully balanced and polished; avoid visual clutter.

Implementation: `core/ui/MenuBackdrop.gd`, `MenuShade.gdshader`, `MenuTheme.gd`, and the existing `MainMenu.gd` / `scenes/menu/MainMenu.tscn`. The background uses Godot's [aspect-cover TextureRect mode](https://docs.godotengine.org/en/stable/classes/class_texturerect.html#enum-texturerect-stretchmode) so it fills different window shapes without distortion.

Validation: `tests/MainMenuPresentationSmoke.tscn` uses a separate fixture catalog, checks page navigation and world creation, cancel-delete, host/join controls, saved-server selection, settings access, and a small window. Run it with a GPU backend for screenshots under `.testlogs/menu-*.png`. It does not connect to a server or delete real worlds.

Verified in Godot 4.7.1 with the Compatibility renderer: navigation, visible Play/Connect buttons, creation page, settings reopening, and 800x480 layout passed. The isolated world-save catalog regression passed. All 104 game/test/mod scene references resolved; 40 mod manifests parsed, and the Windows Voxel Tools binaries remained present. Unused BoardGames asset-pack demo scenes have preexisting references relative to the original demo project; those were excluded from the game-scene result and left untouched.

Run the repeatable headless checks with `tools/run_smoke.ps1 -Suite menu`. Network hosting/joining itself was not exercised by the presentation test.
