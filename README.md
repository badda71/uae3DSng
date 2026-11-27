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

## WHDLoad games (.lha) – experimental

*Memory and Disk Options → "WHDLoad Games (.lha)..."* unpacks a WHDLoad archive straight into a folder on the SD card and starts it
(the folder becomes the boot "HD Dir", a startup sequence runs the game's `.slave`). Not tested on hardware yet. On the SD card, under `3ds/uae3DSng/`:

* `whdload/C/WHDLoad` – the WHDLoad program. If you have none there, the copy built into the app (WHDLoad 20.0, `romfs/whdload`) is put there when you
  start a game; a file you put there yourself is never replaced (use it for another version; delete it to get the built-in one back).
  WHDLoad is © Bert Jahn, all rights reserved, see `romfs/whdload/WHDLoad-NOTICE.txt`.
* `kickstarts/` – your Kickstart ROMs as for floppies. The ones WHDLoad needs are copied to `whdload/Devs/Kickstarts` under their WHDLoad names on the first
  start (`kick13.rom` → `kick34005.A500`, `kick31.rom` → `kick40068.A1200`, ...), together with any `.rtb`, `.pat` and `.key` files found there.
* Games land in `whdload/<archive name>/`. Your current CPU/chipset/memory/Kickstart settings are kept, so pick the machine the game needs before you start it.

## Credits

The 3D model in the CIA banner is based on "Commodore Amiga 500 || Computer" by [dark_igorek](https://sketchfab.com/dark_igorek) on Sketchfab,
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); it was converted and modified for use as a 3DS banner.
