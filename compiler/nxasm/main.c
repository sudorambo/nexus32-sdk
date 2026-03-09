/*
 * nxasm — NEXUS-32 assembler. AT&T-style .asm -> .nxo.
 * Supports .section .text/.data/.rodata/.bss, .global, labels, integer mnemonics per spec §2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define NXO_MAGIC 0x004F584E  /* "NXO\0" */
#define NXO_VERSION 1
#define MAX_SYMBOLS 256
#define MAX_LABEL_LEN 63
#define TEXT_BASE  0x00000400u
#define DATA_BASE  0x00400000u

typedef enum { SECT_TEXT, SECT_DATA, SECT_RODATA, SECT_BSS } section_t;

typedef struct {
	char name[MAX_LABEL_LEN + 1];
	section_t section;
	uint32_t value;
	int is_global;
} symbol_t;

static section_t cur_section = SECT_TEXT;
static uint32_t text_size, data_size, rodata_size, bss_size;
static uint32_t text_off, data_off;
static unsigned char *text_buf;
static unsigned char *data_buf;
static size_t text_cap, data_cap;
static symbol_t symbols[MAX_SYMBOLS];
static int num_symbols;
static int global_names[MAX_SYMBOLS];
static int num_globals;
#define MAX_RELOCS 64
typedef struct { uint32_t offset; unsigned char type; char name[MAX_LABEL_LEN + 1]; } reloc_t;
static reloc_t relocs[MAX_RELOCS];
static int num_relocs;
#define RELOC_JAL 0

static void emit_text(uint32_t word)
{
	if (text_off + 4 > text_cap) {
		text_cap = text_cap ? text_cap * 2 : 4096;
		text_buf = realloc(text_buf, text_cap);
	}
	text_buf[text_off++] = (unsigned char)(word);
	text_buf[text_off++] = (unsigned char)(word >> 8);
	text_buf[text_off++] = (unsigned char)(word >> 16);
	text_buf[text_off++] = (unsigned char)(word >> 24);
	text_size = text_off;
}

static void emit_data(unsigned char byte)
{
	if (data_off + 1 > data_cap) {
		data_cap = data_cap ? data_cap * 2 : 4096;
		data_buf = realloc(data_buf, data_cap);
	}
	data_buf[data_off++] = byte;
	data_size = data_off;
}

static int find_symbol(const char *name)
{
	for (int i = 0; i < num_symbols; i++)
		if (strcmp(symbols[i].name, name) == 0)
			return i;
	return -1;
}

static int add_symbol(const char *name, section_t sec, uint32_t val, int global)
{
	if (num_symbols >= MAX_SYMBOLS) return -1;
	int i = find_symbol(name);
	if (i >= 0) {
		symbols[i].value = val;
		symbols[i].section = sec;
		if (global) symbols[i].is_global = 1;
		return i;
	}
	strncpy(symbols[num_symbols].name, name, MAX_LABEL_LEN);
	symbols[num_symbols].name[MAX_LABEL_LEN] = '\0';
	symbols[num_symbols].section = sec;
	symbols[num_symbols].value = val;
	symbols[num_symbols].is_global = global ? 1 : 0;
	return num_symbols++;
}

static void skip_whitespace(char **p)
{
	while (**p && (isspace((unsigned char)**p) || **p == '#')) {
		if (**p == '#') while (**p && **p != '\n') (*p)++;
		else (*p)++;
	}
}

static int parse_reg(char **p)
{
	skip_whitespace(p);
	if ((**p == 'r' || **p == '$') && isdigit((unsigned char)(*p)[1])) {
		int r = 0;
		if (**p == '$') (*p)++;
		(*p)++;
		while (isdigit((unsigned char)**p)) r = r * 10 + (**p - '0'), (*p)++;
		if (r <= 31) return r;
	}
	return -1;
}

