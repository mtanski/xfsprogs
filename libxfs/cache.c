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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <xfs/platform_defs.h>
#include <xfs/xfs_list.h>
#include <xfs/cache.h>

#define CACHE_DEBUG 1
#undef CACHE_DEBUG
#define CACHE_DEBUG 1
#undef CACHE_ABORT
/* #define CACHE_ABORT 1 */
#define	HASH_CACHE_RATIO	8

static unsigned int cache_generic_bulkrelse(struct cache *, struct list_head *);

struct cache *
cache_init(
	unsigned int		hashsize,
	struct cache_operations	*cache_operations)
{
	struct cache *		cache;
	unsigned int		i, maxcount;

	maxcount = hashsize * HASH_CACHE_RATIO;

	if (!(cache = malloc(sizeof(struct cache))))
		return NULL;
	if (!(cache->c_hash = calloc(hashsize, sizeof(struct cache_hash)))) {
		free(cache);
		return NULL;
	}

	cache->c_count = 0;
	cache->c_max = 0;
	cache->c_hits = 0;
	cache->c_misses = 0;
	cache->c_maxcount = maxcount;
	cache->c_hashsize = hashsize;
	cache->hash = cache_operations->hash;
	cache->alloc = cache_operations->alloc;
	cache->flush = cache_operations->flush;
	cache->relse = cache_operations->relse;
	cache->compare = cache_operations->compare;
	cache->bulkrelse = cache_operations->bulkrelse ?
		cache_operations->bulkrelse : cache_generic_bulkrelse;
	pthread_mutex_init(&cache->c_mutex, NULL);

	for (i = 0; i < hashsize; i++) {
		list_head_init(&cache->c_hash[i].ch_list);
		cache->c_hash[i].ch_count = 0;
		pthread_mutex_init(&cache->c_hash[i].ch_mutex, NULL);
	}
	return cache;
}

void
cache_walk(
	struct cache *		cache,
	cache_walk_t		visit)
{
	struct cache_hash *	hash;
	struct list_head *	head;
	struct list_head *	pos;
	unsigned int		i;

	for (i = 0; i < cache->c_hashsize; i++) {
		hash = &cache->c_hash[i];
		head = &hash->ch_list;
		pthread_mutex_lock(&hash->ch_mutex);
		for (pos = head->next; pos != head; pos = pos->next)
			visit((struct cache_node *)pos);
		pthread_mutex_unlock(&hash->ch_mutex);
	}
}

#ifdef CACHE_ABORT
#define cache_abort()	abort()
#else
#define cache_abort()	do { } while (0)
#endif

#ifdef CACHE_DEBUG
static void
cache_zero_check(
	struct cache_node *	node)
{
	if (node->cn_count > 0) {
		fprintf(stderr, "%s: refcount is %u, not zero (node=%p)\n",
			__FUNCTION__, node->cn_count, node);
		cache_abort();
	}
}
#define cache_destroy_check(c)	cache_walk((c), cache_zero_check)
#else
#define cache_destroy_check(c)	do { } while (0)
#endif

void
cache_destroy(
	struct cache *		cache)
{
	unsigned int		i;

	cache_destroy_check(cache);
	for (i = 0; i < cache->c_hashsize; i++) {
		list_head_destroy(&cache->c_hash[i].ch_list);
		pthread_mutex_destroy(&cache->c_hash[i].ch_mutex);
	}
	pthread_mutex_destroy(&cache->c_mutex);
	free(cache->c_hash);
	free(cache);
}

static int
cache_shake_node(
	struct cache *		cache,
	cache_key_t		key,
	struct cache_node *	node)
{
	struct list_head *	head;
	struct list_head *	pos;
	struct list_head *	n;
	struct cache_hash *	hash;
	int			count = -1;

	hash = cache->c_hash + cache->hash(key, cache->c_hashsize);
	head = &hash->ch_list;
	pthread_mutex_lock(&hash->ch_mutex);
	for (pos = head->next, n = pos->next;
	     pos != head;
	     pos = n, n = pos->next) {
		if ((struct cache_node *)pos != node)
			continue;
		pthread_mutex_lock(&node->cn_mutex);
		count = node->cn_count;
		pthread_mutex_unlock(&node->cn_mutex);
		if (count != 0)
			break;
		pthread_mutex_destroy(&node->cn_mutex);
		list_del_init(&node->cn_list);
		hash->ch_count--;
		cache->relse(node);
		break;
	}
	pthread_mutex_unlock(&hash->ch_mutex);
	return count;
}

/*
 * We've hit the limit on cache size, so we need to start reclaiming
 * nodes we've used.  This reclaims from the one given hash bucket
 * only.  Returns the number of freed up nodes, its left to the
 * caller to updates the global counter of used nodes for the cache.
 * The hash chain lock is held for the hash list argument, must be
 * dropped before returning.
 * We walk backwards through the hash (remembering we keep recently
 * used nodes toward the front) until we hit an in-use node.  We'll
 * stop there if its a low priority call but keep going if its not.
 */
