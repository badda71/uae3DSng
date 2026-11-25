# Debug-only changes (to be removed once everything works)

This file lists every change in the repository that exists **only to find a problem**, with where it lives and how it is
removed. Rule: whoever adds debug code adds an entry here in the same commit. When the emulator works as wanted, the clean-up
is done from this list (and `grep` for the markers at the end), then `PORTING.md` rounds 8-16 are shortened to what is still true.

Legend for the *Removing it* column: **delete** = nothing else depends on it; **check** = other code calls it, remove the callers too.

## A. Pure diagnostics (no effect on the emulated machine)

| # | What | Where | Removing it |
|---|------|-------|-------------|
| A1 | Log file on the SD card (`uae3DSng.log`, `stdout.log`), lazy sync, `log3ds_mem`, `log3ds_linear_free_kb`; `write_log` mapped to it | `src/3ds/log3ds.{cpp,h}`, `src/include/sysdeps.h` (`#define write_log log3ds`), `log3ds_tick()` call in `sdlgfx.cpp`, `log3ds_lazy()` calls in `main.cpp` / `menu.cpp` | **decide at clean-up**: keep a reduced log (start-up steps, errors) or turn `write_log` into a no-op as upstream does |
| A2 | Start-up / shutdown breadcrumbs (`step: ...`, `leave: ...`), build date + flavour line, `log3ds_mem(...)` calls | `src/main.cpp` (`real_main`, `leave_program`) | delete the `write_log` lines |
| A3 | `m68k_go` breadcrumbs and the per-run `config:` lines (chip/slow/fast, cpu_level, chipset, CPU speed, DF0/DF1) | `src/m68k/m68k_cmn_intrf.cpp` (`m68k_go`) | delete the `write_log` lines; keep the `crashdiag_reset()` call only while A8 exists |
| A4 | Speed line every 5 s (fps, spare, emu/draw/flip split, worst frame, frames > 35 ms, disk reads, free memory) and the first 5 `frame n:` lines | `src/sdlgfx.cpp` (frame limiter block), counters `dbg_draw_us`/`dbg_draw_max` in `src/drawing.cpp` (`finish_drawing_frame`), `disk_io_us`/`disk_io_calls` in `src/disk.cpp` (`read_floppy_data`) | **check**: remove the block in `sdlgfx.cpp`, then the three counters |
| A5 | Menu / display logs: `menu: N flips ...`, `init_text: SetVideoMode ...`, `update_display: ...` | `src/gp2x/menu/menu.cpp`, `src/gp2x/menu/menu_helper.cpp` | delete the `write_log` lines (and the timing variables next to them) |
| A6 | Disk logging: `disk: DFn '...' inserted`, `DISK_dump_state()`, disk event ring (`dt()`, `dt_ring`, `dt_*` counters, events T/P/M/D/W/F/A) | `src/disk.cpp` (`#ifdef __3DS__` blocks: after `static drive floppy[4]`, in `drive_step`, `drive_motor`, `drive_fill_bigbuf`, `disk_dmafinished`, `DISK_handler`, `DSKLEN`, `DISK_reset`, `DISK_dump_state`) | delete the blocks; the `#else` branch defines `dt()` as nothing, so a missing `#ifdef` shows up at compile time |
| A7 | Native trap history and `TRAP:` logging | `src/autoconf.cpp` (`trap_hist`, `autoconf_dump_traphist`, trap logging in `call_calltrap`) | **check**: called from `dump_cpu_context` and `cd_wp_hit` (A8) |
| A8 | Crash diagnostics: `dump_cpu_context`, `crashdiag_*`, `cd_*` helpers, per-frame check for trashed exec memory, illegal-instruction throttling and counting, write watch on chip offset 0 (`cd_wp_*`, `crashdiag_watch_page`), DOS packet finder, memory dumps, PC ring, `crashdiag_snapshot()` | `src/m68k/fame/m68k_intrf.cpp` (the `#ifdef __3DS__` blocks and the `__3DS__` branch in `uae_chk_handler`); callers: `crashdiag_frame()` in `sdlgfx.cpp`, `crashdiag_reset()` in `m68k_cmn_intrf.cpp`, `crashdiag_snapshot()` in `gui.cpp` (`goMenu`), `crashdiag_mem` / `crashdiag_largest_chip_kb` in `sdlgfx.cpp`, `crashdiag_watch_page()` in `map_zone()` (same file); snapshot additions: `cd_dump_dos_list`, `cd_snapshot_packets`, `cd_bstr`; `crashdiag_exception()` (logs bus/address error, divide by zero, CHK, TRAPV) called from `execute_exception()` in `src/m68k/fame/famec.cpp` (3 lines under `#ifdef __3DS__`) | **check**: remove the callers first; keep the Guru/halt part if C1 is kept |
| A9 | Hardfile request history and counters (`hf_hist`, `hardfile_dump_history`, `hardfile_request_count`) | `src/hardfile.cpp` | **check**: used by A8 |
| A10 | CI: crash address lookup step and the address list | `.github/workflows/build.yml` (step "Resolve crash addresses"), `tools/crashaddrs.txt` | delete both |
| A11 | Makefile note "this is NOT a release build" | `Makefile` (last `@$(if $(filter release...` line) | optional, harmless |
| A12 | PORTING.md notes of the debugging rounds (8-16) | `PORTING.md` | shorten at clean-up |