static int parse_imm(char **p, int32_t *out)
{
	skip_whitespace(p);
	if (!**p) return -1;
	char *end;
	long v = strtol(*p, &end, 0);
	if (end == *p) return -1;
	*p = end;
	*out = (int32_t)v;
	return 0;
}

static int parse_label_ref(char **p, char *buf, size_t bufsz)
{
	skip_whitespace(p);
	size_t i = 0;
	while (**p && (isalnum((unsigned char)**p) || **p == '_') && i < bufsz - 1)
		buf[i++] = *(*p)++;
	buf[i] = '\0';
	return i > 0 ? 0 : -1;
}

/* Encode R-type: op=0, func, rd, rs, rt, shamt=0 */
static uint32_t enc_r(unsigned func, unsigned rd, unsigned rs, unsigned rt)
{
	return (0u << 26) | (rs << 21) | (rt << 16) | (rd << 11) | (0u << 6) | (func & 63u);
}
/* Encode I-type: op, rs, rt, imm16 */
static uint32_t enc_i(unsigned op, unsigned rs, unsigned rt, int32_t imm)
{
	return (op << 26) | (rs << 21) | (rt << 16) | ((uint32_t)(uint16_t)imm);
}
/* Encode J-type: op, target (26-bit, will be shifted by caller if needed) */
static uint32_t enc_j(unsigned op, uint32_t target)
{
	return (op << 26) | (target & 0x3FFFFFFu);
}
/* Encode S-type: op=0x3F, func */
static uint32_t enc_s(unsigned func)
{
	return (0x3Fu << 26) | (func & 63u);
}

