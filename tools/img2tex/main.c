/*
 * img2tex — Convert image to .tex. Supports 24/32-bit BMP (no external deps).
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
		fprintf(stderr, "usage: img2tex -o out.tex input.bmp\n");
		return 1;
	}
	FILE *f = fopen(in, "rb");
	if (!f) { fprintf(stderr, "img2tex: cannot open %s\n", in); return 1; }
	unsigned char hdr[54];
	if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
		fprintf(stderr, "img2tex: not BMP\n");
		fclose(f);
		return 1;
	}
	int w = (int)(hdr[18] | (hdr[19]<<8) | (hdr[20]<<16) | (hdr[21]<<24));
	int h = (int)(hdr[22] | (hdr[23]<<8) | (hdr[24]<<16) | (hdr[25]<<24));
	int bpp = (int)(hdr[28] | (hdr[29]<<8));
	if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32)) {
		fclose(f);
		return 1;
	}
	int row = ((w * bpp / 8) + 3) & ~3;
	size_t pix_size = (size_t)(row * h);
	unsigned char *pix = malloc(pix_size);
	if (!pix) { fclose(f); return 1; }
	if (fread(pix, 1, pix_size, f) != pix_size) { free(pix); fclose(f); return 1; }
	fclose(f);
	FILE *outf = fopen(out, "wb");
	if (!outf) { free(pix); return 1; }
	unsigned fmt = 1;
	unsigned mips = 1;
	fwrite(&fmt, 4, 1, outf);
	fwrite(&w, 4, 1, outf);
	fwrite(&h, 4, 1, outf);
	fwrite(&mips, 4, 1, outf);
	for (int y = h - 1; y >= 0; y--) {
		for (int x = 0; x < w; x++) {
			unsigned char *p = pix + (size_t)y * row + (size_t)x * (bpp/8);
			unsigned char r = p[2], g = p[1], b = p[0], a = (bpp == 32) ? p[3] : 255;
			fwrite(&r, 1, 1, outf);
			fwrite(&g, 1, 1, outf);
			fwrite(&b, 1, 1, outf);
			fwrite(&a, 1, 1, outf);
		}
	}
	free(pix);
	fclose(outf);
	return 0;
}
