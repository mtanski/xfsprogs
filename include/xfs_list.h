/*
 * Copyright (c) 2006 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef __LIST_H__
#define __LIST_H__

/*
 * Simple, generic doubly-linked list implementation.
 */

struct list_head {
	struct list_head *next;
	struct list_head *prev;
};

static inline void list_head_init(struct list_head *list)
{
	list->next = list->prev = list;
}

static inline void list_head_destroy(struct list_head *list)
{
	list->next = list->prev = NULL;
}

static inline void __list_add(struct list_head *add,
			      struct list_head *prev, struct list_head *next)
{
	next->prev = add;
	add->next = next;
	add->prev = prev;
	prev->next = add;
}

static inline void list_add(struct list_head *add, struct list_head *head)
{
	__list_add(add, head, head->next);
}

static inline void list_add_tail(struct list_head *add, struct list_head *head)
{
	__list_add(add, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	list_head_init(entry);
}

static inline void list_move(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

static inline void list_move_tail(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#endif	/* __LIST_H__ */
