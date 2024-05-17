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
/* $Id: main.c,v 1.32 2005/07/27 17:27:47 raph Exp $ */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <fcntl.h>
#include <math.h>
#include <errno.h>
#include "libspc.h"
#include "id666.h"

#include "spc_structs.h"
#include "SDL.h"

#include "sdlfont.h"
#include "wavewriter.h"
#include "soundux.h"

int last_pc=-1;

#define PACKAGE "spcview"
#define VERSION "0.01"

#define MEMORY_VIEW_X	16
#define MEMORY_VIEW_Y	40
#define PORTTOOL_X		540
#define PORTTOOL_Y		380
#define INFO_X			540
#define INFO_Y			420

// 5 minutes default
#define DEFAULT_SONGTIME	(60*5) 

#define PROG_NAME_VERSION_STRING "vspcplay v"VERSION_STR
#define CREDITS "vspcplay v" VERSION_STR " by Raphael Assenat (http://vspcplay.raphnet.net). APU emulation code from snes9x."

SPC_Config spc_config = {
    44100,
    16,
    2,
    0, // interpolation
    0 // echo
};

// those are bigger so I dont have to do a range test
// each time I want to log the PC address (where I assume
// a 5 byte instruction)
unsigned char used[0x10006];
unsigned char used2[0x101];

extern struct SAPU APU;
extern struct SIAPU IAPU;

static unsigned char g_cfg_filler = 0x00;
static int g_cfg_apply_block = 0;
static int g_cfg_statusline = 0;
static int g_cfg_nice = 0;
static int g_cfg_extratime = 0;
static int g_cfg_ignoretagtime = 0;
static int g_cfg_defaultsongtime = DEFAULT_SONGTIME;
static int g_cfg_autowritemask = 0;
static int g_cfg_nosound = 0;
static int g_cfg_novideo = 0;
static int g_cfg_update_in_callback = 0;
static int g_cfg_num_files = 0;
static char **g_cfg_playlist = NULL;
static int g_paused = 0;
static int g_cur_entry = 0;
static char *g_real_filename=NULL; // holds the filename minus path
static const char *g_outwavefile = NULL;
static WaveWriter *g_waveWriter = NULL;
static unsigned char muted_at_startup[8];
static int just_show_info = 0;

SDL_Surface *screen=NULL;
SDL_Texture *texture=NULL;
SDL_Window *window=NULL;
SDL_Renderer *renderer=NULL;
SDL_Surface *memsurface=NULL;
unsigned char *memsurface_data=NULL;
#define BUFFER_SIZE 65536
static unsigned char audiobuf[BUFFER_SIZE];
static int audio_buf_bytes=0, spc_buf_size;

Uint32 color_screen_white, color_screen_black, color_screen_cyan, color_screen_magenta, color_screen_yellow, color_screen_red;
Uint32 color_screen_gray;
Uint32 colorscale[12];

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void put4pixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void report_memread(unsigned short address, unsigned char value);
void report_memwrite(unsigned short address, unsigned char value);

void fade_arrays()
{
	int i;
	for (i=0; i<512*512*4; i++)
	{
		if (memsurface_data[i] > 0x40) { memsurface_data[i]--; }
	}
}

static int audio_samples_written=0;

void applyBlockMask(char *filename)
{
	FILE *fptr;
	unsigned char nul_arr[256];
	int i;

	memset(nul_arr, g_cfg_filler, 256);
	
	fptr = fopen(filename, "r+");
	if (!fptr) { perror("fopen"); }

	printf("[");
	for (i=0; i<256; i++)
	{
		fseek(fptr, 0x100+(i*256), SEEK_SET);
		
		if (!used2[i]) {
			printf(".");
			fwrite(nul_arr, 256, 1, fptr);
		} else {
			printf("o");
		}
		fflush(stdout);
	}
	printf("]\n");
	
	fclose(fptr);
}

