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
#include <ctype.h>

int lexer_open(lexer_state_t *ls, const char *path, vstr_t *token)
{
	ls->error = 0;
	ls->path = path;

	ls->fp = fopen(path, "r");
	if (!ls->fp)
		return -errno;

	ls->eof = false;

	ls->token = token;
	ls->buf_e = ls->buf_c = ls->buf;
	ls->cc = ls->lc = ls->Cc = 0;

	ls->in_token = false;
	ls->in_quote = false;
	ls->in_comment = false;

	return 0;
}

void lexer_close(lexer_state_t *ls)
{
	fclose(ls->fp);
}

//RETURN VALUES
//	<0 on error
//	0 on success
//	note: sets ls->eof to true if there's no more data left
static int fill_buffer(lexer_state_t *ls)
{
	size_t read;

	read = fread(ls->buf, 1, sizeof(ls->buf), ls->fp);
	debug("read = %zu\n", read);
	if (read < sizeof(ls->buf)) {
		if (ferror(ls->fp))
			return -errno;

		ls->eof = true;
		debug("no data left, ls->fp closed\n");
	}

	ls->buf_c = ls->buf;
	ls->buf_e = ls->buf + read;
	return 0;
}

//RETURN VALUES
//	-ENOMEM
//	-EAGAIN when the buffer runs out
//	0 on success
//	1 when no data is left
static int read_buffer(lexer_state_t *ls)
{
	while (ls->buf_c < ls->buf_e) {
		bool ret_token = false;

		debug("*ls->buf_c = %c, ls->last = %c\n", *ls->buf_c, ls->last);

		if (*ls->buf_c == '\n') {
			ls->lc++;
			ls->Cc = 0;
		}

		if (ls->in_comment) {
			if (*ls->buf_c == '\n')
				ls->in_comment = false;
		} else if (isspace(*ls->buf_c) && !ls->in_quote) {
			if (ls->in_token) {
				ls->in_token = false;
				ret_token = true;
			}
		} else if (*ls->buf_c == '/' && ls->last == '/') {
			ls->in_comment = true;
			ls->in_token = false;

			ls->token->size--; // remove the first slash
			if (ls->token->size)
				ret_token = true;
		} else if (*ls->buf_c == '\"' &&
		           (ls->cc && ls->last != '\\')) {
			ls->in_quote = !ls->in_quote;

			if (!ls->in_quote) {
				ls->in_token = false;
				ret_token = true;
			}
		} else {
			if (!ls->in_token) {
				ls->in_token = true;
			}
		}

		if (ls->in_token)
			if (vstr_putc(ls->token, *ls->buf_c)) {
				ls->error = ENOMEM;
				return -ENOMEM;
			}

		ls->last = *ls->buf_c;
		ls->buf_c++;
		ls->cc++;
		ls->Cc++;

		if (ret_token)
			return 0;
	}

	if (ls->eof) {
		if (ls->token->size > 0)
			return 0;
		return 1;
	}

	return -EAGAIN;
}

//RETURN VALUES
//	<0 on error
//	0 on success
//	1 when no data is left
int lexer_get_token(lexer_state_t *ls)
{
	int ret;

	vstr_clear(ls->token);

	while (1) {
		ret = read_buffer(ls);
		debug("read_buffer = %i\n", ret);
		if (ret != -EAGAIN)
			return ret;

		ret = fill_buffer(ls);
		debug("fill_buffer = %i\n", ret);
		if (ret < 0)
			return ret;
	}
}

void lexer_perror(lexer_state_t *ls, const char *fmt, ...)
{
	va_list vl;

	fprintf(stderr, "%s:%zu:%zu: ", ls->path, ls->lc + 1, ls->Cc + 1);

	if (ls->error) {
		perror(NULL);
	} else {
		va_start(vl, fmt);
		vfprintf(stderr, fmt, vl);
		va_end(vl);
	}
}

void lexer_perror_eg(lexer_state_t *ls, const char *expected)
{
	if (ls->eof && ls->buf_c == ls->buf_e)
		lexer_perror(ls, "expected %s, got EOF\n", expected);
	else {
		vstr_termz(ls->token);
		lexer_perror(ls, "expected %s, got \"%s\"\n", expected,
		             ls->token->data);
	}
}

int lexer_assert(lexer_state_t *ls, const char *match, const char *desc)
{
	int ret;

	ret = lexer_get_token(ls);
	if (ret) {
		lexer_perror(ls, "expected %s%s\"%s\", got EOF\n",
		             (desc ? desc : ""), (desc ? " " : ""), match);
		return 1;
	}

	if (vstr_cmp(ls->token, match)) {
		vstr_termz(ls->token);
		lexer_perror(ls, "expected %s%s\"%s\", got \"%s\"\n",
		             (desc ? desc : ""), (desc ? " " : ""), match,
		             ls->token->data);
		return 1;
	}

	return 0;
} 

//RETURN VALUE
//	-1 on eof (also success)
//	0 on success
//	1 on error
int lexer_assert_or_eof(lexer_state_t *ls, const char *match, const char *desc)
{
	int ret;

	ret = lexer_get_token(ls);
	if (ret < 0) {
		perror("lexer");
		return 1;
	}

	if (ret == 1) {
		return -1;
	}

	if (vstr_cmp(ls->token, match)) {
		lexer_perror(ls, "expected %s%s\"%s\" or EOF, got \"%.*s\"\n",
		             (desc ? desc : ""), (desc ? " " : ""), match,
		             (int)ls->token->size, ls->token->data);
		return 1;
	}

	return 0;
} 

int lexer_get_floats(lexer_state_t *ls, float *out, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (lexer_get_token(ls)) {
			lexer_perror_eg(ls, "a number");
			return 1;
		}

		out[i] = vstr_atof(ls->token);
	}

	return 0;
}