## B. Diagnostics that touch the emulated machine (remove with extra care, they change behaviour or speed)

| # | What | Effect while present | Where |
|---|------|----------------------|-------|
| B1 | **AllocMem / AllocVec trace**: two guest stubs are patched into exec's jump table on every boot (`filesys_diagentry`); a native trap logs failed or large (>= 150000 bytes) allocations with the running task | every allocation of the Amiga goes through a stub; guest code that inspects the vector sees the stub | `src/filesys.cpp` (`alloctrace_*`, `alloctrace_install`, `alloctrace_emit`, call in `filesys_diagentry`), trap name `alloctrace` in `src/autoconf.cpp` (excluded from the trap history) |
| B2 | **Write watch on chip RAM page 0** (and the chip mirror): write handlers replace the direct pointer for those 64 KB pages | writes to the first 64 KB of chip RAM are slower (exec structures, vectors) | `famec_SetWriteHandlers` in `src/m68k/fame/famec.cpp` + `fame.h`; `cd_wp_*` and the `map_zone()` hook in `m68k_intrf.cpp` |
| B3 | Illegal-instruction logging throttle and counting | none on the machine, only on the log | `uae_chk_handler` in `m68k_intrf.cpp` |

## C. Features that grew out of the debugging (keep unless you say otherwise)

| # | What | Where |
|---|------|-------|
| C1 | **Guru Meditation banner** on the main screen for a runaway CPU, and the halt of the emulated machine behind it (`cd_halt_machine`: SR interrupt mask 7, HALTED flag, audio DMA off). The banner replaced an earlier bottom-screen notice. | `src/3ds/guru.{cpp,h}`, `m68k_intrf.cpp` (`cd_check_runaway`, `cd_halt_machine`, `crashdiag_frame`) |

## D. Fixes found while debugging: NOT debug-only, must stay

* `chipmem_{l,w,b}{get,put}` / `check` / `xlate` always mask the address (`src/memory.cpp`; `SAFE_MEMORY_ACCESS` was never defined and the 1 MB chip mirror ran past the host buffer).
* `SDL_OpenAudio` result is checked and the sound buffer wait has a timeout (`src/sound_sdl_new.cpp`).
* `uaehf.device` error codes and the check of the hardfile `fread`/`fwrite` results (`src/hardfile.cpp`).
* SDL 3DS driver frees the previous spritesheet texture, semaphore and event on every `SetVideoMode` (`UAE3DS_SDL/src/video/n3ds/SDL_n3dsvideo.c`; leaked 2.3 MB per menu trip).
* Bounded catch-up in the frame limiter, 3DS audio through ndsp, log writes no longer stall the emulation during file access.
* `cd_long()` and friends read Amiga memory through FAME's map (only needed while A8 exists, but A8 must never use `get_long()` for this).

## Markers for the final `grep`

```
grep -rn "crashdiag\|alloctrace\|dt (\|DISK_dump_state\|autoconf_dump_traphist\|hardfile_dump_history\|dbg_draw\|disk_io_\|log3ds" src
grep -rn "famec_SetWriteHandlers" src
```

## Change log of this file

* 2026-11-25: file created; lists everything up to commit `5638344` (snapshot v2 with the disk event ring).
* 2026-11-25: snapshot v3: DOS device list walk with cycle detection, pending DOS packets of the handlers, BCPL frame strings, exception log (`crashdiag_exception`, hook in `famec.cpp`).
