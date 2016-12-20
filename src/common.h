/*
Copyright (C) 2016  Paweł Redman

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include "elist.h"

#ifdef DEBUG
#define debug(fmt, ...) fprintf(stderr, "%s: " fmt, __func__, ##__VA_ARGS__)
#else
#define debug(fmt, ...)
#endif

#define PROGRAM_NAME "mapcat"
#define PROGRAM_VERSION "0.1.0"

// common.c

typedef struct {
	char *data;
	size_t size, alloc;
} vstr_t;

void vstr_init(vstr_t *vstr);
void vstr_free(vstr_t *vstr);
void vstr_clear(vstr_t *vstr);
int vstr_putc(vstr_t *vstr, char ch);
int vstr_cmp(vstr_t *vstr, const char *str);
char *vstr_strdup(vstr_t *vstr);
void vstr_termz(vstr_t *vstr);
float vstr_atof(vstr_t *vstr);

// lexer.c

#define LEXER_BUFFER 1024

typedef struct {
	int error;
	const char *path;
	FILE *fp;
	bool eof;

	vstr_t *token;
	char buf[LEXER_BUFFER];
	char *buf_c, *buf_e;

	size_t cc, lc, Cc; // character, line, and column counters
	char last;

	bool in_token;
	bool in_quote;
	bool in_comment;
} lexer_state_t;

int lexer_open(lexer_state_t *ls, const char *path, vstr_t *token);
int lexer_get_token(lexer_state_t *ls);
int lexer_assert(lexer_state_t *ls, const char *match, const char *desc);
int lexer_assert_or_eof(lexer_state_t *ls, const char *match, const char *desc);
void lexer_perror(lexer_state_t *ls, const char *fmt, ...);
void lexer_perror_eg(lexer_state_t *ls, const char *expected);
int lexer_get_floats(lexer_state_t *ls, float *out, size_t count);

// mapcat.c

int mapcat_load(const char *path);
int mapcat_save(const char *path);
void mapcat_free(void);
