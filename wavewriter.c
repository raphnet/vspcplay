#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wavewriter.h"

struct _waveWriter {
	FILE *fptr;
	unsigned int samples_written;
	unsigned short channels;
	int bytes_per_sample;
};

WaveWriter *waveWriter_create(const char *filename)
{
	unsigned char tmp[64];
	unsigned int samplesPerSecond = 44100;
	unsigned int bytesPerSecond;
	unsigned short blockalign;
	unsigned short bits_per_sample = 16;
	WaveWriter *wr;

	if (!filename) {
		return NULL;
	}

	wr = calloc(1, sizeof(WaveWriter));
	if (!wr) {
		perror("WaveWriter");
		return NULL;
	}

	wr->fptr = fopen(filename, "wb");
	if (!wr->fptr) {
		perror(filename);
		free(wr);
		return NULL;
	}

	wr->channels = 2;
	wr->bytes_per_sample = 2;
	bytesPerSecond = wr->bytes_per_sample * wr->channels * samplesPerSecond;
	blockalign = wr->channels * wr->bytes_per_sample;

	memset(tmp, 0, sizeof(tmp));

	// riff chunk
	tmp[0] = 'R';
	tmp[1] = 'I';
	tmp[2] = 'F';
	tmp[3] = 'F';
#define RIFF_CHUNK_SIZE_OFFSET	4
	// 4,5,6,7 -> chunk size  -- to be set when closing

	// ID
	tmp[8] = 'W';
	tmp[9] = 'A';
	tmp[10] = 'V';
	tmp[11] = 'E';

	// format chunk
	tmp[12] = 'f';
	tmp[13] = 'm';
	tmp[14] = 't';
	tmp[15] = ' ';

	tmp[16] = 16; // chunk size = 16
	tmp[17] = 0;
	tmp[18] = 0;
	tmp[19] = 0;

	tmp[20] = 0x01; // 0x0001 PCM format
	tmp[21] = 0x00;
	tmp[22] = wr->channels; // 2 channels
	tmp[23] = wr->channels >> 2;
	tmp[24] = samplesPerSecond;
	tmp[25] = samplesPerSecond >> 8;
	tmp[26] = samplesPerSecond >> 16;
	tmp[27] = samplesPerSecond >> 24;

	tmp[28] = bytesPerSecond;
	tmp[29] = bytesPerSecond >> 8;
	tmp[30] = bytesPerSecond >> 16;
	tmp[31] = bytesPerSecond >> 24;

	tmp[32] = blockalign;
	tmp[33] = blockalign >> 8;

	tmp[34] = bits_per_sample;
	tmp[35] = bits_per_sample >> 8;

	tmp[36] = 'd';
	tmp[37] = 'a';
	tmp[38] = 't';
	tmp[39] = 'a';

#define DATA_CHUNK_SIZE_OFFSET	40
	// 40,41,42,43 : data size

	if (1 != fwrite(tmp, 44, 1, wr->fptr)) {
		perror("could not write WAVE header");
		fclose(wr->fptr);
		free(wr);
		return NULL;
	}

	return wr;
}

int waveWriter_init(WaveWriter *wr, const char *filename)
{


	return 0;
}

int waveWriter_addSamples(WaveWriter *wr, void *data, int n_samples)
{
	if (!wr) {
		return -1;
	}
	if (!wr->fptr) {
		return -1;
	}

	if (1 != fwrite(data, wr->bytes_per_sample * wr->channels * n_samples, 1, wr->fptr)) {
		perror("error writing to wave file");
		return -1;
	}

	wr->samples_written += n_samples;

	return 0;
}

int waveWriter_close(WaveWriter *wr)
{
	unsigned int datasize;
	unsigned char buf[4];

	if (!wr) {
		return -1;
	}
	if (!wr->fptr) {
		return -1;
	}

	datasize = wr->samples_written * wr->bytes_per_sample * wr->channels;
	buf[0] = datasize;
	buf[1] = datasize >> 8;
	buf[2] = datasize >> 16;
	buf[3] = datasize >> 24;
	if (fseek(wr->fptr, RIFF_CHUNK_SIZE_OFFSET, SEEK_SET)) {
		goto error;
	}
	if (1 != fwrite(buf, 4, 1, wr->fptr)) {
		goto error;
	}

	datasize += 28; // add format chunk and WAVEID fields
	buf[0] = datasize;
	buf[1] = datasize >> 8;
	buf[2] = datasize >> 16;
	buf[3] = datasize >> 24;
	if (fseek(wr->fptr, DATA_CHUNK_SIZE_OFFSET, SEEK_SET)) {
		goto error;
	}
	if (1 != fwrite(buf, 4, 1, wr->fptr)) {
		goto error;
	}

	fclose(wr->fptr);
	wr->fptr = NULL;
	return 0;

error:
	perror("error finalizing wave file");
	fclose(wr->fptr);
	wr->fptr = NULL;

	return -1;
}

void waveWriter_free(WaveWriter *wr)
{
	if (wr) {
		if (wr->fptr) {
			fprintf(stderr, "WaveWriter: auto-calling close()\n");
			waveWriter_close(wr);
		}
		free(wr);
	}
}