void my_audio_callback(void *userdata, Uint8 *stream, int len)
{
//	printf("Callback %d  audio_buf_bytes: %d\n", len, audio_buf_bytes);

	SDL_memset(stream, 0, len); // As part of the migration to SDL2, the audio callback needs to set every byte in the buffer.
	                            // I'm not sure if your audio callback is gaurenteed to do that,
								// so I add this line here to initalize the buffer (ass specified in the migration guide).
	                            // Better safe than sorry. If your audio callback does do this, feel free to comment out, or remove this line.
	
	if (g_cfg_update_in_callback)
	{
		while (len>audio_buf_bytes)
		{
			SPC_update(&audiobuf[audio_buf_bytes]);
			if (g_waveWriter) {
				waveWriter_addSamples(g_waveWriter, &audiobuf[audio_buf_bytes], spc_buf_size/4);
			}

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
		//SDL_LockAudio();

		if (audio_buf_bytes<len) {
			printf("Underrun\n");
			memset(stream, 0, len);
		//	SDL_UnlockAudio();
			return;
		}

		memcpy(stream, audiobuf, len);
		memmove(audiobuf, &audiobuf[len], audio_buf_bytes - len);
		audio_buf_bytes -= len;
		
	}
	audio_samples_written += len/4; // 16bit stereo
}

static void printHelp(void)
{
	printf("Usage: ./vspcplay [options] files...\n");
	printf("\n");
	printf("Valid options:\n");
	printf(" -h, --help             Print help\n");
	printf(" --nosound              Do not output sound\n");
	printf(" --novideo              Do not open video window\n");
	printf(" --waveout file.wav     (Also) Create a wave file\n");
	printf(" --update_in_callback   Update spc sound buffer inside\n");
	printf("                        sdl audio callback\n");
	printf(" --interpolation        Use sound interpolatoin\n");
	printf(" --echo                 Enable echo\n");
	printf(" --auto_write_mask      Write mask file automatically when a\n");
	printf("                        tune ends due to playtime from tag or\n");
	printf("                        default play time.\n");
	printf(" --default_time t       Set the default play time in seconds\n");
	printf("                        for when there is not id666 tag. (default: %d\n", DEFAULT_SONGTIME);
	printf(" --ignore_tag_time      Ignore the time from the id666 tag and\n");
	printf("                        use default time\n");
	printf(" --extra_time t         Set the number of extra seconds to play (relative to\n");
	printf("                        the tag time or default time).\n");
	printf(" --nice                 Try to use less cpu for graphics\n");
	printf(" --status_line          Enable a text mode status line\n");
	printf(" --mute channel         Start with channel muted (1-8 or all)\n");
	printf(" --unmute channel       Unmute specified channel (1-8)\n");
	printf(" --id666                Display id666 tag info and exit.\n");

	printf("\nAdvanced options:\n");
	printf("!!! Careful with those!, they can ruin your sets so backup first!!!\n");
	printf(" --apply_mask_block     Apply the mask to the file (replace unused blocks(256 bytes) with a pattern)\n");
	printf(" --filler val           Set the pattern byte value. Use with the option above. Default 0\n");
	printf("\n");
	printf("The mask will be applied when the tune ends due to playtime from tag\n");
	printf("or default playtime.\n");
	printf("\n");

	printf("Keys: (graphical mode only)\n");
	printf(" 1-8                   Toggle mute on channel 1-8\n");
	printf(" 0                     Toggle all mute/unmute\n");
	printf(" n                     Play next file\n");
	printf(" p                     Play previous file\n");
	printf(" r                     Restart current file\n");
	printf(" ESC                   Quit\n");
}

enum {
	OPT_NOSOUND = 256,
	OPT_NOVIDEO,
	OPT_UPDATE_IN_CALLBACK,
	OPT_INTERPOLATION,
	OPT_ECHO,
	OPT_DEFAULT_TIME,
	OPT_IGNORE_TAG_TIME,
	OPT_EXTRA_TIME,
	OPT_YIELD,
	OPT_AUTO_WRITE_MASK,
	OPT_STATUS_LINE,
	OPT_APPLY_MASK_BLOCK,
	OPT_APPLY_MASK_BYTE,
	OPT_FILLER,
	OPT_WAVEOUT,
	OPT_MUTE,
	OPT_UNMUTE,
	OPT_FILEINFO,
};

static struct option long_options[] = {
	{ "help",               no_argument,       0, 'h'},

	{ "nosound",            no_argument,       0, OPT_NOSOUND            },
	{ "novideo",            no_argument,       0, OPT_NOVIDEO            },
	{ "waveout",            required_argument, 0, OPT_WAVEOUT            },
	{ "update_in_callback", no_argument,       0, OPT_UPDATE_IN_CALLBACK },
	{ "echo",               no_argument,       0, OPT_ECHO               },
	{ "interpolation",      no_argument,       0, OPT_INTERPOLATION      },
	{ "default_time",       required_argument, 0, OPT_DEFAULT_TIME       },
	{ "ignore_tag_time",    no_argument,       0, OPT_IGNORE_TAG_TIME    },
	{ "extra_time",         required_argument, 0, OPT_EXTRA_TIME         },
	{ "yield",              no_argument,       0, OPT_YIELD              },
	{ "auto_write_mask",    no_argument,       0, OPT_AUTO_WRITE_MASK    },
	{ "status_line",        no_argument,       0, OPT_STATUS_LINE        },
	{ "apply_mask_block",   no_argument,       0, OPT_APPLY_MASK_BLOCK   },
	{ "apply_mask_byte",    no_argument,       0, OPT_APPLY_MASK_BYTE    },
	{ "filler",             required_argument, 0, OPT_FILLER             },
	{ "mute",               required_argument, 0, OPT_MUTE               },
	{ "unmute",             required_argument, 0, OPT_UNMUTE             },
	{ "id666",              no_argument,       0, OPT_FILEINFO           },

	{0,0,0,0}
};

int parse_args(int argc, char **argv)
{
	int res;
	int i;

	while ((res=getopt_long(argc, argv, "h",
				long_options, NULL))!=-1)
	{
		switch(res)
		{
			case OPT_UNMUTE:
				if (0 == strcasecmp(optarg,"all")) {
					for (i=0; i<8; i++) {
						muted_at_startup[i] = 0;
					}
				}
				else {
					i = atoi(optarg);
					if (i<1 || i>8) {
						fprintf(stderr, "Invalid channel number. Valid arguments: 1-8 or all\n");
						return -1;
					}
					muted_at_startup[i-1] = 0;
				}
				break;

			case OPT_MUTE:
				if (0 == strcasecmp(optarg,"all")) {
					for (i=0; i<8; i++) {
						muted_at_startup[i] = 1;
					}
				}
				else {
					i = atoi(optarg);
					if (i<1 || i>8) {
						fprintf(stderr, "Invalid channel number. Valid arguments: 1-8 or all\n");
						return -1;
					}
					muted_at_startup[i-1] = 1;
				}
				break;
			case OPT_NOSOUND:
				g_cfg_nosound = 1;
				break;
			case OPT_NOVIDEO:
				g_cfg_novideo = 2;
				break;
			case OPT_UPDATE_IN_CALLBACK:
				g_cfg_update_in_callback = 1;
				break;
			case OPT_INTERPOLATION:
				spc_config.is_interpolation = 1;
				break;
			case OPT_ECHO:
				spc_config.is_echo = 1;
				break;
			case OPT_DEFAULT_TIME:
				g_cfg_defaultsongtime = atoi(optarg);
				break;
			case OPT_IGNORE_TAG_TIME:
				g_cfg_ignoretagtime = 1;
				break;
			case OPT_EXTRA_TIME:
				g_cfg_extratime = atoi(optarg);
				break;
			case OPT_YIELD:
				g_cfg_nice = 1;
				break;
			case OPT_AUTO_WRITE_MASK:
				g_cfg_autowritemask = 1;
				break;
			case OPT_STATUS_LINE:
				g_cfg_statusline = 1;
				break;
			case OPT_APPLY_MASK_BLOCK:
				g_cfg_apply_block = 1;
				break;
			case OPT_FILLER:
				g_cfg_filler = strtol(optarg, NULL, 0);
				break;
			case OPT_WAVEOUT:
				g_outwavefile = optarg;
				break;
			case 'h':
				printHelp();
				exit(0);
				break;
			case OPT_FILEINFO:
				just_show_info = 1;
				break;
		}
	}


	g_cfg_num_files = argc-optind;
	g_cfg_playlist = &argv[optind];

	return 0;
}

void write_mask(unsigned char packed_mask[32])
{
	FILE *msk_file;
	char *sep;
	char filename[1024];
	unsigned char tmp;
	int i;

	strncpy(filename, g_cfg_playlist[g_cur_entry], 1023);
#ifdef WIN32
	sep = strrchr(filename, '\\');
#else
	sep = strrchr(filename, '/');
#endif
	// keep only the path
	if (sep) { sep++; *sep = 0; } 
	else { 
		filename[0] = 0; 
	}

	// add the filename
	strncat(filename, g_real_filename, 1023);

	// but remove the extension if any
	sep = strrchr(filename, '.');
	if (sep) { *sep = 0; }

	// and use the .msk extension
	strncat(filename, ".msk", 1023);

	// make sure it is terminated (in case it is 1023 bytes long and strncat
	// did not add a NULL
	filename[1023] = 0;

	msk_file = fopen(filename, "wb");
	if (!msk_file) {
		perror("fopen");
	}
	else
	{
		fwrite(packed_mask, 32, 1, msk_file);
	}

	printf("Writing mask to '%s'\n", filename);

	// the first 32 bytes are for the 256BytesBlock mask
	printf("256 Bytes-wide block mask:\n");
	for (i=0; i<32; i++) {
		printf("%02X",packed_mask[i]);
	}
	printf("\n");

	printf("Byte level mask..."); fflush(stdout);
	memset(packed_mask, 0, 32);
	for (i=0; i<65536; i+=8)
	{
		tmp = 0;
		if (used[i]) tmp |= 128;
		if (used[i+1]) tmp |= 64;
		if (used[i+2]) tmp |= 32;
		if (used[i+3]) tmp |= 16;
		if (used[i+4]) tmp |= 8;
		if (used[i+5]) tmp |= 4;
		if (used[i+6]) tmp |= 2;
		if (used[i+7]) tmp |= 1;
		fwrite(&tmp, 1, 1, msk_file);
	}
	printf("Done.\n");
	fclose(msk_file);
}

static char now_playing[1024];
static char *marquees[3] = { CREDITS, now_playing, NULL };
static char *cur_marquee = NULL;
static int cur_marquee_id = 0;

void do_scroller(int elaps_milli)
{
	int i;
	char c[2] = { 0, 0 };	
	static int cs;
	static int p = 0;
	static int cur_len;
	static int cur_min;
	SDL_Rect tmprect;
	static float start_angle = 0.0;
	float angle;
	int off;
	int steps;
	static int keep=0;

	keep += elaps_milli;	
//	printf("%d %d\n", keep, elaps_milli);
//	return;
	
	steps = keep*60/1000;
	if (!steps) { return; }

	elaps_milli = keep;
	keep=0;

	if (cur_marquee == NULL) { 
		cur_marquee = marquees[cur_marquee_id]; 
		p = screen->w;
	}
	
	cur_len = strlen(cur_marquee);
	cur_min = -cur_len*8;

	angle = start_angle;
				
	cs = audio_samples_written/44100;
	cs %= 12;

	// clear area	
	tmprect.x = 0;
	tmprect.y = 0;
	tmprect.w = screen->w;
	tmprect.h = 28;
	SDL_FillRect(screen, &tmprect, color_screen_black);
	
	
	for (i=0; i<cur_len; i++)
	{
		off = sin(angle)*8.0;
		c[0] = cur_marquee[i];
		if (	(tmprect.x + i*8 + p > 0) && (tmprect.x + i*8 + p < screen->w) )
		{
			sdlfont_drawString(screen, tmprect.x + i*8 + p, 12 + off, c, colorscale[cs]);
		}
		angle-=0.1;
	}
	start_angle += steps * 0.02;
	p-=steps;

	if (p<cur_min) {
		if (marquees[cur_marquee_id+1]!=NULL) {
			cur_marquee = marquees[++cur_marquee_id];
		}
		else {
			p = screen->w;
		}
	}
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
		window = SDL_CreateWindow(PROG_NAME_VERSION_STRING,
                  SDL_WINDOWPOS_UNDEFINED,
                  SDL_WINDOWPOS_UNDEFINED,
                  800, 600,
                  SDL_WINDOW_OPENGL);

		if (window == NULL) {
			printf("%s\n", SDL_GetError());
			exit(1);
		}

		renderer = SDL_CreateRenderer(window, -1, 0);

		if (renderer == NULL) {
			printf("%s\n", SDL_GetError());
			exit(1);
		}

		screen = SDL_CreateRGBSurface(0, 800, 600, 32,
			0x00FF0000,
			0x0000FF00,
			0x000000FF,
			0xFF000000);

		if (screen == NULL) {
			printf("%s\n", SDL_GetError());
			exit(1);
		}

		texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ARGB8888,
			SDL_TEXTUREACCESS_STREAMING,
			800, 600);

		if (texture == NULL) {
			printf("%s\n", SDL_GetError());
			exit(1);
		}
		
		// precompute some colors
		color_screen_black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
		color_screen_white = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);
		color_screen_yellow = SDL_MapRGB(screen->format, 0xff, 0xff, 0x00);
		color_screen_cyan = SDL_MapRGB(screen->format, 0x00, 0xff, 0xff);
		color_screen_magenta = SDL_MapRGB(screen->format, 0xff, 0x00, 0xff);
		color_screen_gray = SDL_MapRGB(screen->format, 0x7f, 0x7f, 0x7f);
		color_screen_red = SDL_MapRGB(screen->format, 0xff, 0x00, 0x00);

		colorscale[0] = SDL_MapRGB(screen->format, 0xff, 0x00, 0x00);
		colorscale[1] = SDL_MapRGB(screen->format, 0xff, 0x7f, 0x00);
		colorscale[2] = SDL_MapRGB(screen->format, 0xff, 0xff, 0x00);
		colorscale[3] = SDL_MapRGB(screen->format, 0x7f, 0xff, 0x00);
		colorscale[4] = SDL_MapRGB(screen->format, 0x00, 0xff, 0x00);
		colorscale[5] = SDL_MapRGB(screen->format, 0x00, 0xff, 0x7f);
		colorscale[6] = SDL_MapRGB(screen->format, 0x00, 0xff, 0xff);
		colorscale[7] = SDL_MapRGB(screen->format, 0x00, 0x7f, 0xff);
		colorscale[8] = SDL_MapRGB(screen->format, 0x00, 0x00, 0xff);
		colorscale[9] = SDL_MapRGB(screen->format, 0x7f, 0x00, 0xff);
		colorscale[10] = SDL_MapRGB(screen->format, 0xff, 0x00, 0xff);
		colorscale[11] = SDL_MapRGB(screen->format, 0xff, 0x00, 0x7f);
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

