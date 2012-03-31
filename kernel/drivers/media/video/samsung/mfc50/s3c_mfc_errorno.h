/* 
 * drivers/media/video/samsung/mfc40/s3c_mfc_errorno.h
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

#ifndef _S3C_MFC_ERRORNO_H_
#define _S3C_MFC_ERRORNO_H_

typedef enum
{
	MFCINST_RET_OK = 1,
	MFCINST_ERR_INVALID_PARAM = -1001,
	MFCINST_ERR_STATE_INVALID = -1002,
	MFCINST_ERR_POWER_OFF = -1003,
	MFCINST_ERR_WRONG_CODEC_MODE = -1004,
	MFCINST_ERR_INIT_FAIL = -1005,
	MFCINST_ERR_FILE_OPEN_FAIL = -1006,
	MFCINST_ERR_INTR_TIME_OUT = -1007,
	MFCINST_ERR_INTR_INIT_FAIL = -1008,


	MFCINST_ERR_DEC_INIT_CMD_FAIL = -2001,
	MFCINST_ERR_DEC_HEADER_DECODE_FAIL = -2002,
	MFCINST_ERR_DEC_DECODE_CMD_FAIL = -2003,
	MFCINST_ERR_DEC_DECODE_DONE_FAIL = -2004,
	MFCINST_ERR_DEC_INVALID_STRM  = -2005,
	MFCINST_ERR_DEC_STRM_SIZE_INVALID  = -2006,

	MFCINST_ERR_ENC_INIT_CMD_FAIL = -3001,
	MFCINST_ERR_ENC_ENCODE_CMD_FAIL = -3002,
	MFCINST_ERR_ENC_ENCODE_DONE_FAIL = -3003,
	MFCINST_ERR_ENC_PARAM_INVALID_VALUE = -3004,

	MFCINST_ERR_STRM_BUF_INVALID = -4001,
	MFCINST_ERR_FRM_BUF_INVALID = -4002,
	MFCINST_ERR_FRM_BUF_SIZE = -4003,

	MFCINST_ERR_FW_LOAD_FAIL = -5001,
	MFCINST_ERR_FW_MEMORY_INVALID = -5002,
	MFCINST_ERR_FW_DMA_SET_FAIL = -5003,
	MFCINST_ERR_FW_INIT_FAIL = -5004,
	MFCINST_ERR_SEQ_START_FAIL = -5005,

	MFCINST_INST_NUM_INVALID = -6001,
	MFCINST_INST_NUM_EXCEEDED = -6002,
	MFCINST_ERR_SET_CONF = -6003,


	MFCINST_MEMORY_ALLOC_FAIL = -8001,
	MFCINST_MUTEX_CREATE_FAIL = -8002,
	MFCINST_POWER_INIT_FAIL = -8003,
	MFCINST_POWER_ON_OFF_FAIL = -8004,
	MFCINST_POWER_STATE_INVALID = -8005,
	MFCINST_POWER_MANAGER_ERR = -8006,

	MFCINST_MEMORY_INVAILD_ADDR = -8101,
	MFCINST_MEMORY_MAPPING_FAIL = -8102,

	MFCAPI_RET_FAIL = -9001,
} MFC_ERROR_CODE;

#endif /* _S3C_MFC_ERRORNO_H_ */