static int encode_insn(const char *mnem, char *operands, uint32_t *out, uint32_t pc, uint32_t text_byte_off)
{
	(void)pc;
	char *p = operands;
	uint32_t w = 0;

	if (strcmp(mnem, "nop") == 0) { *out = enc_r(0, 0, 0, 0); return 0; }
	if (strcmp(mnem, "halt") == 0) { *out = enc_s(3); return 0; }
	if (strcmp(mnem, "eret") == 0) { *out = enc_s(4); return 0; }
	if (strcmp(mnem, "syscall") == 0) { *out = enc_s(0); return 0; }
	if (strcmp(mnem, "break") == 0) { *out = enc_s(1); return 0; }

	int rd = parse_reg(&p);
	if (rd < 0 && strchr("jr jalr", mnem[0]) ? 0 : 1) { /* some don't have rd */ }
	if (strcmp(mnem, "jr") == 0) { *out = enc_r(0x08, 0, rd, 0); return 0; }
	if (strcmp(mnem, "jalr") == 0) {
		skip_whitespace(&p);
		if (*p == ',') p++;
		int rs = parse_reg(&p);
		*out = enc_r(0x09, rd, rs, 0);
		return 0;
	}

	if (strcmp(mnem, "j") == 0 || strcmp(mnem, "jal") == 0) {
		char lab[MAX_LABEL_LEN + 1];
		if (parse_label_ref(&p, lab, sizeof(lab)) < 0) return -1;
		int idx = find_symbol(lab);
		uint32_t addr = (idx >= 0) ? symbols[idx].value : 0;
		addr = (addr >> 2) & 0x3FFFFFFu;
		*out = enc_j(mnem[1] == 'a' ? 0x03u : 0x02u, addr);
		return 0;
	}

	/* R-type with rd, rs, rt */
	if (strcmp(mnem, "add") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x20, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "addu") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x21, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "sub") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x22, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "subu") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x23, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "and") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x24, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "or") == 0)   { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x25, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "xor") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x26, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "nor") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x27, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "slt") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x2A, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "sltu") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); w = enc_r(0x2B, rd, rs, rt); *out = w; return 0; }
	if (strcmp(mnem, "sll") == 0)  {
		int rt = parse_reg(&p); if (*p == ',') p++;
		int shamt = 0;
		if (isdigit((unsigned char)*p)) { shamt = (int)strtol(p, &p, 0); }
		else { int rs = parse_reg(&p); (void)rs; shamt = 0; }
		w = enc_r(0x00, rd, 0, rt) | ((shamt & 31) << 6); *out = w; return 0;
	}
	if (strcmp(mnem, "srl") == 0)  { int rt = parse_reg(&p); if (*p == ',') p++; int shamt = (int)strtol(p, &p, 0); w = enc_r(0x02, rd, 0, rt) | ((shamt & 31) << 6); *out = w; return 0; }
	if (strcmp(mnem, "sra") == 0)  { int rt = parse_reg(&p); if (*p == ',') p++; int shamt = (int)strtol(p, &p, 0); w = enc_r(0x03, rd, 0, rt) | ((shamt & 31) << 6); *out = w; return 0; }

	/* I-type */
	int32_t imm = 0;
	if (strcmp(mnem, "addiu") == 0 || strcmp(mnem, "addi") == 0) {
		int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm);
		w = enc_i(0x09, rs, rd, imm); *out = w; return 0;
	}
	if (strcmp(mnem, "lui") == 0) {
		parse_imm(&p, &imm);
		w = enc_i(0x0F, 0, rd, (int32_t)(uint16_t)imm); *out = w; return 0;
	}
	if (strcmp(mnem, "andi") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm); w = enc_i(0x0C, rs, rd, imm); *out = w; return 0; }
	if (strcmp(mnem, "ori") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm); w = enc_i(0x0D, rs, rd, imm); *out = w; return 0; }
	if (strcmp(mnem, "xori") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm); w = enc_i(0x0E, rs, rd, imm); *out = w; return 0; }
	if (strcmp(mnem, "slti") == 0)  { int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm); w = enc_i(0x0A, rs, rd, imm); *out = w; return 0; }
	if (strcmp(mnem, "sltiu") == 0) { int rs = parse_reg(&p); if (*p == ',') p++; parse_imm(&p, &imm); w = enc_i(0x0B, rs, rd, imm); *out = w; return 0; }
	if (strcmp(mnem, "lw") == 0) {
		int rs; if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } else { parse_imm(&p, &imm); if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } }
		w = enc_i(0x23, rs, rd, imm); *out = w; return 0;
	}
	if (strcmp(mnem, "lhu") == 0) {
		int rs; if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } else { parse_imm(&p, &imm); if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } }
		w = enc_i(0x25, rs, rd, imm); *out = w; return 0;
	}
	if (strcmp(mnem, "sw") == 0) {
		int rs; if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } else { parse_imm(&p, &imm); if (*p == '(') { p++; rs = parse_reg(&p); if (*p == ')') p++; } }
		w = enc_i(0x2B, rs, rd, imm); *out = w; return 0;
	}
	if (strcmp(mnem, "beq") == 0) {
		int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); if (*p == ',') p++;
		char lab[MAX_LABEL_LEN + 1];
		if (parse_label_ref(&p, lab, sizeof(lab)) < 0) return -1;
		int idx = find_symbol(lab);
		uint32_t target = (idx >= 0) ? symbols[idx].value : pc + 4;
		imm = (int32_t)(target - (pc + 4)) >> 2;
		w = enc_i(0x04, rs, rt, imm); *out = w; return 0;
	}
	if (strcmp(mnem, "bne") == 0) {
		int rs = parse_reg(&p); if (*p == ',') p++; int rt = parse_reg(&p); if (*p == ',') p++;
		char lab[MAX_LABEL_LEN + 1];
		if (parse_label_ref(&p, lab, sizeof(lab)) < 0) return -1;
		int idx = find_symbol(lab);
		uint32_t target = (idx >= 0) ? symbols[idx].value : pc + 4;
		imm = (int32_t)(target - (pc + 4)) >> 2;
		w = enc_i(0x05, rs, rt, imm); *out = w; return 0;
	}

	if (strcmp(mnem, "jal") == 0) {
		char lab[MAX_LABEL_LEN + 1];
		if (parse_label_ref(&p, lab, sizeof(lab)) < 0) return -1;
		int idx = find_symbol(lab);
		uint32_t addr;
		if (idx >= 0) {
			addr = (symbols[idx].value >> 2) & 0x3FFFFFFu;
			*out = enc_j(0x03, addr);
		} else {
			*out = enc_j(0x03, 0);
			if (num_relocs < MAX_RELOCS) {
				relocs[num_relocs].offset = text_byte_off;
				relocs[num_relocs].type = RELOC_JAL;
				strncpy(relocs[num_relocs].name, lab, MAX_LABEL_LEN);
				relocs[num_relocs].name[MAX_LABEL_LEN] = '\0';
				num_relocs++;
			}
		}
		return 0;
	}

	return -1;
}

