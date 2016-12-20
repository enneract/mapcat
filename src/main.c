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
#include <unistd.h>

#define error(fmt, ...) fprintf(stderr, PROGRAM_NAME ": " fmt, ##__VA_ARGS__)

void print_version(void)
{
	puts(PROGRAM_NAME " " PROGRAM_VERSION ", Copyright (C) 2016  Paweł Redman\n"
	     PROGRAM_NAME " comes with ABSOLUTELY NO WARRANTY.\n"
	     "This is free software, and you are welcome to redistribute it\n"
             "under certain conditions. See the file COPYING.");
}

void print_usage(void)
{
	puts(PROGRAM_NAME " " PROGRAM_VERSION "\n"
	     "usage: " PROGRAM_NAME " [-q] -o outfile infile...\n"
	     "    or " PROGRAM_NAME " -v\n"
	     "    or " PROGRAM_NAME " -h");
}

typedef struct {
	char *path;
	elist_header_t list;
} input_file_t;

int main(int argc, char **argv)
{
	int rv = 1, i;
	input_file_t *inputs = NULL, *input, *next;
	char *output = NULL;
	bool read_flags = true, quiet = false;
	map_t map;

	for (i = 1; i < argc; i++) {
		if (read_flags && !strcmp(argv[i], "-v")) {
			print_version();
			rv = 0;
			goto out;
		} else if (read_flags && !strcmp(argv[i], "-h")) {
			print_usage();
			rv = 0;
			goto out;
		} else if (read_flags && !strcmp(argv[i], "-q")) {
			quiet = true;
		} else if (read_flags && !strcmp(argv[i], "-o")) {
			if (i + 1 >= argc) {
			o_needs_an_argument:
				error("-o needs an argument\n");
				goto out;
			}

			if (!strcmp(argv[i + 1], "--")) {
				i++;

				if (i + 1 >= argc)
					goto o_needs_an_argument;
			}

			if (output) {
				error("-o can be specified only once\n");
				goto out;
			}

			output = argv[i + 1];
			i++;
		} else if (read_flags && !strcmp(argv[i], "--")) {
			read_flags = false;
		} else {
			input = malloc(sizeof(input_file_t));
			if (!input) {
				error("out of memory\n");
				goto out;
			}

			input->path = argv[i];
			elist_append(&inputs, input, list);
		}
	}

	if (!inputs) {
		error("no input files specified, try '" PROGRAM_NAME " -h'\n");
		goto out;
	}

	if (!output) {
		error("no output file specified, try '" PROGRAM_NAME " -h'\n");
		goto out;
	}

	map_init(&map);

	elist_for(input, inputs, list) {
		map_t part;

		map_init(&part);

		if (map_read(&part, input->path)) {
			error("error: couldn't read %s\n", input->path);
			goto out;
		}

		if (!quiet)
			map_print_stats(input->path, &part);

		// team_* and info_* ents are kept only in the first part
		if (map_postprocess(&part, (input != inputs))) {
			map_free(&map);
			map_free(&part);
			goto out;
		}

		if (map_merge(&map, &part)) {
			error("error: couldn't merge %s into %s\n",
			      input->path, output);
			map_free(&map);
			map_free(&part);
			goto out;
		}
	}

	if (!quiet)
		map_print_stats(output, &map);

	if (map_write(&map, output)) {
		error("error: couldn't write %s\n", output);
		map_free(&map);
		goto out;
	}

	map_free(&map);
	rv = 0;
out:
	for (input = inputs; input; input = next) {
		next = elist_next(input, list);
		free(input);
	}

	return rv;
}
