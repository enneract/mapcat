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

#define offsetin(V, M) ((size_t)&(V)->M - (size_t)(V))

typedef struct {
	void *prev, *next;
} elist_header_t;

static inline elist_header_t *elist_header(void *entry, size_t offs)
{
	return (elist_header_t*)((char*)entry + offs);
}

static inline const elist_header_t *elist_cheader(const void *entry, size_t offs)
{
	return (const elist_header_t*)((const char*)entry + offs);
}

static inline void elist_append_real(void **head, void *entry, size_t offs)
{
	elist_header_t *head_header, *entry_header;

	entry_header = elist_header(entry, offs);

	if (!*head) {
		*head = entry;
		entry_header->prev = entry;
		entry_header->next = NULL;
		return;
	}

	head_header = elist_header(*head, offs);

	entry_header->prev = head_header->prev;
	if (head_header->prev)
		elist_header(head_header->prev, offs)->next = entry;
	head_header->prev = entry;

	entry_header->next = NULL;
}

#define elist_append(head, entry, member) \
elist_append_real((void**)(head), (entry), offsetin((entry), member))

#define elist_next(entry, member) \
(elist_header((entry), offsetin((entry), member))->next)

#define elist_cnext(entry, member) \
(elist_cheader((entry), offsetin((entry), member))->next)

#define elist_for(iter, head, member) \
for ((iter) = (head); (iter); (iter) = elist_next((iter), member))

#define elist_cfor(iter, head, member) \
for ((iter) = (head); (iter); (iter) = elist_cnext((iter), member))

static inline void elist_unlink_real(void **head, void *entry, size_t offs)
{
	elist_header_t *entry_header;

	entry_header = elist_header(entry, offs);

	if (entry_header->prev && *head != entry) {
		elist_header_t *prev_header;
		prev_header = elist_header(entry_header->prev, offs);
		prev_header->next = entry_header->next;
	}

	if (entry_header->next) {
		elist_header_t *next_header;
		next_header = elist_header(entry_header->next, offs);
		next_header->prev = entry_header->prev;
	} else {
		elist_header_t *head_header;
		head_header = elist_header(*head, offs);
		head_header->prev = entry_header->prev;
	}

	if (*head == entry)
		*head = entry_header->next;

}

#define elist_unlink(head, entry, member) \
elist_unlink_real((void**)(head), (entry), offsetin((entry), member))

static inline void elist_append_list_real(void **head1, void *head2, size_t offs)
{
	elist_header_t *head1_header, *head2_header, *last1_header;

	if (!*head1) {
		*head1 = head2;
		return;
	}

	if (!head2)
		return;

	head1_header = elist_header(*head1, offs);
	head2_header = elist_header(head2, offs);
	last1_header = elist_header(head1_header->prev, offs);

	head1_header->prev = head2_header->prev;
	last1_header->next = head2;
	head2_header->prev = head1_header->prev;
}

#define elist_append_list(head1, head2, member) \
elist_append_list_real((void**)(head1), (head2), offsetin((head2), member))