static int process_line(char *line, int pass, uint32_t *pc_text, uint32_t *pc_data)
{
	char *p = line;
	skip_whitespace(&p);
	if (!*p) return 0;

	if (*p == '.') {
		if (strncmp(p, ".section", 8) == 0) {
			p += 8; skip_whitespace(&p);
			if (strncmp(p, ".text", 5) == 0) cur_section = SECT_TEXT;
			else if (strncmp(p, ".data", 5) == 0) cur_section = SECT_DATA;
			else if (strncmp(p, ".rodata", 7) == 0) cur_section = SECT_RODATA;
			else if (strncmp(p, ".bss", 4) == 0) cur_section = SECT_BSS;
			return 0;
		}
		if (strncmp(p, ".global", 7) == 0) {
			p += 7;
			char name[MAX_LABEL_LEN + 1];
			skip_whitespace(&p);
			size_t i = 0;
			while (*p && (isalnum((unsigned char)*p) || *p == '_') && i < MAX_LABEL_LEN)
				name[i++] = *p++;
			name[i] = '\0';
			if (i > 0) global_names[num_globals++] = add_symbol(name, cur_section, 0, 1);
			return 0;
		}
		if (strncmp(p, ".word", 5) == 0 && cur_section == SECT_DATA) {
			p += 5;
			for (;;) {
				skip_whitespace(&p);
				if (!*p) break;
				uint32_t v = (uint32_t)strtoul(p, &p, 0);
				if (pass == 2) {
					emit_data((unsigned char)(v));
					emit_data((unsigned char)(v >> 8));
					emit_data((unsigned char)(v >> 16));
					emit_data((unsigned char)(v >> 24));
				}
				*pc_data += 4;
				skip_whitespace(&p);
				if (*p != ',') break;
				p++;
			}
			return 0;
		}
		return 0;
	}

	/* Label */
	char *label_end = p;
	while (*label_end && (isalnum((unsigned char)*label_end) || *label_end == '_')) label_end++;
	if (*label_end == ':') {
		char name[MAX_LABEL_LEN + 1];
		size_t len = (size_t)(label_end - p);
		if (len > MAX_LABEL_LEN) len = MAX_LABEL_LEN;
		memcpy(name, p, len); name[len] = '\0';
		uint32_t addr = (cur_section == SECT_TEXT) ? *pc_text : *pc_data;
		add_symbol(name, cur_section, addr, 0);
		p = label_end + 1;
		skip_whitespace(&p);
		if (!*p) return 0;
	}

	/* Instruction */
	char mnem[32];
	size_t mi = 0;
	while (*p && !isspace((unsigned char)*p) && mi < sizeof(mnem) - 1) mnem[mi++] = *p++;
	mnem[mi] = '\0';
	skip_whitespace(&p);

	if (cur_section == SECT_TEXT) {
		uint32_t word;
		if (encode_insn(mnem, p, &word, *pc_text, text_off) < 0) {
			fprintf(stderr, "nxasm: unknown or bad instruction '%s'\n", mnem);
			return -1;
		}
		if (pass == 2) emit_text(word);
		*pc_text += 4;
	}
	return 0;
}