void pack_mask(unsigned char packed_mask[32])
{
	int i;
	
	memset(packed_mask, 0, 32);
	for (i=0; i<256; i++)
	{
		if (used2[i])
		packed_mask[i/8] |=	128 >> (i%8);
	}
}

void printInfo(void)
{
	int i;
	FILE *fptr;
	id666_tag id6;

	printf("%d files\n", g_cfg_num_files);

	for (i=0; i<g_cfg_num_files; i++) {
		if (i>0) {
			printf("------\n");
		}

		printf("File: %s\n", g_cfg_playlist[i]);

		fptr = fopen(g_cfg_playlist[i], "rb");
		if (!fptr) {
			printf("Could not open file %s: %s\n", g_cfg_playlist[i], strerror(errno));
			continue;
		}

		read_id666(fptr, &id6);
		print_id666(&id6);

		fclose(fptr);
	}
}

int main(int argc, char **argv)
{
	int tmp, i;
	int cur_mouse_address=0x0000;
	SDL_Rect tmprect;
	SDL_Rect memrect;
	char tmpbuf[256];
	id666_tag tag;
	int song_time; // in seconds
	Uint32 current_ticks, song_started_ticks;
	unsigned char packed_mask[32];
	Uint32 time_last=0, time_cur=0;
	int hexdump_address=0x0000, hexdump_locked=0;
	int mouse_x, mouse_y;
	
	//memset(used, 0, 65536);

	printf("%s\n", PROG_NAME_VERSION_STRING);
	
	parse_args(argc, argv);

	if (g_cfg_num_files < 1) {
		printf("No files specified\n");
		return 1;
	}

	if (just_show_info) {
		printInfo();
		return 0;
	}

	if (g_outwavefile) {
		g_waveWriter = waveWriter_create(g_outwavefile);
		if (!g_waveWriter) {
			return -1;
		}
	}

    spc_buf_size = SPC_init(&spc_config);
	printf("spc buffer size: %d\n", spc_buf_size);

	for (i=0; i<8; i++) {
		if (muted_at_startup[i]) {
			SoundData.forceMute[i] = 1;
			printf("Channel %d muted from command-line\n", i+1);
		}
	}

	init_sdl();
	time_cur = time_last = SDL_GetTicks();

	memsurface_data = malloc(512*512*4);
	memset(memsurface_data, 0, 512*512*4);

reload:
#ifdef WIN32
	g_real_filename = strrchr(g_cfg_playlist[g_cur_entry], '\\');
#else
	g_real_filename = strrchr(g_cfg_playlist[g_cur_entry], '/');
#endif
	if (!g_real_filename) {
		g_real_filename = g_cfg_playlist[g_cur_entry];
	}
	else {
		// skip path sep
		g_real_filename++;
	}	
	
	{
		FILE *fptr;
		fptr = fopen(g_cfg_playlist[g_cur_entry], "rb");
		if (fptr==NULL) {
			printf("Failed to open %s\n", g_cfg_playlist[g_cur_entry]);
				if (g_cur_entry == g_cfg_num_files-1) {
				g_cfg_num_files--;
			}
			else
			{
				memmove(&g_cfg_playlist[g_cur_entry], &g_cfg_playlist[g_cur_entry+1], g_cfg_num_files-g_cur_entry);
				g_cfg_num_files--;
				g_cur_entry++;
			}
			if (g_cfg_num_files<=0) { goto clean; }
			if (g_cur_entry >= g_cfg_num_files) { g_cur_entry = 0; }
			goto reload;
		}
		
		read_id666(fptr, &tag); 

		/* decide how much time the song will play */
		if (!g_cfg_ignoretagtime) {
			song_time = atoi(tag.seconds_til_fadeout);
			if (song_time <= 0) {
				song_time = g_cfg_defaultsongtime;
			}
		}
		else {
			song_time = g_cfg_defaultsongtime;
		}

		song_time += g_cfg_extratime;

		now_playing[0] = 0;
		if (strlen(tag.title)) {
			sprintf(now_playing, "Now playing: %s (%s), dumped by %s\n", tag.title, tag.game_title, tag.name_of_dumper);
		}
		if (strlen(now_playing)==0) {
			sprintf(now_playing, "Now playing: %s\n", g_cfg_playlist[g_cur_entry]);
		}
		
		fclose(fptr);
	}
	
    if (!SPC_load(g_cfg_playlist[g_cur_entry])) 
	{
		printf("Failure\n");
		if (g_cur_entry == g_cfg_num_files-1) {
			g_cfg_num_files--;
		}
		else
		{
			memmove(&g_cfg_playlist[g_cur_entry], &g_cfg_playlist[g_cur_entry+1], g_cfg_num_files-g_cur_entry);
			g_cfg_num_files--;
			g_cur_entry++;
		}
		if (g_cfg_num_files<=0) { goto clean; }
		if (g_cur_entry >= g_cfg_num_files) { g_cur_entry = 0; }
	}
	memset(memsurface_data, 0, 512*512*4);
	memset(used, 0, sizeof(used));
	memset(used2, 0, sizeof(used2));
	cur_mouse_address =0;
	audio_samples_written = 0;
	last_pc = -1;

	// draw one-time stuff
	if (!g_cfg_novideo)
	{
		SDL_FillRect(screen, NULL, 0);
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

		snprintf(tmpbuf, sizeof(tmpbuf), " QUIT - PAUSE - RESTART - PREV - NEXT - WRITE MASK");
		sdlfont_drawString(screen, 0, screen->h-9, tmpbuf, color_screen_yellow);

		/* information */
		sdlfont_drawString(screen, INFO_X, INFO_Y, "      - Info -", color_screen_white);
		snprintf(tmpbuf, sizeof(tmpbuf), "Filename: %s", g_real_filename);
		sdlfont_drawString(screen, INFO_X, INFO_Y+8, tmpbuf, color_screen_white);
		snprintf(tmpbuf, sizeof(tmpbuf), "Title...: %s", tag.title);
		sdlfont_drawString(screen, INFO_X, INFO_Y+16, tmpbuf, color_screen_white);
		snprintf(tmpbuf, sizeof(tmpbuf), "Game....: %s", tag.game_title);
		sdlfont_drawString(screen, INFO_X, INFO_Y+24, tmpbuf, color_screen_white);
		snprintf(tmpbuf, sizeof(tmpbuf), "Dumper..: %s", tag.name_of_dumper);
		sdlfont_drawString(screen, INFO_X, INFO_Y+32, tmpbuf, color_screen_white);
		snprintf(tmpbuf, sizeof(tmpbuf), "Comment.: %s", tag.comments);
		sdlfont_drawString(screen, INFO_X, INFO_Y+40, tmpbuf, color_screen_white);
		
		snprintf(tmpbuf, sizeof(tmpbuf), "Echo....: %s", spc_config.is_echo ? "On" : "Off");	
		sdlfont_drawString(screen, INFO_X, INFO_Y+56, tmpbuf, color_screen_white);

		snprintf(tmpbuf, sizeof(tmpbuf), "Interp. : %s", spc_config.is_interpolation ? "On" : "Off");	
		sdlfont_drawString(screen, INFO_X, INFO_Y+64, tmpbuf, color_screen_white);
	
		snprintf(tmpbuf, sizeof(tmpbuf), "Autowrite mask.: %s", g_cfg_autowritemask ? "Yes" : "No");
		sdlfont_drawString(screen, INFO_X, INFO_Y+72, tmpbuf, color_screen_white);

		snprintf(tmpbuf, sizeof(tmpbuf), "Ignore tag time: %s", g_cfg_ignoretagtime ? "Yes" : "No");
		sdlfont_drawString(screen, INFO_X, INFO_Y+80, tmpbuf, color_screen_white);

		snprintf(tmpbuf, sizeof(tmpbuf), "Default time...: %d", g_cfg_defaultsongtime);
		sdlfont_drawString(screen, INFO_X, INFO_Y+88, tmpbuf, color_screen_white);

		
		sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y, "     - Port tool -", color_screen_white);
	}


	song_started_ticks = 0;

	if (!g_cfg_nosound) {
		SDL_PauseAudio(0);
	}
	g_paused = 0;
    for (;;) 
	{
		SDL_Event ev;

		pack_mask(packed_mask);
		
		if (g_cfg_statusline) {
			printf("%s  %d / %d (%d %%)        \r", 
					g_cfg_playlist[g_cur_entry],
					audio_samples_written/44100,
					song_time,
					(audio_samples_written/44100)/(song_time));
			fflush(stdout);
		}
		
		/* Check if it is time to change tune.
		 */		
		if (audio_samples_written/44100 >= song_time) 
		{
			if (g_cfg_autowritemask) {
				write_mask(packed_mask);
				if (g_cfg_apply_block) {
					printf("Applying mask on file %s using $%02X as filler\n", g_cfg_playlist[g_cur_entry], g_cfg_filler);
					applyBlockMask(g_cfg_playlist[g_cur_entry]);
				}
			}
			if (!g_cfg_nosound) {
				SDL_PauseAudio(1);
			}
			g_cur_entry++;
			if (g_cur_entry>=g_cfg_num_files) { goto clean; }
			goto reload;
		}
		
		
		if (!g_cfg_novideo)
		{
			//if (0))
			while (SDL_PollEvent(&ev))
			{
				switch (ev.type)
				{
					case SDL_KEYDOWN:
						{
							SDL_Keycode sym = ev.key.keysym.sym;

							if (sym == SDLK_ESCAPE) {
								if (!g_cfg_nosound) {
									SDL_PauseAudio(1);
								}
								goto clean;
							} else if (sym == SDLK_SPACE) {
								g_paused = !g_paused;
								SDL_PauseAudio(g_paused);
							}
							else if ((sym >= SDLK_1) && (sym <= SDLK_8)) {
								SoundData.forceMute[sym-SDLK_1] = !SoundData.forceMute[sym-SDLK_1];
							}
							else if (sym == SDLK_0) { // mute/unmute all
								int c, mute;
								mute = !SoundData.forceMute[0];
								for (c=0; c<8; c++) {
									SoundData.forceMute[c] = mute;
								}
							}
							else if (sym == SDLK_n) {
								SDL_PauseAudio(1);
								g_cur_entry++;
								if (g_cur_entry>=g_cfg_num_files) { g_cur_entry = 0; }
								goto reload;
							}
							else if (sym == SDLK_p) {
								SDL_PauseAudio(1);
								g_cur_entry--;
								if (g_cur_entry<0) { g_cur_entry = g_cfg_num_files-1; }
								goto reload;
							}
							else if (sym == SDLK_r) {
								SDL_PauseAudio(1);
								goto reload;
							}
						}
						break;
					case SDL_QUIT:
						if (!g_cfg_nosound) {
							SDL_PauseAudio(1);
						}
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
								if (!hexdump_locked) {
									hexdump_address = cur_mouse_address;
								}
	//							printf("%d,%d: $%04X\n", x, y, y*256+x);
							}
							else
							{
								//cur_mouse_address = -1;
							}

						
						}
						break;
					case SDL_MOUSEBUTTONDOWN:						
						{
							// click in memory view. Toggle lock
							if (	ev.motion.x >= MEMORY_VIEW_X && 
									ev.motion.x < MEMORY_VIEW_X + 512 &&
									ev.motion.y >= MEMORY_VIEW_Y &&
									ev.motion.y < MEMORY_VIEW_Y + 512)
							{
								hexdump_locked = (!hexdump_locked);
							}

							/* porttool */
							if (	(ev.button.x >= PORTTOOL_X + (8*5)) &&
									ev.button.y >= PORTTOOL_Y)
							{
								int x, y;
								x = ev.button.x - (PORTTOOL_X + (8*5));
								x /= 8;
								y = ev.button.y - PORTTOOL_Y;
								y /= 8;
								if (y==1) //top row in porttool
								{
//									printf("%d\n", x);
									if (ev.button.button == SDL_BUTTON_LEFT)
									{
										switch (x)
										{
											case 1: IAPU.RAM[0xf4]++; break;
											case 4: IAPU.RAM[0xf4]--; break;
											case 6: IAPU.RAM[0xf5]++; break;
											case 9: IAPU.RAM[0xf5]--; break;
											case 11: IAPU.RAM[0xf6]++; break;
											case 14: IAPU.RAM[0xf6]--; break;
											case 16: IAPU.RAM[0xf7]++; break;
											case 19: IAPU.RAM[0xf7]--; break;
										}
									}
								}
							}

							/* menu bar */
							if (
								((ev.button.y >screen->h-12) && (ev.button.y<screen->h)))
							{
								int x = ev.button.x / 8;
								if (x>=1 && x<=4) { goto clean; } // exit
								if (x>=8 && x<=12) { 
									if (g_paused) { 
										g_paused = 0; SDL_PauseAudio(0); 
									} else {
										g_paused = 1; SDL_PauseAudio(1);
									}
								} // pause

								if (x>=16 && x<=22) {  // restart
									SDL_PauseAudio(1);
									goto reload;
								}

								if (x>=26 && x<=29) {  // prev
									SDL_PauseAudio(1);
									g_cur_entry--;
									if (g_cur_entry<0) { g_cur_entry = g_cfg_num_files-1; }
									goto reload;

								}

								if (x>=33 && x<=36) { // next
									SDL_PauseAudio(1);
									g_cur_entry++;
									if (g_cur_entry>=g_cfg_num_files) { g_cur_entry = 0; }
									goto reload;
								}

								if (x>=41 && x<=50) { // write mask
									write_mask(packed_mask);
								}
							}
						}
						break;

					case SDL_MOUSEWHEEL:
#if (SDL_VERSION_ATLEAST(2,26,0))
							mouse_x = ev.wheel.mouseX - (PORTTOOL_X + (8*5));
							mouse_y = ev.wheel.mouseY - PORTTOOL_Y;
#else
							SDL_GetMouseState(&mouse_x, &mouse_y);
#endif

						/* portool */
						if (	(mouse_x >= PORTTOOL_X + (8*5)) &&
									mouse_y >= PORTTOOL_Y)
						{
							int x, y;

							x = mouse_x - (PORTTOOL_X + (8*5));
							y = mouse_y - PORTTOOL_Y;
							x /= 8;
							y /= 8;

							printf("Mousewheel %d %d\n", x, y);
							if (y==1) {
								if (ev.wheel.y > 0) { i=1; } else { i = -1; }
								if (x>1 && x<4) { IAPU.RAM[0xf4] += i; }
								if (x>6 && x<9) { IAPU.RAM[0xf5] += i; }
								if (x>11 && x<14) { IAPU.RAM[0xf6] += i; }
								if (x>16 && x<19) { IAPU.RAM[0xf7] += i; }
							}
						}
						break;
				}
			} // while (pollevent)

		} // !g_cfg_novideo

		if (g_cfg_nosound) {
			int i;
			for (i=0; i<4; i++) {
				SPC_update(&audiobuf[audio_buf_bytes]);
				audio_samples_written += spc_buf_size/4; // 16bit stereo
				if (g_waveWriter) {
					waveWriter_addSamples(g_waveWriter, &audiobuf[audio_buf_bytes], spc_buf_size/4);
				}
			}
		}
		else
		{	
			if (!g_cfg_update_in_callback && !g_paused)
			{
				// fill the buffer when possible

				while (BUFFER_SIZE - audio_buf_bytes >= spc_buf_size )
				{
					if (!g_cfg_novideo) {
						if (SDL_MUSTLOCK(memsurface)) {
							SDL_LockSurface(memsurface);
						}
					}
					while (BUFFER_SIZE - audio_buf_bytes >= spc_buf_size) {						
						SDL_LockAudio();						
						SPC_update(&audiobuf[audio_buf_bytes]);						
						if (g_waveWriter) {
							waveWriter_addSamples(g_waveWriter, &audiobuf[audio_buf_bytes], spc_buf_size/4);
						}
						SDL_UnlockAudio();
						audio_buf_bytes += spc_buf_size;
					}
					
					if (!g_cfg_novideo) {
						if (SDL_MUSTLOCK(memsurface)) {
							SDL_UnlockSurface(memsurface);
						}
					}					
				}
			}

		}

