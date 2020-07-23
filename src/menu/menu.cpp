#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "menu.h"

#include <SDL.h>
#include <SDL_image.h>
#include "uae.h"
#include "options.h"
#include "zfile.h"
#include "sound.h"
#include <png.h>
#include "xwin.h"
#include "uibottom.h"
#include "keyboard.h"

extern int bReloadKickstart;

SDL_Surface *current_screenshot = NULL;
SDL_Surface *text_screen=NULL, *text_image, *text_background, *text_window_background;

Uint32 menu_text_color=0;			// text
Uint32 menu_text_color_inactive=0;	// inactive text
Uint32 menu_inv_color=0;			// dark text underlay
Uint32 menu_inv2_color=0;			// light text underlay (almost like background)
Uint32 menu_win0_color=0;			// window dropshadow
Uint32 menu_win1_color=0;			// window frame
Uint32 menu_barra0_color=0;			// progress bar color 0
Uint32 menu_barra1_color=0;			// progress bar color 1

int displaying_menu = 1;

int menu_screen_width = 400;
int menu_screen_height = 240;

static void obten_colores(void)
{
	menu_text_color=SDL_MapRGB(text_screen->format, 0x30, 0x30, 0x30);
	menu_text_color_inactive=SDL_MapRGB(text_screen->format, 0x80, 0x80, 0x80);
	menu_inv_color=SDL_MapRGB(text_screen->format, 0x9a, 0x9a, 0xaa);
	menu_inv2_color=SDL_MapRGB(text_screen->format, 0xce, 0xcf, 0xce);
	menu_win0_color=SDL_MapRGB(text_screen->format, 0x20, 0x20, 0x30);
	menu_win1_color=SDL_MapRGB(text_screen->format, 0x70, 0x70, 0x80);
	menu_barra0_color=SDL_MapRGB(text_screen->format, 0x40, 0x40, 0x50);
	menu_barra1_color=SDL_MapRGB(text_screen->format, 0x80, 0x80, 0x90);
}

