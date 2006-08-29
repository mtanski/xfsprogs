/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
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

#include <libxfs.h>
#include "dir_stack.h"
#include "err_protos.h"
#include "threads.h"

/*
 * a directory stack for holding directories while
 * we traverse filesystem hierarchy subtrees.
 * names are kind of misleading as this is really
 * implemented as an inode stack.  so sue me...
 */

static dir_stack_t	dirstack_freelist;
static int		dirstack_init = 0;
static pthread_mutex_t	dirstack_mutex;
static pthread_mutexattr_t dirstack_mutexattr;


void
dir_stack_init(dir_stack_t *stack)
{
	stack->cnt = 0;
	stack->head = NULL;

	if (dirstack_init == 0)  {
		dirstack_init = 1;
		PREPAIR_MTX_ATTR_INIT(&dirstack_mutexattr);
#ifdef PTHREAD_MUTEX_SPINBLOCK_NP
		PREPAIR_MTX_ATTR_SET(&dirstack_mutexattr, PTHREAD_MUTEX_SPINBLOCK_NP);
#endif
		PREPAIR_MTX_LOCK_INIT(&dirstack_mutex, &dirstack_mutexattr);
		dir_stack_init(&dirstack_freelist);
	}

	stack->cnt = 0;
	stack->head = NULL;

	return;
}

static void
dir_stack_push(dir_stack_t *stack, dir_stack_elem_t *elem)
{
	ASSERT(stack->cnt > 0 || (stack->cnt == 0 && stack->head == NULL));

	elem->next = stack->head;
	stack->head = elem;
	stack->cnt++;

	return;
}

static dir_stack_elem_t *
dir_stack_pop(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;

	if (stack->cnt == 0)  {
		ASSERT(stack->head == NULL);
		return(NULL);
	}

	elem = stack->head;

	ASSERT(elem != NULL);

	stack->head = elem->next;
	elem->next = NULL;
	stack->cnt--;

	return(elem);
}

void
push_dir(dir_stack_t *stack, xfs_ino_t ino)
{
	dir_stack_elem_t *elem;

	PREPAIR_MTX_LOCK(&dirstack_mutex);
	if (dirstack_freelist.cnt == 0)  {
		if ((elem = malloc(sizeof(dir_stack_elem_t))) == NULL)  {
			PREPAIR_MTX_UNLOCK(&dirstack_mutex);
			do_error(
		_("couldn't malloc dir stack element, try more swap\n"));
			exit(1);
		}
	} else  {
		elem = dir_stack_pop(&dirstack_freelist);
	}
	PREPAIR_MTX_UNLOCK(&dirstack_mutex);

	elem->ino = ino;

	dir_stack_push(stack, elem);

	return;
}

xfs_ino_t
pop_dir(dir_stack_t *stack)
{
	dir_stack_elem_t *elem;
	xfs_ino_t ino;

	elem = dir_stack_pop(stack);

	if (elem == NULL)
		return(NULLFSINO);

	ino = elem->ino;
	elem->ino = NULLFSINO;

	PREPAIR_MTX_LOCK(&dirstack_mutex);
	dir_stack_push(&dirstack_freelist, elem);
	PREPAIR_MTX_UNLOCK(&dirstack_mutex);

	return(ino);
}
