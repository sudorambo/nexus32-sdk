/*
 * map2lvl — Convert tile map to .lvl. Reads simple text: first line "W H", then W*H tile IDs (decimal, space/newline separated).
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
		fprintf(stderr, "usage: map2lvl -o out.lvl input.txt\n");
		return 1;
	}
	FILE *f = fopen(in, "r");
	if (!f) { fprintf(stderr, "map2lvl: cannot open %s\n", in); return 1; }
	int w, h;
	if (fscanf(f, "%d %d", &w, &h) != 2 || w <= 0 || h <= 0 || w > 4096 || h > 4096) {
		fclose(f);
		return 1;
	}
	size_t n = (size_t)(w * h);
	unsigned short *tiles = malloc(n * sizeof(unsigned short));
	if (!tiles) { fclose(f); return 1; }
	for (size_t i = 0; i < n; i++) {
		int t;
		if (fscanf(f, "%d", &t) != 1) t = 0;
		tiles[i] = (unsigned short)(t & 0xFFFF);
	}
	fclose(f);
	FILE *outf = fopen(out, "wb");
	if (!outf) { free(tiles); return 1; }
	unsigned u32 = (unsigned)w;
	fwrite(&u32, 4, 1, outf);
	u32 = (unsigned)h;
	fwrite(&u32, 4, 1, outf);
	fwrite(tiles, 2, n, outf);
	free(tiles);
	fclose(outf);
	return 0;
}
