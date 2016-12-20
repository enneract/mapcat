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

//
// freeing
//

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

	if (brush->patch) {
		free(brush->patch->shader);
		free(brush->patch->def);
		free(brush->patch);
	} else {
		for (face = brush->faces; face; face = face_next) {
			face_next = elist_next(face, list);

			free(face->shader);
			free(face);
		}
	}

	free(brush);
}

// this frees an entity_t and its children
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

//
// reading
//

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

static int read_brush_patch_points(lexer_state_t *ls, brush_patch_t *patch)
{
	size_t y, x;

	for (y = 0; y < patch->yres; y++) {
		if (lexer_assert(ls, "(", "the beginning of a patch row"))
			return 1;

		for (x = 0; x < patch->xres; x++) {
			size_t offs;

			if (lexer_assert(ls, "(", "the beginning of a patch cell"))
				return 1;

			offs = (y * patch->xres + x) * 5;
			if (lexer_get_floats(ls, patch->def + offs, 5))
				return 1;

			if (lexer_assert(ls, ")", "the end of a patch cell"))
				return 1;
		}

		if (lexer_assert(ls, ")", "the end of a patch row"))
			return 1;
	}

	return 0;
}

static int read_brush_patch(lexer_state_t *ls, brush_t *brush)
{
	if (lexer_assert(ls, "{", NULL))
		return 1;

	brush->patch = malloc(sizeof(brush_patch_t));
	if (!brush->patch) {
		lexer_perror(ls, "out of memory\n");
		return 1;
	}

	memset(brush->patch, 0, sizeof(*brush->patch));

	if (lexer_get_token(ls)) {
		lexer_perror_eg(ls, "the shader name");
		return 1;
	}

	brush->patch->shader = vstr_strdup(ls->token);
	if (!brush->patch->shader) {
		lexer_perror_eg(ls, "out of memory\n");
		return 1;
	}

	if (lexer_assert(ls, "(", NULL))
		return 1;

	if (lexer_get_token(ls)) {
		lexer_perror_eg(ls, "this patch's Y resolution");
		return 1;
	}
	brush->patch->yres = vstr_atoz(ls->token);

	if (lexer_get_token(ls)) {
		lexer_perror_eg(ls, "this patch's X resolution");
		return 1;
	}
	brush->patch->xres = vstr_atoz(ls->token);

	brush->patch->def = malloc(sizeof(float) * brush->patch->xres *
	                           brush->patch->yres * 5);
	if (!brush->patch->def) {
		lexer_perror(ls, "out of memory\n");
		return 1;
	}

	if (lexer_assert(ls, "0", NULL) ||
	    lexer_assert(ls, "0", NULL) ||
	    lexer_assert(ls, "0", NULL) ||
	    lexer_assert(ls, ")", "the end of this patch's header") ||
	    lexer_assert(ls, "(", "the beginning of this patch's points"))
		return 1;

	if (read_brush_patch_points(ls, brush->patch))
		return 1;

	if (lexer_assert(ls, ")", "the end of this patch"))
		return 1;

	if (lexer_assert(ls, "}", "the end of this brush"))
		return 1;

	return 0;
}

static int read_brush_faces(lexer_state_t *ls, brush_t *brush)
{
	while (1) {
		if (lexer_get_token(ls)) {
		bad_token:
			lexer_perror_eg(ls, "the beginning of a brush face, "
			                    " \"(\", the end of this brush"
			                    " \"}\" or the beginning of a"
			                    " patch \"patchDef2\"");
			return 1;
		}

		if (!vstr_cmp(ls->token, "(")) {
			if (read_brush_face(ls, brush))
				return 1;
		} else if (!vstr_cmp(ls->token, "}"))
			break;
		else if (!vstr_cmp(ls->token, "patchDef2")) {
			if (read_brush_patch(ls, brush))
				return 1;
		} else
			goto bad_token;
	}

	return 0;
}

