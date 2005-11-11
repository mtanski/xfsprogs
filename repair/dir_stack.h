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

typedef struct dir_stack_elem  {
	xfs_ino_t		ino;
	struct dir_stack_elem	*next;
} dir_stack_elem_t;

typedef struct dir_stack  {
	int			cnt;
	dir_stack_elem_t	*head;
} dir_stack_t;


void		dir_stack_init(dir_stack_t *stack);

void		push_dir(dir_stack_t *stack, xfs_ino_t ino);
xfs_ino_t	pop_dir(dir_stack_t *stack);
