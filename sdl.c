#include "id.h"
#include "config.h"
#include "sdl.h"
#include "SDL/SDL.h"
#include "shared.h"

#if SDLVersion 2

#endif

void my_audio_callback(void *userdata, Uint8 *stream, int len)
{
//	printf("Callback %d  audio_buf_bytes: %d\n", len, audio_buf_bytes);
	
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

void VSPCPlay_SDL_Init() {
	SDL_AudioSpec desired;
	uint32_t flags=0;
	
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

		SDL_WM_SetCaption(PROG_NAME_VERSION_STRING, NULL);
		
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