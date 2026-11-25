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
* `Makefile`: explicit source list (dirs `src src/3ds src/gp2x src/gp2x/menu src/menu src/m68k
  src/m68k/fame`) with `EXCLUDE_FILES`; defines mirror rsn8887's Switch CMake minus SDL2/64-bit.
* `romfs/data/`: uae4all2 menu graphics (`upstream-data/` is the pristine copy, minus the removed vkbd bitmaps).

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
  **SDL fork leak (fixed):** every `SDL_SetVideoMode()` in `UAE3DS_SDL/src/video/n3ds/SDL_n3dsvideo.c` allocated the spritesheet texture
  (`C3D_TexInit`, linear memory: 1 MB for the 1024x512 menu mode) plus a semaphore and an event without freeing the previous ones. The menu
  switches modes 3x per round trip, so after ~33 calls the 26 MB of linear memory was gone, the texture pointer was NULL, the transfer went to
  address 0 and the top screen went black with a frozen emulator (hardware log: 11 mode switches at start/selection + 9 menu trips).
  The log now shows `linear free N KB` on every `init_text`/`update_display`/`speed:` line; it must stay flat across menu trips.
  `TRAP: exter_int_helper` (~100/s with a HD mounted) is no longer logged: each log line is a synced SD write and it cost 10-20 fps.
  **Low-memory write watch:** the 1 MB-chip freeze is not RAM exhaustion (hardware log: 695 KB chip free at the moment of the crash). exec's
  vectors and ExecBase pointer were overwritten by the first block of an executable (`000003f3 00000000 00000005 ...` = HUNK_HEADER at
  address 0), i.e. something copied file data to address 0 (a Read() into a NULL buffer?) a frame before the CPU ran off. FAME reaches chip RAM
  through direct page pointers, so `map_zone()` installs *write-only* handlers (`famec_SetWriteHandlers`) on the page(s) mapping chip offset 0;
  every write to chip offset 0..7 (and to the mirror above the real chip RAM, e.g. 0x100000.. with 1 MB) logs a short `lowwrite:` line, and
  the first few hits from different PCs that are in user mode (or >300 frames after the reset) get a full dump: registers, ThisTask name
  and CLI command name, exec free chunks, DosPacket candidates (ACTION_READ arg2 = buffer), stack and the native trap/hardfile history.
  Reads/fetches stay direct, so the cost is small. First version armed itself on stale exec data left from the previous Kickstart and
  spent its log budget on boot writes (Kickstart's chip-size test writes 0xF2D4B689 to 0x100000 and checks address 0; exec sets the ExecBase
  pointer) - hence the supervisor/300-frame rule. If the frame check reports a trash but the watch saw no later hit, it was not the CPU
  (DMA or native code). Hardware result so far: the A1200 profile (2 MB chip + 8 MB fast, AGA) completes the same install, the A500 profile
  (1 MB chip only, ECS) trashes address 0 at the same point every time (identical registers) - the speed line now shows the largest free
  chip chunk to judge the failed-allocation theory (an installer using a NULL from AllocMem would do exactly this on a real machine too).
  **Root cause of the A500-profile freeze (hardware log, 2nd watch build):** the hit was the `DF1:` handler process running its ROM copy loop
  for an `ACTION_READ` packet with `arg2 = 0` (buffer) and `arg3 = 290000` (length) - a program asked DOS to read 290000 bytes into address 0,
  i.e. it used the NULL result of a failed allocation (largest free chip chunk was 696 KB, so the request must have wanted something the
  machine lacks - most likely MEMF_FAST; the A1200 profile has 8 MB fast RAM and works). A real A500 without fast RAM would trash its vectors the
  same way. To confirm, `filesys.cpp` wraps exec's AllocMem (-198) and AllocVec (-684) with guest stubs in the rtarea (`alloctrace_*`,
  patched into the jump table by `filesys_diagentry` on every boot) and logs every failed or >=150000-byte request: size, flags, result,
  caller, running task and CLI command. The lowwrite dump also names the task that sent a DosPacket (reply port -> mp_SigTask).
  **Confirmed (3rd hardware log):** `AllocMem: size 296960 flags 00010005 FAST CLEAR -> 00000000 *** FAILED ***, task 'Install'` ~6 s before the
  trash, and the DosPacket's sender is the same task: the installer asks for 290 KB of *fast* RAM, gets NULL on the chip-only A500 profile and
  passes it to Read(). Not an emulator bug - a real A500 without fast/slow RAM crashes the same way; with fast or slow RAM (slow RAM counts as
  fast for AllocMem) or the A1200 profile it works. UX: when the vector table gets trashed the emulator now shows a **Guru Meditation** banner on the *main* screen (src/3ds/guru.cpp: black box, flashing
  red border, red 8x8-font text, integer-scaled to the screen width; drawn over the picture right before each flip, so nothing in the emulated machine is touched).
  A click of the left mouse button reboots the Amiga, like a dead-end alert. The alert number is synthetic - `8000_0004` (CPU exception 4, illegal
  instruction), the real machine's exact exception is unknowable - and the second number is the address of the task that was running at the last good
  frame, so it can be matched against the log. The log also names the last failed allocation (`lowmem: last failed allocation: 290K by 'Install'`).
  Deliberately no bottom-screen message: a real Amiga shows nothing there, and a program that handles a failed AllocMem gracefully must not look like a crash.
  The write watch ignores writes that do not change anything (the ROM's `CLR.L 0` at boot) and goes quiet after the trash. Also: the `chipmem_*` bank accessors
  always mask now (they only did with `SAFE_MEMORY_ACCESS`, so native accesses to the mirror at 0x100000..0x1FFFFF of a 1 MB machine ran
  past the host buffer).
* **Menus**: every file that polls events in the menus must use `N3DS_PollEvent` (via `#define SDL_PollEvent`), otherwise
  no button reaches it. `menu_memory_disk.cpp` and `menu_fileinfo.cpp` were missing it (Memory/Disk menu froze).
* **Quit**: `app_exiting` is set before `SDL_Quit()` so the audio thread's `WaitAudio()` returns.
* `Illegal instruction: 4e7b at 00fc0564` in the log is Kickstart 1.3 probing for a 68010 (MOVEC) - expected.
* Debug log: `sdmc:/3ds/uae3DSng/uae3DSng.log` (+ `stdout.log`), flushed per line; `write_log()` maps to it.

## Menu, display, status line (hardware feedback, round 3)

* **Menu = 400x240, 1:1.** It used to be an 854x480 surface (the Vita's), pixel-doubled in software by `SDL_SoftStretch`, uploaded as a
  1024x512 texture and scaled down to ~0.42x/0.44x by the GPU. Now `menu_screen_width/height` = 400x240, `MENU_ZOOM` = 1 (the 427x240
  `text_screen` of the Vita becomes 400x240 and is blitted, not stretched) and the driver shows it unscaled (`video3ds_set_native`, same as
  uae3DS's menu). Nothing of the menu can be cut off any more.
* **Select -> menu took 2-3 s.** The log showed `init_text: SetVideoMode(854,480)` twice, 3.0 s apart: `goMenu()` and `run_mainMenu()` both call
  `init_text()`, and each call ran 10x `FillRect+SDL_Flip` (a Vita flip-chain clean-up) on whatever surface was up, then freed the surface and
  called `SDL_SetVideoMode` again (the driver rebuilds its render targets and video thread every time). Now: `init_text()` does nothing when the
  menu mode is already up, one clearing flip instead of 10 (4 in `update_display()`), and the surface is 6x smaller.
* **Blitter "Improved" had no effect.** Not wired wrong, but only applied at an Amiga reset: `currprefs.immediate_blits` is copied from
  `changed_prefs` in `customreset()` only, and the blitter checks `currprefs.immediate_blits` *before* `blitter_in_partial_mode`, so with
  "Immediate" active (the A500/A1200 preset default) choosing "Improved" did nothing until a reset. `UpdateChipsetSettings()` now sets
  `blitter_partial_wanted` + `changed_prefs.immediate_blits` and calls `blitter_sync_mode()`, which applies both as soon as no blit is in flight
  (else `hsync_handler` retries); the log shows `blitter: improved mode`. The emulation code itself is unchanged from upstream.
* **File dialogs.** 3DS L = `SDLK_RSHIFT` = page-up in the lists; in the HD-directory dialog it now selects the directory (SELECT still does).
  Button names follow the physical positions (`input3ds.h`: Square=Y, Triangle=X, Cross=B, Circle=A) like the Switch port: titles say "X = Info",
  the Controls / Autofire entries say Y/X/B/A.
* **Status line.** The emulator's own status line (idle %, power LED + fps, HD LED, one LED + track per drive) is no longer drawn into the top
  screen (`#ifndef __3DS__` in `drawing.cpp`); `uibottom.cpp` draws the same on the bottom screen when the "Status Line" option is on (default on
  for the 3DS). Messages ("Saved", ...) cover the whole bar for their duration.
* **Round 4.** (a) Savestate thumbnails were sized for the Vita's 2x menu surface (`132*2` px high, centred at `2*(y+66)`); `SS_ZOOM` is 1 on the
  3DS now. (b) The "Menu Background" option is gone; the menu always uses uae3DS's `romfs:/bg_top.png` (400x240, loaded with `IMG_Load`, no
  tiled/moving variant). The bottom screen already used uae3DS's `bg_bot.png` + `kbd1/kbd2.png`, with no option. (c) The "Status Line" option is gone: the
  bottom-screen status bar is always on (the config key `showstatus` is still read/written for compatibility but ignored). (d) File dialogs: the
  Vita pseudo root (`/` listing `ux0:`/`uma0:`) is not used on the 3DS; `menuLoadLoop()` replaces any directory that does not start with `sdmc:`
  (an old `adfdir.conf` containing `/`, `ux0:/...`) by `sdmc:/`, `..` never leaves `sdmc:/` ("sdmc:" without the slash is relative to the
  device's cwd), a vanished directory falls back to `sdmc:/`, and the HD-dir path no longer becomes `sdmc://name`.
* **Round 5.** (a) Savestate menu: the screenshot (PNG, up to 640x240) was decoded + scaled with SDL_gfx on *every* left/right key repeat and
  drawn as a 32 bit RGBA surface (alpha blit per frame). Now the picture is only decoded once the cursor has rested on a slot for 140 ms,
  shrunk with an area-average loop into a 16 bit display-format surface (plain copy when drawing); the timings go to the log
  (`savestate thumbnail: decode .. ms, shrink .. ms`). (b) The top-screen virtual keyboard is gone: `src/vkbd`, the 48 `vkbd*.bmp`,
  `USE_UAE4ALL_VKBD` and everything under it (Start no longer opens it), the "Keyb. Lang./Style" options. `vkbdlanguage=`/`vkbdstyle=` stay in
  the config file as unused keys because the file is read positionally. (c) Menus use uae3DS's 8x8 font (`romfs:/8x8font.bmp`, glyph `(c-32)`
  at `(c&15)*8,(c>>4)*8`, white on black = colour key): cells are `MENU_CW` = 8 px wide, text lines were 7 px apart (`MENU_RH`, 6 px since round 9) (glyph rows 0-6, row 7 only
  descenders), `MENU_X0` centres a 44-column grid, the file dialog is 48 columns wide. The "Menu Font" option and `text*.bmp` are unused on
  the 3DS. (d) Blur: the Vita->3DS mapping of the requested picture rectangle was proportional to 960x544, which gave 0.94 x 1.0 for the default 320x240
  picture and so GPU_LINEAR. `SDL_SetVideoModeScaling` now maps by *aspect only*: Shader "1:1" = scale 1 (hires: 0.5 x 1, exactly 2:1),
  Shader "Fit height" = uniform `240 / lines`; scales within 0.5 % of 1 snap to 1, and the driver selects GPU_NEAREST unless the scale is
  really different from 1 (tolerance 0.001). BILINEAR/POINT were removed from the 3DS shader list (a saved 2/3 becomes 1).
* **Round 6 (menu look).** The menu font is black (the font bitmap's two palette entries are swapped at load, white is the colour key) on the light grey
  window pattern; the title is black on the frame. The frame/shadow colours are constant (`menu_win1_color`/`menu_win0_color` in `romfs/data/colors.txt`;
  `update_window_color()`, which made them pulse, is not called any more). On the 3DS `text_screen` is a transparent *layer* (colour key magenta,
  software surface): `text_draw_background()` clears it and forgets the window, the first `text_draw_window()` of a screen only records x/y/width/title,
  and `text_flip()` (-> `menu_compose()`) draws background, then the window with the height of the text that was written (`write_text*()`/bars call
  `text_force_extent()`), centered horizontally and vertically (dx/dy), then the layer at (dx, dy). A second `text_draw_window()` in the same screen (message
  box) is drawn into the layer unchanged. The file dialog forces its nominal height (`text_force_extent`), the savestate thumbnail is drawn after the
  layer at `y + dy`.
* **Round 7 (speed while the Amiga uses files).** The log from the round-6 build showed 2,583 `TRAP: filesys_handler` lines: one per DOS packet of the
  directory file system (thousands while a game loads). Every `write_log()` was `fflush` + `fsync`, and an `fsync` on the SD card takes 10..90 ms: the
  frames in which the Amiga accessed files ran at 28..47 fps with 130..430 ms hitches, frames without file access were fine (49.6 fps, worst frame 20 ms,
  spare time the same as in the pre-round-3 builds). Changes: (1) `autoconf.cpp` logs each trap only its first 3 times; (2) `log3ds` has a lazy mode:
  while frames are emulated, lines are written but not synced, and `log3ds_tick()` (called per frame from `flush_block`) syncs at most every 2 s, only when
  the limiter has >= 6 ms of idle time to hide it in (the limiter's 4-frame catch-up covers the rest; forced after 10 s). `init_text()` (menu) and
  `do_leave_program()` switch back to sync-every-line, so start-up, menu and shutdown breadcrumbs stay crash-safe. (3) Savestate thumbnails are read with
  one `read()` into memory and decoded with `IMG_Load_RW`: libpng's many small stdio reads cost several ms each on uncached SD sectors (1.8 s for a slot
  that was not in the OS cache, 0.15 s for a fresh one). New diagnostics: the `speed:` line has `flip avg/max` (time in `SDL_Flip` per frame), the log has
  one `menu:` line per menu visit (average/maximum time of a menu pass, of which compose and `SDL_Flip`), and the thumbnail line shows the file size.
* **Round 8 (A500 vs. A1200: why the emulated Amiga is faster, and what it costs the 3DS).** The user noticed that choosing A1200 instead of A500 makes a
  big difference to how fast the emulated Amiga is. That comes from the menu's *Preset System Setup*, not from the port: `setSystem()` (menu_main.cpp) sets
  A500 = 1 MB chip, Kickstart 1.x, OCS/ECS, 68000, **CPU speed 7 MHz**; A1200 = 2 MB chip, 8 MB fast, Kickstart 3.x, **AGA**, 68020 flag, **CPU speed
  14 MHz**. The FAME core is 68000-only and the "CPU speed" setting only multiplies the cycles it gets per emulated time slice (`famec.cpp`:
  `io_cycle_counter = cycles << mainMenu_CPU_speed`), so A1200 executes twice as many instructions per frame, plus it has fast RAM. The price is host time
  (spare time of the frame limiter in the 3DS logs): idle Workbench 64 % spare on A500, 55 % on A1200; the CPU-bound `Num` program ran with 13..15 % spare at
  a steady 50 fps on A1200 (the A500 run is not comparable: its 1.1 MB allocation failed, so it did less work). For the next log the `speed:` line starts
  with the model (`[AGA, cpu 14 MHz]`) and splits the busy time of an average frame into `emu` (emulated CPU + chipset + sound + disk), `draw`
  (`finish_drawing_frame`: bitplanes to screen, average and max) and `flip`; the `config:` line shows the CPU speed. That shows how much headroom is left, e.g.
  whether *Misc > CPU Speed = 28 MHz* could hold 50 fps for CPU-heavy programs, and whether `draw` (C line conversion, no NEON on ARM11) matters at all.
* **Round 9 (menu tweaks).** (1) Text rows are `MENU_RH` = 6 px apart instead of 7 (menu.h; all row arithmetic in menu.cpp, the file dialog's window extent and
  the savestate thumbnail position use it). Menu items are two rows apart, so they are 12 px apart instead of 14; the highlight bars of `write_text_inv()` are now
  `2*MENU_RH-2` = 10 px high (was 12) so that the bars of neighbouring items keep their 2 px gap, popup windows (`h*8` units of the old 7 px rows) are scaled by
  6/7. Two text rows directly below each other would now overlap by one pixel row (glyph rows 0-6); the menus only put text on every second row. (2) Text that
  would run over the right edge of the window it is written into is cut one column before the frame (`fit_text()` in `write_text()`/`write_text_inv()`,
  using the columns of the last `text_draw_window()`; it matters for long file names in the DF0..DF3 lines and the hard-file lines). (3) The file dialog no
  longer lists `..` in `sdmc:/` (it only led back to the same directory); an empty root keeps one `..` entry so that the list is never empty.
* **Round 10 (CIA metadata, banner, banner audio).** CIA metadata: product code `CTR-P-UANG`, unique ID `0x049645` (the old `0x049644` may collide with an installed
  uae3DS), publisher `badda71`, long description "Amiga 500/1200 emulator for the New 3DS (based on uae4all2 and uae3DS)". `meta/banner.cgfx` (spinning 3D banner, the
  HOME menu does the rotating; `BANNER_IMAGE` in the Makefile, bannertool gets `-ci` for a `.cgfx`) and `meta/audio.wav` (2.4 s, 44.1 kHz stereo 16-bit) are supplied
  by badda71; the banner model is credited in README.md. `meta/banner.png` is no longer used by the build. **The banner has not been seen on hardware yet.**
* **Round 11 (default mouse buttons).** Without custom controls, L = left mouse button and R = right mouse button (they were swapped, a Vita convention); the New 3DS
  ZL button is a second right mouse button (`triggerL2`/`triggerR2` are now polled on the 3DS too; both read 0 on an Old 3DS). The default of custom preset 1 follows
  the same convention (`menu_config.cpp`). Saved configs keep their stored mapping. ZL/ZR cannot be assigned in the custom controls menu yet.
* **Round 12 (display redesign).** The 400x240 top screen made Preset Width/Height, Displayed Lines, Vertical Position and Shader pointless, so they are gone from
  the Display menu (3DS only; the Vita/Switch code is untouched). The emulator now always renders the whole PAL window, 320x256 lores (`moveY` 18 = Amiga line 44
  .. 299; `video3ds_fix_window()` forces the old preset/lines/width variables to that, also after loading a config), and the GPU places it (`src/3ds/video3ds.cpp`,
  `video3ds_apply_display()` -> `N3DS_SetScalingDirect`/`N3DS_SetYOffset`, so a change is instant and costs no re-render). *Mode*: **1:1** (default; sharp, the
  middle 240 of the 256 lines are visible), **Fit height** (256 lines = 94 %), **Fit width** (125 %, 192 lines visible), **Custom** (*Zoom* 50..200 %, 100 % = 1:1,
  *Vertical Pan* in screen pixels, 0 = centered, positive = picture lower, limited so that the picture's edge stays on the screen; always centered horizontally).
  Only exactly 100 % is drawn with GPU_NEAREST, everything else smooth. **Start + DPad** adjusts it while playing: left/right = zoom in **1 %** steps, up/down =
  pan 1 px, with auto-repeat that speeds up (nothing for 0.4 s, 12/s, from 1 s 25/s, from 2 s once per frame); a short readout ("Zoom 105%   Pan -3") goes to the
  status bar of the bottom screen; the DPad/circle pad do not reach the game while Start is held. The first adjustment from another mode switches to Custom
  at the zoom that was on the screen (menu and gesture alike). The old Start+R+DPad (vertical position / quick-switch resolutions) is gone on the 3DS; Start+L
  (custom control set) is unchanged. Nothing in the port bound Start to Reset. Config file: three new lines `displaymode=`, `zoom=`, `vpan=` at the end (older
  files do not have them: 1:1, except an old fit-height file with >= 250 lines becomes Fit height); the old positional keys (`shader`, `presetModeId`, `moveX/Y`,
  `displayedLines`, `screenWidth`, `cutLeft/Right`) stay in the file but their values are ignored. The Guru banner is drawn on the first *visible* line
  (`video3ds_visible_top()`); savestate thumbnails are the whole 320x256 window. Not reachable any more: lines outside the PAL window (overscan), hires. Also fixed:
  the "Frameskip" label of the Display menu sat on top of the digits.
* **Round 13 (Guru false positive, menu spacing).** A trackloader game (Shadow of the Beast, hardware log 13:02) "crashed" after minutes of play: the **Guru Meditation**
  banner appeared and the game's fire button (left mouse button) rebooted the Amiga. The game was alive all the time (50 fps, still loading from disk behind the
  banner): it takes over the machine and writes its own data over the exception vectors 08..10 (the same three words `0005164c 00059770 0000190c` on the A500 and A1200 profiles,
  the level-3 vector at 0x6c points to its own handler), which `crashdiag_frame()` took for trashed exec structures. The 8-byte write watch cannot see those
  writes (hence "3 writes" in the log). Now the trash is only logged (as before); the banner needs the *runaway signature* as well: >= 20 illegal-instruction exceptions
  within a window of 50 frames (a runaway CPU with garbage vectors raises thousands, a game that took over raises none). The genuine case (Install reading into a NULL
  buffer, ExecBase pointer -> 0) still shows the banner a frame or two after the trash. The log line `lowmem: N illegal instructions in this window of 50 frames;
  Guru Meditation banner shown / only if it floods` tells which case a log is. Also: the "(in game: Start + DPad = zoom / pan)" hint of the Display menu sits on the
  same 2-row grid as the other items now (it was one row below "Vertical Pan").
* **Round 14 (Shadow of the Beast, second hardware log).** The banner after a real runaway (A1200 profile, `Beast I (a).adf`: `jsr $500.w` into garbage, 126 illegal
  instructions in a second) is correct, but the machine used to run on behind it (garbage interrupts, noise, `DSKDAT written` - a runaway CPU could even write to the
  ADF). The banner now **halts the machine** like a dead-end alert: CPU in STOP with the interrupt mask at 7 (re-applied every frame), audio DMA off; a click still
  reboots. The per-run `config:` line now prints the model of *this* run (`cpu_level N (core was M)`; the old `cpu` value was the previous run's model, which looked
  like a lagging core but is not: `uae4all_reset()` switches the jump table right after the line) and a new `config: DF0 '...' DF1 '...'` line, and every disk
  insertion logs `disk: DFn '<file>' inserted, <size> bytes` (an ADF is 901120 bytes), so a log can be matched to the disks without guessing. What the log shows
  about the game: Disk1 (then Disk2 swapped in through the menu) runs for minutes in the A500 profile; in the A1200 profile it hooks different vectors (it goes the
  OS-friendly way: level-3 vector stays on the ROM handler) and is much more fragile - an old trackloader written for Kickstart 1.x.

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
   in `EXCLUDE_FILES`. (The interim top-screen virtual keyboard `vkbd` was removed in round 5.) Dependencies of the uae3DS code that must be provided/adapted:
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
