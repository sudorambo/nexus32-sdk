/*
 * wav2smp — Convert WAV to NEXUS-32 .smp. Reads RIFF WAV, outputs format/rate/length/samples.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
	const char *in = NULL, *out = NULL;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'o' && i + 1 < argc) { out = argv[++i]; continue; }
		if (argv[i][0] != '-') in = argv[i];
	}
	if (!in || !out) {
		fprintf(stderr, "usage: wav2smp -o out.smp input.wav\n");
		return 1;
	}
	FILE *f = fopen(in, "rb");
	if (!f) { fprintf(stderr, "wav2smp: cannot open %s\n", in); return 1; }
	unsigned char hdr[44];
	if (fread(hdr, 1, 44, f) != 44 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
		fprintf(stderr, "wav2smp: not a WAV file\n");
		fclose(f);
		return 1;
	}
	unsigned channels = (unsigned)hdr[22] | ((unsigned)hdr[23] << 8);
	unsigned rate = (unsigned)hdr[24] | ((unsigned)hdr[25]<<8) | ((unsigned)hdr[26]<<16) | ((unsigned)hdr[27]<<24);
	unsigned bits = (unsigned)hdr[34] | ((unsigned)hdr[35] << 8);
	(void)channels;
	(void)bits;
	fseek(f, 0, SEEK_END);
	long data_len = ftell(f) - 44;
	if (data_len < 0) data_len = 0;
	unsigned total_samples = (unsigned)(data_len / 2);
	if (rate != 48000 && rate != 0) {
		total_samples = (unsigned)((unsigned long)total_samples * 48000UL / (unsigned long)rate);
		rate = 48000;
	}
	rewind(f);
	fseek(f, 44, SEEK_SET);
	unsigned char *samples = malloc(total_samples * 2 > 0 ? (size_t)(total_samples * 2) : 1);
	if (!samples) { fclose(f); return 1; }
	size_t read_len = fread(samples, 1, (size_t)(data_len > (long)(total_samples*2) ? total_samples*2 : data_len), f);
	fclose(f);
	FILE *outf = fopen(out, "wb");
	if (!outf) { free(samples); return 1; }
	unsigned v32 = 1;
	fwrite(&v32, 4, 1, outf);
	fwrite(&rate, 4, 1, outf);
	v32 = total_samples;
	fwrite(&v32, 4, 1, outf);
	v32 = 0;
	fwrite(&v32, 4, 1, outf);
	fwrite(samples, 1, read_len, outf);
	free(samples);
	fclose(outf);
	return 0;
}
