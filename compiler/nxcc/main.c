/*
 * nxcc — minimal NEXUS-32 C compiler. Subset: int/void, functions, return,
 * if/while, basic expressions (literals, binary ops, calls). Emits .asm, invokes nxasm.
 * Calling convention: r4-r7 args, r2 return, r30=sp, r31=ra.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

static FILE *out;
static FILE *cur_in;
static int cur_char;
static int line_no;
static char tok_val[256];
static int tok;

enum { T_EOF, T_INT, T_VOID, T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN, T_LBRACE, T_RBRACE,
       T_LPAREN, T_RPAREN, T_SEMI, T_COMMA, T_IDENT, T_NUM, T_PLUS, T_MINUS,
       T_STAR, T_SLASH, T_LT, T_GT, T_EQ, T_NE, T_LE, T_GE };

#define NEXT() do { cur_char = cur_in ? getc(cur_in) : EOF; if (cur_char == '\n') line_no++; } while(0)
static void skip_ws(void) {
	for (;;) {
		while (cur_char == ' ' || cur_char == '\t' || cur_char == '\n' || cur_char == '\r') NEXT();
		if (cur_char != '/') return;
		NEXT();
		if (cur_char == '/') { while (cur_char != '\n' && cur_char != EOF) NEXT(); continue; }
		if (cur_char == '*') {
			NEXT();
			while (cur_char != EOF) {
				if (cur_char == '*') { NEXT(); if (cur_char == '/') { NEXT(); break; } }
				else NEXT();
			}
			continue;
		}
		ungetc(cur_char, cur_in); cur_char = '/'; return;
	}
}
static void lex_ident(void) {
	int i = 0;
	while (isalnum((unsigned char)cur_char) || cur_char == '_')
		{ if (i < 255) tok_val[i++] = cur_char; NEXT(); }
	tok_val[i] = '\0';
	if (strcmp(tok_val, "int") == 0) tok = T_INT;
	else if (strcmp(tok_val, "void") == 0) tok = T_VOID;
	else if (strcmp(tok_val, "if") == 0) tok = T_IF;
	else if (strcmp(tok_val, "else") == 0) tok = T_ELSE;
	else if (strcmp(tok_val, "while") == 0) tok = T_WHILE;
	else if (strcmp(tok_val, "for") == 0) tok = T_FOR;
	else if (strcmp(tok_val, "return") == 0) tok = T_RETURN;
	else tok = T_IDENT;
}
static void scan(void) {
	skip_ws();
	if (cur_char == EOF) { tok = T_EOF; return; }
	if (isalpha((unsigned char)cur_char) || cur_char == '_') { lex_ident(); return; }
	if (isdigit((unsigned char)cur_char)) {
		int i = 0;
		while (isdigit((unsigned char)cur_char)) { if (i < 255) tok_val[i++] = cur_char; NEXT(); }
		tok_val[i] = '\0';
		tok = T_NUM;
		return;
	}
	switch (cur_char) {
		case '{': tok = T_LBRACE; NEXT(); return;
		case '}': tok = T_RBRACE; NEXT(); return;
		case '(': tok = T_LPAREN; NEXT(); return;
		case ')': tok = T_RPAREN; NEXT(); return;
		case ';': tok = T_SEMI; NEXT(); return;
		case ',': tok = T_COMMA; NEXT(); return;
		case '+': tok = T_PLUS; NEXT(); return;
		case '-': tok = T_MINUS; NEXT(); return;
		case '*': tok = T_STAR; NEXT(); return;
		case '/': tok = T_SLASH; NEXT(); return;
		case '<': NEXT(); if (cur_char == '=') { NEXT(); tok = T_LE; } else tok = T_LT; return;
		case '>': NEXT(); if (cur_char == '=') { NEXT(); tok = T_GE; } else tok = T_GT; return;
		case '!': NEXT(); if (cur_char == '=') { NEXT(); tok = T_NE; return; } break;
		case '=': NEXT(); if (cur_char == '=') { NEXT(); tok = T_EQ; return; } break;
	}
	tok = T_EOF;
}
static void expect(int t) { if (tok != t) { fprintf(stderr, "nxcc: expected token\n"); exit(1); } scan(); }

static int label_id;
static void emit(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(out, fmt, ap);
	va_end(ap);
	fputc('\n', out);
}

static int expr(void);
static int term(void) {
	if (tok == T_NUM) {
		long v = atol(tok_val);
		scan();
		emit("  addiu r2, r0, %ld", v);
		return 2;
	}
	if (tok == T_IDENT) {
		char name[256];
		strncpy(name, tok_val, 255); name[255] = '\0';
		scan();
		if (tok == T_LPAREN) {
			scan();
			int regs[] = {4, 5, 6, 7}, n = 0;
			if (tok != T_RPAREN) {
				expr();
				emit("  addiu r%d, r2, 0", regs[n++]);
				while (tok == T_COMMA) { scan(); expr(); emit("  addiu r%d, r2, 0", regs[n++]); }
			}
			expect(T_RPAREN);
			emit("  jal %s", name);
			return 2;
		}
		fprintf(stderr, "nxcc: global variables not supported in minimal build\n");
		exit(1);
	}
	if (tok == T_LPAREN) {
		scan();
		expr();
		expect(T_RPAREN);
		return 2;
	}
	fprintf(stderr, "nxcc: unexpected token\n");
	exit(1);
	return 0;
}
static int unary(void) {
	if (tok == T_MINUS) { scan(); unary(); emit("  subu r2, r0, r2"); return 2; }
	return term();
}
static int mul_expr(void) {
	unary();
	while (tok == T_STAR || tok == T_SLASH) {
		int op = tok;
		scan();
		emit("  addiu r3, r2, 0");
		unary();
		if (op == T_STAR) emit("  mul r2, r3, r2");
		else emit("  div r2, r3, r2");
	}
	return 2;
}
static int add_expr(void) {
	mul_expr();
	while (tok == T_PLUS || tok == T_MINUS) {
		int op = tok;
		scan();
		emit("  addiu r3, r2, 0");
		mul_expr();
		if (op == T_PLUS) emit("  add r2, r3, r2");
		else emit("  sub r2, r3, r2");
	}
	return 2;
}
static int rel_expr(void) {
	add_expr();
	if (tok == T_LT || tok == T_LE || tok == T_GT || tok == T_GE || tok == T_EQ || tok == T_NE) {
		int op = tok;
		scan();
		emit("  addiu r3, r2, 0");
		add_expr();
		if (op == T_LT) emit("  slt r2, r3, r2");
		else if (op == T_LE) { emit("  slt r2, r2, r3"); emit("  xori r2, r2, 1"); }
		else if (op == T_GT) emit("  slt r2, r2, r3");
		else if (op == T_GE) { emit("  slt r2, r3, r2"); emit("  xori r2, r2, 1"); }
		else if (op == T_EQ) { emit("  xor r2, r3, r2"); emit("  sltiu r2, r2, 1"); }
		else { emit("  xor r2, r3, r2"); emit("  sltu r2, r0, r2"); }
	}
	return 2;
}
static int expr(void) { return rel_expr(); }

static void stmt(void);
static void block(void) {
	expect(T_LBRACE);
	while (tok != T_RBRACE && tok != T_EOF) stmt();
	expect(T_RBRACE);
}
static void stmt(void) {
	if (tok == T_RETURN) {
		scan();
		if (tok != T_SEMI) expr();
		else emit("  addiu r2, r0, 0");
		expect(T_SEMI);
		emit("  jr ra");
		return;
	}
	if (tok == T_IF) {
		scan();
		expect(T_LPAREN);
		expr();
		expect(T_RPAREN);
		int l = label_id++;
		emit("  beq r2, r0, if_else_%d", l);
		if (tok == T_LBRACE) block();
		else stmt();
		if (tok == T_ELSE) {
			scan();
			int l2 = label_id++;
			emit("  j if_end_%d", l2);
			emit("if_else_%d:", l);
			if (tok == T_LBRACE) block();
			else stmt();
			emit("if_end_%d:", l2);
		} else emit("if_else_%d:", l);
		return;
	}
	if (tok == T_WHILE) {
		scan();
		int start = label_id++, end = label_id++;
		emit("while_cond_%d:", start);
		expect(T_LPAREN);
		expr();
		expect(T_RPAREN);
		emit("  beq r2, r0, while_end_%d", end);
		if (tok == T_LBRACE) block();
		else stmt();
		emit("  j while_cond_%d", start);
		emit("while_end_%d:", end);
		return;
	}
	if (tok == T_FOR) {
		scan();
		expect(T_LPAREN);
		int Lcond = label_id++, Lbody = label_id++, Lend = label_id++, Linc = label_id++;
		if (tok != T_SEMI) expr();
		expect(T_SEMI);
		emit("for_cond_%d:", Lcond);
		if (tok != T_SEMI) { expr(); emit("  beq r2, r0, for_end_%d", Lend); }
		expect(T_SEMI);
		emit("  j for_body_%d", Lbody);
		emit("for_inc_%d:", Linc);
		if (tok != T_RPAREN) { expr(); expect(T_SEMI); }
		expect(T_RPAREN);
		emit("  j for_cond_%d", Lcond);
		emit("for_body_%d:", Lbody);
		if (tok == T_LBRACE) block();
		else stmt();
		emit("  j for_inc_%d", Linc);
		emit("for_end_%d:", Lend);
		return;
	}
	if (tok == T_LBRACE) { block(); return; }
	if (tok != T_SEMI) expr();
	expect(T_SEMI);
}
static void func(const char *name, int ret_void) {
	emit(".section .text");
	emit(".global %s", name);
	emit("%s:", name);
	emit("  addiu r30, r30, -16");
	emit("  sw r31, 0(r30)");
	block();
	emit("  lw r31, 0(r30)");
	emit("  addiu r30, r30, 16");
	emit("  jr ra");
}
static void parse_decl(int is_void) {
	if (tok != T_IDENT) { fprintf(stderr, "nxcc: expected identifier\n"); exit(1); }
	char name[256];
	strncpy(name, tok_val, 255); name[255] = '\0';
	scan();
	if (tok == T_LPAREN) {
		scan();
		if (tok == T_VOID) scan();
		expect(T_RPAREN);
		func(name, is_void);
		return;
	}
	expect(T_SEMI);
	emit(".section .data");
	emit(".global %s", name);
	emit("%s:", name);
	emit("  .word 0");
}
static void program(void) {
	while (tok != T_EOF) {
		int ret_void = (tok == T_VOID);
		if (tok == T_INT || tok == T_VOID) scan();
		else { fprintf(stderr, "nxcc: expected int or void\n"); exit(1); }
		parse_decl(ret_void);
	}
}

int main(int argc, char **argv) {
	const char *out_nxo = NULL;
	const char *in_file = NULL;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) continue;
		if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { out_nxo = argv[++i]; continue; }
		if (argv[i][0] != '-') in_file = argv[i];
	}
	if (!in_file || !out_nxo) {
		fprintf(stderr, "usage: nxcc -c <in.c> -o <out.nxo>\n");
		return 1;
	}
	cur_in = fopen(in_file, "r");
	if (!cur_in) { fprintf(stderr, "nxcc: cannot open %s\n", in_file); return 1; }
	NEXT();
	scan();

	char asm_path[1024];
	snprintf(asm_path, sizeof(asm_path), "%s.nxcc.asm", in_file);
	out = fopen(asm_path, "w");
	if (!out) { fprintf(stderr, "nxcc: cannot write asm\n"); fclose(cur_in); return 1; }
	label_id = 0;
	line_no = 1;
	program();
	fclose(out);
	fclose(cur_in);

	char cmd[1024];
	snprintf(cmd, sizeof(cmd), "nxasm -o %s %s", out_nxo, asm_path);
	int r = system(cmd);
	remove(asm_path);
	if (r != 0) { fprintf(stderr, "nxcc: nxasm failed\n"); return 1; }
	return 0;
}
