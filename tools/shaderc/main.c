/*
 * shaderc — Mini shader compiler. Parses assembly-like source, emits binary .shd.
 * Uses opcodes from spec §5.6 (MOV=0, ADD=1, ... NOP=0x15). Max 64 instructions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_INSNS 64

static const struct { const char *mnem; unsigned op; } opcodes[] = {
	{"MOV",0},{"ADD",1},{"SUB",2},{"MUL",3},{"MAD",4},{"DP3",5},{"DP4",6},
	{"RSQ",7},{"RCP",8},{"MIN",9},{"MAX",0x0A},{"CLAMP",0x0B},{"LERP",0x0C},
	{"TEX",0x0D},{"CMP",0x0E},{"ABS",0x0F},{"NEG",0x10},{"FRC",0x11},
	{"FLR",0x12},{"EXP",0x13},{"LOG",0x14},{"NOP",0x15},
	{NULL,0}
};

static unsigned get_opcode(const char *mnem) {
	for (int i = 0; opcodes[i].mnem; i++)
		if (strcmp(opcodes[i].mnem, mnem) == 0)
			return opcodes[i].op;
	return 0xFF;
}

int main(int argc, char **argv) {
	const char *in = NULL, *out = NULL;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'o' && i + 1 < argc) { out = argv[++i]; continue; }
		if (argv[i][0] != '-') in = argv[i];
	}
	if (!in || !out) {
		fprintf(stderr, "usage: shaderc -o out.shd input.shader\n");
		return 1;
	}
	FILE *f = fopen(in, "r");
	if (!f) { fprintf(stderr, "shaderc: cannot open %s\n", in); return 1; }
	unsigned char insns[MAX_INSNS];
	int n = 0;
	char line[256];
	while (n < MAX_INSNS && fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || *p == '\0') continue;
		char mnem[16];
		int i = 0;
		while (*p && !isspace((unsigned char)*p) && i < 15) mnem[i++] = *p++;
		mnem[i] = '\0';
		if (i == 0) continue;
		unsigned op = get_opcode(mnem);
		if (op == 0xFF) { fprintf(stderr, "shaderc: unknown opcode %s\n", mnem); fclose(f); return 1; }
		insns[n++] = (unsigned char)op;
	}
	fclose(f);
	FILE *outf = fopen(out, "wb");
	if (!outf) { fprintf(stderr, "shaderc: cannot write %s\n", out); return 1; }
	if (fwrite(insns, 1, (size_t)n, outf) != (size_t)n) { fclose(outf); return 1; }
	while (n < MAX_INSNS) { unsigned char pad = 0x15; fwrite(&pad, 1, 1, outf); n++; }
	fclose(outf);
	return 0;
}