static unsigned int
cache_shake_hash(
	struct cache *		cache,
	struct cache_hash *	hash,
	unsigned int		priority)
{
	struct list_head	temp;
	struct list_head *	head;
	struct list_head *	pos;
	struct list_head *	n;
	struct cache_node *	node;
	unsigned int		inuse = 0;

	list_head_init(&temp);
	head = &hash->ch_list;
	for (pos = head->prev, n = pos->prev;
	     pos != head;
	     pos = n, n = pos->prev) {
		node = (struct cache_node *)pos;
		pthread_mutex_lock(&node->cn_mutex);
		if (!(inuse = (node->cn_count > 0))) {
			hash->ch_count--;
			list_move_tail(&node->cn_list, &temp);
		}
		pthread_mutex_unlock(&node->cn_mutex);
		if (inuse && !priority)
			break;
	}
	pthread_mutex_unlock(&hash->ch_mutex);
	return cache->bulkrelse(cache, &temp);
}

/*
 * Generic implementation of bulk release, which just iterates over
 * the list calling the single node relse routine for each node.
 */
static unsigned int
cache_generic_bulkrelse(
	struct cache *		cache,
	struct list_head *	list)
{
	struct cache_node *	node;
	unsigned int		count = 0;

	while (!list_empty(list)) {
		node = (struct cache_node *)list->next;
		pthread_mutex_destroy(&node->cn_mutex);
		list_del_init(&node->cn_list);
		cache->relse(node);
		count++;
	}
	return count;
}

/*
 * We've hit the limit on cache size, so we need to start reclaiming
 * nodes we've used.  Start by shaking this hash chain only, unless
 * the shake priority has been increased already.
 * The hash chain lock is held for the hash list argument, must be
 * dropped before returning.
 * Returns new priority at end of the call (in case we call again).
 */
static unsigned int
cache_shake(
	struct cache *		cache,
	struct cache_hash *	hash,
	unsigned int		priority)
{
	unsigned int		count;
	unsigned int		i;

	if (!priority) {	/* do just one */
		count = cache_shake_hash(cache, hash, priority);
	} else {	/* use a bigger hammer */
		pthread_mutex_unlock(&hash->ch_mutex);
		for (count = 0, i = 0; i < cache->c_hashsize; i++) {
			hash = &cache->c_hash[i];
			pthread_mutex_lock(&hash->ch_mutex);
			count += cache_shake_hash(cache, hash, priority - 1);
		}
	}
	if (count) {
		pthread_mutex_lock(&cache->c_mutex);
		cache->c_count -= count;
		pthread_mutex_unlock(&cache->c_mutex);
	}
	return ++priority;
}

/*
 * Allocate a new hash node (updating atomic counter in the process),
 * unless doing so will push us over the maximum cache size.
 */
struct cache_node *
cache_node_allocate(
	struct cache *		cache,
	struct cache_hash *	hashlist)
{
	unsigned int		nodesfree;
	struct cache_node *	node;

	pthread_mutex_lock(&cache->c_mutex);
	if ((nodesfree = (cache->c_count < cache->c_maxcount))) {
		cache->c_count++;
		if (cache->c_count > cache->c_max)
			cache->c_max = cache->c_count;
	}
	cache->c_misses++;
	pthread_mutex_unlock(&cache->c_mutex);
	if (!nodesfree)
		return NULL;
	if (!(node = cache->alloc())) {	/* uh-oh */
		pthread_mutex_lock(&cache->c_mutex);
		cache->c_count--;
		pthread_mutex_unlock(&cache->c_mutex);
		return NULL;
	}
	pthread_mutex_init(&node->cn_mutex, NULL);
	list_head_init(&node->cn_list);
	node->cn_count = 1;
	return node;
}

/*
 * Lookup in the cache hash table.  With any luck we'll get a cache
 * hit, in which case this will all be over quickly and painlessly.
 * Otherwise, we allocate a new node, taking care not to expand the
 * cache beyond the requested maximum size (shrink it if it would).
 * Returns one if hit in cache, otherwise zero.  A node is _always_
 * returned, however.
 */
int
cache_node_get(
	struct cache *		cache,
	cache_key_t		key,
	struct cache_node **	nodep)
{
	struct cache_node *	node = NULL;
	struct cache_hash *	hash;
	struct list_head *	head;
	struct list_head *	pos;
	int			priority = 0;
	int			allocated = 0;

	hash = cache->c_hash + cache->hash(key, cache->c_hashsize);
	head = &hash->ch_list;

  restart:
	pthread_mutex_lock(&hash->ch_mutex);
	for (pos = head->next; pos != head; pos = pos->next) {
		node = (struct cache_node *)pos;
		if (cache->compare(node, key) == 0)
			continue;
		pthread_mutex_lock(&node->cn_mutex);
		node->cn_count++;
		pthread_mutex_unlock(&node->cn_mutex);
		pthread_mutex_lock(&cache->c_mutex);
		cache->c_hits++;
		pthread_mutex_unlock(&cache->c_mutex);
		break;
	}
	if (pos == head) {
		node = cache_node_allocate(cache, hash);
		if (!node) {
			priority = cache_shake(cache, hash, priority);
			goto restart;
		}
		allocated = 1;
		hash->ch_count++;	/* new entry */
	}
	/* looked at it, move to hash list head */
	list_move(&node->cn_list, &hash->ch_list);
	pthread_mutex_unlock(&hash->ch_mutex);
	*nodep = node;
	return allocated;
}

