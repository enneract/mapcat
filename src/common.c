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

#include "common.h"

void vstr_init(vstr_t *vstr)
{
	memset(vstr, 0, sizeof(*vstr));
}

void vstr_free(vstr_t *vstr)
{
	free(vstr->data);
}

void vstr_clear(vstr_t *vstr)
{
	vstr->size = 0;
}

static int vstr_enlarge(vstr_t *vstr)
{
	size_t new_alloc;

	new_alloc = (vstr->alloc + 4) * 3 / 2;
	debug("%zu -> %zu\n", vstr->alloc, new_alloc);

	vstr->data = realloc(vstr->data, new_alloc);
	if (!vstr->data)
		return 1;

	vstr->alloc = new_alloc;
	return 0;
}

int vstr_putc(vstr_t *vstr, char ch)
{
	// note: keep at least one character free at all times for vstr_termz
	if (vstr->size + 2 > vstr->alloc)
		if (vstr_enlarge(vstr))
			return -ENOMEM;

	vstr->data[vstr->size] = ch;
	vstr->size++;
	return 0;
}

int vstr_cmp(vstr_t *vstr, const char *str)
{
	size_t len;

	len = strlen(str);
	if (vstr->size < len)
		len = vstr->size;

	return memcmp(vstr->data, str, len);
}

char *vstr_strdup(vstr_t *vstr)
{
	char *str;

	str = malloc(vstr->size + 1);
	if (!str)
		return NULL;

	memcpy(str, vstr->data, vstr->size);
	str[vstr->size] = 0;

	return str;
}

void vstr_termz(vstr_t *vstr)
{
	vstr->data[vstr->size] = 0;
}

float vstr_atof(vstr_t *vstr)
{
	if (!vstr->size)
		return 0;

	vstr_termz(vstr);
	return atof(vstr->data);
}

size_t vstr_atoz(vstr_t *vstr)
{
	if (!vstr->size)
		return 0;

	vstr_termz(vstr);
	return strtoull(vstr->data, NULL, 10);
}
