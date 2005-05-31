/*
 * spcplay - main (program entry point)
 *
 * Copyright (C) 2000 AGAWA Koji <kaoru-k@self-core.104.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* $Id: main.c,v 1.2 2005/05/31 16:38:36 raph Exp $ */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
//#include <sys/soundcard.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "getopt.h"
#include "libspc.h"

#include "spc_structs.h"
#include "SDL.h"

#include "sdlfont.h"

static int is_verbose;

static int get_options (int argc, char **argv);
static void usage (void);
static void display_version (void);
static int last_pc=0;

#define PACKAGE "spcview"
#define VERSION "0.01"

#define MEMORY_VIEW_X	16
#define MEMORY_VIEW_Y	40
#define PORTTOOL_X		540
#define PORTTOOL_Y		500

SPC_Config spc_config = {
    44100,
    16,
    2,
    0,
    0
};

static unsigned char used[65536];
static unsigned char used2[256];

extern struct SAPU APU;
extern struct SIAPU IAPU;

static int g_cfg_nosound = 0;
static int g_cfg_novideo = 0;
static int g_cfg_update_in_callback = 0;
static int g_cfg_num_files = 0;
static char **g_cfg_playlist = NULL;


SDL_Surface *screen=NULL;
SDL_Surface *memsurface=NULL;
static unsigned char *memsurface_data=NULL;
#define BUFFER_SIZE 65536
static unsigned char audiobuf[BUFFER_SIZE];
static int audio_buf_bytes=0, spc_buf_size;

Uint32 color_screen_white, color_screen_black, color_screen_cyan, color_screen_magenta, color_screen_yellow;
Uint32 color_screen_gray;

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void put4pixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void report_memread(unsigned short address, unsigned char value);
void report_memwrite(unsigned short address, unsigned char value);

void report_memread3(unsigned short address, unsigned char value)
{
//	used[address&0xffff] = 1;
//	memsurface_data[((address&0xffff)<<2)]=0xff;
}

void report_memread2(unsigned short address, unsigned char value)
{
	int idx = (address&0xff00)<<4;
	
	idx += (address % 256)<<3;
	
	used[address&0xffff] = 1;	
	used2[(address&0xff00)>>8] = 1;
	
	memsurface_data[idx]=0xff;
	memsurface_data[idx+2048]=0xff;
	memsurface_data[idx+4]=0xff;
	memsurface_data[idx+4+2048]=0xff;

	last_pc = address;
}
	
void report_memread(unsigned short address, unsigned char value)
{
	int idx = ((address&0xff00)<<4) + 2;
	
	idx += (address % 256)<<3;
	memsurface_data[idx]=0xff;
	memsurface_data[idx+2048]=0xff;
	memsurface_data[idx+4]=0xff;
	memsurface_data[idx+4+2048]=0xff;
	used[address&0xffff] = 1;
	used2[(address&0xff00)>>8] = 1;
}

void report_memwrite(unsigned short address, unsigned char value)
{
	int idx = ((address&0xff00)<<4) + 1;

	idx += (address % 256)<<3;
	
	memsurface_data[idx]=0xff;
	memsurface_data[idx+4]=0xff;
	memsurface_data[idx+2048]=0xff;
	memsurface_data[idx+4+2048]=0xff;
	//used[address&0xffff] = 1;
	//used2[(address&0xff)>>8] = 1;
}
/*
void draw_arrays(int X, int Y)
{
	int x,y,i;
	Uint32 color;

	for (y=0; y<256; y++)
	{
		for (x=0; x<256; x++)
		{
			i = y*256+x;
			if (used[i]) {
				color = SDL_MapRGB(screen->format, 
					 executed[i], 
					 written_to[i], 
					 read_from[i]);
				put4pixel(screen, x*2+X, y*2+Y, color);
			}
			
		}
	}
}
*/
void fade_arrays()
{
	int i;
	for (i=0; i<512*512*4; i++)
	{
		if (memsurface_data[i] > 0x40) { memsurface_data[i]--; }
	//	if (executed[i]>0x40) { executed[i]--; }
	//	if (written_to[i]>0x40) { written_to[i]--; }
	//	if (read_from[i]>0x40) { read_from[i]--; }
	}
}


