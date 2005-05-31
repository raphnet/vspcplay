#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "plugin.h"
#include "plugin_manager.h"
#include "file_info.h"
#include "libspc.h"


#define PLUGIN_NAME	"spcplay"

static int playing = 0;
static int paused = 0;

static unsigned char *buffer=NULL;
static int buffer_size=0;
static int bytes_in_buffer=0;

static MikMood_FileInfo *fi=NULL;
static AudioFMT fmt;
static MikMood_OutputPlugin *g_outp=NULL;

static SPC_Config spc_config;
static int bytes_per_update = 0;
static SPC_ID666 *id666 = NULL;

static char *fileexts[] = { "spc", NULL };

static PLUGIN_CONFIG *cur_config = NULL;

#define DEFAULT_PLAYTIME	150
#define DEFAULT_FADETIME	2

DECLARE_TIME_COUNTER(elaps_seconds);

static int plugin_in_init()
{
	
//	buffer = malloc(sizeof(short)*SAMPLES_BUF);
//	if (buffer == NULL) { return -1; }

	cur_config = pluginConfig_create(PLUGIN_NAME);

	pluginConfig_addParam(cur_config, "ECHO", MM_PARAM_BOOLEAN);
	pluginConfig_setBool(cur_config, "ECHO", 0);
	pluginConfig_addParam(cur_config, "INTERPOLATION", MM_PARAM_BOOLEAN);
	pluginConfig_setBool(cur_config, "INTERPOLATION", 0);
	
	spc_config.sampling_rate = 44100;
	spc_config.resolution = 16;
	spc_config.channels = 2;
	spc_config.is_interpolation = 0;
	spc_config.is_echo = 0;

	bytes_per_update = SPC_init(&spc_config);
	printf("bytes_per_update = %d\n", bytes_per_update);
	
	if (!bytes_per_update) { return -1; }

	buffer_size = bytes_per_update * 2;
//	buffer_size *= 4;
	
	
	/* 16 bit, stereo */
	buffer = malloc(buffer_size);
	if (buffer == NULL) {
		SPC_close();
		return -1;
	}

	playing = 0;
	paused = 0;

	return 0;
}

static void plugin_in_shutdown()
{
	if (fi) { FI_free(fi); fi = NULL; }
	if (buffer) { free(buffer); buffer = NULL; }
	SPC_close();
	if (id666) { free(id666); id666 = NULL; }
	g_outp = NULL;
}

static int plugin_in_testFP(FILE *fp)
{
	unsigned char magic[12];
	
	fseek(fp, 0, SEEK_SET);
	fread(magic, 11, 1, fp);
	magic[11] = 0;

	if (!strcmp("SNES-SPC700", magic))
	{
		return 1; // looks good
	}
	return 0;
}

static int plugin_in_setFP(FILE *fp)
{
	int res;

	playing = 0;
	paused = 1;
	
	if (fi) { FI_free(fi); fi = NULL; }

	fseek(fp, 0, SEEK_SET);
	res = SPC_loadFP(fp);

	if (!res) { return -1; }

	fi = FI_create(0, 0);
	if (!fi) {
		SPC_close(); 
		return -1;
	}

	if (id666) { free(id666); }
	id666 = SPC_get_id666FP(fp);	
	if (id666)
	{
		fi->title = id666->songname;
		fi->comments = id666->comments;
		fi->author = id666->author;
		fi->total_time = id666->playtime + id666->fadetime;
		fi->current_time = 0;
	}
	
	fi->type = "SNES SPC700 Sound File Data v0.30";
	fi->num_channels = 8;
	fi->num_channels_in_use = 8;
	if (!fi->total_time) { fi->total_time = DEFAULT_PLAYTIME; }


	return 0;
}

static int plugin_in_play(MikMood_OutputPlugin *outp)
{
	g_outp = outp;
	
	if (g_outp==NULL) { return -1; }

	SPC_set_state(&spc_config);

	fmt.format = AUDIO_FORMAT_16_S;
	fmt.sample_rate = 44100;
	fmt.stereo = 1;
	g_outp->openAudio(&fmt);
	
	playing = 1;
	paused = 0;
	
	return 0;
}