static bool brush_discard(brush_t *brush)
{
	brush_face_t *face;

	if (brush->patch)
		return !strcmp(brush->patch->shader, MAPCAT_DISCARD_SHADER);

	elist_for(face, brush->faces, list)
		if (!strcmp(face->shader, MAPCAT_DISCARD_SHADER))
			return true;

	return false;
}

static int read_entity(lexer_state_t *ls, map_t *map)
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

		if (brush_discard(brush)) {
			if (brush->patch)
				map->num_discarded_patches++;
			else
				map->num_discarded_brushes++;

			free_brush(brush);
		} else {
			elist_append(&entity->brushes, brush, list);

			if (brush->patch)
				map->num_patches++;
			else
				map->num_brushes++;
		}
	}

no_brushes:
	if (!strcmp(entity->classname, "worldspawn")) {
		if (map->worldspawn) {
			lexer_perror(ls, "this entity is a worldspawn, but a "
			                 "worldspawn was already read earlier");
			goto error;
		}

		map->worldspawn = entity;
	} else {
		elist_append(&map->entities, entity, list);
		map->num_entities++;
	}

	return 0;
error:
	free_entity(entity);
	return 1;
}

//
// writing
//

static int write_brush_patch(FILE *fp, const brush_patch_t *patch)
{
	size_t y, x, i;

	fprintf(fp, "patchDef2\n{\n%s\n( %zu %zu 0 0 0 )\n(\n",
	        patch->shader, patch->yres, patch->xres);

	for (y = 0; y < patch->yres; y++) {
		fprintf(fp, "(");

		for (x = 0; x < patch->xres; x++) {
			size_t offs = (y * patch->xres + x) * 5;

			fprintf(fp, " (");
			for (i = 0; i < 5; i++)
				fprintf(fp, " %f", patch->def[offs + i]);
			fprintf(fp, " )");
		}

		fprintf(fp, " )\n");
	}

	fprintf(fp, ")\n}\n");

	return 0;
}

static int write_brush(FILE *fp, const brush_t *brush)
{
	const brush_face_t *face;

	if (brush->patch)
		return write_brush_patch(fp, brush->patch);

	elist_cfor(face, brush->faces, list) {
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

static int write_entity(FILE *fp, const entity_t *entity)
{
	const entity_key_t *key;
	const brush_t *brush;
	size_t brush_counter = 0;

	if (entity->classname)
		fprintf(fp, "\"classname\" \"%s\"\n", entity->classname);

	elist_cfor(key, entity->keys, list)
		fprintf(fp, "\"%s\" \"%s\"\n", key->key, key->value);

	elist_cfor(brush, entity->brushes, list) {
		fprintf(fp, "// brush %zu\n{\n", brush_counter);
		write_brush(fp, brush);
		fprintf(fp, "}\n");
		brush_counter++;
	}

	if (ferror(fp))
		return -errno;

	return 0;
}

//
// entry points
//

void map_init(map_t *map)
{
	memset(map, 0, sizeof(*map));
}

void map_free(map_t *map)
{
	entity_t *entity, *next;

	if (map->worldspawn)
		free_entity(map->worldspawn);

	for (entity = map->entities; entity; entity = next) {
		next = elist_next(entity, list);
		free_entity(entity);
	}
}

int map_read(map_t *map, const char *path)
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

		if (read_entity(&lexer, map))
			goto out;
	}

	rv = 0;
out:
	vstr_free(&token);
	lexer_close(&lexer);

	if (rv)
		map_free(map);

	return rv;
}