void
cache_node_put(
	struct cache_node *	node)
{
	pthread_mutex_lock(&node->cn_mutex);
#ifdef CACHE_DEBUG
	if (node->cn_count < 1) {
		fprintf(stderr, "%s: node put on refcount %u (node=%p)\n",
				__FUNCTION__, node->cn_count, node);
		cache_abort();
	}
#endif
	node->cn_count--;
	pthread_mutex_unlock(&node->cn_mutex);
}

/*
 * Purge a specific node from the cache.  Reference count must be zero.
 */
int
cache_node_purge(
	struct cache *		cache,
	cache_key_t		key,
	struct cache_node *	node)
{
	int			refcount;

	refcount = cache_shake_node(cache, key, node);
	if (refcount == 0) {
		pthread_mutex_lock(&cache->c_mutex);
		cache->c_count--;
		pthread_mutex_unlock(&cache->c_mutex);
	}
#ifdef CACHE_DEBUG
	if (refcount >= 1) {
		fprintf(stderr, "%s: refcount was %u, not zero (node=%p)\n",
				__FUNCTION__, refcount, node);
		cache_abort();
	}
	if (refcount == -1) {
		fprintf(stderr, "%s: purge node not found! (node=%p)\n",
			__FUNCTION__, node);
		cache_abort();
	}
#endif
	return (refcount == 0);
}

/*
 * Purge all nodes from the cache.  All reference counts must be zero.
 */
void
cache_purge(
	struct cache *		cache)
{
	struct cache_hash *	hash;

	hash = &cache->c_hash[0];
	pthread_mutex_lock(&hash->ch_mutex);
	cache_shake(cache, hash, (unsigned int)-1);
#ifdef CACHE_DEBUG
	if (cache->c_count != 0) {
		fprintf(stderr, "%s: shake on cache %p left %u nodes!?\n",
				__FUNCTION__, cache, cache->c_count);
		cache_abort();
	}
#endif
	/* flush any remaining nodes to disk */
	cache_flush(cache);
}

/*
 * Flush all nodes in the cache to disk. 
 */
void
cache_flush(
	struct cache *		cache)
{
	struct cache_hash *	hash;
	struct list_head *	head;
	struct list_head *	pos;
	struct cache_node *	node;
	int			i;
	
	if (!cache->flush)
		return;
	
	for (i = 0; i < cache->c_hashsize; i++) {
		hash = &cache->c_hash[i];
		
		pthread_mutex_lock(&hash->ch_mutex);
		head = &hash->ch_list;
		for (pos = head->next; pos != head; pos = pos->next) {
			node = (struct cache_node *)pos;
			pthread_mutex_lock(&node->cn_mutex);
			cache->flush(node);
			pthread_mutex_unlock(&node->cn_mutex);
		}
		pthread_mutex_unlock(&hash->ch_mutex);
	}
}

#define	HASH_REPORT	(3*HASH_CACHE_RATIO)
void
cache_report(FILE *fp, const char *name, struct cache * cache)
{
	int i;
	unsigned long count, index, total;
	unsigned long hash_bucket_lengths[HASH_REPORT+2];

	if ((cache->c_hits+cache->c_misses) == 0)
		return;

	/* report cache summary */
	fprintf(fp, "%s: %p\n"
			"Max supported entries = %u\n"
			"Max utilized entries = %u\n"
			"Active entries = %u\n"
			"Hash table size = %u\n"
			"Hits = %llu\n"
			"Misses = %llu\n"
			"Hit ratio = %5.2f\n",
			name, cache,
			cache->c_maxcount,
			cache->c_max,
			cache->c_count,
			cache->c_hashsize,
			cache->c_hits,
			cache->c_misses,
			(double) (cache->c_hits*100/(cache->c_hits+cache->c_misses))
	);

	/* report hash bucket lengths */
	bzero(hash_bucket_lengths, sizeof(hash_bucket_lengths));

	for (i = 0; i < cache->c_hashsize; i++) {
		count = cache->c_hash[i].ch_count;
		if (count > HASH_REPORT)
			index = HASH_REPORT + 1;
		else
			index = count;
		hash_bucket_lengths[index]++;
	}

	total = 0;
	for (i = 0; i < HASH_REPORT+1; i++) {
		total += i*hash_bucket_lengths[i];
		if (hash_bucket_lengths[i] == 0)
			continue;
		fprintf(fp, "Hash buckets with  %2d entries %5ld (%3ld%%)\n", 
			i, hash_bucket_lengths[i], (i*hash_bucket_lengths[i]*100)/cache->c_count);
	}
	if (hash_bucket_lengths[i])	/* last report bucket is the overflow bucket */
		fprintf(fp, "Hash buckets with >%2d entries %5ld (%3ld%%)\n", 
			i-1, hash_bucket_lengths[i], ((cache->c_count-total)*100)/cache->c_count);
}
