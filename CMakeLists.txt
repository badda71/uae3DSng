cmake_minimum_required(VERSION 3.0)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(VITASDK "/usr/local/vitasdk")
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR "armv7-a")
set(CMAKE_C_COMPILER "${VITASDK}/bin/arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "${VITASDK}/bin/arm-vita-eabi-g++")

set(CMAKE_C_FLAGS "-Wl,-q" CACHE STRING "C flags")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}" CACHE STRING "C++ flags")

#There are two optional assembler optimizations that were used on the Pandora:
#-DUSE_ARMV7
#(use assembler-optimized "arm_memset" and "arm_memcpy" routines 
#in "gp2x/memset.s" and "gp2x/memcpy.s")
#-DUSE_ARMNEON
#(use assembler-optimized drawing routines for the line drawing of Amiga screens
#in "neon_helper.s")
#Currently, these compile for the Vita but cause a crash on first leaving the menu
#I suspect these have to be re-written for the Vita CPU to make use of asm-optimization
#
#enable_language(ASM)
#set(CMAKE_ASM_COMPILER "${VITASDK}/bin/arm-vita-eabi-g++")
#set(CMAKE_ASM_FLAGS "${CMAKE_CXX_FLAGS}" CACHE STRING "ASM flags")

execute_process(COMMAND "date" "+%Y.%m.%d" OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)

project(uae4all2)
set(TITLE_ID "UAE4ALL00")

set(INC_FILES
        ${VITASDK}/arm-vita-eabi/include
        ${VITASDK}/arm-vita-eabi/include/SDL
        ../common/include
        include
        src
        src/psp2
        src/gp2x
        src/menu
        src/include
        src/gp2x/menu
        src/vkbd
        )

set(FLAGS
        -Wl,-q -O3
        -Wno-write-strings
        -DVERSION=\"${BUILD_DATE}\"
        -DGP2X -DPANDORA -D__PSP2__ -DUSE_AUTOCONFIG -DUSE_ZFILE -DUSE_FAME_CORE -DWITH_TESTMODE
        -DUSE_SDL -DGCCCONSTFUNC="__attribute__\(\(const\)\)" -DUSE_UNDERSCORE
        -DSHM_SUPPORT_LINKS=0 -DUNALIGNED_PROFITABLE -DOPTIMIZED_FLAGS -DOS_WITHOUT_MEMORY_MANAGEMENT
        -DROM_PATH_PREFIX=\"ux0:/data/uae4all/roms/\" -DDATA_PREFIX=\"app0:/data/\" -DSAVE_PREFIX=\"ux0:/data/uae4all/saves/\"
-DUSE_UAE4ALL_VKBD
-DVKBD_ALWAYS
-DLARGEKEYBOARD
#-DUSE_ARMV7
#-DUSE_ARMNEON 
        -fomit-frame-pointer -Wno-unused -Wno-format
        -ffast-math -fstrict-aliasing -mstructure-size-boundary=32 -fexpensive-optimizations
        -fweb -frename-registers -fomit-frame-pointer
        -falign-functions=32 -finline -finline-functions -fno-builtin
        -march=armv7-a -mcpu=cortex-a9 -mtune=cortex-a9 -mfloat-abi=hard -mfpu=neon
        )

set(DEBUG_FLAGS
        -Wl,-q
        -Wno-write-strings
        -DVERSION=\"${BUILD_DATE}\"
        -DGP2X -DPANDORA -D__PSP2__ -DUSE_AUTOCONFIG -DUSE_ZFILE -DUSE_FAME_CORE -DWITH_TESTMODE
        -DUSE_SDL -DGCCCONSTFUNC="__attribute__\(\(const\)\)" -DUSE_UNDERSCORE
        -DSHM_SUPPORT_LINKS=0 -DUNALIGNED_PROFITABLE -DOPTIMIZED_FLAGS -DOS_WITHOUT_MEMORY_MANAGEMENT
        -DROM_PATH_PREFIX=\"ux0:/data/uae4all/roms/\" -DDATA_PREFIX=\"app0:/data/\" -DSAVE_PREFIX=\"ux0:/data/uae4all/saves/\"
-DUSE_UAE4ALL_VKBD
-DVKBD_ALWAYS
-DLARGEKEYBOARD
        -DDEBUG -DDEBUG_UAE4ALL -DUAE_CONSOLE
        )