static void plugin_in_stop(void)
{
	if (g_outp==NULL) { return; }
	g_outp->closeAudio();
	g_outp = NULL;

	playing = 0;
}

static void plugin_in_update(void)
{
	int must_do;
	int null_stride = 0;

	if (!playing || paused) { return; }
	
	if (g_outp==NULL) { return; }
	must_do = g_outp->getFree(); // must_do is the number of *samples*

	while (buffer_size - bytes_in_buffer > bytes_per_update)
	{
		SPC_update(&buffer[bytes_in_buffer]);
		bytes_in_buffer += bytes_per_update;
	}
	
	if (must_do*4 > bytes_in_buffer) {
		/* the sound driver is ready for more samples than
		we can offer right now... give what we have. */
		must_do = bytes_in_buffer/4;
	}
#if 0
	{
		int i;
		for (i=0; i<must_do * 4; i++)
		{
//			printf("%02x ", buffer[i]);
			if (!buffer[i]) null_stride++; else null_stride = 0;
			if (null_stride > 60) {
				printf("Stride!\n"); null_stride = 0;
			}
		}
//		printf("\n");
	}
#endif
	g_outp->writeAudio(buffer, must_do);
	memmove(buffer, &buffer[must_do*4], buffer_size - must_do*4);
	bytes_in_buffer -= must_do * 4;

	ADD_SAMPLES_TO_COUNTER(elaps_seconds, must_do, &fmt);
	
	fi->current_time += GET_SECONDS_FROM_COUNTER(elaps_seconds, &fmt);
	//fi->sng_progress += GET_SECONDS_FROM_COUNTER(elaps_seconds, &fmt);
//	printf("fi->current_time %d\n", fi->current_time);
	
	REMOVE_SECONDS_FROM_COUNTER(elaps_seconds, &fmt);

	if (fi->current_time > fi->total_time) {
		plugin_in_stop();	
	}
}

static int plugin_in_isStopped(void)
{
	return !playing;
}

static void plugin_in_togglePause(void)
{
	paused = !(paused);
}

static int plugin_in_isPaused(void)
{
	return paused;
}

static char *plugin_in_getName(void) { return PLUGIN_NAME; }
static char *plugin_in_getAbout(void) { return  "SPC file input plugin based Snes9x's APU emulation code"; }
static char **plugin_in_listFileExts(void) { return fileexts; }

static PLUGIN_CONFIG *plugin_in_getConfig(void) 
{
	return cur_config;
}

static void plugin_in_applyConfig(PLUGIN_CONFIG *cfg) 
{
	pluginConfig_copyParam(cur_config, cfg, "interpolation");
	pluginConfig_copyParam(cur_config, cfg, "echo");
	
	/* configure libspc */
	pluginConfig_getBool(cur_config, "interpolation", &spc_config.is_interpolation);
	pluginConfig_getBool(cur_config, "echo", &spc_config.is_echo);

	/* will be effective on next play */
}

static MikMood_FileInfo *plugin_in_getFileInfo(void)
{
	return fi;
}

static MikMood_InputPlugin plugin_in_spcplay = {
	plugin_in_getName,
	plugin_in_getAbout,
	plugin_in_init,
	plugin_in_shutdown,
	plugin_in_listFileExts,
	NULL, // testURL
	plugin_in_testFP,
	NULL, // testFile
	NULL, // setURL
	plugin_in_setFP,
	NULL, // setFile
	plugin_in_play,
	plugin_in_stop,
	plugin_in_isStopped,
	plugin_in_togglePause,
	plugin_in_isPaused,
	plugin_in_update,
	plugin_in_getFileInfo,
	NULL, // restart
	NULL, // ff
	NULL, // rew
	plugin_in_getConfig,
	plugin_in_applyConfig,

	NULL, // next
	NULL  // dlhandle
};

MikMood_InputPlugin *getInputPlugin(void) { return &plugin_in_spcplay; }

