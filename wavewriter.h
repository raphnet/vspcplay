#ifndef _wavewriter_h__
#define _wavewriter_h__

// A simple 44khz, 16-bit, stereo only wave writer

typedef struct _waveWriter WaveWriter;

WaveWriter *waveWriter_create(const char *filename);
int waveWriter_addSamples(WaveWriter *wr, void *data, int n_samples);
int waveWriter_close(WaveWriter *wr);
void waveWriter_free(WaveWriter *wr);


#endif // _wavewriter_h__