set(SRC_FILES
        src/psp2/psp2_shader.cpp
        src/psp2/psp2_input.c
        src/psp2/psp2-dirent.c
        src/vkbd/vkbd.cpp
#       src/neon_helper.s
#       src/gp2x/memset.s
#       src/gp2x/memcpy.s
        src/audio.cpp
        src/autoconf.cpp
        src/blitfunc.cpp
        src/blittable.cpp
        src/blitter.cpp
        src/cfgfile.cpp
        src/cia.cpp
        src/savedisk.cpp
        src/savestate.cpp
        src/custom.cpp
        src/disk.cpp
        src/drawing.cpp
        src/ersatz.cpp
        src/expansion.cpp
        src/filesys.cpp
        src/fsdb.cpp
        src/fsdb_unix.cpp
        src/fsusage.cpp
        src/gfxutil.cpp
        src/hardfile.cpp
        src/keybuf.cpp
        src/main.cpp
        src/memory.cpp
        src/missing.cpp
        src/native2amiga.cpp
        src/gui.cpp
        src/od-joy.cpp
        src/scsi-none.cpp
        src/sdlgfx.cpp
        src/writelog.cpp
        src/zfile.cpp
        src/menu/fade.cpp
        src/gp2x/gp2x.cpp
        src/gp2x/inputmode.cpp
        src/gp2x/menu/menu_helper.cpp
        src/gp2x/menu/menu_config.cpp
        src/gp2x/menu/menu.cpp
        src/gp2x/menu/menu_fileinfo.cpp
        src/gp2x/menu/menu_load.cpp
        src/gp2x/menu/menu_main.cpp
        src/gp2x/menu/menu_savestates.cpp
        src/gp2x/menu/menu_misc.cpp
        src/gp2x/menu/menu_controls.cpp
        src/gp2x/menu/menu_display.cpp
        src/gp2x/menu/menu_memory_disk.cpp
        src/sound_sdl_new.cpp
        src/m68k/fame/famec.cpp
        src/m68k/fame/m68k_intrf.cpp
        src/m68k/m68k_cmn_intrf.cpp
        )

add_executable(${PROJECT_NAME}.elf ${SRC_FILES})
target_compile_options(${PROJECT_NAME}.elf PRIVATE ${FLAGS})
target_include_directories(${PROJECT_NAME}.elf PRIVATE ${INC_FILES})
target_link_libraries(${PROJECT_NAME}.elf
        -Wl,-q
        SDL SDL_image vita2d
        ${CMAKE_SOURCE_DIR}/src/psp2/vita-shader-collection/lib/libvitashaders.a
        SceSysmodule_stub SceDisplay_stub SceGxm_stub
        SceCtrl_stub ScePgf_stub ScePower_stub SceCommonDialog_stub
        SceAudio_stub SceShellSvc_stub png jpeg z m c
        )
add_custom_target(${PROJECT_NAME}.vpk
        DEPENDS ${PROJECT_NAME}.elf
        COMMAND rm -rf vpk && mkdir -p vpk/sce_sys
        COMMAND ${VITASDK}/bin/vita-elf-create ${PROJECT_NAME}.elf ${PROJECT_NAME}.velf
        COMMAND ${VITASDK}/bin/vita-make-fself -s -c ${PROJECT_NAME}.velf vpk/eboot.bin
        COMMAND ${VITASDK}/bin/vita-mksfoex -s TITLE_ID=${TITLE_ID} "${PROJECT_NAME}" vpk/sce_sys/param.sfo
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/psp2data/* vpk
        COMMAND cd vpk && zip -r ../uae4all-${BUILD_DATE}.vpk . && cd ..
        )

add_executable(${PROJECT_NAME}-debug.elf ${SRC_FILES})
target_compile_options(${PROJECT_NAME}-debug.elf PRIVATE ${DEBUG_FLAGS})
target_include_directories(${PROJECT_NAME}-debug.elf PRIVATE ${INC_FILES})
target_link_libraries(${PROJECT_NAME}-debug.elf
        -Wl,-q
        SDL SDL_image vita2d
        ${CMAKE_SOURCE_DIR}/src/psp2/vita-shader-collection/lib/libvitashaders.a
        SceSysmodule_stub SceDisplay_stub SceGxm_stub
        SceCtrl_stub ScePgf_stub ScePower_stub SceCommonDialog_stub
        SceAudio_stub png jpeg z m c
        psp2shell SceSysmodule_stub SceNetCtl_stub SceNet_stub SceAppUtil_stub SceAppMgr_stub SceShellSvc_stub
        )
add_custom_target(${PROJECT_NAME}-debug.vpk
        DEPENDS ${PROJECT_NAME}-debug.elf
        COMMAND rm -rf vpk && mkdir -p vpk/sce_sys
        COMMAND ${VITASDK}/bin/vita-elf-create ${PROJECT_NAME}-debug.elf ${PROJECT_NAME}-debug.velf
        COMMAND ${VITASDK}/bin/vita-make-fself -c ${PROJECT_NAME}-debug.velf vpk/eboot.bin
        COMMAND ${VITASDK}/bin/vita-mksfoex -s TITLE_ID=${TITLE_ID} "${PROJECT_NAME}" vpk/sce_sys/param.sfo
        COMMAND cp -r ${CMAKE_SOURCE_DIR}/psp2data/* vpk
        COMMAND cd vpk && zip -r ../uae4all-debug-${BUILD_DATE}.vpk . && cd ..
        )