int save_png(SDL_Surface* surface,char *path)
{
	SDL_Surface* s = SDL_CreateRGBSurface(SDL_SWSURFACE, surface->w, surface->h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	if (!s) return 0;
	SDL_BlitSurface(surface, NULL, s, NULL);

	png_image image = {0};
	image.version = PNG_IMAGE_VERSION;
	image.width = s->w;
	image.height = s->h;
	image.format = PNG_FORMAT_RGBA;
	png_image_write_to_file(&image, path, 0, s->pixels, 0, NULL);
	SDL_FreeSurface(s);
	return 1;
}

void CreateScreenshot(int code)
{
	int w, h;

  if(current_screenshot != NULL)
  {
    SDL_FreeSurface(current_screenshot);
    current_screenshot = NULL;
  }

  if (code == SCREENSHOT)
  {
		w=prSDLScreen->w;
		h=prSDLScreen->h;
	}
	else
	{
		w=32;
		h=32;
	}
	
	current_screenshot = SDL_CreateRGBSurface(prSDLScreen->flags,w,h,prSDLScreen->format->BitsPerPixel,prSDLScreen->format->Rmask,prSDLScreen->format->Gmask,prSDLScreen->format->Bmask,prSDLScreen->format->Amask);
	SDL_BlitSurface(prSDLScreen, NULL, current_screenshot, NULL);
}

int save_thumb(int code,char *path)
{
//	CreateScreenshot(code);
	int ret = 0;
	if(current_screenshot != NULL)
	  ret = save_png(current_screenshot, path);
	return ret;
}

void menu_raise(void)
{
}

void menu_unraise(void)
{
}

void text_draw_background()
{
	SDL_BlitSurface(text_background,NULL,text_screen,NULL);
}

void text_flip_with_image(SDL_Surface *img, int x, int y)
{
	SDL_Rect dst;
	dst.x = x;
	dst.y = y;
	dst.w = img->w;
	dst.h = img->h;
	SDL_BlitSurface(img, NULL, prSDLScreen, &dst);
	SDL_Flip(prSDLScreen);
	SDL_Delay(20);
}

void text_flip(void)
{
//	SDL_BlitSurface(text_screen,NULL,prSDLScreen,NULL);
	uib_update();
	SDL_Flip(prSDLScreen);
	SDL_Delay(20);
}

void init_kickstart()
{
	if (uae4all_init_rom(romfile))
	{
		init_text(0);
		text_messagebox("--- ERROR ---", "kickstart rom(s) not found!", MB_OK);
	}
	else
	{
		bReloadKickstart=1;
	}
}

void init_text(int splash)
{
	displaying_menu = 1;
	SDL_Surface *tmp;

	prSDLScreen=SDL_SetVideoMode(400,240,16,SDL_HWSURFACE);
	SDL_ShowCursor(SDL_DISABLE);
	text_screen=prSDLScreen;
	if (text_screen==NULL)
		exit(-1);

	if (!text_image)
	{
		//text_screen=SDL_CreateRGBSurface(prSDLScreen->flags,prSDLScreen->w,prSDLScreen->h,prSDLScreen->format->BitsPerPixel,prSDLScreen->format->Rmask,prSDLScreen->format->Gmask,prSDLScreen->format->Bmask,prSDLScreen->format->Amask);
		text_image=SDL_LoadBMP(MENU_FILE_TEXT);
		if (text_image==NULL)
			exit(-2);
		SDL_SetColorKey(text_image, SDL_SRCCOLORKEY, 0x00000000);
		tmp=IMG_Load(MENU_FILE_BACKGROUND);
		if (tmp==NULL)
			exit(-3);
		text_background=SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
		if (text_background==NULL)
			exit(-3);
		tmp=SDL_LoadBMP(MENU_FILE_WINDOW);
		if (tmp==NULL)
			exit(-4);
		text_window_background=SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
		if (text_window_background==NULL)
			exit(-5);
		SDL_SetAlpha(text_window_background, 0, 255);
		obten_colores();
	}
}

void quit_text(void)
{
/*
	SDL_FreeSurface(text_image);
	SDL_FreeSurface(text_background);
	SDL_FreeSurface(text_window_background);
	SDL_FreeSurface(text_screen);
	text_image = text_background = text_window_background = text_screen = NULL;
*/
}

typedef struct {
	SDL_Surface *img;
	int w;
	int h;
} font_info;

void get_font_info(font_info *f, enum font_size size) {
	switch (size) {
		default:
			f->img = text_image;
			f->w = f->h = 8;
	}
}

void write_text_full (SDL_Surface *s, const char *str, int x, int y, int maxchars, enum str_alignment align, enum font_size size, Uint32 col, int inv) {
	int xof, w;
	if (!s) s=text_screen;
	if (str==NULL || str[0]==0) return;
	font_info f;
	w=strlen(str);
	if (maxchars > 0 && w > maxchars) w=maxchars;
	get_font_info(&f,size);

	switch (align) {
		case ALIGN_CENTER:
			xof=x-(w*f.w/2);
			break;
		case ALIGN_RIGHT:
			xof=x-w*f.w;
			break;
		default:
			xof=x;
	}

	if (inv) {
		SDL_Rect dest;
		dest.x = xof - 2;
		dest.y = y - 2;
		dest.w = (w * 8) + 4;
		dest.h = 12;
		SDL_FillRect(s, &dest, menu_inv_color);
	}

	int c;
	SDL_Color scol;
	SDL_GetRGB(col, text_screen->format, &(scol.r), &(scol.g), &(scol.b));
	SDL_SetPalette(f.img, SDL_LOGPAL, &scol, 1, 1);
	for (int i=0; i < w; i++) {
		c=(str[i] & 0x7f)-32;
		if (c<0) c=0;
		SDL_BlitSurface(
			f.img,
			&(SDL_Rect){.x=(c&0x0f)*f.w, .y=(c>>4)*f.h, .w=f.w, .h=f.h},
			s,
			&(SDL_Rect){.x = xof+i*f.w, .y = y});
	}
	SDL_SetPalette(f.img, SDL_LOGPAL, &(SDL_Color){0xff,0xff,0xff,0}, 1, 1);
}

void write_text_pos(int x, int y, const char * str)
{
	write_text_full (text_screen, str, x, y, 0, ALIGN_LEFT, FONT_NORMAL, menu_text_color, 0);
}

void write_text(int x, int y, const char * str)
{
	write_text_pos(x*8, y*8, str);
}

void write_text_inv_pos(int x, int y, const char * str)
{
	write_text_full (text_screen, str, x, y, 0, ALIGN_LEFT, FONT_NORMAL, menu_text_color, 1);
}

void write_text_inv(int x, int y, const char * str)
{
	write_text_inv_pos (x*8, y*8, str);
}

void write_centered_text(int y, char * str)
{
  write_text(40 - (strlen(str) / 2), y/2, str);
}

void write_num(int x, int y, int v)
{
  char str[24];

  snprintf(str, 24, "%d", v);
  write_text(x, y, str);
}

void write_num_inv(int x, int y, int v)
{
  SDL_Rect dest;
  int i,l=1;

  for(i=10;i<1000000;i*=10)
	if (v/i)
		l++;
  	else
		break;

  dest.x = (x * 7) -2 ;
  dest.y = (y * 8) /*10*/ - 2;
  dest.w = (l * 7) + 4;
  dest.h = 12;

  SDL_FillRect(text_screen, &dest, menu_inv_color);

  write_num(x, y, v);
}

void text_draw_window(int x, int y, int w, int h, const char *title)
{
	int i,j;
	SDL_Rect dest;

	x*=8;
	y*=8;
	w*=8;
	h*=8;

	// dropshadow
	dest.x = x + 2;
	dest.y = y - 6;
	dest.w = w + 12;
	dest.h = h + 20;
	SDL_FillRect(text_screen, &dest, menu_win0_color);

	// window
	dest.x = x - 6;
	dest.y = y - 14;
	dest.w = w + 12;
	dest.h = h + 20;
	SDL_FillRect(text_screen, &dest, menu_win1_color);

	// window contents
	SDL_SetClipRect(text_screen, &(SDL_Rect){.x=x-4, .y=y-4, .w=w+8, .h=h+8});
	for(i=0;i<w+8;i+=32)
		for(j=0;j<h+8;j+=24)
		{
			dest.x=x-4+i;
			dest.y=y-4+j;
			dest.w=32;
			dest.h=24;
			SDL_BlitSurface(text_window_background,NULL,text_screen,&dest);
		}
	SDL_SetClipRect(text_screen, NULL);

	// window title
	write_text_pos(x + ( MAX (((w - strlen(title)*8) / 2), 32)), y - 12, title);
//	write_text(x/8 + MAX(((w/8 - strlen(title)) / 2), 4), y/8 - 1,  title);
}

void _text_draw_window(SDL_Surface *sf, int x, int y, int w, int h, const char *title)
{
	SDL_Surface *back=text_screen;
	text_screen=sf;
	text_draw_window(x,y,w,h,title);
	text_screen=back;
}

void text_draw_barra(int x, int y, int w, int h, int per, int max)
{
	SDL_Rect dest;
	if (h>5)
		h-=4;
	dest.x=x-1;
	dest.y=y-1;
	dest.w=w+2;
	dest.h=h+2;
	SDL_FillRect(text_screen, &dest, menu_barra1_color);
	if (per>max)
		per=max;
	dest.x=x;
	dest.y=y;
	dest.h=h;
	dest.w=(w*per)/max;
	SDL_FillRect(text_screen, &dest, menu_barra0_color);
}

void text_draw_window_bar(int x, int y, int w, int h, int per, int max, const char *title)
{
	text_draw_window(x,y,w,h,title);
	text_draw_barra(x+4, y+28, w-24, 12, per, max);
	write_text((x/8)+4,(y/8)+1,"Please wait");
}

void _text_draw_window_bar(SDL_Surface *sf, int x, int y, int w, int h, int per, int max, const char *title)
{
	SDL_Surface *back=text_screen;
	text_screen=sf;
	text_draw_window_bar(x,y,w,h,per,max,title);
	text_screen=back;
}

#define FONT_H 8
#define FONT_W 8
#define MSGBOX_PADDING 8

int text_messagebox(char *title, char *message, mb_mode mode) {
	int height = 1;
	int width = strlen(title) + 4,w;
	char buf[200];
	char *c;
	extern SDL_Surface *text_screen;

	int maxwidth = (text_screen->w - MSGBOX_PADDING * 2) / FONT_W - 2;
	int maxheight = text_screen->h / FONT_H - 3 - MSGBOX_PADDING * 2 / FONT_H;;

	for (c=message, w=0; *c!=0; c++)
	{
		if (*c == '\n')
		{
			++height;
			if (width<w) width=w;
			w=0;
		}
		else
		{
			++w;
			if (w > maxwidth) {
 				++height;
				width=maxwidth;
				w=0;
			}
		}
	}
	if (width<w) width=w;
	height += mode==MB_NONE ? 0 : 2;
	if (mode == MB_OK && width<4) width=4;
	if (mode == MB_YESNO && width<9) width=9;
	if (width > maxwidth) width = maxwidth;
	if (height > maxwidth) height = maxheight;

	int x= (text_screen->w / FONT_W - width) / 2;
	int y= (text_screen->h / FONT_H - height) / 2;
	
	int selected=0;
	int frame = 0;
	do {
		int yo=0;
		SDL_Event e;
		text_draw_background();
		text_draw_window(x - MSGBOX_PADDING / FONT_W, y - MSGBOX_PADDING / FONT_H, width + MSGBOX_PADDING*2/FONT_H, height + MSGBOX_PADDING*2/FONT_H, title);

		char *n=NULL;
		for (c=message; *c!=0; c=n)
		{
			n=strchr(c,'\n');
			if (!n) n=c+strlen(c);
			if (n-c > width) n=c+width;
			snprintf(buf, n-c+1, "%s", c);
			write_text(x, y+yo, buf);
			++yo;
			if (*n == '\n') ++n;
		}

		int flash = frame / 3;

		if (mode != MB_NONE &&
			SDL_PollEvent(&e) &&
			!uib_handle_event(&e) &&
			e.type==SDL_KEYDOWN)
		{
			switch (e.key.keysym.sym) {
			case DS_RIGHT1:
			case DS_RIGHT2:
			case DS_RIGHT3:
			case AK_RT:
			case DS_DOWN1:
			case DS_DOWN2:
			case DS_DOWN3:
			case AK_DN:
			case DS_LEFT1:
			case DS_LEFT2:
			case DS_LEFT3:
			case AK_LF:
			case DS_UP1:
			case DS_UP2:
			case DS_UP3:
			case AK_UP: if (mode == MB_YESNO) selected = (selected+1) % 2; break;
			case DS_A:
			case DS_START:
			case AK_RET:
			case AK_SPC: return selected; break;
			case AK_ESC:
			case DS_B: return -1; break;
			}
		}
		switch (mode) {
			case MB_OK:
				write_text(x+width/2-2, y+yo+1, "    ");
				if (flash)
					write_text_inv(x+width/2-1, y+yo+1, "OK");
				else
					write_text(x+width/2-1, y+yo+1, "OK");
				break;
			case MB_YESNO:
				write_text(x+width/2-5, y+yo+1, "         ");
				if (flash && selected==0)
					write_text_inv(x+width/2-4, y+yo+1, "YES");
				else
					write_text(x+width/2-4, y+yo+1, "YES");
				if (flash && selected==1)
					write_text_inv(x+width/2+1, y+yo+1, "NO");
				else
					write_text(x+width/2+1, y+yo+1, "NO");
				break;
		}
		// text_flip() but without the SDL_Delay
		uib_update();
		SDL_Flip(prSDLScreen);
		if (mode == MB_NONE) break;
		SDL_Delay(20);
		frame = (frame + 1) % 6;
	} while (1);
	return 0;
}