void my_audio_callback(void *userdata, Uint8 *stream, int len)
{
//	printf("Callback %d  audio_buf_bytes: %d\n", len, audio_buf_bytes);
	if (g_cfg_update_in_callback)
	{
		while (len>audio_buf_bytes)
		{
			SPC_update(&audiobuf[audio_buf_bytes]);
	//		printf("."); fflush(stdout);
			audio_buf_bytes+=spc_buf_size;
		}
		memcpy(stream, audiobuf, len);
		memmove(audiobuf, &audiobuf[len], audio_buf_bytes - len);
		audio_buf_bytes -= len;
	//		printf("."); fflush(stdout);
	}
	else
	{
		SDL_LockAudio();

		if (audio_buf_bytes<len) {
			printf("Underrun\n");
			memset(stream, 0, len);
			SDL_UnlockAudio();
			return;
		}

		memcpy(stream, audiobuf, len);
		memmove(audiobuf, &audiobuf[len], audio_buf_bytes - len);
		audio_buf_bytes -= len;
		
	//	for (i=0; i<100; i++) {
	//		printf("%02X ", stream[i]);
	//	}
	//	printf("\n");
		SDL_UnlockAudio();
	}
}

int parse_args(int argc, char **argv)
{
	int res;
	static struct option long_options[] = {
		{"nosound", 0, 0, 0},
		{"novideo", 0, 0, 1},
		{"update_in_callback", 0, 0, 2},
		{"help", 0, 0, 'h'},
		{0,0,0,0}
	};

	while ((res=getopt_long(argc, argv, "h",
				long_options, NULL))!=-1)
	{
		switch(res)
		{
			case 0:
				g_cfg_nosound = 1;
				break;
			case 1:
				g_cfg_novideo = 2;
				break;
			case 'h':
				printf("Usage: ./vspcplay [options] files...\n");
				printf("\n");
				printf("Valid options:\n");
				printf(" -h, --help     Print help\n");
				printf(" --nosound      Dont output sound\n");
				printf(" --novideo      Dont open video window\n");
				printf(" --update_in_callback   Update spc sound buffer inside\n");
				printf("                        sdl audio callback\n");
				exit(0);
				break;
		}
	}

	g_cfg_num_files = argc-optind;
	g_cfg_playlist = &argv[optind];
	
	return 0;
}

int init_sdl()
{
	SDL_AudioSpec desired;
	Uint32 flags=0;
	
	/* SDL initialisation */
	if (!g_cfg_novideo) { flags |= SDL_INIT_VIDEO; }
	if (!g_cfg_nosound) { flags |= SDL_INIT_AUDIO; }

	SDL_Init(flags);	

	if (!g_cfg_novideo) {
		// video
		screen = SDL_SetVideoMode(800, 600, 0, SDL_SWSURFACE);
		if (screen == NULL) {
			printf("Failed to set video mode\n");
			return 0;
		}

		// precompute some colors
		color_screen_black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
		color_screen_white = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);
		color_screen_yellow = SDL_MapRGB(screen->format, 0xff, 0xff, 0x00);
		color_screen_cyan = SDL_MapRGB(screen->format, 0x00, 0xff, 0xff);
		color_screen_magenta = SDL_MapRGB(screen->format, 0xff, 0x00, 0xff);
		color_screen_gray = SDL_MapRGB(screen->format, 0x7f, 0x7f, 0x7f);
	}
	
	if (!g_cfg_nosound) {
		// audio
		desired.freq = 44100;
		desired.format = AUDIO_S16SYS;
		desired.channels = 2;
		desired.samples = 1024;
		//desired.samples = 4096;
		desired.callback = my_audio_callback;
		desired.userdata = NULL;
		if ( SDL_OpenAudio(&desired, NULL) < 0 ){
			fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
			return -1;
		}

		printf("sdl audio frag size: %d\n", desired.samples *4);
	}
	    
	return 0;	
}

