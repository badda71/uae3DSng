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

*Memory and Disk Options → "WHDLoad Games (.lha)..."* installs a WHDLoad archive into **one Amiga disk image** on the SD card and starts it
(the image is mounted as a hard file, `WHDGame:`, a startup sequence runs the game's `.slave`). Tested on a New 3DS (installs in seconds, the game runs at 50 fps). Why an image and not a
folder of files: the first version extracted the archive into a folder, and on a real New 3DS that took 3 minutes for 639 files (3.8 MB) plus about a minute
to find the `.slave` and 6 more minutes of loading inside the emulator: every create/stat/open of a file on the 3DS SD card is slow once a folder holds hundreds of files.
One big file is written at full speed and opened once. On the SD card, under `3ds/uae3DSng/`:

* `whdload/C/WHDLoad` – the WHDLoad program. If you have none there, the copy built into the app (WHDLoad 20.1, `romfs/whdload`) is put there when you
  start a game; a file you put there yourself is never replaced (use it for another version; delete it to get the built-in one back).
  WHDLoad is © Bert Jahn, all rights reserved, see `romfs/whdload/WHDLoad-NOTICE.txt`.
* `kickstarts/` – your Kickstart ROMs as for floppies. The ones WHDLoad needs are copied to `whdload/Devs/Kickstarts` under their WHDLoad names on the first
  start (`kick13.rom` → `kick34005.A500`, `kick31.rom` → `kick40068.A1200`, ...), together with any `.rtb`, `.pat` and `.key` files found there.
  The Kickstart relocation tables WHDLoad needs for those ROMs (`kick34005.A500.RTB` and friends, © Bert Jahn like WHDLoad, see `romfs/whdload/WHDLoad-NOTICE.txt`)
  are built into the app (`romfs/whdload/Kickstarts`) and copied to `whdload/Devs/Kickstarts` when missing; a table of your own with another size replaces the built-in one.
  The Kickstart ROMs themselves are not included, you have to supply them.
* A game lands in `whdload/<archive name>.hdf` (an OFS disk image, 2 MB + 3 % larger than the game so savegames fit) with a small `<archive name>.whd` next to it that
  names the `.slave`. Both files belong together; delete both to remove a game. The image is also a normal hard file: it can be opened with Amiga tools (volume `WHDGame`).
  Folders `whdload/<name>/` from the first version (games extracted into a folder) are no longer used: install the game again and delete the folder.
  Amiga file names are at most 30 characters long: a longer name inside an archive is shortened in the middle and keeps its extension
  (`FateGatesOfDawnUncensored.slave` → `FateGatesOfDawnUncensore.slave`).
* To uninstall a game, select it in the *WHDLoad Games* list and press **X** (asked first): its `.hdf` and its `.whd` are deleted.
  A game that is loaded in the running emulator cannot be deleted: start another game or eject the hard
  disk and reset first. Saved games inside the image go with it.
* **Favourites:** press **Y** on a game in the list to mark it (`*`) or unmark it; favourites are listed first. They are kept in `whdload/favorites.txt` (one game name per line).
* **Options:** many slaves have settings you give on the WHDLoad command line (`CUSTOM1=1`, `QUITKEY=$5D`, `NOCACHE`, ...). Press **L** on a game: the 3DS keyboard asks for
  them (empty = none); they are shown under the list and added to the command line after `PRELOAD` every time the game starts. They are stored per game as an
  `options=` line in `whdload/<name>.whd` (you can also edit that file) and survive installing the same archive again.
* Starting a game sets "HD Dir" to the `whdload` folder (boots from it) and hard file 0 to the game's image, ejects the floppies and resets. Your current
  CPU/chipset/memory/Kickstart settings are kept, so pick the machine the game needs before you start it. Hard file 0 is overwritten by this.

## Credits

The 3D model in the CIA banner is based on "Commodore Amiga 500 || Computer" by [dark_igorek](https://sketchfab.com/dark_igorek) on Sketchfab,
licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/); it was converted and modified for use as a 3DS banner.
