# uae3DSng – porting notes

Port of the Switch/Vita flavour of **UAE4All2** (`rsn8887/uae4all2`) to the Nintendo 3DS, reusing the
platform layer of **uae3DS** (`badda71/uae3DS`: SDL 1.2 fork for 3DS, bottom-screen UI).

> **Status: runs on a New 3DS (tested on hardware).** Boots to Kickstart, runs games from ADF at a steady
> 50 fps (including while loading), sound works (direct DSP output), menus and quit work. Not done yet:
> bottom-screen UI/keyboard (`uae3ds.cpp`, `uibottom.cpp`, `autofire.cpp` are excluded from the build), Old 3DS
> testing, real 3DS-specific menus, CIA/icon. Details of what was found on hardware are in the sections below
> and under [Verification](#verification).

## Provenance (git history is the changelog)

| commit | content |
|---|---|
| `75f98b6` | pristine `rsn8887/uae4all2 @ 44955a2`, `src/psp2` and `src/switch` removed |
| `d0a4c79` | pristine `badda71/uae3DS @ 8f1ddcf`: `UAE3DS_SDL`, `src/3ds`, `quirc`, `romfs`, `meta` |
| later | all uae3DSng changes |

`git diff 75f98b6 HEAD -- src ':!src/3ds' ':!src/quirc'` = every change made to the uae4all2 sources.

## Why this base, and why this layering

* The Switch build is `rsn8887/uae4all2`, not `lubomyr/uae4all2` (which has no Switch code and uses a
  Guichan GUI for which no 3DS library exists). rsn8887's build uses the plain-SDL "gp2x" text menu.
* uae4all2 is SDL **1.2**-style code (Switch only adds a thin SDL2 shim), which matches the
  `UAE3DS_SDL` fork one-to-one. `UAE3DS_SDL` is reused unchanged, as a sublibrary like in uae3DS.
* The core (FAME 68k core in C, custom chips, AGA/OCS/ECS, 68020, HDF, WHDLoad) is untouched. NEON
  (`USE_ARMNEON`, `USE_ARMV7`) stays undefined; the ARM11 has no NEON. `neon_helper.s` and the
  NetBSD `memcpy.s`/`memset.s` are not built (newlib's versions are used).

## Layers added for 3DS (`src/3ds`)

| file | role |
|---|---|
| `input3ds.h/.cpp` | `PAD_*` = libctru HID masks (replaces `psp2_input.h`); `input3ds_button/axis` = look-alikes of `SDL_JoystickGetButton/Axis` reading hid; `N3DS_PollEvent` = 3DS version of Vita's `PSP2_PollEvent` (synthesizes the key events the gp2x menus expect while `inside_menu`); `input3ds_init` unbinds the SDL driver's default hid→key mapping so buttons are not typed into the Amiga |
| `video3ds.h/.cpp` | `SDL_SetVideoModeScaling/Bilinear/Sync` (called by the Vita/Switch display code) on top of the fork's `N3DS_SetScalingDirect/N3DS_SetYOffset`. Converts the 960×544 destination rect to scale factors for the 400×240 top screen – **first approximation**, tune on hardware |
| `uibottom.h/.cpp` | bottom screen (from uae3DS): background, status bar (drive/power LEDs with track number or fps, message line, turbo icon) and the touch keyboard (slides in/out, sticky Shift/Ctrl/Alt/Amiga keys); touching outside the keyboard is a synaptics-style touchpad (move, tap = click, double-tap-and-drag). Drawn by the SDL fork's video thread via `SDL_RequestCall`. Differences to uae3DS: the touchscreen is read with `hidTouchRead` once per emulated frame (not via the driver's SDL mouse events, which scale with the SDL surface size), keys go to `record_key()` as Amiga key codes, mouse motion/left button go to `lastmx/lastmy/buttonstate`, the keyboard slide is time based (no worker thread). `gui_set_message()` (a no-op upstream) feeds the message line. Not ported yet: `uae3ds.cpp` (button→key mapping UI, QR/wifi helpers), `autofire.cpp`, turbo toggle |

Face buttons follow the physical position (as the Switch port does): A=circle(right), B=cross(bottom),
X=triangle(top), Y=square(left). In menus A=confirm, B=back (Nintendo layout; Vita default is reversed).

## Change inventory in the uae4all2 sources

* **Mechanical pass**: every `defined(__PSP2__) || defined(__SWITCH__)` (and negated forms) got
  `|| defined(__3DS__)` — 215 sites (no `sync/chmod/mmap/soundcard`, in-process zlib/minizip
  decompression instead of `system()`, `snprintf` temp names, 16-byte alignment, …).
  Reviewed in the core/system files; **not individually reviewed in the menu files**.
* **Reverted for 3DS** (Vita-only plumbing): `psp2shell` debug output, `SDL_PollEvent → PSP2_PollEvent`
  (now `N3DS_PollEvent`).
* `sysconfig.h`: includes `input3ds.h`+`video3ds.h` instead of `psp2_input.h`.
* `gui.cpp`: `gui_handle_events()` reads the pad through `JOY_BTN/JOY_AXIS/JOY_UPDATE` macros (hid on
  3DS, SDL joysticks elsewhere); `gui_init()` calls `input3ds_init()`.
* `od-joy.cpp`: `init/close_joystick()` – one built-in "joystick", no SDL handles.
* `sdlgfx.cpp`: SDL mouse events ignored on 3DS (the fork reports touches as *absolute* mouse events).
* `main.cpp`, `gp2x/gp2x.cpp`: `romfsInit()`, `osSetSpeedupEnable(true)`, `launchDir = sdmc:/3ds/uae3DSng`
  with `roms saves conf kickstarts thumbs tmp` created at start-up.
* `zfile.cpp`: minizip include for 3DS. `menu_display.cpp`/`menu_config.cpp`: Switch shader set
  (none / sharp-bilinear / bilinear / point) used for 3DS. `SDL_ttf.h` includes removed (nothing calls `TTF_*`).
* `Makefile`: explicit source list (dirs `src src/3ds src/gp2x src/gp2x/menu src/menu src/vkbd src/m68k
  src/m68k/fame`) with `EXCLUDE_FILES`; defines mirror rsn8887's Switch CMake minus SDL2/64-bit.
* `romfs/data/`: uae4all2 menu graphics and the virtual-keyboard bitmaps (`upstream-data/` is the pristine copy).

## Build

devkitARM + libctru, and `pacman -S 3ds-sdl_image 3ds-sdl_gfx 3ds-libpng 3ds-zlib` (+ libjpeg that
`3ds-sdl_image` pulls in). Then `make release` (builds `UAE3DS_SDL` first). Kickstarts go to
`sdmc:/3ds/uae3DSng/kickstarts/` as `kick12.rom kick13.rom kick20.rom kick31.rom kickcustom.rom`.

## Verification

**CI build (GitHub Actions, `.github/workflows/build.yml`)**: `devkitpro/devkitarm` container
(devkitARM r68 / GCC 16.1, libctru 2.7.1), `make release`. All 54 sources compile, `UAE3DS_SDL` builds,
`uae3DSng.elf` links and `uae3DSng.3dsx` is produced (with romfs). The `.cia`/`.3ds` steps fail in CI only
because `bannertool` and `makerom` are not in the image. Build outputs are uploaded as the
`uae3DSng-build` artifact of each run. First real-toolchain findings were fixed in-tree:
`<sys/mman.h>` does not exist in newlib (`menu_load.cpp`), and an unused `-DGCCCONSTFUNC="..."` define broke
devkitPro's C rule through shell quoting (removed).

Before CI existed (kept for reference; the host check is still useful for quick local syntax passes):

1. `make -n` against a stub devkitARM: the Makefile selects 52 C++ + 2 C files, excludes the intended
   ones, and passes the ARM11 flags and all defines. Real flags/rules of `3ds_rules` were not exercised.
2. `tools/hostcheck/check.sh`: clang **syntax-only** pass, 32-bit target, against the **real libctru
   headers** (clone of `devkitPro/libctru`, HEAD 2026-11-14) and `UAE3DS_SDL` SDL 1.2 headers.
   All 52 C++ and 2 C files pass. Limits: glibc instead of newlib headers (stubbed where they differ),
   no `SDL_image/SDL_rotozoom` real headers (small stubs), no code generation, no linking.

**Not verified:** running (boot, video, audio, input, UI), performance, memory use, any behaviour.
Runtime risks I already know of are listed below; expect more once it runs on hardware.

## Audio, timing, shutdown (found on hardware)

* **Audio** (`src/3ds/sound3ds.cpp`, replaces `sound_sdl_new.cpp` on 3DS): upstream paces the emulator by
  blocking it on a semaphore until the SDL audio callback consumed a buffer. On 3DS the SDL audio thread never
  reached the callback (log: `callbacks=0` while the DSP's frame callback kept running), so the emulator
  froze after Reset and Quit hung joining that thread. The 3DS build now drives the DSP (ndsp) directly:
  samples are written into linear-memory slots and finished slots are queued on DSP channel 0 from
  `finish_sound_buffer()`; nothing ever waits (too far behind = drop a buffer, underrun = brief silence).
  The log has `audio[...]` lines with the slot states (0 free, 1 queued, 2 playing, 3 done) and ndsp state.
  Root cause of the SDL path not working is not known (uae3DS uses the same driver successfully).
* **Speed**: with audio no longer pacing the emulator, `sdlgfx.cpp` uses a real frame limiter on 3DS (sleep until
  `next_synctime`, clamped to 2 frames). Upstream only slept when more than a whole frame ahead, which assumes
  audio pacing and made the game run too fast once audio was off.
  Upstream also restarts the schedule from "now" whenever a frame ends >15 ms late, so every slow frame (SD read,
  heavy scene) permanently loses time. Hardware log showed windows at 32-44 fps although the average emulation
  cost was well under 20 ms/frame; the 3DS limiter now catches up through up to 4 frames of lateness.
  The `speed:` log line reports fps, limiter slack, worst frame, frames >35 ms and time spent reading the ADF.
  Hardware log of that build: while loading, ~1000 ADF reads per 5 s took ~2.5 ms each (an fseek+fread of one 512-byte
  sector on the SD card) = half of all wall time, with frames up to 180 ms. `zfile.cpp` therefore keeps the whole
  floppy image (<= 2 MB) in RAM on 3DS; writes still go through to the file.
* **Hard disks**: hardfiles mount through the WinUAE boot ROM (`filesys_bootrom.cpp`, `#include`d by `filesys.cpp`). The
  boot code looks the volume's DosType up in `FileSystem.resource`, which only exists from Kickstart 2.0 on, so an
  FFS-formatted HDF is not mounted under Kickstart 1.3 (hardware log: the filesys traps all run, no drive appears).
  `filesys_dev_bootfilesys` now logs `hardfile unit N: DosType ...` and whether a handler was found.
  Crash diagnostics: the first illegal instruction that is not the Kickstart's MOVEC probe dumps registers, stack, vectors,
  the last 24 native traps and the last 16 hardfile requests to the log (illegal-instruction lines are throttled, a runaway CPU
  produced 5000 synced log writes). `uaehf.device` now returns `IOERR_BADLENGTH/BADADDRESS` for bad requests instead of
  silently "succeeding", and no longer rejects odd data pointers (the copy is byte-wise).
  `crashdiag_frame()` (once per frame) logs exec's version/free memory once, appends Amiga free chip/other memory to every `speed:` line,
  and reports the frame in which ExecBase/the exception vectors get overwritten (hardware log: 1 MB chip-only machine, Format on a 25 MB
  HDF, then low memory trashed and the CPU ran off into the rtarea strings).
  Crash dumps from the 3DS exception screen can be resolved by putting the PC/LR addresses into `tools/crashaddrs.txt`: CI prints function,
  source line and disassembly into the build log (`crashlookup.log`). Lesson: diagnostics must read Amiga memory through FAME's map
  (`m68k_fetch`), not `get_long()`: `kickmem_lget()` does not mask and crashed the host while the ROM overlay was mapped at 0.
  `TRAP: exter_int_helper` (~100/s with a HD mounted) is no longer logged: each log line is a synced SD write and it cost 10-20 fps.
* **Menus**: every file that polls events in the menus must use `N3DS_PollEvent` (via `#define SDL_PollEvent`), otherwise
  no button reaches it. `menu_memory_disk.cpp` and `menu_fileinfo.cpp` were missing it (Memory/Disk menu froze).
* **Quit**: `app_exiting` is set before `SDL_Quit()` so the audio thread's `WaitAudio()` returns.
* `Illegal instruction: 4e7b at 00fc0564` in the log is Kickstart 1.3 probing for a 68010 (MOVEC) - expected.
* Debug log: `sdmc:/3ds/uae3DSng/uae3DSng.log` (+ `stdout.log`), flushed per line; `write_log()` maps to it.

## Known gaps / risks

1. **245 platform-conditional lines are unaudited** (`grep -rnE "__PSP2__|__SWITCH__|USE_SDL2"`,
   mostly `gp2x/menu/*`, `od-joy.cpp`, `gui.cpp`, `sdlgfx.cpp`, `sound_sdl_new.cpp`). Their 3DS branch is
   the generic/Linux `#else`, which compiles but may be wrong (e.g. file-browser start paths, Switch-only
   sound buffer macro in `audio.cpp`/`sound_sdl_new.cpp`, `mainMenu_swapAB`).
2. **Memory**: uae4all2 allocates chip/slow/fast RAM and several tables with `malloc`. Check that the
   defaults fit libctru's malloc heap on Old 3DS (the 64 MB application region is shared with the
   linear heap that the SDL driver uses for textures). uae3DS builds with `SYSTEM_MODE_EXT 124MB` for
   New 3DS; the same is configured in this Makefile. I have not measured anything.
3. **Speed**: the FAME core is C; AGA and 68020 will not be playable on Old 3DS (268 MHz), and likely
   marginal on New 3DS. Consider hiding/blocking those options on Old 3DS (`osIsNew3DS()`).
4. **Bottom screen is not ported yet.** `uae3ds.cpp`, `uibottom.cpp`, `autofire.cpp` are in the tree but
   in `EXCLUDE_FILES`. The virtual keyboard (`vkbd`, drawn on the *top* screen) is compiled in as the
   interim keyboard. Dependencies of the uae3DS code that must be provided/adapted:
   * from uae3DS's own menu: `text_messagebox`/`MB_*`, `sdl_3dsbuttons buttons3ds[]`, `show_message(_str)`,
     `write_text_full`/`FONT_*`/`ALIGN_*`/`menu_text_color`, `storeConfig`, `nowSuperThrottle`,
     `mainMenu_{max_tap_time,click_time,single_tap_timeout,max_double_tap_time,locked_drag_timeout,
     tap_and_drag_gesture,locked_drags}` (touchpad gestures), `gui_data` drive LEDs (check uae4all2 has it);
   * from the SDL fork (exported symbols, so `UAE3DS_SDL` must stay as is): `VideoSurface2`, `projection2`,
     `privateSem1`, `uLoc_projection`, `SDL_RequestCall`;
   * **key convention mismatch**: uibottom pushes *Amiga* key codes (`AK_*`, < 0x100) as SDL keysyms and
     uae3DS's loop does `record_key(sym << 1)`; uae4all2's loop calls `keycode2amiga(&keysym)`. Needs a
     3DS branch in `handle_events()` (sym < 0x100 ⇒ already an Amiga code).
   * mouse: uibottom emits synthetic mouse events with `button.which == 1`; real touches have `which == 0`.
     The temporary "ignore all mouse events" guard in `sdlgfx.cpp` has to be replaced by
     `uib_handle_event()` + handling of `which == 1` events.
5. Not ported from uae3DS: key-mapping layer, super-throttle, status line, CIA self-update, curl/QR
   download, df-selection menu. (`3ds_cia`, `http`, `update`, `qr-scanner`, `quirc` are imported, unused.)
6. Display scaling (`video3ds.cpp`) and the filter options are approximations; the menu screen
   (`init_text`) shares the same shim.

## Suggested order

* **M1 – boots**: build, fix compiler/linker errors, get Kickstart 1.3 to the insert-disk screen on the
  top screen, hear audio, control with pad, open menu with SELECT, load an ADF.
* **M2 – bottom screen**: bring in `uibottom` (keyboard, touchpad, status line) per gap 4, drop `vkbd`.
* **M3 – 3DS-native menu/options**: touch-friendly file selection, per-model gating (Old/New 3DS),
  tune scaling and the control presets, memory sizing.
* **M4 – extras**: CIA self-update, QR/URL download, key remapping UI.
