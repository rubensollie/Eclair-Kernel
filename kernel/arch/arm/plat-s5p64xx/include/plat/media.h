/* linux/arch/arm/plat-s5p64xx/include/plat/media.h
 *
 * Copyright 2009 Samsung Electronics
 *	Jinsung Yang
 *	http://samsungsemi.com/
 *
 * Samsung Media device descriptions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _S3C_MEDIA_H
#define _S3C_MEDIA_H

#include <linux/types.h>

/* 3 fimc indexes should be fixed as 0, 1 and 2 */
#define S3C_MDEV_FIMC0		0
#define S3C_MDEV_FIMC1		1
#define S3C_MDEV_FIMC2		2
#define S3C_MDEV_POST		3
#define S3C_MDEV_GVG		4
#define S3C_MDEV_MFC		5
#define S3C_MDEV_JPEG		6
#define S3C_MDEV_MAX		7

struct s3c_media_device {
	int		id;
	const char 	*name;
	int		node;
	size_t		memsize;
	dma_addr_t	paddr;
};

extern dma_addr_t s3c_get_media_memory(int dev_id);
extern dma_addr_t s3c_get_media_memory_node(int dev_id, int node);
extern size_t s3c_get_media_memsize(int dev_id);
extern size_t s3c_get_media_memsize_node(int dev_id, int node);

#endif
