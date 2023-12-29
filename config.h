#include "wavewriter.h"
#include <stddef.h>
#include "shared.h"
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
static char *g_real_filename = NULL; // holds the filename minus path
static const char *g_outwavefile = NULL;
static WaveWriter *g_waveWriter = NULL;
static unsigned char muted_at_startup[8];
static int just_show_info = 0;