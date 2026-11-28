/*
 * uae3DSng - "WHDLoad Games" screen (reached from Memory and Disk Options).
 *
 *   - Install game from .lha archive...  : pick an archive, it is installed into ONE Amiga disk image
 *                                           sdmc:/3ds/uae3DSng/whdload/<name>.hdf (hundreds of small files are far
 *                                           too slow on the 3DS SD card)
 *   - <game>                              : write S/Startup-Sequence, mount the whdload folder as the boot "HD Dir"
 *                                           and the image as a hard file behind it, eject the floppies and reset
 *                                           (the main menu does the reset)
 *   - X on a <game>                       : uninstall it (asks first): deletes the image and its info file
 *
 * The work itself is in src/3ds/whdload3ds.cpp (tested on a PC), this file is only the user interface.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"
#include "menu_config.h"

#include <SDL.h>
#include "options.h"
#include "gui.h"
#include "sound.h"
#include "cfgfile.h"
#include "whdload3ds.h"
#include "log3ds.h"

#ifdef __3DS__
#define SDL_PollEvent N3DS_PollEvent
#endif

char whdload_lha_path[300] = "";            /* set by the file selector (MENU_LOAD_LHA) */

extern char currentDir[300];
extern int emulating;
extern int quit_pressed_in_submenu;
extern bool resetOnStartingApp;
extern SDL_Surface *text_screen;
extern int kickstart;
extern const char *text_str_memdisk_separator;

#define LIST_ROWS 12                        /* visible entries; like in all menus, items are two text lines apart */
#define LIST_FIRST_LINE 5                   /* text line of the first game (the install row is line 3, the separator line 4) */
#define MAX_GAMES 256

static char games[MAX_GAMES][WHD_NAME_MAX];
static int n_games = 0;
static int have_builtin_whdload = 0;        /* a WHDLoad ships in the romfs (the SD card copy, if any, is preferred) */

static void refresh_games(void)
{
    n_games = whd_list_games(WHD_ROOT, games, MAX_GAMES);
}

static int find_game(const char *name)
{
    for (int i = 0; i < n_games; i++)
        if (strcmp(games[i], name) == 0)
            return i;
    return -1;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* messages                                                                                                         */
/* ---------------------------------------------------------------------------------------------------------------- */

/* Wrapped text in a box. A message waits for A or B. A question (ask = 1) returns 1 for A (yes) and 0 for B (no). */
static int whd_box(const char *title, const char *text, int ask)
{
    const int width = 34;
    char lines[10][48];
    int n = 0;
    const char *p = text;
    while (*p && n < 10) {
        while (*p == ' ')
            p++;
        int len = (int)strlen(p);
        int take = len > width ? width : len;
        if (len > width) {                          /* break at the last space that fits, else hard */
            int cut = take;
            while (cut > 0 && p[cut] != ' ')
                cut--;
            if (cut > 0)
                take = cut;
        }
        const char *nl = (const char *)memchr(p, '\n', take);
        if (nl)
            take = (int)(nl - p);
        memcpy(lines[n], p, take);
        lines[n][take] = '\0';
        n++;
        p += take;
        if (*p == '\n')
            p++;
    }

    int answer = 0;
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
        SDL_Delay(5);
    for (;;) {
        text_draw_background();
        text_draw_window(2, 4, 40, n + 7, title);
        for (int i = 0; i < n; i++)
            write_text(4, 6 + i, lines[i]);
        write_text(4, 7 + n, ask ? "A = yes     B = no" : "Press A or B to continue");
        text_flip();
        SDL_Delay(30);
        int done = 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                exit(1);
            if (ev.type == SDL_KEYDOWN &&
                (ev.key.keysym.sym == SDLK_PAGEDOWN || ev.key.keysym.sym == SDLK_END || ev.key.keysym.sym == SDLK_LCTRL)) {
                if (!done)
                    answer = ev.key.keysym.sym == SDLK_PAGEDOWN;
                done = 1;
            }
        }
        if (done)
            break;
    }
    while (SDL_PollEvent(&ev))
        SDL_Delay(5);
    return ask ? answer : 0;
}

static void whd_message_box(const char *title, const char *text)
{
    whd_box(title, text, 0);
}

