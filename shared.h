#ifndef VSPCPLAY_SHARED
#define VSPCPLAY_SHARED

// 5 minutes default
#define DEFAULT_SONGTIME	(60*5)

// TODO: Temporary, move into a file for audio?
#define AUDIO_BUFFER_SIZE 65536

unsigned char audiobuf[AUDIO_BUFFER_SIZE];
int audio_buf_bytes=0, spc_buf_size;
int audio_samples_written=0;

#endif