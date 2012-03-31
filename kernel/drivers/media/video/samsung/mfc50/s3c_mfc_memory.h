/* 
 * drivers/media/video/samsung/mfc40/s3c_mfc_memory.h
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 *
 * Jaeryul Oh, Copyright (c) 2009 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _S3C_MFC_MEMORY_H_
#define _S3C_MFC_MEMORY_H_

#include "s3c_mfc_common.h"
#include "s3c_mfc_types.h"

#ifdef CONFIG_VIDEO_MFC_MAX_INSTANCE
#define MFC_MAX_INSTANCE_NUM (CONFIG_VIDEO_MFC_MAX_INSTANCE)
#endif

/* Reserved memory area */
#define MFC_RESERVED_DRAM0_START 	0x35000000 	// 0x3000_0000 ~ 0x3800_0000 (128MB) 
#define MFC_DATA_DRAM0_BUF_SIZE 	0x3000000	// 0x300_0000 (48MB)  	
#define MFC_RESERVED_DRAM1_START 	0x40000000	// 0x4000_0000 ~ 0x4800_0000 (128MB)
#define MFC_DATA_DRAM1_BUF_SIZE 	0x3000000	// 0x300_0000 (48MB) 

#if 0
/* All buffer size have to be aligned to 64K */
#define FIRMWARE_CODE_SIZE	(0x40000) /* 0x306c0(198,336 byte) ~ 0x40000 */
#define MFC_FW_SYSTEM_SIZE	(0x100000) /* 1MB : 1024x1024 */
#define MFC_FW_BUF_SIZE		(0x80000) /* 512KB : 512x1024 size per instance */
#define MFC_FW_TOTAL_BUF_SIZE	(MFC_FW_SYSTEM_SIZE + MFC_MAX_INSTANCE_NUM * MFC_FW_BUF_SIZE) 

#define CPB_BUF_SIZE		(0x400000) /* 4MB : 4x1024x1024 for decoder */
#define DESC_BUF_SIZE		(0x20000)  /* 128KB : 128x1024 */

#define STREAM_BUF_SIZE		(0x200000) /* 2MB : 2x1024x1024 for encoder */
#define MV_BUF_SIZE		(0x10000) /* 64KB : 64x1024 for encoder */
#else
/* All buffer size have to be aligned to 64K */
// Modified
#define FIRMWARE_CODE_SIZE	(0x40000) /* 0x306c0(198,336 byte) ~ 0x40000 */
#define MFC_FW_SYSTEM_SIZE	(0x300000) /* 3MB : 3x1024x1024 */
#define MFC_FW_BUF_SIZE		(0x80000) /* 512KB : 512x1024 size per instance */
#define MFC_FW_TOTAL_BUF_SIZE	(MFC_FW_SYSTEM_SIZE + MFC_MAX_INSTANCE_NUM * MFC_FW_BUF_SIZE) 
#if 0
#define DESC_BUF_SIZE		(0x100000)  /* 2MB : 2x1024x1024 */
#define RISC_BUF_SIZE		(0x180000)  /* 2.5MB : 2.5x1024x024 */
#else	// OK
#define DESC_BUF_SIZE		(0x20000)   /* 128KB : 128x1024 */
#define RISC_BUF_SIZE		(0x80000)   /* 512KB : 512x1024 size per instance */
#endif
#define CPB_BUF_SIZE		(0x400000) /* 4MB : 4x1024x1024 for decoder */

#define STREAM_BUF_SIZE		(0x200000) /* 2MB : 2x1024x1024 for encoder */
#define MV_BUF_SIZE		(0x10000) /* 64KB : 64x1024 for encoder */
#endif



volatile unsigned char *s3c_mfc_get_fw_buf_virt_addr(void);		
volatile unsigned char *s3c_mfc_get_data_buf_virt_addr(void);
volatile unsigned char *s3c_mfc_get_dpb_luma_buf_virt_addr(void);

unsigned int s3c_mfc_get_fw_buf_phys_addr(void);
unsigned int s3c_mfc_get_risc_buf_phys_addr(int instNo);
unsigned int s3c_mfc_get_data_buf_phys_addr(void);
unsigned int s3c_mfc_get_dpb_luma_buf_phys_addr(void);

#endif /* _S3C_MFC_MEMORY_H_ */
