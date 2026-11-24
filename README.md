# uae3DSng

Work-in-progress port of [UAE4All2](https://github.com/rsn8887/uae4all2) (the Vita/Switch flavour of the
UAE-based Amiga emulator) to the Nintendo 3DS, built on the platform layer of
[uae3DS](https://github.com/badda71/uae3DS).

**Builds in CI (devkitARM), not run on hardware yet** – see [PORTING.md](PORTING.md) for status, design, what was verified, known gaps and milestones.

Build (devkitARM + libctru + the portlibs below; on Windows use the devkitPro MSYS2 shell):

    pacman -S --needed 3ds-dev 3ds-sdl_image 3ds-sdl_gfx 3ds-libpng 3ds-zlib 3ds-libjpeg-turbo
    make release            # builds UAE3DS_SDL first; `make` alone is a -O0 debug build

`fatal error: SDL_rotozoom.h: No such file or directory` means `3ds-sdl_gfx` is missing. The `.cia`/`.3ds` steps at the end need
`bannertool` and `makerom` in the PATH; the `.3dsx` is built before them.

## Credits

The 3D model in the CIA banner is based on "Commodore Amiga 500 || Computer" by [dark_igorek](https://sketchfab.com/dark_igorek) on Sketchfab,
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); it was converted and modified for use as a 3DS banner.