static int whd_confirm_box(const char *title, const char *text)
{
    return whd_box(title, text, 1);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* install                                                                                                          */
/* ---------------------------------------------------------------------------------------------------------------- */

typedef struct {
    Uint32 last_draw;
    char archive[WHD_NAME_MAX];
} InstallUi;

static void draw_progress(const InstallUi *ui, const char *item, unsigned done, unsigned total, uint64_t bd, uint64_t bt)
{
    char line[80];
    text_draw_background();
    text_draw_window(2, 2, 40, 14, "Installing WHDLoad game");
    write_text(3, 4, ui->archive);
    write_text(3, 6, item ? item : "");
    snprintf(line, sizeof(line), "%u / %u files   %u / %u KB", done, total, (unsigned)(bd >> 10), (unsigned)(bt >> 10));
    write_text(3, 8, line);
    int max = (int)(bt >> 10);
    if (max < 1)
        max = 1;
    text_draw_barra(MENU_X0 + 3 * MENU_CW, 10 * MENU_RH, 34 * MENU_CW, 8, (int)(bd >> 10), max);
    write_text(3, 13, "B = cancel");
    text_flip();
}

static int install_progress(void *user, const char *item, unsigned done, unsigned total, uint64_t bd, uint64_t bt)
{
    InstallUi *ui = (InstallUi *)user;
    int cancel = 0;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT)
            cancel = 1;
        else if (ev.type == SDL_KEYDOWN && (ev.key.keysym.sym == SDLK_END || ev.key.keysym.sym == SDLK_LCTRL))
            cancel = 1;
    }
    Uint32 now = SDL_GetTicks();
    if (now - ui->last_draw >= 150 || done == total) {
        ui->last_draw = now;
        draw_progress(ui, item, done, total, bd, bt);
    }
    return cancel;
}

