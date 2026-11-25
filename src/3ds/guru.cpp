/*
 * uae3DSng - "Guru Meditation" banner on the emulated (top) screen, see guru.h.
 *
 * The banner is rendered once per screen width into a surface of the screen's pixel format (the 8x8 menu font
 * from romfs, scaled by an integer factor) and blitted over the emulated picture every frame, followed by the
 * flashing border. Nothing here touches the emulated machine.
 */
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "guru.h"
#include "uibottom.h"
#include "log3ds.h"

extern SDL_Surface *prSDLScreen;
extern int buttonstate[3];
extern void uae_reset(void);

static int g_on;
static unsigned g_code, g_task;
static Uint32 g_since;
static int g_prev_button;
static SDL_Surface *g_banner;
static int g_w, g_k;

void guru_hide(void)
{
	g_on = 0;
	if (g_banner) { SDL_FreeSurface(g_banner); g_banner = NULL; }
}

void guru_show(unsigned code, unsigned task)
{
	g_code = code;
	g_task = task;
	g_since = SDL_GetTicks();
	g_prev_button = 1;          /* a button that is already down must be released and pressed again */
	g_on = 1;
	log3ds("guru: Guru Meditation #%08X.%08X shown on the main screen\n", code, task);
}

/* integer scale of the 8x8 font: the longest line has 36 characters */
static int pick_scale(int w)
{
	int k = 1;
	while (k < 3 && 36 * 8 * (k + 1) + 24 <= w) k++;
	return k;
}

static void build_banner(void)
{
	SDL_Surface *scr = prSDLScreen, *font = uib_font();
	int w = scr->w, k = pick_scale(w), i, n;
	int bpp = scr->format->BytesPerPixel;
	char line[3][48];
	int h = 38 * k;
	SDL_PixelFormat *f = scr->format;

	if (g_banner) { SDL_FreeSurface(g_banner); g_banner = NULL; }
	if (h > scr->h) h = scr->h;
	g_banner = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, f->Amask);
	if (!g_banner) return;
	g_w = w;
	g_k = k;
	SDL_FillRect(g_banner, NULL, SDL_MapRGB(g_banner->format, 0, 0, 0));
	if (!font || bpp != 2) return;     /* no font (bottom UI not up): the box and the flashing border still show */

	snprintf(line[0], sizeof line[0], "Software Failure.");
	snprintf(line[1], sizeof line[1], "Press left mouse button to continue.");
	snprintf(line[2], sizeof line[2], "Guru Meditation #%08X.%08X", g_code, g_task);

	/* glyph cell in the screen's format: font bit 1 -> red, colour key 0 -> stays black */
	SDL_Surface *cell = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, f->Amask);
	if (!cell) return;
	SDL_Color red = {0xff, 0x20, 0x00, 0};
	SDL_SetPalette(font, SDL_LOGPAL, &red, 1, 1);
	Uint16 *dst = (Uint16 *)g_banner->pixels;
	int dpitch = g_banner->pitch / 2;
	Uint16 on = (Uint16)SDL_MapRGB(g_banner->format, red.r, red.g, red.b);
	for (n = 0; n < 3; n++) {
		int len = (int)strlen(line[n]);
		int x0 = (w - len * 8 * k) / 2;                   /* centred */
		int y0 = (2 + 3) * k + n * 10 * k;                /* border + padding, then 8 rows + 2 gap per line */
		for (i = 0; i < len; i++) {
			int c = (line[n][i] & 0x7f) - 32;
			if (c < 0) c = 0;
			SDL_Rect from = { (Sint16)((c & 0x0f) * 8), (Sint16)((c >> 4) * 8), 8, 8 };
			SDL_FillRect(cell, NULL, 0);
			SDL_BlitSurface(font, &from, cell, NULL);
			const Uint16 *src = (const Uint16 *)cell->pixels;
			int spitch = cell->pitch / 2, gx, gy, sx, sy;
			for (gy = 0; gy < 8; gy++)
				for (gx = 0; gx < 8; gx++) {
					if (!src[gy * spitch + gx]) continue;
					for (sy = 0; sy < k; sy++)
						for (sx = 0; sx < k; sx++) {
							int px = x0 + (i * 8 + gx) * k + sx, py = y0 + gy * k + sy;
							if (px >= 0 && px < w && py >= 0 && py < h) dst[py * dpitch + px] = on;
						}
				}
		}
	}
	SDL_FreeSurface(cell);
}

void guru_draw(void)
{
	SDL_Surface *scr = prSDLScreen;
	if (!g_on || !scr) return;
	if (!g_banner || g_w != scr->w) build_banner();
	if (!g_banner) return;

	/* the picture may be zoomed / panned so that its first lines are off the screen: put the banner on the first visible line */
	int oy = video3ds_visible_top();
	SDL_Rect r = { 0, (Sint16)oy, (Uint16)g_banner->w, (Uint16)g_banner->h };
	SDL_BlitSurface(g_banner, NULL, scr, &r);

	/* the border of the real alert flashes about once a second */
	int b = 2 * g_k, w = g_banner->w, h = g_banner->h;
	Uint32 col = SDL_MapRGB(scr->format, ((SDL_GetTicks() - g_since) / 600) & 1 ? 0 : 0xff, 0, 0);
	SDL_Rect top = { 0, (Sint16)oy, (Uint16)w, (Uint16)b }, bot = { 0, (Sint16)(oy + h - b), (Uint16)w, (Uint16)b };
	SDL_Rect lft = { 0, (Sint16)oy, (Uint16)b, (Uint16)h }, rgt = { (Sint16)(w - b), (Sint16)oy, (Uint16)b, (Uint16)h };
	SDL_FillRect(scr, &top, col);
	SDL_FillRect(scr, &bot, col);
	SDL_FillRect(scr, &lft, col);
	SDL_FillRect(scr, &rgt, col);

	/* click = reboot (what a real dead-end alert does); ignore clicks in the first second so a button held
	 * down while the machine died does not dismiss it unseen */
	int down = buttonstate[0] != 0;
	if (down && !g_prev_button && SDL_GetTicks() - g_since > 1000) {
		log3ds("guru: left mouse button pressed, rebooting the Amiga\n");
		guru_hide();
		uae_reset();
		return;
	}
	g_prev_button = down;
}
