/*
 * nxld — NEXUS-32 linker. Reads .nxo (and libnx.a), outputs .nxbin.
 * Entry point 0x00000400; code at 0x400, data at 0x00400000 per spec §3.2.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NXO_MAGIC  0x004F584Eu
#define NXB_MAGIC  0x0042584Eu
#define TEXT_BASE  0x00000400u
#define MAX_INPUTS 128
#define MAX_SYMS 512
#define MAX_RELOCS_PER_OBJ 32
#define AR_ID "!<arch>\n"

typedef struct { char name[64]; uint32_t addr; int section; } sym_ent_t;
typedef struct { uint32_t offset; unsigned char type; char name[64]; } reloc_ent_t;
typedef struct { size_t base; reloc_ent_t r[MAX_RELOCS_PER_OBJ]; int n; } reloc_list_t;
#define DATA_BASE 0x00400000u

static uint32_t read_u32(FILE *f)
{
	unsigned char b[4];
	if (fread(b, 1, 4, f) != 4) return 0;
	return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

typedef struct { const char *path; long offset; long size; } ar_member_t;

static uint32_t read_u32_buf(const unsigned char **p, size_t *len)
{
	if (*len < 4) return 0;
	uint32_t v = (uint32_t)(*p)[0] | ((uint32_t)(*p)[1] << 8) | ((uint32_t)(*p)[2] << 16) | ((uint32_t)(*p)[3] << 24);
	*p += 4; *len -= 4;
	return v;
}

static int load_nxo(FILE *f, unsigned char **text, uint32_t *text_size,
	unsigned char **data, uint32_t *data_size, uint32_t *entry_out, int set_entry,
	sym_ent_t *syms_out, int *num_syms_out, reloc_ent_t *relocs_out, int *num_relocs_out)
{
	uint32_t magic = read_u32(f);
	if (magic != NXO_MAGIC) return -1;
	uint32_t version = read_u32(f);
	uint32_t ts = read_u32(f), ds = read_u32(f), ro = read_u32(f), bs = read_u32(f);
	uint32_t num_sym = read_u32(f), num_reloc = read_u32(f);
	(void)version; (void)ro; (void)bs;

	unsigned char *tb = ts ? malloc(ts) : NULL;
	unsigned char *db = ds ? malloc(ds) : NULL;
	if (ts && !tb) return -1;
	if (ds && !db) { free(tb); return -1; }
	if (ts && fread(tb, 1, ts, f) != ts) { free(tb); free(db); return -1; }
	if (ds && fread(db, 1, ds, f) != ds) { free(tb); free(db); return -1; }

	uint32_t ent = TEXT_BASE;
	int ns = 0;
	for (uint32_t i = 0; i < num_sym && (!syms_out || ns < MAX_SYMS); i++) {
		int len = fgetc(f);
		if (len < 0) break;
		char name[256];
		if (len > 255) len = 255;
		if (fread(name, 1, len, f) != (size_t)len) break;
		name[len] = '\0';
		int sec = fgetc(f);
		uint32_t val = read_u32(f);
		int global = fgetc(f);
		if (set_entry && global && sec == 0 && (strcmp(name, "_start") == 0 || strcmp(name, "start") == 0))
			ent = TEXT_BASE + val;
		if (syms_out) {
			strncpy(syms_out[ns].name, name, 63); syms_out[ns].name[63] = '\0';
			syms_out[ns].addr = val;
			syms_out[ns].section = sec;
			ns++;
		}
	}
	int nr = 0;
	for (uint32_t i = 0; i < num_reloc; i++) {
		uint32_t o = read_u32(f);
		int typ = fgetc(f);
		int nlen = fgetc(f);
		if (nlen < 0 || nlen > 63) { if (nlen >= 0) fseek(f, nlen, SEEK_CUR); continue; }
		char n[64];
		if (fread(n, 1, nlen, f) != (size_t)nlen) break;
		n[nlen] = '\0';
		if (relocs_out && nr < MAX_RELOCS_PER_OBJ) {
			relocs_out[nr].offset = o;
			relocs_out[nr].type = (unsigned char)typ;
			strncpy(relocs_out[nr].name, n, 63);
			relocs_out[nr].name[63] = '\0';
			nr++;
		}
	}
	if (num_relocs_out) *num_relocs_out = nr;
	if (num_syms_out) *num_syms_out = ns;
	*text = tb;
	*data = db;
	*text_size = ts;
	*data_size = ds;
	if (set_entry) *entry_out = ent;
	return 0;
}

static int load_nxo_buf(const unsigned char *buf, size_t buf_len,
	unsigned char **text, uint32_t *text_size, unsigned char **data, uint32_t *data_size,
	uint32_t *entry_out, int set_entry,
	sym_ent_t *syms_out, int *num_syms_out, reloc_ent_t *relocs_out, int *num_relocs_out)
{
	if (buf_len < 32) return -1;
	const unsigned char *p = buf;
	size_t left = buf_len;
	uint32_t magic = read_u32_buf(&p, &left);
	if (magic != NXO_MAGIC) return -1;
	(void)read_u32_buf(&p, &left);
	uint32_t ts = read_u32_buf(&p, &left), ds = read_u32_buf(&p, &left);
	read_u32_buf(&p, &left); read_u32_buf(&p, &left);
	uint32_t num_sym = read_u32_buf(&p, &left);
	uint32_t num_reloc = read_u32_buf(&p, &left);
	if (left < ts + ds) return -1;
	unsigned char *tb = ts ? malloc(ts) : NULL;
	unsigned char *db = ds ? malloc(ds) : NULL;
	if (ts && !tb) return -1;
	if (ds && !db) { free(tb); return -1; }
	if (ts) memcpy(tb, p, ts);
	p += ts;
	if (ds) memcpy(db, p, ds);
	p += ds;
	left -= ts + ds;
	uint32_t ent = TEXT_BASE;
	int ns = 0;
	for (uint32_t i = 0; i < num_sym && left >= 1; i++) {
		int len = *p++;
		left--;
		if (len > 255) len = 255;
		if (left < (size_t)len + 6) break;
		const unsigned char *n = p;
		p += len;
		int sec = *p++;
		uint32_t val = (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
		p += 4;
		int global = *p++;
		left -= 1u + (size_t)len + 6u;
		if (set_entry && global && sec == 0) {
			char name[256];
			memcpy(name, n, len); name[len] = '\0';
			if (strcmp(name, "_start") == 0 || strcmp(name, "start") == 0)
				ent = TEXT_BASE + val;
		}
		if (syms_out && ns < MAX_SYMS) {
			memcpy(syms_out[ns].name, n, len); syms_out[ns].name[len] = '\0';
			if (len > 63) syms_out[ns].name[63] = '\0';
			syms_out[ns].addr = val;
			syms_out[ns].section = sec;
			ns++;
		}
	}
	int nr = 0;
	for (uint32_t i = 0; i < num_reloc && left >= 6; i++) {
		uint32_t o = (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
		p += 4; left -= 4;
		int typ = *p++; left--;
		int nlen = *p++; left--;
		if (nlen < 0 || nlen > 63 || left < (size_t)nlen) { p += nlen; left -= nlen; continue; }
		if (relocs_out && nr < MAX_RELOCS_PER_OBJ) {
			relocs_out[nr].offset = o;
			relocs_out[nr].type = (unsigned char)typ;
			memcpy(relocs_out[nr].name, p, nlen);
			relocs_out[nr].name[nlen] = '\0';
			nr++;
		}
		p += nlen; left -= nlen;
	}
	if (num_syms_out) *num_syms_out = ns;
	if (num_relocs_out) *num_relocs_out = nr;
	*text = tb;
	*data = db;
	*text_size = ts;
	*data_size = ds;
	if (set_entry) *entry_out = ent;
	return 0;
}

/* Parse BSD-style ar; fill ar_member_t for each .nxo member. */
static int collect_ar_members(const char *ar_path, ar_member_t *members, int max_members, int *count)
{
	FILE *f = fopen(ar_path, "rb");
	if (!f) return -1;
	char magic[8];
	if (fread(magic, 1, 8, f) != 8) { fclose(f); return -1; }
	if (memcmp(magic, AR_ID, 7) != 0) { fclose(f); return -1; }
	*count = 0;
	for (;;) {
		char hdr[60];
		if (fread(hdr, 1, 60, f) != 60) break;
		char name[17];
		memcpy(name, hdr, 16);
		name[16] = '\0';
		for (int i = 15; i >= 0 && (name[i] == ' ' || name[i] == '/'); i--) name[i] = '\0';
		char size_str[11];
		memcpy(size_str, hdr + 48, 10);
		size_str[10] = '\0';
		long sz = strtol(size_str, NULL, 10);
		if (sz <= 0) break;
		long pos = ftell(f);
		size_t namelen = strlen(name);
		int is_nxo = (namelen > 4 && strcmp(name + namelen - 4, ".nxo") == 0);
		if (is_nxo && *count < max_members) {
			members[*count].path = ar_path;
			members[*count].offset = pos;
			members[*count].size = sz;
			(*count)++;
		}
		fseek(f, pos + ((sz + 1) & ~1), SEEK_SET);
	}
	fclose(f);
	return 0;
}