/* returns the index of the installed game in games[] (or -1) */
static int install_from_archive(const char *archive)
{
    InstallUi ui;
    ui.last_draw = 0;
    const char *base = strrchr(archive, '/');
    snprintf(ui.archive, sizeof(ui.archive), "%s", base ? base + 1 : archive);
    draw_progress(&ui, "reading the archive...", 0, 0, 0, 0);

    char folder[WHD_NAME_MAX], err[200];
    whd_install_stats st;
    int ok = whd_install_lha(WHD_ROOT, archive, WHD_INSTALL_IMAGE, install_progress, &ui, folder, sizeof(folder), &st,
                             err, sizeof(err));
    refresh_games();
    if (!ok) {
        char msg[300];
        snprintf(msg, sizeof(msg), "Not installed: %s", err);
        whd_message_box("WHDLoad", msg);
        return -1;
    }
    char msg[300];
    double secs = st.elapsed_ms / 1000.0;
    snprintf(msg, sizeof(msg), "Installed \"%s\"\n%u files, %.1f MB in %.1f s%s", folder, st.files,
             st.bytes / 1048576.0, secs, st.renamed ? "\nSome long names were shortened (see log)" : "");
    whd_message_box("WHDLoad", msg);
    return find_game(folder);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* launch                                                                                                           */
/* ---------------------------------------------------------------------------------------------------------------- */

static int launch_game(const char *game)
{
    char err[200];
    whd_launch_info info;
    if (!whd_prepare_launch(WHD_ROOT, game, WHD_KICKSTART_DIR, NULL, &info, err, sizeof(err))) {
        whd_message_box("Cannot start", err);
        return 0;
    }

    /* The game's disk image becomes hard file 0. The setting holds the geometry spec in front of the path
       ("32:1:2:512:<path>", the same as the HDF selection in the HD menu makes), which has to fit into 255 bytes. */
    char hardfile[256];
    hardfile[0] = '\0';
    if (info.hdf_path[0]) {
        if (strlen(info.hdf_path) + 16 > sizeof(hardfile)) {
            whd_message_box("Cannot start", "The path of the game image is too long.");
            return 0;
        }
        snprintf(hardfile, sizeof(hardfile), "%s", info.hdf_path);
        if (!make_hard_file_cfg_line(hardfile)) {
            whd_message_box("Cannot start", "Cannot open the game image.");
            return 0;
        }
    }

    /* the whdload folder is the boot "HD Dir" (the main menu's Reset builds the units from these settings) */
    strncpy(uae4all_hard_dir, WHD_ROOT, 255);
    uae4all_hard_dir[255] = '\0';
    make_hard_dir_cfg_line(uae4all_hard_dir);
    if (hardfile[0]) {
        memcpy(uae4all_hard_file0, hardfile, sizeof(hardfile));
    } else if (strstr(uae4all_hard_file0, WHD_ROOT)) {
        uae4all_hard_file0[0] = '\0';                  /* the image of the game started before must not stay mounted */
    }
    mainMenu_bootHD = 1;                            /* Dir first, then the hard files: the folder boots, the image is "WHDGame:" */
    uae4all_image_file0[0] = uae4all_image_file1[0] = uae4all_image_file2[0] = uae4all_image_file3[0] = '\0';
    resetOnStartingApp = true;                      /* main menu: reset / start right after this menu closes */
    log3ds("whdload: starting '%s' (HD dir '%s', hard file 0 '%s', chip %d, slow %d, fast %d, kickstart %d)", game,
           uae4all_hard_dir, uae4all_hard_file0, mainMenu_chipMemory, mainMenu_slowMemory, mainMenu_fastMemory,
           kickstart);
    return 1;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* uninstall                                                                                                        */
/* ---------------------------------------------------------------------------------------------------------------- */

typedef struct {
    Uint32 last_draw;
    char game[WHD_NAME_MAX];
} DeleteUi;

static void draw_delete(const DeleteUi *ui, unsigned files)
{
    char line[64];
    text_draw_background();
    text_draw_window(2, 2, 40, 10, "Deleting WHDLoad game");
    write_text(3, 4, ui->game);
    snprintf(line, sizeof(line), "%u files deleted", files);
    write_text(3, 6, line);
    text_flip();
}

static void delete_progress(void *user, unsigned files)
{
    DeleteUi *ui = (DeleteUi *)user;
    SDL_Event ev;
    while (SDL_PollEvent(&ev))                      /* keep the event queue from filling up; nothing to cancel here */
        ;
    Uint32 now = SDL_GetTicks();
    if (now - ui->last_draw >= 150) {
        ui->last_draw = now;
        draw_delete(ui, files);
    }
}

/* is the image of this game one of the hard files (of a running emulator, or of the saved settings)? */
static int hard_file_is_game(const char *spec, const char *hdf_path)
{
    return spec[0] && strstr(spec, hdf_path) != NULL;
}

/* returns 1 when the game is gone */
static int uninstall_game(const char *game)
{
    char hdf_path[300];
    snprintf(hdf_path, sizeof(hdf_path), "%s/%s.hdf", WHD_ROOT, game);
    char *const specs[4] = { uae4all_hard_file0, uae4all_hard_file1, uae4all_hard_file2, uae4all_hard_file3 };
    int mounted = 0;
    for (int i = 0; i < 4; i++)
        if (hard_file_is_game(specs[i], hdf_path))
            mounted = 1;
    if (mounted && emulating) {                     /* the emulator has the file open: deleting it under its feet is not safe */
        whd_message_box("Cannot delete",
                        "This game is loaded in the running emulator.\nStart another game (or eject the hard disk and "
                        "reset the emulator), then delete it.");
        return 0;
    }

    char text[200];
    snprintf(text, sizeof(text), "Delete \"%s\"?\nThe game and everything saved inside its disk image is removed.", game);
    if (!whd_confirm_box("Uninstall", text))
        return 0;

    DeleteUi ui;
    ui.last_draw = 0;
    snprintf(ui.game, sizeof(ui.game), "%s", game);
    draw_delete(&ui, 0);
    char err[200];
    int ok = whd_uninstall(WHD_ROOT, game, delete_progress, &ui, err, sizeof(err));
    refresh_games();
    if (!ok) {
        char msg[300];
        snprintf(msg, sizeof(msg), "Not deleted: %s", err);
        whd_message_box("WHDLoad", msg);
        return 0;
    }
    for (int i = 0; i < 4; i++)                     /* a hard file setting that points to the deleted image would not mount */
        if (hard_file_is_game(specs[i], hdf_path))
            specs[i][0] = '\0';
    return 1;
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* the list                                                                                                         */
/* ---------------------------------------------------------------------------------------------------------------- */

static void draw_list(int sel, int top)
{
    static int b = 0;
    int bb = (b % 6) / 3;
    b++;

    text_draw_background();
    text_draw_window(2, 2, 40, 30, "WHDLoad Games");

    int line = 3;
    if (sel == 0 && bb)
        write_text_inv(3, line, "Install game from .lha archive...");
    else
        write_text(3, line, "Install game from .lha archive...");
    line++;
    write_text(3, line, text_str_memdisk_separator);        /* the only thing that may sit between two items */
    line = LIST_FIRST_LINE;

    if (n_games == 0) {
        write_text(3, line, "No games installed yet.");
        if (have_builtin_whdload) {
            write_text(3, line + 2, "A WHDLoad is built in. To use your");
            write_text(3, line + 3, "own, put it into whdload/C/WHDLoad");
            write_text(3, line + 4, "on the SD card (3ds/uae3DSng).");
        } else {
            write_text(3, line + 2, "WHDLoad itself is not included:");
            write_text(3, line + 3, "put it into whdload/C/WHDLoad");
            write_text(3, line + 4, "on the SD card (3ds/uae3DSng).");
        }
    } else {
        for (int i = 0; i < LIST_ROWS && top + i < n_games; i++) {
            int idx = top + i + 1;                  /* list item 0 is the install row */
            if (sel == idx && bb)
                write_text_inv(3, line, games[top + i]);
            else
                write_text(3, line, games[top + i]);
            line += 2;
        }
        line = LIST_FIRST_LINE + 2 * LIST_ROWS;
        char info[64];
        snprintf(info, sizeof(info), "%d game%s", n_games, n_games == 1 ? "" : "s");
        write_text(3, line, info);
    }
    write_text(3, LIST_FIRST_LINE + 2 * LIST_ROWS + 2,
               (sel == 0 || n_games == 0) ? "A = install              B = back" : "A = start    X = delete    B = back");
    text_flip();
}

/* returns 1: a game was started (close all menus, the main menu resets), 0: back, -1: leave the menu completely */
int run_menuWhdload(void)
{
    refresh_games();
    {
        FILE *f = whd_builtin_whdload ? fopen(whd_builtin_whdload, "rb") : NULL;
        have_builtin_whdload = f != NULL;
        if (f)
            fclose(f);
    }
    int sel = 0, top = 0;
    int result = 0;

    SDL_Event ev;
    SDL_Delay(150);
    while (SDL_PollEvent(&ev))
        SDL_Delay(10);

    for (;;) {
        draw_list(sel, top);
        int hit0 = 0, hit1 = 0, hit2 = 0, del = 0, up = 0, down = 0, left = 0, right = 0;
        while (SDL_PollEvent(&ev) > 0) {
            if (ev.type == SDL_QUIT)
                hit1 = 1;
            else if (ev.type == SDL_KEYDOWN) {
                uae4all_play_click();
                switch (ev.key.keysym.sym) {
                case SDLK_UP:       up = 1; break;
                case SDLK_DOWN:     down = 1; break;
                case SDLK_LEFT:     left = 1; break;
                case SDLK_RIGHT:    right = 1; break;
                case SDLK_PAGEDOWN: hit0 = 1; break;
                case SDLK_END:      hit1 = 1; break;
                case SDLK_LCTRL:    hit2 = 1; break;
                case SDLK_DELETE: case SDLK_BACKSPACE: case SDLK_ESCAPE: case SDLK_PAGEUP: del = 1; break;     /* X, as for "eject HD" */
                default: break;
                }
            }
        }
        int items = n_games + 1;
        if (up)
            sel = sel > 0 ? sel - 1 : items - 1;
        if (down)
            sel = sel < items - 1 ? sel + 1 : 0;
        if (left)
            sel = sel > 10 ? sel - 10 : 0;
        if (right)
            sel = sel + 10 < items ? sel + 10 : items - 1;
        /* keep the selected game inside the visible part of the list */
        if (sel >= 1) {
            if (sel - 1 < top)
                top = sel - 1;
            if (sel - 1 >= top + LIST_ROWS)
                top = sel - LIST_ROWS;
        } else {
            top = 0;
        }

        if (del && sel >= 1 && sel - 1 < n_games) {
            if (uninstall_game(games[sel - 1])) {
                int items_now = n_games + 1;
                if (sel >= items_now)
                    sel = items_now - 1;
                if (top > 0 && top + LIST_ROWS > n_games)
                    top = n_games > LIST_ROWS ? n_games - LIST_ROWS : 0;
            }
            continue;                               /* the key events of this round are used up */
        }
        if (hit2 && emulating) {
            quit_pressed_in_submenu = 1;
            result = -1;
            break;
        }
        if (hit1) {
            result = 0;
            break;
        }
        if (hit0) {
            if (sel == 0) {
                whdload_lha_path[0] = '\0';
                if (run_menuLoad(currentDir, MENU_LOAD_LHA) && whdload_lha_path[0]) {
                    int idx = install_from_archive(whdload_lha_path);
                    if (idx >= 0) {
                        sel = idx + 1;
                        top = idx > LIST_ROWS - 1 ? idx - LIST_ROWS + 1 : 0;
                    }
                }
                while (SDL_PollEvent(&ev))
                    SDL_Delay(5);
            } else if (sel - 1 < n_games) {
                if (launch_game(games[sel - 1])) {
                    result = 1;
                    break;
                }
            }
        }
    }
    while (SDL_PollEvent(&ev))                      /* swallow the key release of the button that closed us */
        SDL_Delay(5);
    return result;
}
