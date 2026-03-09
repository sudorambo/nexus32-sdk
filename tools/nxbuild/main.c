/*
 * nxbuild — Build orchestrator. Reads build.toml; runs nxcc, nxasm, nxld, rompack.
 * Minimal build.toml: name, entry, sources (space-separated), screen_width, screen_height, cycle_budget.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char name[128] = "game";
static char entry[64] = "main";
static char sources[512] = "";
static int screen_w = 320, screen_h = 240, cycle_budget = 1000000;

static void parse_line(char *line) {
	while (*line == ' ' || *line == '\t') line++;
	if (*line == '#' || *line == '[' || *line == '\0') return;
	char *eq = strchr(line, '=');
	if (!eq) return;
	*eq = '\0';
	char *key = line;
	char *val = eq + 1;
	while (*val == ' ') val++;
	if (*val == '"') { val++; char *end = strchr(val, '"'); if (end) *end = '\0'; }
	if (strstr(key, "name") && strchr(key, 'e')) { strncpy(name, val, 127); name[127] = '\0'; return; }
	if (strstr(key, "entry")) { strncpy(entry, val, 63); entry[63] = '\0'; return; }
	if (strstr(key, "sources")) { strncpy(sources, val, 511); sources[511] = '\0'; return; }
	if (strstr(key, "screen_width")) { screen_w = atoi(val); return; }
	if (strstr(key, "screen_height")) { screen_h = atoi(val); return; }
	if (strstr(key, "cycle_budget")) { cycle_budget = atoi(val); return; }
}

static int run(const char *cmd) {
	int r = system(cmd);
	if (r != 0) fprintf(stderr, "nxbuild: %s failed\n", cmd);
	return r;
}

int main(int argc, char **argv) {
	const char *config = "build.toml";
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) { config = argv[++i]; continue; }
	}
	FILE *f = fopen(config, "r");
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f)) parse_line(line);
		fclose(f);
	}

	static char nxo_names[32][64];
	char *src = sources;
	char *tok;
	int n_nxo = 0;
	while (n_nxo < 32 && (tok = strtok(src, " \t,\""))) {
		src = NULL;
		size_t len = strlen(tok);
		if (len < 2) continue;
		char out[512];
		if (len > 2 && tok[len-2] == '.' && (tok[len-1] == 'c' || tok[len-1] == 'C')) {
			snprintf(out, sizeof(out), "nxcc -c %s -o %.*s.nxo", tok, (int)(len-2), tok);
			if (run(out) != 0) return 1;
			snprintf(nxo_names[n_nxo], sizeof(nxo_names[0]), "%.*s.nxo", (int)(len-2), tok);
			n_nxo++;
		} else if (len > 4 && strcmp(tok + len - 4, ".asm") == 0) {
			snprintf(out, sizeof(out), "nxasm -o %.*s.nxo %s", (int)(len-4), tok, tok);
			if (run(out) != 0) return 1;
			snprintf(nxo_names[n_nxo], sizeof(nxo_names[0]), "%.*s.nxo", (int)(len-4), tok);
			n_nxo++;
		}
	}

	if (n_nxo == 0) {
		fprintf(stderr, "nxbuild: no sources\n");
		return 1;
	}

	char cmd[2048];
	snprintf(cmd, sizeof(cmd), "nxld -o %s.nxbin", name);
	for (int i = 0; i < n_nxo; i++) {
		strncat(cmd, " ", sizeof(cmd) - strlen(cmd) - 1);
		strncat(cmd, nxo_names[i], sizeof(cmd) - strlen(cmd) - 1);
	}
	strncat(cmd, " -L lib -lnx", sizeof(cmd) - strlen(cmd) - 1);
	if (run(cmd) != 0) return 1;

	snprintf(cmd, sizeof(cmd), "rompack -o %s.nxrom %s.nxbin 2>/dev/null || true", name, name);
	run(cmd);
	return 0;
}
