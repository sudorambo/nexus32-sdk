/*
 * obj2mesh — Convert OBJ to .mesh. Parses v, vt, vn, f; triangulates; outputs vertex/index data.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_V 65536
#define MAX_F 65536

static float vx[MAX_V], vy[MAX_V], vz[MAX_V];
static float vtx[MAX_V], vty[MAX_V];
static float vnx[MAX_V], vny[MAX_V], vnz[MAX_V];
static int nv, nf;
static unsigned indices[MAX_F * 3];

static int parse_face(const char *p) {
	int a = 0, b = 0, c = 0;
	sscanf(p, "%d/%d/%d", &a, &b, &c);
	if (a < 0) a += nv + 1;
	if (b < 0) b += nv + 1;
	if (c < 0) c += nv + 1;
	return (a > 0 && a <= nv) ? a - 1 : 0;
}

int main(int argc, char **argv) {
	const char *in = NULL, *out = NULL;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == 'o' && i + 1 < argc) { out = argv[++i]; continue; }
		if (argv[i][0] != '-') in = argv[i];
	}
	if (!in || !out) {
		fprintf(stderr, "usage: obj2mesh -o out.mesh input.obj\n");
		return 1;
	}
	FILE *f = fopen(in, "r");
	if (!f) { fprintf(stderr, "obj2mesh: cannot open %s\n", in); return 1; }
	nv = 0; nf = 0;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == 'v' && line[1] == ' ') {
			float x, y, z;
			sscanf(line + 2, "%f %f %f", &x, &y, &z);
			if (nv < MAX_V) { vx[nv]=x; vy[nv]=y; vz[nv]=z; nv++; }
		} else if (line[0] == 'v' && line[1] == 't') {
			float u, v;
			sscanf(line + 3, "%f %f", &u, &v);
			if (nv > 0 && nv <= MAX_V) { vtx[nv-1]=u; vty[nv-1]=v; }
		} else if (line[0] == 'v' && line[1] == 'n') {
			float x, y, z;
			sscanf(line + 3, "%f %f %f", &x, &y, &z);
			if (nv > 0 && nv <= MAX_V) { vnx[nv-1]=x; vny[nv-1]=y; vnz[nv-1]=z; }
		} else if (line[0] == 'f' && line[1] == ' ') {
			char *p = line + 2;
			int i0 = parse_face(p);
			p = strchr(p, ' '); if (p) p++;
			int i1 = p ? parse_face(p) : 0;
			p = p ? strchr(p, ' ') : NULL; if (p) p++;
			int i2 = p ? parse_face(p) : 0;
			if (nf * 3 + 3 <= (int)(sizeof(indices)/sizeof(indices[0]))) {
				indices[nf*3]=i0; indices[nf*3+1]=i1; indices[nf*3+2]=i2;
				nf++;
			}
			p = p ? strchr(p, ' ') : NULL;
			if (p && *p) {
				int i3 = parse_face(p);
				if (nf * 3 + 3 <= (int)(sizeof(indices)/sizeof(indices[0]))) {
					indices[nf*3]=i0; indices[nf*3+1]=i2; indices[nf*3+2]=i3;
					nf++;
				}
			}
		}
	}
	fclose(f);
	FILE *outf = fopen(out, "wb");
	if (!outf) return 1;
	unsigned u32 = (unsigned)nv;
	fwrite(&u32, 4, 1, outf);
	u32 = (unsigned)(nf * 3);
	fwrite(&u32, 4, 1, outf);
	u32 = 1;
	fwrite(&u32, 4, 1, outf);
	for (int i = 0; i < nv; i++) {
		fwrite(&vx[i], 4, 1, outf);
		fwrite(&vy[i], 4, 1, outf);
		fwrite(&vz[i], 4, 1, outf);
		fwrite(&vtx[i], 4, 1, outf);
		fwrite(&vty[i], 4, 1, outf);
		fwrite(&vnx[i], 4, 1, outf);
		fwrite(&vny[i], 4, 1, outf);
		fwrite(&vnz[i], 4, 1, outf);
	}
	for (int i = 0; i < nf * 3; i++) {
		u32 = (unsigned)indices[i];
		fwrite(&u32, 4, 1, outf);
	}
	fclose(outf);
	return 0;
}