//return 0; // not reached		

		if (!g_cfg_novideo)
		{
			time_cur = SDL_GetTicks();
			do_scroller(time_cur - time_last);
			
			fade_arrays();			
			memrect.x = MEMORY_VIEW_X; memrect.y = MEMORY_VIEW_Y;
			
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
			
			snprintf(tmpbuf, sizeof(tmpbuf), "Blocks used: %3d/256 (%.1f%%)  ", tmp, (float)tmp*100.0/256.0);
			sdlfont_drawString(screen, MEMORY_VIEW_X, MEMORY_VIEW_Y + memsurface->h + 2, tmpbuf, color_screen_white);

			if (1)
			{
				snprintf(tmpbuf, sizeof(tmpbuf), "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
						packed_mask[0], packed_mask[1], packed_mask[2], packed_mask[3],
						packed_mask[4], packed_mask[5], packed_mask[6], packed_mask[7],
						packed_mask[8], packed_mask[9], packed_mask[10], packed_mask[11],
						packed_mask[12], packed_mask[13], packed_mask[14], packed_mask[15],
						packed_mask[16], packed_mask[17], packed_mask[18], packed_mask[19],
						packed_mask[20], packed_mask[21], packed_mask[22], packed_mask[23],
						packed_mask[24], packed_mask[25], packed_mask[26], packed_mask[27],
						packed_mask[28], packed_mask[29], packed_mask[30], packed_mask[31]);

				sdlfont_drawString(screen, MEMORY_VIEW_X, MEMORY_VIEW_Y + memsurface->h + 2 + 9, tmpbuf, color_screen_white);
			}
			i = 32;

			// write the address under mouse cursor
			if (cur_mouse_address >=0)
			{
				snprintf(tmpbuf, sizeof(tmpbuf), "Addr mouse: $%04X", cur_mouse_address);
				sdlfont_drawString(screen, MEMORY_VIEW_X+8*(23), MEMORY_VIEW_Y-10, tmpbuf, color_screen_white);
			}

			// write the program counter
			snprintf(tmpbuf, sizeof(tmpbuf), "PC: $%04x  ", last_pc);
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
				
				snprintf(tmpbuf, sizeof(tmpbuf),"%d:",i);
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

				snprintf(tmpbuf, sizeof(tmpbuf),"%d:", i);

				if (SoundData.forceMute[i]) {
					sdlfont_drawString(screen, MEMORY_VIEW_X+720, tmp + (i*10), "MUTED", color_screen_white);
				} else {
				}

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

			sdlfont_drawString(screen, MEMORY_VIEW_X+520, tmp, "  - Mouseover Hexdump -", color_screen_white);
			if (hexdump_locked) {
				sdlfont_drawString(screen, MEMORY_VIEW_X+520+24*8, tmp, "locked", color_screen_red);
			} else {
				sdlfont_drawString(screen, MEMORY_VIEW_X+520+24*8, tmp, "      ", color_screen_red);
			}
			
			tmp+=9;
			if (cur_mouse_address>=0)
			{
		
				for (i=0; i<128; i+=8)
				{
					unsigned char *st = &IAPU.RAM[(hexdump_address+i) & 0xffff];
					int p = MEMORY_VIEW_X+520, j;
					snprintf(tmpbuf, sizeof(tmpbuf), "%04X: ", (hexdump_address+i) & 0xffff);
					sdlfont_drawString(screen, p, tmp, tmpbuf, color_screen_white);
					p += 6*8;
					for (j=0; j<8; j++) {
						int idx = ((hexdump_address+i+j)&0xff00)<<4;	
						Uint32 color;

						idx += ((hexdump_address+i+j) % 256)<<3;
						color = SDL_MapRGB(screen->format, 
								0x7f + (memsurface_data[idx]>>1), 
								0x7f + (memsurface_data[idx+1]>>1), 
								0x7f + (memsurface_data[idx+2]>>1)
								);
								
						snprintf(tmpbuf, sizeof(tmpbuf), "%02X ", *st);
						st++;
						if (st - IAPU.RAM == 0x10000) {
							st = IAPU.RAM;
						}
						sdlfont_drawString(screen, p, tmp, tmpbuf, color);
						p+= 2*8 + 4;
					}
					
					tmp += 9;
				}
			}


			sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y+8, " APU:", color_screen_white);
			sdlfont_drawString(screen, PORTTOOL_X, PORTTOOL_Y+16, "SNES:", color_screen_white);

			snprintf(tmpbuf, sizeof(tmpbuf), " +%02X- +%02X- +%02X- +%02X-", IAPU.RAM[0xf4], IAPU.RAM[0xf5], IAPU.RAM[0xf6], IAPU.RAM[0xf7]);		
			sdlfont_drawString(screen, PORTTOOL_X + (8*5), PORTTOOL_Y+8, tmpbuf, color_screen_white);
			
			snprintf(tmpbuf, sizeof(tmpbuf), "  %02X   %02X   %02X   %02X", APU.OutPorts[0], APU.OutPorts[1], APU.OutPorts[2], APU.OutPorts[3]);		
			sdlfont_drawString(screen, PORTTOOL_X + (8*5), PORTTOOL_Y+16, tmpbuf, color_screen_white);

			current_ticks = audio_samples_written/44100;
			snprintf(tmpbuf, sizeof(tmpbuf), "Time....: %0d:%02d / %0d:%02d", 
					(current_ticks-song_started_ticks)/60,
					((current_ticks-song_started_ticks))%60,
					song_time/60, song_time%60);
			sdlfont_drawString(screen, INFO_X, INFO_Y+48, tmpbuf, color_screen_white);



			SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);

			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);

			time_last = time_cur;
			if (g_cfg_nice) {  SDL_Delay(100); }
		} // if !g_cfg_novideo
	}
clean:
	SDL_PauseAudio(1);
	SDL_Quit();
    SPC_close();
	if (g_waveWriter) {
		waveWriter_close(g_waveWriter);
		waveWriter_free(g_waveWriter);
	}

    return 0;
}


