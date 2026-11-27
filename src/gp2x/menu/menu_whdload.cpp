/*
 * uae3DSng - "WHDLoad Games" screen (reached from Memory and Disk Options).
 *
 *   - Install game from .lha archive...  : pick an archive, it is extracted into sdmc:/3ds/uae3DSng/whdload/<name>/
 *   - <game>                              : write S/Startup-Sequence, mount the whdload folder as the boot "HD Dir",
 *                                           eject the floppies and reset (the main menu does the reset)
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

#define LIST_ROWS 13                        /* visible entries */
#define MAX_GAMES 256

static char games[MAX_GAMES][WHD_NAME_MAX];
static int n_games = 0;

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

/* Wrapped text in a box; waits for A or B. */
static void whd_message_box(const char *title, const char *text)
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

    SDL_Event ev;
    while (SDL_PollEvent(&ev))
        SDL_Delay(5);
    for (;;) {
        text_draw_background();
        text_draw_window(2, 4, 40, n + 7, title);
        for (int i = 0; i < n; i++)
            write_text(4, 6 + i, lines[i]);
        write_text(4, 7 + n, "Press A or B to continue");
        text_flip();
        SDL_Delay(30);
        int done = 0;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                exit(1);
            if (ev.type == SDL_KEYDOWN &&
                (ev.key.keysym.sym == SDLK_PAGEDOWN || ev.key.keysym.sym == SDLK_END || ev.key.keysym.sym == SDLK_LCTRL))
                done = 1;
        }
        if (done)
            break;
    }
    while (SDL_PollEvent(&ev))
        SDL_Delay(5);
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
    int ok = whd_install_lha(WHD_ROOT, archive, install_progress, &ui, folder, sizeof(folder), &st, err, sizeof(err));
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
             st.bytes / 1048576.0, secs, st.renamed ? "\nSome file names had to be changed (see log)" : "");
    whd_message_box("WHDLoad", msg);
    return find_game(folder);
}

/* ---------------------------------------------------------------------------------------------------------------- */
/* launch                                                                                                           */
/* ---------------------------------------------------------------------------------------------------------------- */

static int launch_game(const char *game)
{
    char err[200];
    if (!whd_prepare_launch(WHD_ROOT, game, WHD_KICKSTART_DIR, NULL, err, sizeof(err))) {
        whd_message_box("Cannot start", err);
        return 0;
    }
    /* the whdload folder is the boot "HD Dir" (the main menu's Reset builds the units from these settings) */
    strncpy(uae4all_hard_dir, WHD_ROOT, 255);
    uae4all_hard_dir[255] = '\0';
    make_hard_dir_cfg_line(uae4all_hard_dir);
    mainMenu_bootHD = 1;                            /* Dir first (the HD files, if any, stay mounted behind it) */
    uae4all_image_file0[0] = uae4all_image_file1[0] = uae4all_image_file2[0] = uae4all_image_file3[0] = '\0';
    resetOnStartingApp = true;                      /* main menu: reset / start right after this menu closes */
    log3ds("whdload: starting '%s' (HD dir '%s', chip %d, slow %d, fast %d, kickstart %d)", game, uae4all_hard_dir,
           mainMenu_chipMemory, mainMenu_slowMemory, mainMenu_fastMemory, kickstart);
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
    write_text(3, line, text_str_memdisk_separator);
    line++;

    if (n_games == 0) {
        write_text(3, line + 1, "No games installed yet.");
        write_text(3, line + 3, "WHDLoad itself is not included:");
        write_text(3, line + 4, "put it into whdload/C/WHDLoad");
        write_text(3, line + 5, "on the SD card (3ds/uae3DSng).");
    } else {
        for (int i = 0; i < LIST_ROWS && top + i < n_games; i++) {
            int idx = top + i + 1;                  /* list item 0 is the install row */
            if (sel == idx && bb)
                write_text_inv(3, line, games[top + i]);
            else
                write_text(3, line, games[top + i]);
            line++;
        }
        line = 3 + 2 + LIST_ROWS;
        char info[64];
        snprintf(info, sizeof(info), "%d game%s", n_games, n_games == 1 ? "" : "s");
        write_text(3, line, info);
    }
    write_text(3, 3 + 2 + LIST_ROWS + 2, "A = install / start     B = back");
    text_flip();
}

/* returns 1: a game was started (close all menus, the main menu resets), 0: back, -1: leave the menu completely */
int run_menuWhdload(void)
{
    refresh_games();
    int sel = 0, top = 0;
    int result = 0;

    SDL_Event ev;
    SDL_Delay(150);
    while (SDL_PollEvent(&ev))
        SDL_Delay(10);

    for (;;) {
        draw_list(sel, top);
        int hit0 = 0, hit1 = 0, hit2 = 0, up = 0, down = 0, left = 0, right = 0;
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
