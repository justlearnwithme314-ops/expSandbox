This voxel set was brought to you by Cosmo Myzrail Gorynych, a software and game developer.
See my games and tools on https://comigo.itch.io

The set includes:

- 93 models in .OBJ format;
- sources for Magica Voxel ( https://ephtracy.github.io/ );
- additional palette-texture for emissive effect;
- tutorial for quick setup in Unreal Engine.

This set is licensed under Creative Commons 0 license, meaning that you can use these models in any commercial or non-commercial projects, without the need to credit the author (though I will be happy if you credit me :)

You can also edit and/or redistribute this pack if you need so. Btw, if you create something cool with this set, tell about it on https://comigo.itch.io/puzzle-set :3



Quick setup for UE4
-------------------

1. Unpack the set somewhere and import textures.
2. Create a material called M_Palette like seen on image _01_MakeMaterial.JPG
3. Import .OBJ Files. Set Import Rotation to 90 — 0 — 0. I also recommend setting Import Uniform Scale to 4 so models' size will be somewhat comparable to real world scale. Next, set Import Material to true and Import Textures to false. Lastly, set Base Material Name to M_Palette.
4. A new material instance will appear. Rename it to MI_Palette_Emissive. You will probably need another material instance with zero intensity for powered-off tubes and wires.
5. Don't forget to disable Mip Maps for MagicaVoxel's palettes so it looks identically on different levels of graphic quality.  I also recommend setting interpolation method to Nearest. See how to set it on _05_TextureSettings.JPG