int map_write(const map_t *map, const char *path)
{
	int rv = 1;
	FILE *fp;
	const entity_t *entity;
	size_t entity_counter = 1; // worldspawn is #0

	fp = fopen(path, "w");
	if (!fp) {
		perror(path);
		goto out;
	}

	if (!map->worldspawn) {
		fprintf(stderr, "error: worldspawn is missing\n");
		goto out;
	}

	fprintf(fp, "// entity 0\n{\n");
	write_entity(fp, map->worldspawn);
	fprintf(fp, "}\n");

	elist_cfor(entity, map->entities, list) {
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

int map_postprocess(map_t *map, bool filter_team_ents)
{
	entity_t *entity;
	entity_key_t *key;
	char *prefix = NULL;

	if (filter_team_ents) {
		entity_t *next;

		for (entity = map->entities; entity; entity = next) {
			next = elist_next(entity, list);

			if (strncmp(entity->classname, "team_", 5) &&
			    strncmp(entity->classname, "info_", 5))
				continue;

			map->num_entities--;
			map->num_discarded_entities++;
			elist_unlink(&map->entities, entity, list);
			free_entity(entity);
		}
	}

	if (map->worldspawn) {
		entity_key_t *next;

		for (key = map->worldspawn->keys; key; key = next) {
			next = elist_next(key, list);

			if (!strcmp(key->key, "mapcat_prefix")) {
				// shouldn't happen, but just in case
				// (to prevent memory leaks)
				if (prefix)
					free(prefix);

				prefix = key->value;
				elist_unlink(&map->worldspawn->keys, key, list);
				free(key->key);
				// key->value is freed later (as prefix)
				free(key);
			}
		}
	}

	if (prefix) {
		size_t prefix_len = strlen(prefix);

		elist_for(entity, map->entities, list)
		elist_for(key, entity->keys, list) {
			char *new;
			size_t value_len;

			if (strcmp(key->key, "target") &&
			    strcmp(key->key, "targetname"))
				continue;

			value_len = strlen(key->value);

			new = malloc(value_len + prefix_len + 1);
			if (!new) {
				fprintf(stderr, "error: out of memory\n");
				return 1;
			}

			memcpy(new, prefix, prefix_len);
			memcpy(new + prefix_len, key->value, value_len);
			new[prefix_len + value_len] = 0;

			free(key->value);
			key->value = new;
		}

		free(prefix);
	}

	return 0;
}

//RETURN VALUE
//	always 0 (this function cannot fail (yet))
// slave is left in invalid state after this function returns, do not use it
int map_merge(map_t *master, map_t *slave)
{
	if (!master->worldspawn) {
		// the first worldspawn is kept intact
		master->worldspawn = slave->worldspawn;
	} else if (slave->worldspawn) {
		// worldspawns of subsequent maps are discarded, except their
		// brushes, which are appended to the master's worldspawn
		elist_append_list(&master->worldspawn->brushes,
		                  slave->worldspawn->brushes, list);

		free_entity_keys(slave->worldspawn);
		free(slave->worldspawn->classname);
		free(slave->worldspawn);
	}

	// entities are always kept intact
	elist_append_list(&master->entities, slave->entities, list);

	master->num_entities += slave->num_entities;
	master->num_discarded_entities += slave->num_discarded_entities;
	master->num_brushes += slave->num_brushes;
	master->num_discarded_brushes += slave->num_discarded_brushes;
	master->num_patches += slave->num_patches;
	master->num_discarded_patches += slave->num_discarded_patches;

	return 0;
}

void map_print_stats(const char *path, const map_t *map)
{
	size_t ents_with_worldspawn;

	ents_with_worldspawn = map->num_entities + (map->worldspawn ? 1 : 0);

	printf("%s: ", path);

	printf("%zu entit%s", ents_with_worldspawn,
	       (ents_with_worldspawn == 1 ? "y" : "ies"));
	if (map->num_discarded_entities)
		printf(" (%zu discarded)", map->num_discarded_entities);

	printf(", %zu brush%s", map->num_brushes,
	       (map->num_brushes == 1 ? "" : "es"));
	if (map->num_discarded_brushes)
		printf(" (%zu discarded)", map->num_discarded_brushes);

	printf(", %zu patch%s", map->num_patches,
	       (map->num_patches == 1 ? "" : "es"));
	if (map->num_discarded_patches)
		printf(" (%zu discarded)", map->num_discarded_patches);

	printf("\n");
}