static int write_nxo(FILE *out)
{
	uint32_t hdr[8];
	hdr[0] = NXO_MAGIC;
	hdr[1] = NXO_VERSION;
	hdr[2] = text_size;
	hdr[3] = data_size;
	hdr[4] = rodata_size;
	hdr[5] = bss_size;
	hdr[6] = (uint32_t)num_symbols;
	hdr[7] = (uint32_t)num_relocs;
	for (int i = 0; i < 8; i++) {
		uint32_t v = hdr[i];
		putc((int)(v & 0xFF), out);
		putc((int)((v >> 8) & 0xFF), out);
		putc((int)((v >> 16) & 0xFF), out);
		putc((int)((v >> 24) & 0xFF), out);
	}
	if (fwrite(text_buf, 1, text_size, out) != text_size) return -1;
	if (fwrite(data_buf, 1, data_size, out) != data_size) return -1;
	for (int i = 0; i < num_symbols; i++) {
		size_t len = strlen(symbols[i].name);
		if (len > 255) len = 255;
		putc((int)len, out);
		if (fwrite(symbols[i].name, 1, len, out) != len) return -1;
		putc(symbols[i].section, out);
		uint32_t v = symbols[i].value;
		putc((int)(v & 0xFF), out); putc((int)((v >> 8) & 0xFF), out);
		putc((int)((v >> 16) & 0xFF), out); putc((int)((v >> 24) & 0xFF), out);
		putc(symbols[i].is_global ? 1 : 0, out);
	}
	for (int i = 0; i < num_relocs; i++) {
		size_t len = strlen(relocs[i].name);
		if (len > 255) len = 255;
		uint32_t o = relocs[i].offset;
		putc((int)(o & 0xFF), out); putc((int)((o >> 8) & 0xFF), out);
		putc((int)((o >> 16) & 0xFF), out); putc((int)((o >> 24) & 0xFF), out);
		putc(relocs[i].type, out);
		putc((int)len, out);
		if (fwrite(relocs[i].name, 1, len, out) != len) return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	const char *out_path = NULL;
	const char *in_path = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { out_path = argv[++i]; continue; }
		if (argv[i][0] != '-') { in_path = argv[i]; break; }
	}
	if (!in_path || !out_path) {
		fprintf(stderr, "usage: nxasm -o <out.nxo> <file.asm>\n");
		return 1;
	}

	FILE *f = fopen(in_path, "r");
	if (!f) {
		fprintf(stderr, "nxasm: cannot open %s\n", in_path);
		return 1;
	}

	text_buf = NULL; data_buf = NULL;
	text_cap = data_cap = 0;
	text_off = data_off = 0;
	text_size = data_size = rodata_size = bss_size = 0;
	num_symbols = num_globals = 0;
	num_relocs = 0;
	cur_section = SECT_TEXT;

	char line[1024];
	uint32_t pc_text = 0, pc_data = 0;

	/* Pass 1: collect labels and sizes */
	while (fgets(line, sizeof(line), f)) {
		if (process_line(line, 1, &pc_text, &pc_data) < 0) { fclose(f); return 1; }
	}
	rewind(f);
	text_off = 0; data_off = 0;
	pc_text = 0; pc_data = 0;

	/* Pass 2: emit */
	while (fgets(line, sizeof(line), f)) {
		if (process_line(line, 2, &pc_text, &pc_data) < 0) { fclose(f); return 1; }
	}
	fclose(f);

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "nxasm: cannot write %s\n", out_path);
		return 1;
	}
	if (write_nxo(out) < 0) {
		fprintf(stderr, "nxasm: write failed\n");
		fclose(out);
		return 1;
	}
	fclose(out);
	free(text_buf);
	free(data_buf);
	return 0;
}
