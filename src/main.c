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
	     "usage: " PROGRAM_NAME " -o outfile infile...\n"
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

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			print_version();
			rv = 0;
			goto out;
		} else if (!strcmp(argv[i], "-h")) {
			print_usage();
			rv = 0;
			goto out;
		} else if (!strcmp(argv[i], "-o")) {
			if (i + 1 >= argc) {
				error("-o needs an argument\n");
				goto out;
			}

			if (output) {
				error("-o can be specified only once\n");
				goto out;
			}

			output = argv[i + 1];
			i++;
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

	elist_for (input, inputs, list)
		if (mapcat_load(input->path))
			goto out;

	if (mapcat_save(output))
		goto out;

	rv = 0;
out:
	mapcat_free();

	for (input = inputs; input; input = next) {
		next = elist_next(input, list);
		free(input);
	}

	return rv;
}
