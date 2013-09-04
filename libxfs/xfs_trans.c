/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <xfs.h>

kmem_zone_t	*xfs_trans_zone;
kmem_zone_t	*xfs_log_item_desc_zone;

/*
 * Initialize the precomputed transaction reservation values
 * in the mount structure.
 */
void
xfs_trans_init(
	struct xfs_mount	*mp)
{
	xfs_trans_resv_calc(mp, &mp->m_reservations);
}

/*
 * Add the given log item to the transaction's list of log items.
 *
 * The log item will now point to its new descriptor with its li_desc field.
 */
void
xfs_trans_add_item(
	struct xfs_trans	*tp,
	struct xfs_log_item	*lip)
{
	struct xfs_log_item_desc *lidp;

	ASSERT(lip->li_mountp == tp->t_mountp);
	ASSERT(lip->li_ailp == tp->t_mountp->m_ail);

	lidp = kmem_zone_zalloc(xfs_log_item_desc_zone, KM_SLEEP | KM_NOFS);

	lidp->lid_item = lip;
	lidp->lid_flags = 0;
	list_add_tail(&lidp->lid_trans, &tp->t_items);

	lip->li_desc = lidp;
}

STATIC void
xfs_trans_free_item_desc(
	struct xfs_log_item_desc *lidp)
{
	list_del_init(&lidp->lid_trans);
	kmem_zone_free(xfs_log_item_desc_zone, lidp);
}

/*
 * Unlink and free the given descriptor.
 */
void
xfs_trans_del_item(
	struct xfs_log_item	*lip)
{
	xfs_trans_free_item_desc(lip->li_desc);
	lip->li_desc = NULL;
}

/*
 * Roll from one trans in the sequence of PERMANENT transactions to
 * the next: permanent transactions are only flushed out when
 * committed with XFS_TRANS_RELEASE_LOG_RES, but we still want as soon
 * as possible to let chunks of it go to the log. So we commit the
 * chunk we've been working on and get a new transaction to continue.
 */
int
xfs_trans_roll(
	struct xfs_trans	**tpp,
	struct xfs_inode	*dp)
{
	struct xfs_trans	*trans;
	unsigned int		logres, count;
	int			error;

	/*
	 * Ensure that the inode is always logged.
	 */
	trans = *tpp;
	xfs_trans_log_inode(trans, dp, XFS_ILOG_CORE);

	/*
	 * Copy the critical parameters from one trans to the next.
	 */
	logres = trans->t_log_res;
	count = trans->t_log_count;
	*tpp = xfs_trans_dup(trans);

	/*
	 * Commit the current transaction.
	 * If this commit failed, then it'd just unlock those items that
	 * are not marked ihold. That also means that a filesystem shutdown
	 * is in progress. The caller takes the responsibility to cancel
	 * the duplicate transaction that gets returned.
	 */
	error = xfs_trans_commit(trans, 0);
	if (error)
		return (error);

	trans = *tpp;

	/*
	 * Reserve space in the log for th next transaction.
	 * This also pushes items in the "AIL", the list of logged items,
	 * out to disk if they are taking up space at the tail of the log
	 * that we want to use.  This requires that either nothing be locked
	 * across this call, or that anything that is locked be logged in
	 * the prior and the next transactions.
	 */
	error = xfs_trans_reserve(trans, 0, logres, 0,
				  XFS_TRANS_PERM_LOG_RES, count);
	/*
	 *  Ensure that the inode is in the new transaction and locked.
	 */
	if (error)
		return error;

	xfs_trans_ijoin(trans, dp, 0);
	return 0;
}