int main(int argc, char **argv)
{
	const char *out_path = NULL;
	const char *lib_dirs[32];
	int num_lib_dirs = 0;
	int link_libnx = 0;
	const char *inputs[MAX_INPUTS];
	int num_inputs = 0;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { out_path = argv[++i]; continue; }
		if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
			if (num_lib_dirs < 32) lib_dirs[num_lib_dirs++] = argv[++i];
			else i++;
			continue;
		}
		if (strcmp(argv[i], "-lnx") == 0) { link_libnx = 1; continue; }
		if (argv[i][0] != '-') {
			if (num_inputs < MAX_INPUTS) inputs[num_inputs++] = argv[i];
			continue;
		}
	}
	ar_member_t ar_members[32];
	int num_ar = 0;
	static char ar_file_path[512];
	ar_file_path[0] = '\0';
	if (link_libnx) {
		for (int d = 0; d < num_lib_dirs; d++) {
			snprintf(ar_file_path, sizeof(ar_file_path), "%s/libnx.a", lib_dirs[d]);
			FILE *t = fopen(ar_file_path, "rb");
			if (t) { fclose(t); break; }
			ar_file_path[0] = '\0';
		}
		if (!ar_file_path[0]) {
			FILE *t = fopen("libnx.a", "rb");
			if (t) { fclose(t); strcpy(ar_file_path, "libnx.a"); }
		}
		if (ar_file_path[0] && collect_ar_members(ar_file_path, ar_members, 32, &num_ar) != 0)
			num_ar = 0;
	}
	if (!out_path || num_inputs == 0) {
		fprintf(stderr, "usage: nxld -o <out.nxbin> <file.nxo> ... [-L dir] [-lnx]\n");
		return 1;
	}

	sym_ent_t global_syms[MAX_SYMS];
	int num_global_syms = 0;
	reloc_list_t reloc_lists[MAX_INPUTS + 32];
	int num_reloc_lists = 0;

	size_t total_text = 0, total_data = 0;
	unsigned char *merged_text = NULL, *merged_data = NULL;
	size_t text_cap = 0, data_cap = 0;
	uint32_t entry = TEXT_BASE;
	int first_with_entry = 1;

	for (int i = 0; i < num_inputs; i++) {
		unsigned char *text = NULL, *data = NULL;
		uint32_t ts, ds;
		sym_ent_t obj_syms[MAX_SYMS];
		int num_obj_syms = 0;
		reloc_ent_t obj_relocs[MAX_RELOCS_PER_OBJ];
		int num_obj_relocs = 0;
		FILE *f = fopen(inputs[i], "rb");
		if (!f) { fprintf(stderr, "nxld: cannot open %s\n", inputs[i]); return 1; }
		if (load_nxo(f, &text, &ts, &data, &ds, &entry, first_with_entry,
			obj_syms, &num_obj_syms, obj_relocs, &num_obj_relocs) != 0) {
			fprintf(stderr, "nxld: not a .nxo or read error: %s\n", inputs[i]);
			fclose(f);
			return 1;
		}
		fclose(f);
		if (first_with_entry && entry != TEXT_BASE) first_with_entry = 0;
		size_t text_base = total_text, data_base = total_data;
		if (ts) {
			while (total_text + ts > text_cap) text_cap = text_cap ? text_cap * 2 : 4096;
			merged_text = realloc(merged_text, text_cap);
			memcpy(merged_text + total_text, text, ts);
			total_text += ts;
		}
		if (ds) {
			while (total_data + ds > data_cap) data_cap = data_cap ? data_cap * 2 : 4096;
			merged_data = realloc(merged_data, data_cap);
			memcpy(merged_data + total_data, data, ds);
			total_data += ds;
		}
		for (int k = 0; k < num_obj_syms && num_global_syms < MAX_SYMS; k++) {
			strncpy(global_syms[num_global_syms].name, obj_syms[k].name, 63);
			global_syms[num_global_syms].name[63] = '\0';
			if (obj_syms[k].section == 0)
				global_syms[num_global_syms].addr = TEXT_BASE + (uint32_t)text_base + obj_syms[k].addr;
			else
				global_syms[num_global_syms].addr = DATA_BASE + (uint32_t)data_base + obj_syms[k].addr;
			num_global_syms++;
		}
		if (num_reloc_lists < MAX_INPUTS + 32) {
			reloc_lists[num_reloc_lists].base = text_base;
			reloc_lists[num_reloc_lists].n = num_obj_relocs;
			memcpy(reloc_lists[num_reloc_lists].r, obj_relocs, (size_t)num_obj_relocs * sizeof(reloc_ent_t));
			num_reloc_lists++;
		}
		free(text);
		free(data);
	}
	for (int i = 0; i < num_ar; i++) {
		FILE *f = fopen(ar_file_path, "rb");
		if (!f) continue;
		fseek(f, ar_members[i].offset, SEEK_SET);
		unsigned char *buf = malloc((size_t)ar_members[i].size);
		if (!buf || fread(buf, 1, (size_t)ar_members[i].size, f) != (size_t)ar_members[i].size) {
			free(buf);
			fclose(f);
			continue;
		}
		fclose(f);
		unsigned char *text = NULL, *data = NULL;
		uint32_t ts, ds;
		sym_ent_t obj_syms[MAX_SYMS];
		int num_obj_syms = 0;
		reloc_ent_t obj_relocs[MAX_RELOCS_PER_OBJ];
		int num_obj_relocs = 0;
		if (load_nxo_buf(buf, (size_t)ar_members[i].size, &text, &ts, &data, &ds, &entry, first_with_entry,
			obj_syms, &num_obj_syms, obj_relocs, &num_obj_relocs) != 0) {
			free(buf);
			continue;
		}
		free(buf);
		if (first_with_entry && entry != TEXT_BASE) first_with_entry = 0;
		size_t text_base = total_text, data_base = total_data;
		if (ts) {
			while (total_text + ts > text_cap) text_cap = text_cap ? text_cap * 2 : 4096;
			merged_text = realloc(merged_text, text_cap);
			memcpy(merged_text + total_text, text, ts);
			total_text += ts;
		}
		if (ds) {
			while (total_data + ds > data_cap) data_cap = data_cap ? data_cap * 2 : 4096;
			merged_data = realloc(merged_data, data_cap);
			memcpy(merged_data + total_data, data, ds);
			total_data += ds;
		}
		for (int k = 0; k < num_obj_syms && num_global_syms < MAX_SYMS; k++) {
			strncpy(global_syms[num_global_syms].name, obj_syms[k].name, 63);
			global_syms[num_global_syms].name[63] = '\0';
			if (obj_syms[k].section == 0)
				global_syms[num_global_syms].addr = TEXT_BASE + (uint32_t)text_base + obj_syms[k].addr;
			else
				global_syms[num_global_syms].addr = DATA_BASE + (uint32_t)data_base + obj_syms[k].addr;
			num_global_syms++;
		}
		if (num_reloc_lists < MAX_INPUTS + 32) {
			reloc_lists[num_reloc_lists].base = text_base;
			reloc_lists[num_reloc_lists].n = num_obj_relocs;
			memcpy(reloc_lists[num_reloc_lists].r, obj_relocs, (size_t)num_obj_relocs * sizeof(reloc_ent_t));
			num_reloc_lists++;
		}
		free(text);
		free(data);
	}

	for (int i = 0; i < num_reloc_lists; i++) {
		for (int j = 0; j < reloc_lists[i].n; j++) {
			reloc_ent_t *re = &reloc_lists[i].r[j];
			uint32_t addr = 0;
			for (int k = 0; k < num_global_syms; k++)
				if (strcmp(global_syms[k].name, re->name) == 0) { addr = global_syms[k].addr; break; }
			if (re->type == 0 && addr != 0) {
				size_t patch_off = reloc_lists[i].base + re->offset;
				if (patch_off + 4 <= total_text) {
					uint32_t word = (uint32_t)merged_text[patch_off] | ((uint32_t)merged_text[patch_off+1]<<8) |
						((uint32_t)merged_text[patch_off+2]<<16) | ((uint32_t)merged_text[patch_off+3]<<24);
					word = (word & 0xFC000000u) | ((addr >> 2) & 0x3FFFFFFu);
					merged_text[patch_off] = (unsigned char)(word);
					merged_text[patch_off+1] = (unsigned char)(word >> 8);
					merged_text[patch_off+2] = (unsigned char)(word >> 16);
					merged_text[patch_off+3] = (unsigned char)(word >> 24);
				}
			}
		}
	}

	FILE *out = fopen(out_path, "wb");
	if (!out) {
		fprintf(stderr, "nxld: cannot write %s\n", out_path);
		free(merged_text);
		free(merged_data);
		return 1;
	}
	uint32_t hdr[4];
	hdr[0] = NXB_MAGIC;
	hdr[1] = entry;
	hdr[2] = (uint32_t)total_text;
	hdr[3] = (uint32_t)total_data;
	for (int i = 0; i < 4; i++) {
		uint32_t v = hdr[i];
		putc((int)(v & 0xFF), out);
		putc((int)((v >> 8) & 0xFF), out);
		putc((int)((v >> 16) & 0xFF), out);
		putc((int)((v >> 24) & 0xFF), out);
	}
	if (total_text && fwrite(merged_text, 1, total_text, out) != total_text) { fclose(out); free(merged_text); free(merged_data); return 1; }
	if (total_data && fwrite(merged_data, 1, total_data, out) != total_data) { fclose(out); free(merged_text); free(merged_data); return 1; }
	fclose(out);
	free(merged_text);
	free(merged_data);
	return 0;
}
