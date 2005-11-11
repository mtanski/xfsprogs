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

#include <xfs.h>

/*
 * Initialize the precomputed transaction reservation values
 * in the mount structure.
 */
void
xfs_trans_init(
	xfs_mount_t	*mp)
{
	xfs_trans_reservations_t	*resp;

	resp = &(mp->m_reservations);
	resp->tr_write =
		(uint)(XFS_CALC_WRITE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_itruncate =
		(uint)(XFS_CALC_ITRUNCATE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_rename =
		(uint)(XFS_CALC_RENAME_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_link = (uint)XFS_CALC_LINK_LOG_RES(mp);
	resp->tr_remove =
		(uint)(XFS_CALC_REMOVE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_symlink =
		(uint)(XFS_CALC_SYMLINK_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_create =
		(uint)(XFS_CALC_CREATE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_mkdir =
		(uint)(XFS_CALC_MKDIR_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_ifree =
		(uint)(XFS_CALC_IFREE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_ichange =
		(uint)(XFS_CALC_ICHANGE_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_growdata = (uint)XFS_CALC_GROWDATA_LOG_RES(mp);
	resp->tr_swrite = (uint)XFS_CALC_SWRITE_LOG_RES(mp);
	resp->tr_writeid = (uint)XFS_CALC_WRITEID_LOG_RES(mp);
	resp->tr_addafork =
		(uint)(XFS_CALC_ADDAFORK_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_attrinval = (uint)XFS_CALC_ATTRINVAL_LOG_RES(mp);
	resp->tr_attrset =
		(uint)(XFS_CALC_ATTRSET_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_attrrm =
		(uint)(XFS_CALC_ATTRRM_LOG_RES(mp) + XFS_DQUOT_LOGRES(mp));
	resp->tr_clearagi = (uint)XFS_CALC_CLEAR_AGI_BUCKET_LOG_RES(mp);
	resp->tr_growrtalloc = (uint)XFS_CALC_GROWRTALLOC_LOG_RES(mp);
	resp->tr_growrtzero = (uint)XFS_CALC_GROWRTZERO_LOG_RES(mp);
	resp->tr_growrtfree = (uint)XFS_CALC_GROWRTFREE_LOG_RES(mp);
}
