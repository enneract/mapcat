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

#define DEBUG
#include "common.h"

typedef struct {
	float def[9];
	char *shader;
	float texmap[8];
	elist_header_t list;
} brush_face_t;

typedef struct {
	brush_face_t *faces;
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

entity_t *worldspawn;
entity_t *entities;

static int read_entity_key(lexer_state_t *ls, entity_t *entity)
{
	entity_key_t *key;

	// classnames are stored separately for easier access later
	if (!vstr_cmp(ls->token, "classname")) {
		if (entity->classname) {
			lexer_perror(ls, "warning: duplicate classname\n");
			free(entity->classname);
			entity->classname = NULL;
		}

		if (lexer_get_token(ls)) {
			lexer_perror_eg(ls, "the classname");
			return 1;
		}

		entity->classname = vstr_strdup(ls->token);
		if (!entity->classname) {
			lexer_perror(ls, "out of memory\n");
			return 1;
		}

		return 0;
	}

	key = malloc(sizeof(entity_key_t));
	if (!key)
		goto error_oom;

	memset(key, 0, sizeof(*key));
	elist_append(&entity->keys, key, list);

	key->key = vstr_strdup(ls->token);
	if (!key->key)
		goto error_oom;

	if (lexer_get_token(ls)) {
		lexer_perror_eg(ls, "the key value");
		return 1;
	}

	key->value = vstr_strdup(ls->token);
	if (!key->value)
		goto error_oom;

	return 0;
error_oom:
	lexer_perror(ls, "out of memory\n");
	return 1;
}

static int read_brush_face(lexer_state_t *ls, brush_t *brush)
{
	brush_face_t *face;

	face = malloc(sizeof(brush_face_t));
	if (!face) {
		lexer_perror(ls, "out of memory\n");
		return 1;
	}

	memset(face, 0, sizeof(*face));
	elist_append(&brush->faces, face, list);

	if (lexer_get_floats(ls, face->def, 3))
		return 1;

	if (lexer_assert(ls, ")", "the end of this face's 1st vector"))
		return 1;

	if (lexer_assert(ls, "(", "the beginning of this face's 2nd vector"))
		return 1;

	if (lexer_get_floats(ls, face->def + 3, 3))
		return 1;

	if (lexer_assert(ls, ")", "the end of this face's 2nd vector"))
		return 1;

	if (lexer_assert(ls, "(", "the beginning of this face's 3rd vector"))
		return 1;

	if (lexer_get_floats(ls, face->def + 6, 3))
		return 1;

	if (lexer_assert(ls, ")", "the end of this face's 3rd vector"))
		return 1;

	if (lexer_get_token(ls)) {
		lexer_perror_eg(ls, "a shader name");
		return 1;
	}

	face->shader = vstr_strdup(ls->token);
	if (!face->shader) {
		lexer_perror(ls, "out of memory\n");
		return 1;
	}

	if (lexer_get_floats(ls, face->texmap, 8))
		return 1;

	return 0;
}

static int read_brush_faces(lexer_state_t *ls, brush_t *brush)
{
	while (1) {
		if (lexer_get_token(ls)) {
			lexer_perror_eg(ls, "the beginning of a brush face "
			                    " ('(') or the end of this brush"
			                    " ('}')");
			return 1;
		}

		if (!vstr_cmp(ls->token, "(")) {
			if (read_brush_face(ls, brush))
				return 1;
		} else if (!vstr_cmp(ls->token, "}"))
			break;
	}

	return 0;
}

static void free_entity_keys(entity_t *entity)
{
	entity_key_t *key, *next;

	for (key = entity->keys; key; key = next) {
		next = elist_next(key, list);

		free(key->key);
		free(key->value);
		free(key);
	}
}

static void free_brush(brush_t *brush)
{
	brush_face_t *face, *face_next;

	for (face = brush->faces; face; face = face_next) {
		face_next = elist_next(face, list);

		free(face->shader);
		free(face);
	}

	free(brush);
}

// this frees an entity_t and its children
// note: this function does NOT unlink the entity from the global list
static void free_entity(entity_t *entity)
{
	brush_t *brush, *next;

	free_entity_keys(entity);
	free(entity->classname);

	for (brush = entity->brushes; brush; brush = next) {
		next = elist_next(brush, list);
		free_brush(brush);
	}

	free(entity);
}

static void merge_into_worldspawn(entity_t *entity)
{
	// the first worldspawn becomes the output's worldspawn.
	// other worldspawn's brushes are appended to it
	if (!worldspawn) {
		worldspawn = entity;
		return;
	}

	// keep only the original key-value pairs
	free_entity_keys(entity);

	elist_append_list(worldspawn->brushes, entity->brushes, list);

	free(entity);
}

static bool brush_discard(brush_t *brush)
{
	brush_face_t *face;

	elist_for(face, brush->faces, list)
		if (!strcmp(face->shader, "mapcat_discard"))
			return true;

	return false;
}

static int read_entity(lexer_state_t *ls)
{
	entity_t *entity;

	entity = malloc(sizeof(entity_t));
	if (!entity) {
		lexer_perror(ls, "out of memory\n");
		return 1;
	}

	memset(entity, 0, sizeof(*entity));

	// read keys and values
	while (1) {
		if (lexer_get_token(ls)) {
			lexer_perror_eg(ls, "a key or the beginning of a brush"
			                    " \"{\" or the end of this entity"
			                    " \"}\"");
			goto error;
		}

		if (!vstr_cmp(ls->token, "{"))
			break;

		if (!vstr_cmp(ls->token, "}"))
			goto no_brushes;

		if (read_entity_key(ls, entity))
			goto error;
	}

	// the opening brace of the first brush in this entity was already
	// read in the loop above, so it has to be skipped in the loop below.
	// the problem is solved by jumping in the middle of the loop
	goto skip_first_brace;

	while (1) {
		brush_t *brush;

		if (lexer_get_token(ls)) {
		L1:
			lexer_perror_eg(ls, "the beginning of a brush \"{\""
			                    " or the end of this entity \"}\"");
			goto error;
		}

		if (!vstr_cmp(ls->token, "}"))
			break;
		else if (vstr_cmp(ls->token, "{"))
			goto L1;

	skip_first_brace:
		brush = malloc(sizeof(brush_t));
		if (!brush) {
			lexer_perror(ls, "out of memory\n");
			goto error;
		}

		memset(brush, 0, sizeof(*brush));

		if (read_brush_faces(ls, brush)) {
			free_brush(brush);
			goto error;
		}

		if (brush_discard(brush))
			free_brush(brush);
		else
			elist_append(&entity->brushes, brush, list);
	}

no_brushes:

	if (entity->classname && !strcmp(entity->classname, "worldspawn"))
		merge_into_worldspawn(entity);
	else
		elist_append(&entities, entity, list);

	return 0;
error:
	free_entity(entity);
	return 1;
}


int mapcat_load(const char *path)
{
	int rv = 1;
	lexer_state_t lexer;
	vstr_t token;

	vstr_init(&token);

	if (lexer_open(&lexer, path, &token)) {
		perror(path);
		return 1;
	}

	while (1) {
		int ret;

		ret = lexer_assert_or_eof(&lexer, "{",
		                          "the beginning of an entity");
		if (ret == -1)
			break;
		if (ret > 0)
			goto out;

		if (read_entity(&lexer))
			goto out;
	}

	rv = 0;
out:
	vstr_free(&token);
	return rv;
}

static int write_brush(FILE *fp, brush_t *brush)
{
	brush_face_t *face;

	elist_for(face, brush->faces, list) {
		size_t i;

		for (i = 0; i < 9; i += 3) {
			if (i)
				fprintf(fp, " ");

			fprintf(fp, "( %f %f %f )", face->def[i],
			        face->def[i + 1], face->def[i + 2]);
		}

		fprintf(fp, " %s", face->shader);

		for (i = 0; i < 5; i++)
			fprintf(fp, " %f", face->texmap[i]);

		// the last three values are integers
		for (i = 5; i < 8; i++)
			fprintf(fp, " %.0f", face->texmap[i]);

		fprintf(fp, "\n");
	}

	if (ferror(fp))
		return -errno;

	return 0;
}

static int write_entity(FILE *fp, entity_t *entity)
{
	entity_key_t *key;
	brush_t *brush;
	size_t brush_counter = 0;

	fprintf(fp, "\"classname\" \"%s\"\n", entity->classname);

	elist_for(key, entity->keys, list)
		fprintf(fp, "\"%s\" \"%s\"\n", key->key, key->value);

	elist_for(brush, entity->brushes, list) {
		fprintf(fp, "// brush %zu\n{\n", brush_counter);
		write_brush(fp, brush);
		fprintf(fp, "}\n");
		brush_counter++;
	}

	if (ferror(fp))
		return -errno;

	return 0;
}

int mapcat_save(const char *path)
{
	int rv = 1;
	FILE *fp;
	entity_t *entity;
	size_t entity_counter = 1; // worldspawn is #0

	fp = fopen(path, "w");
	if (!fp) {
		perror(path);
		goto out;
	}

	if (!worldspawn) {
		fprintf(stderr, "error: worldspawn is missing\n");
		goto out;
	}

	fprintf(fp, "// entity 0\n{\n");
	write_entity(fp, worldspawn);
	fprintf(fp, "}\n");

	elist_for(entity, entities, list) {
		fprintf(fp, "// entity %zu\n{\n", entity_counter);

		if (write_entity(fp, entity)) {
			perror(path);
			goto out;
		}
	 
		fprintf(fp, "}\n");
		entity_counter++;
	}

	if (ferror(fp) || fclose(fp)) {
		perror(path);
		goto out;
	}

	fp = NULL;
	rv = 0;

out:
	if (fp)
		fclose(fp);
	return rv;
}

void mapcat_free(void)
{
	entity_t *entity, *next;

	if (worldspawn)
		free_entity(worldspawn);

	for (entity = entities; entity; entity = next) {
		next = elist_next(entity, list);
		free_entity(entity);
	}
}
