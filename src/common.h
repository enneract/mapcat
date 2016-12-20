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

#define MAPCAT_DISCARD_SHADER "common/discard"

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
size_t vstr_atoz(vstr_t *vstr);

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
void lexer_close(lexer_state_t *ls);
int lexer_get_token(lexer_state_t *ls);
int lexer_assert(lexer_state_t *ls, const char *match, const char *desc);
int lexer_assert_or_eof(lexer_state_t *ls, const char *match, const char *desc);
void lexer_perror(lexer_state_t *ls, const char *fmt, ...);
void lexer_perror_eg(lexer_state_t *ls, const char *expected);
int lexer_get_floats(lexer_state_t *ls, float *out, size_t count);

// mapcat.c

typedef struct {
	float def[9];
	char *shader;
	float texmap[8];
	elist_header_t list;
} brush_face_t;

typedef struct {
	size_t xres, yres;
	float *def; // (xres * yres * 5) floats
	char *shader;
} brush_patch_t;

typedef struct {
	brush_face_t *faces;
	brush_patch_t *patch;
	elist_header_t list;
} brush_t;

typedef struct {
	char *key;
	char *value;
	elist_header_t list;
} entity_key_t;

typedef struct {
	char *classname;
	brush_t *brushes;
	entity_key_t *keys;

	elist_header_t list;
} entity_t;

typedef struct {
	entity_t *worldspawn;
	entity_t *entities;

	// note: num_entities doesn't include the worldspawn
	size_t num_entities, num_discarded_entities;
	size_t num_brushes, num_discarded_brushes;
	size_t num_patches, num_discarded_patches;
} map_t;

void map_init(map_t *map);
void map_free(map_t *map);
int map_read(map_t *map, const char *path);
int map_write(const map_t *map, const char *path);
int map_merge(map_t *master, map_t *slave);
void map_print_stats(const char *path, const map_t *map);
