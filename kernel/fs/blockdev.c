/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <ext4_config.h>
#include <ext4_blockdev.h>
#include <ext4_errno.h>
#include "types.h"
#include "defs.h"
#include "param.h"
#include "buf.h"
#include "debug.h"

#ifndef BSIZE
#define BSIZE 512
#endif

/**********************BLOCKDEV INTERFACE**************************************/
static int blockdev_open(struct ext4_blockdev *bdev);
static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt);
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt);
static int blockdev_close(struct ext4_blockdev *bdev);
static int blockdev_lock(struct ext4_blockdev *bdev);
static int blockdev_unlock(struct ext4_blockdev *bdev);

/******************************************************************************/
EXT4_BLOCKDEV_STATIC_INSTANCE(blockdev, BSIZE, (uint64)1 << 32, blockdev_open,
			      blockdev_bread, blockdev_bwrite, blockdev_close,
			      blockdev_lock, blockdev_unlock);

/******************************************************************************/
static int blockdev_open(struct ext4_blockdev *bdev)
{
	/*blockdev_open: skeleton*/
	return EOK;
}

/******************************************************************************/

static int blockdev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
			 uint32_t blk_cnt)
{
#ifdef __DEBUG_DEV_BREAD
	Log("blockdev_bread: bdev 0x%x, buf 0x%x, blockid 0x%x", bdev, buf, blk_id);
#endif
	uint64 bp = (uint64)buf;
	for(int i = 0; i < blk_cnt; i++) {
		struct buf *b = bread(ROOTDEV, blk_id + i);
		memmove((void*)bp, b->data, BSIZE);
		bp += BSIZE;
		brelse(b);
	}
#ifdef __DEBUG_DEV_BREAD
	Log("blockdev_bread end!");
#endif
	return EOK;
}


/******************************************************************************/
static int blockdev_bwrite(struct ext4_blockdev *bdev, const void *buf,
			  uint64_t blk_id, uint32_t blk_cnt)
{
	uint64 bp = (uint64)buf;
	for(int i = 0; i < blk_cnt; i++) {
		struct buf *b = bget(ROOTDEV, blk_id + i);
		memmove(b->data, (void*)bp, BSIZE);
		bwrite(b);
		bp += BSIZE;
		brelse(b);
	}
	
	return EOK;
}
/******************************************************************************/
static int blockdev_close(struct ext4_blockdev *bdev)
{
	/*blockdev_close: skeleton*/
	return EOK;
}

static int blockdev_lock(struct ext4_blockdev *bdev)
{
	/*blockdev_lock: skeleton*/
	return EOK;
}

static int blockdev_unlock(struct ext4_blockdev *bdev)
{
	/*blockdev_unlock: skeleton*/
	return EOK;
}

/******************************************************************************/
struct ext4_blockdev *ext4_blockdev_get(void)
{
	return &blockdev;
}
/******************************************************************************/

