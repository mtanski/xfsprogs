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
#ifndef __CACHE_H__
#define __CACHE_H__

/*
 * Simple, generic implementation of a cache (arbitrary data).
 * Provides a hash table with a capped number of cache entries.
 */

struct cache;
struct cache_hash;
struct cache_node;

typedef void *cache_key_t;
typedef void (*cache_walk_t)(struct cache_node *);
typedef struct cache_node * (*cache_node_alloc_t)(void);
typedef void (*cache_node_relse_t)(struct cache_node *);
typedef unsigned int (*cache_node_hash_t)(cache_key_t, unsigned int);
typedef int (*cache_node_compare_t)(struct cache_node *, cache_key_t);

struct cache_operations {
	cache_node_hash_t	hash;
	cache_node_alloc_t	alloc;
	cache_node_relse_t	relse;
	cache_node_compare_t	compare;
};

struct cache {
	unsigned int		c_maxcount;	/* max cache nodes */
	unsigned int		c_count;	/* count of nodes */
	pthread_mutex_t		c_mutex;	/* node count mutex */
	cache_node_hash_t	hash;		/* node hash function */
	cache_node_alloc_t	alloc;		/* allocation function */
	cache_node_relse_t	relse;		/* memory free function */
	cache_node_compare_t	compare;	/* comparison routine */
	unsigned int		c_hashsize;	/* hash bucket count */
	struct cache_hash	*c_hash;	/* hash table buckets */
};

struct cache_hash {
	struct list_head	ch_list;	/* hash chain head */
	pthread_mutex_t		ch_mutex;	/* hash chain mutex */
};

struct cache_node {
	struct list_head	cn_list;	/* hash chain */
	unsigned int		cn_count;	/* reference count */
	pthread_mutex_t		cn_mutex;	/* refcount mutex */
};

struct cache *cache_init(unsigned int, struct cache_operations *);
void cache_destroy(struct cache *);
void cache_walk(struct cache *, cache_walk_t);
void cache_purge(struct cache *);

int cache_node_get(struct cache *, cache_key_t, struct cache_node **);
void cache_node_put(struct cache_node *);
int cache_node_purge(struct cache *, cache_key_t, struct cache_node *);

#endif	/* __CACHE_H__ */