int main(int argc, char **argv)
{
    int num_options;
    void *buf=NULL;
	int dsp_fd=-1;
	int tmp, i;
	int res, updates;
	int blocksize;
	int loops=0;
	int cur_mouse_address=-1;
	SDL_Rect tmprect;
	SDL_Rect memrect;
	char tmpbuf[30];

	memset(used, 0, 65536);

	parse_args(argc, argv);

	if (g_cfg_num_files < 1) {
		printf("No files specified\n");
		return 1;
	}
	
    spc_buf_size = SPC_init(&spc_config);
	printf("spc buffer size: %d\n", spc_buf_size);
	buf = malloc(spc_buf_size*2);

	
	init_sdl();

	memsurface_data = malloc(512*512*4);
	memset(memsurface_data, 0, 512*512*4);

	if (!g_cfg_novideo)
	{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		memsurface = SDL_CreateRGBSurfaceFrom(memsurface_data,512,512,32,2048,0xFF000000,0x00FF0000,0x0000FF00,0x0);
#else
		memsurface = SDL_CreateRGBSurfaceFrom(memsurface_data,512,512,32,2048,0xFF,0xFF00,0xFF0000,0x0);		
#endif
	
		tmprect.x = MEMORY_VIEW_X-1;
		tmprect.y = MEMORY_VIEW_Y-1;
		tmprect.w = 512+2;
		tmprect.h = 512+2;
		SDL_FillRect(screen, &tmprect, color_screen_white);	
		
		sdlfont_drawString(screen, MEMORY_VIEW_X, MEMORY_VIEW_Y-10, "spc memory:", color_screen_white);

	}
	
    SPC_load(g_cfg_playlist[0]);
	
	SDL_PauseAudio(0);
    for (;;) 
	{		
		SDL_Event ev;
		
		if (!g_cfg_novideo)
		{
			while (SDL_PollEvent(&ev))
			{
				switch (ev.type)
				{
					case SDL_QUIT:
						SDL_PauseAudio(1);
						goto clean;
						break;
					case SDL_MOUSEMOTION:
						{
							if (	ev.motion.x >= MEMORY_VIEW_X && 
									ev.motion.x < MEMORY_VIEW_X + 512 &&
									ev.motion.y >= MEMORY_VIEW_Y &&
									ev.motion.y < MEMORY_VIEW_Y + 512)
							{
								int x, y;
								x = ev.motion.x - MEMORY_VIEW_X;
								y = ev.motion.y - MEMORY_VIEW_Y;
								x /= 2;
								y /= 2;
								cur_mouse_address = y*256+x;
	//							printf("%d,%d: $%04X\n", x, y, y*256+x);
							}
							else
							{
								cur_mouse_address = -1;
							}

							if (	(ev.motion.x >= PORTTOOL_X + (8*5)) &&
									ev.motion.y >= PORTTOOL_Y)
							{
								int x, y;
								x = ev.motion.x - (PORTTOOL_X + (8*5));
								x /= 8;
								y = ev.motion.y - PORTTOOL_Y;
								y /= 8;
								if (y==1) 
								printf("%d\n", x);
							}
						}
						break;
				}
			} // while (pollevent)
		} // !g_cfg_novideo
	
		if (!g_cfg_update_in_callback)
		{
			// fill the buffer when possible
			updates = 0;
		
			if (BUFFER_SIZE - audio_buf_bytes >= spc_buf_size )
			{
				if (SDL_MUSTLOCK(memsurface)) {
					SDL_LockSurface(memsurface);
				}
				while (BUFFER_SIZE - audio_buf_bytes >= spc_buf_size) {
					SDL_LockAudio();
					SPC_update(&audiobuf[audio_buf_bytes]);
					SDL_UnlockAudio();

					audio_buf_bytes += spc_buf_size;
				}
				if (SDL_MUSTLOCK(memsurface)) {
					SDL_UnlockSurface(memsurface);
				}
			}
		}
		
		fade_arrays();			
		memrect.x = MEMORY_VIEW_X; memrect.y = MEMORY_VIEW_Y;
//		SDL_LockAudio();
		
		// draw the memory read/write display area
		SDL_BlitSurface(memsurface, NULL, screen, &memrect);	

		// draw the 256 bytes block usage bar
		tmprect.x = MEMORY_VIEW_X-3;
		tmprect.w = 2; tmprect.h = 2;
		tmp=0;
		for (i=0; i<256; i++)
		{
			tmprect.y = MEMORY_VIEW_Y + i * 2;
			if (used2[i])
			{
				SDL_FillRect(screen, &tmprect, color_screen_white);	
				tmp++;
			}
		}
		
//		SDL_UnlockAudio();

		sprintf(tmpbuf, "Blocks used: %3d/256 (%.1f%%)  ", tmp, (float)tmp*100.0/256.0);
		sdlfont_drawString(screen, MEMORY_VIEW_X, MEMORY_VIEW_Y + memsurface->h + 2, tmpbuf, color_screen_white);

		{
			unsigned char packed_mask[32];
			memset(packed_mask, 0, 32);
			for (i=0; i<256; i++)
			{
				if (used2[i])
				packed_mask[i/8] |=	128 >> (i%8);
			}
			for (i=0; i<32; i++) {
				sprintf(tmpbuf+(i*2), "%02X",packed_mask[i]);
			}
			sdlfont_drawString(screen, MEMORY_VIEW_X, MEMORY_VIEW_Y + memsurface->h + 2 + 9, tmpbuf, color_screen_white);
		}
		
		// write the address under mouse cursor
		if (cur_mouse_address >=0)
		{
			sprintf(tmpbuf, "Addr mouse: $%04X", cur_mouse_address);
			sdlfont_drawString(screen, MEMORY_VIEW_X+8*(13+9), MEMORY_VIEW_Y-10, tmpbuf, color_screen_white);
		}

		// write the program counter
		sprintf(tmpbuf, "PC: $%04x", last_pc);
		sdlfont_drawString(screen, MEMORY_VIEW_X+8*12, MEMORY_VIEW_Y-10, tmpbuf, color_screen_white);

		tmp = i+10; // y 
		
		sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp, "Voices pitchs:", color_screen_white);
		tmp += 9;
		
		tmprect.x=MEMORY_VIEW_X+520;
		tmprect.y=tmp;
		tmprect.w=screen->w-tmprect.x;
		tmprect.h=8*8;
		SDL_FillRect(screen, &tmprect, color_screen_black);
		tmprect.w=5;
		tmprect.h = 5;
		for (i=0; i<8; i++)
		{
			unsigned short pitch = APU.DSP[2+(i*0x10)] | (APU.DSP[3+(i*0x10)]<<8);
			Uint32 cur_color;
		
			if (APU.DSP[0x5c]&(1<<i)) {
				cur_color = color_screen_white;
			} else {
				cur_color = color_screen_gray;
			}
			
			sprintf(tmpbuf,"%d:",i);
			sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp + (i*8), tmpbuf, color_screen_white);
			
			tmprect.y= tmp+(i*8)+2;
			tmprect.x = MEMORY_VIEW_X+520+18;
			tmprect.x += pitch*(screen->w-tmprect.x-20)/((0x10000)>>2);
			SDL_FillRect(screen, &tmprect, color_screen_white);
			
		}
		tmp += 9*8;
		
		sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp, "Voices volumes:", color_screen_white);
		sdlfont_drawString(screen, MEMORY_VIEW_X+520+(16*8), tmp, "Left", color_screen_yellow);
		sdlfont_drawString(screen, MEMORY_VIEW_X+520+(20*8)+4, tmp, "Right", color_screen_cyan);
		sdlfont_drawString(screen, MEMORY_VIEW_X+520+(26*8), tmp, "Gain", color_screen_magenta);
		tmp += 9;

		tmprect.x=MEMORY_VIEW_X+520;
		tmprect.y=tmp;
		tmprect.w=screen->w-tmprect.x;
		tmprect.h=8*10;
		SDL_FillRect(screen, &tmprect, color_screen_black);		
		tmprect.w=2;
		tmprect.h=2;
		for (i=0; i<8; i++)
		{
			unsigned char left_vol = APU.DSP[0+(i*0x10)];
			unsigned char right_vol = APU.DSP[1+(i*0x10)];
			unsigned char gain = APU.DSP[7+(i*0x10)];

			sprintf(tmpbuf,"%d:", i);
			sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp + (i*10), tmpbuf, color_screen_white);
			tmprect.x = MEMORY_VIEW_X+520+18;
			tmprect.y = tmp+(i*10);
			tmprect.w = left_vol*(screen->w-tmprect.x-20)/255;

			SDL_FillRect(screen, &tmprect, color_screen_yellow);

			tmprect.x = MEMORY_VIEW_X+520+18;
			tmprect.w = right_vol*(screen->w-tmprect.x-20)/255;
			tmprect.y = tmp+(i*10)+3;
			
			SDL_FillRect(screen, &tmprect, color_screen_cyan);

			tmprect.x = MEMORY_VIEW_X+520+18;
			tmprect.w = gain * (screen->w-tmprect.x-20)/255;
			tmprect.y = tmp+(i*10)+6;
			SDL_FillRect(screen, &tmprect, color_screen_magenta);
		}
		tmp += 8*10 + 8;

		sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp, "Mouseover Hexdump:", color_screen_white);
		tmp+=9;
		if (cur_mouse_address>=0)
		{
	
			for (i=0; i<128; i+=8)
			{
				unsigned char *st = &IAPU.RAM[cur_mouse_address+i];
				int p = MEMORY_VIEW_X+520, j;
				sprintf(tmpbuf, "%04X: ", cur_mouse_address+i);
				sdlfont_drawString(screen, p, tmp, tmpbuf, color_screen_white);
				p += 6*8;
				for (j=0; j<8; j++) {
					int idx = ((cur_mouse_address+i+j)&0xff00)<<4;	
					idx += ((cur_mouse_address+i+j) % 256)<<3;
					Uint32 color = SDL_MapRGB(screen->format, 
							0x7f + (memsurface_data[idx]>>1), 
							0x7f + (memsurface_data[idx+1]>>1), 
							0x7f + (memsurface_data[idx+2]>>1)
							);
							
					sprintf(tmpbuf, "%02X ", *st);
					st++;
					sdlfont_drawString(screen, p, tmp, tmpbuf, color);
					p+= 2*8 + 4;
				}
				
				tmp += 9;
			}
		}


		sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y, "     - Port tool -", color_screen_white);
		sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y+8, " APU:", color_screen_white);
		sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y+16, "SNES:", color_screen_white);

		sprintf(tmpbuf, " +%02X- +%02X- +%02X- +%02X-", IAPU.RAM[0xf4], APU.OutPorts[0xf5], APU.OutPorts[0xf6], APU.OutPorts[0xf7]);		
		sdlfont_drawString(screen, PORTTOOL_X + (8*5), PORTTOOL_Y+8, tmpbuf, color_screen_white);
		
		sprintf(tmpbuf, "  %02X   %02X   %02X    %02X", APU.OutPorts[0], APU.OutPorts[1], APU.OutPorts[2], APU.OutPorts[3]);		
		sdlfont_drawString(screen, PORTTOOL_X + (8*5), PORTTOOL_Y+16, tmpbuf, color_screen_white);

		
		SDL_UpdateRect(screen, 0, 0, 0, 0);

	}
clean:
//	close(dsp_fd);
    SPC_close();
	SDL_Quit();

    return 0;
}


