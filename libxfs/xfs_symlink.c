/*
 * Copyright 2013 Red Hat, Inc.
 * All rights reserved.
 */

#include "xfs.h"

/*
 * Each contiguous block has a header, so it is not just a simple pathlen
 * to FSB conversion.
 */
int
xfs_symlink_blocks(
	struct xfs_mount *mp,
	int		pathlen)
{
	int		fsblocks = 0;
	int		len = pathlen;

	do {
		fsblocks++;
		len -= XFS_SYMLINK_BUF_SPACE(mp, mp->m_sb.sb_blocksize);
	} while (len > 0);

	ASSERT(fsblocks <= XFS_SYMLINK_MAPS);
	return fsblocks;
}

/*
 * This is used by mkfs/proto.c to create symlinks.
 */
int
xfs_symlink_hdr_set(
	struct xfs_mount	*mp,
	xfs_ino_t		ino,
	uint32_t		offset,
	uint32_t		size,
	struct xfs_buf		*bp)
{
	struct xfs_dsymlink_hdr	*dsl = bp->b_addr;

	if (!xfs_sb_version_hascrc(&mp->m_sb))
		return 0;

	dsl->sl_magic = cpu_to_be32(XFS_SYMLINK_MAGIC);
	dsl->sl_offset = cpu_to_be32(offset);
	dsl->sl_bytes = cpu_to_be32(size);
	uuid_copy(&dsl->sl_uuid, &mp->m_sb.sb_uuid);
	dsl->sl_owner = cpu_to_be64(ino);
	dsl->sl_blkno = cpu_to_be64(bp->b_bn);
	bp->b_ops = &xfs_symlink_buf_ops;

	return sizeof(struct xfs_dsymlink_hdr);
}

/*
 * Checking of the symlink header is split into two parts. the verifier does
 * CRC, location and bounds checking, the unpacking function checks the path
 * parameters and owner.
 */
bool
xfs_symlink_hdr_ok(
	struct xfs_mount	*mp,
	xfs_ino_t		ino,
	uint32_t		offset,
	uint32_t		size,
	struct xfs_buf		*bp)
{
	struct xfs_dsymlink_hdr *dsl = bp->b_addr;

	if (offset != be32_to_cpu(dsl->sl_offset))
		return false;
	if (size != be32_to_cpu(dsl->sl_bytes))
		return false;
	if (ino != be64_to_cpu(dsl->sl_owner))
		return false;

	/* ok */
	return true;

}

static bool
xfs_symlink_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_target->bt_mount;
	struct xfs_dsymlink_hdr	*dsl = bp->b_addr;

	if (!xfs_sb_version_hascrc(&mp->m_sb))
		return false;
	if (dsl->sl_magic != cpu_to_be32(XFS_SYMLINK_MAGIC))
		return false;
	if (!uuid_equal(&dsl->sl_uuid, &mp->m_sb.sb_uuid))
		return false;
	if (bp->b_bn != be64_to_cpu(dsl->sl_blkno))
		return false;
	if (be32_to_cpu(dsl->sl_offset) +
				be32_to_cpu(dsl->sl_bytes) >= MAXPATHLEN)
		return false;
	if (dsl->sl_owner == 0)
		return false;

	return true;
}

static void
xfs_symlink_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_target->bt_mount;

	/* no verification of non-crc buffers */
	if (!xfs_sb_version_hascrc(&mp->m_sb))
		return;

	if (!xfs_verify_cksum(bp->b_addr, BBTOB(bp->b_length),
				  offsetof(struct xfs_dsymlink_hdr, sl_crc)) ||
	    !xfs_symlink_verify(bp)) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp, bp->b_addr);
		xfs_buf_ioerror(bp, EFSCORRUPTED);
	}
}

static void
xfs_symlink_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_target->bt_mount;
	struct xfs_buf_log_item	*bip = bp->b_fspriv;

	/* no verification of non-crc buffers */
	if (!xfs_sb_version_hascrc(&mp->m_sb))
		return;

	if (!xfs_symlink_verify(bp)) {
		XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp, bp->b_addr);
		xfs_buf_ioerror(bp, EFSCORRUPTED);
		return;
	}

	if (bip) {
		struct xfs_dsymlink_hdr *dsl = bp->b_addr;
		dsl->sl_lsn = cpu_to_be64(bip->bli_item.li_lsn);
	}
	xfs_update_cksum(bp->b_addr, BBTOB(bp->b_length),
			 offsetof(struct xfs_dsymlink_hdr, sl_crc));
}

const struct xfs_buf_ops xfs_symlink_buf_ops = {
	.verify_read = xfs_symlink_read_verify,
	.verify_write = xfs_symlink_write_verify,
};

