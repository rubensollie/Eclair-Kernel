/* 
 * drivers/media/video/samsung/mfc50/s3c_mfc_interface.h
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

#ifndef _S3C_MFC_INTERFACE_H_
#define _S3C_MFC_INTERFACE_H_

#include "s3c_mfc_errorno.h"

#define IOCTL_MFC_DEC_INIT		(0x00800001)
#define IOCTL_MFC_ENC_INIT		(0x00800002)
#define IOCTL_MFC_DEC_EXE		(0x00800003)
#define IOCTL_MFC_ENC_EXE		(0x00800004)

#define IOCTL_MFC_GET_IN_BUF		(0x00800010)
#define IOCTL_MFC_FREE_BUF		(0x00800011)
#define IOCTL_MFC_GET_PHYS_ADDR		(0x00800012)

#define IOCTL_MFC_SET_CONFIG		(0x00800101)
#define IOCTL_MFC_GET_CONFIG		(0x00800102)

/* MFC H/W support maximum 15 extra DPB. */
#define MFC_MAX_EXTRA_DPB		(5) 

typedef enum
{
	H264_DEC = 0x80,
	MPEG4_DEC,
	MP4SH_DEC,
	H263_DEC,
	
	DIVX_DEC,
	XVID_DEC,
	DIVX311_DEC,
	DIVX412_DEC,
	DIVX502_DEC,
	DIVX503_DEC,
	
	VC1AP_DEC,	/* VC1 advaced Profile decoding  */	
	VC1RCV_DEC,	/* VC1 simple/main profile decoding  */
	
	MPEG1_DEC,
	MPEG2_DEC,	
	
	MPEG4_ENC = 0x100,
	H263_ENC,
	H264_ENC	
} MFC_CODEC_TYPE;

typedef enum
{
	DONT_CARE = 0, 	/* (0<<1)|(0<<0) */
	I_FRAME = 1, 	/* (0<<1)|(1<<0) */
	NOT_CODED = 2 	/* (1<<1)|(0<<0) */
} MFC_FORCE_SET_FRAME_TYPE;

typedef enum
{
	MFC_DEC_SETCONF_POST_ENABLE = 1,
	MFC_DEC_SETCONF_EXTRA_BUFFER_NUM,
	MFC_DEC_SETCONF_DISPLAY_DELAY,
	MFC_DEC_SETCONF_IS_LAST_FRAME,
	MFC_DEC_GETCONF_IMG_RESOLUTION,
	MFC_DEC_GETCONF_PHYS_ADDR,
	MFC_DEC_GETCONF_CRC_DATA
}SSBSIP_MFC_DEC_CONF;

typedef enum
{
	MFC_ENC_SETCONF_FRAME_TYPE = 100,	
}SSBSIP_MFC_ENC_CONF;

typedef struct tag_strm_ref_buf_arg
{
	unsigned int strm_ref_y;
	unsigned int mv_ref_yc;
} s3c_mfc_strm_ref_buf_arg_t;

typedef struct tag_frame_buf_arg
{
	unsigned int luma;
	unsigned int chroma;
} s3c_mfc_frame_buf_arg_t;

/* but, due to lack of memory, MFC driver use 5 as maximum */

typedef struct {
	MFC_CODEC_TYPE in_codec_type;	/* [IN]  codec type */
	int in_width;		/* [IN] width of YUV420 frame to be encoded */
	int in_height;		/* [IN] height of YUV420 frame to be encoded */
	int in_profile_level;	/* [IN]  profile & level */
	int in_gop_num;		/* [IN]  GOP Number (interval of I-frame) */	
	int in_vop_quant;	/* [IN]  VOP quant */

	/* [IN]  RC enable */
	int in_RC_frm_enable;	/* [IN]	RC enable (0:disable, 1:frame level RC) */
	int in_RC_framerate; /* [IN]  RC parameter (framerate) */
	int in_RC_bitrate;   /* [IN]  RC parameter (bitrate in kbps) */
	int in_RC_qbound;    /* [IN]  RC parameter (Q bound) */
	int in_RC_rpara;     /* [IN]  RC parameter (Reaction Coefficient) */	

	int in_MS_mode;	     /* [IN] Multi-slice mode (0:single, 1:multiple) */
	int in_MS_size;      /* [IN] Multi-slice size (in num. of mb or byte) */

	int in_mb_refresh;     		/* [IN]  Macroblock refresh */
	int in_interlace_mode; /* [IN] interlace mode(0:progressive, 1:interlace) */	

	/* [IN]  B frame number */
	int in_BframeNum; 		
		
	/* [IN] Enable (1) / Disable (0) padding with the specified values */
	int in_pad_ctrl_on;	
	/* [IN] pad value if pad_ctrl_on is Enable */
	int in_luma_pad_val;	
	int in_cb_pad_val;		
	int in_cr_pad_val;

	unsigned int in_mapped_addr;	
	s3c_mfc_strm_ref_buf_arg_t out_u_addr;
	s3c_mfc_strm_ref_buf_arg_t out_p_addr;
	s3c_mfc_strm_ref_buf_arg_t out_buf_size;

	/* MPEG4 Only
	 */
	int in_qpelME_enable;	/* [IN] Quarter-pel MC enable (1:enabled, 0:disabled) */	

} s3c_mfc_enc_init_mpeg4_arg_t;

typedef s3c_mfc_enc_init_mpeg4_arg_t s3c_mfc_enc_init_h263_arg_t;

typedef struct {
	MFC_CODEC_TYPE in_codec_type;	/* [IN] codec type */
	int in_width;		/* [IN] width  of YUV420 frame to be encoded */
	int in_height;		/* [IN] height of YUV420 frame to be encoded */
	int in_profile_level;	/* [IN] profile & level */
	int in_gop_num;		/* [IN] GOP Number (interval of I-frame) */
	int in_vop_quant;	/* [IN] VOP quant */

	/* [IN]  RC enable */
	int in_RC_frm_enable;	/* [IN]	RC enable (0:disable, 1:frame level RC) */	
	int in_RC_framerate;	/* [IN]  RC parameter (framerate) */
	int in_RC_bitrate;	/* [IN]  RC parameter (bitrate in kbps) */
	int in_RC_qbound;	/* [IN]  RC parameter (Q bound) */
	int in_RC_rpara;	/* [IN]  RC parameter (Reaction Coefficient) */
	
	int in_MS_mode;      /* [IN] Multi-slice mode (0:single, 1:multiple) */
	int in_MS_size;      /* [IN] Multi-slice size (in num. of mb or byte) */

	int in_mb_refresh;   /* [IN] Macroblock refresh */
	int in_interlace_mode; /* [IN] interlace mode(0:progressive, 1:interlace */
	
	/* [IN]  B frame number */	
	int in_BframeNum; 	

	/* [IN] Enable (1) / Disable (0) padding with the specified values */
	int in_pad_ctrl_on;	
	/* [IN] pad value if pad_ctrl_on is Enable */
	int in_luma_pad_val;	
	int in_cb_pad_val;		
	int in_cr_pad_val;

	unsigned int in_mapped_addr;	
	s3c_mfc_strm_ref_buf_arg_t out_u_addr;
	s3c_mfc_strm_ref_buf_arg_t out_p_addr;
	s3c_mfc_strm_ref_buf_arg_t out_buf_size;

	/* H264 Only
	 */
	int in_RC_mb_enable;	/* [IN]	RC enable (0:disable, 1:MB level RC) */
	
	/* [IN]  reference number */
	int in_reference_num;
	/* [IN]  reference number of P frame */
	int in_ref_num_p;

	/* [IN] MB level rate control dark region adaptive feature */
	int in_RC_mb_dark_disable;	/* (0:enable, 1:disable) */
	/* [IN] MB level rate control smooth region adaptive feature */
	int in_RC_mb_smooth_disable;	/* (0:enable, 1:disable) */
	/* [IN] MB level rate control static region adaptive feature */
	int in_RC_mb_static_disable;	/* (0:enable, 1:disable) */
	/* [IN] MB level rate control activity region adaptive feature */
	int in_RC_mb_activity_disable;	/* (0:enable, 1:disable) */

	/* [IN]  disable deblocking filter idc */
	int in_deblock_filt; /* (0: enable,1: disable) */
	/* [IN]  slice alpha C0 offset of deblocking filter */
	int in_deblock_alpha_C0;
	/* [IN]  slice beta offset of deblocking filter */
	int in_deblock_beta;

	/* [IN]  ( 0 : CAVLC, 1 : CABAC ) */
	int in_symbolmode;	
	/* [IN] (0: only 4x4 transform, 1: allow using 8x8 transform)*/
	int in_transform8x8_mode;
	/* [IN] Inter weighted parameter for mode decision */
	int in_md_interweight_pps; 
	/* [IN] Intra weighted parameter for mode decision */
	int in_md_intraweight_pps; 	

} s3c_mfc_enc_init_h264_arg_t;


typedef struct {
	MFC_CODEC_TYPE in_codec_type; /* [IN] codec type */
	unsigned int in_Y_addr;   /*[IN]In-buffer addr of Y component */
	unsigned int in_CbCr_addr;/*[IN]In-buffer addr of CbCr component */
	unsigned int in_Y_addr_vir;   /*[IN]In-buffer addr of Y component */
	unsigned int in_CbCr_addr_vir;/*[IN]In-buffer addr of CbCr component */
	unsigned int in_strm_st;  /*[IN]Out-buffer start addr of encoded strm*/
	unsigned int in_strm_end; /*[IN]Out-buffer end addr of encoded strm */
	unsigned int out_frame_type; /* [OUT] frame type  */
	int out_encoded_size;        /* [OUT] Length of Encoded video stream */
	int out_header_size;         /* [OUT] Length of video stream header */
} s3c_mfc_enc_exe_arg;

typedef struct {
	MFC_CODEC_TYPE in_codec_type; /* [IN] codec type */
	int in_strm_buf;  /* [IN] the physical address of STRM_BUF */
	int in_strm_size; /* [IN] size of video stream filled in STRM_BUF */
	int in_packed_PB; /* [IN]  Is packed PB frame or not, 1: packedPB  0: unpacked */	
	
	int out_img_width;	/* [OUT] width  of YUV420 frame */
	int out_img_height;	/* [OUT] height of YUV420 frame */
	int out_buf_width;	/* [OUT] width  of YUV420 frame */
	int out_buf_height;	/* [OUT] height of YUV420 frame */
	int out_dpb_cnt;  	/* [OUT] the number of buffers which is nessary during decoding. */	

	s3c_mfc_frame_buf_arg_t in_frm_buf;   /* [IN] the address of dpb FRAME_BUF */
	s3c_mfc_frame_buf_arg_t in_frm_size;  /* [IN] size of dpb FRAME_BUF */
	unsigned int in_mapped_addr;
	
	s3c_mfc_frame_buf_arg_t out_u_addr;
	s3c_mfc_frame_buf_arg_t out_p_addr;
	s3c_mfc_frame_buf_arg_t out_frame_buf_size;
} s3c_mfc_dec_init_arg_t;

typedef struct {
	MFC_CODEC_TYPE in_codec_type;/* [IN]  codec type */
	int in_strm_buf;  /* [IN]  the physical address of STRM_BUF */
	int in_strm_size; /* [IN]  Size of video stream filled in STRM_BUF */
	s3c_mfc_frame_buf_arg_t in_frm_buf;   /* [IN] the address of dpb FRAME_BUF */
	s3c_mfc_frame_buf_arg_t in_frm_size;  /* [IN] size of dpb FRAME_BUF */
	int out_display_Y_addr; /* [OUT]  the physical address of display buf */
	int out_display_C_addr; /* [OUT]  the physical address of display buf */

	/*
	 * [OUT] whether display frame exist or not.
	 * (0:no more frame, 1:frame exist)
	 */
	int out_display_status; 
} s3c_mfc_dec_exe_arg_t;

typedef struct {
	int in_config_param;	/* [IN] Configurable parameter type */

	/* [IN] Values to get for the configurable parameter. */
	int out_config_value[4];
	/* Maximum two integer values can be obtained; */
} s3c_mfc_get_config_arg_t;

typedef struct {
	int in_config_param; /* [IN] Configurable parameter type */

	/* [IN]  Values to be set for the configurable parameter. */
	int in_config_value[2];
	/* Maximum two integer values can be set. */

	/* [OUT] Old values of the configurable parameters */
	int out_config_value_old[2];
} s3c_mfc_set_config_arg_t;

typedef struct tag_get_phys_addr_arg
{
	unsigned int u_addr;
	unsigned int p_addr;
} s3c_mfc_get_phys_addr_arg_t;

typedef struct tag_mem_alloc_arg
{
	MFC_CODEC_TYPE codec_type;
	int buff_size;
	unsigned int mapped_addr;
	unsigned int out_addr;
} s3c_mfc_mem_alloc_arg_t;

typedef struct tag_mem_free_arg_t
{
	unsigned int u_addr;
} s3c_mfc_mem_free_arg_t;

typedef union {
	s3c_mfc_enc_init_mpeg4_arg_t enc_init_mpeg4;
	s3c_mfc_enc_init_h263_arg_t enc_init_h263;
	s3c_mfc_enc_init_h264_arg_t enc_init_h264;	
	s3c_mfc_enc_exe_arg enc_exe;
	
	s3c_mfc_dec_init_arg_t dec_init;
	s3c_mfc_dec_exe_arg_t dec_exe;
	
	s3c_mfc_get_config_arg_t get_config;
	s3c_mfc_set_config_arg_t set_config;

	s3c_mfc_mem_alloc_arg_t mem_alloc;
	s3c_mfc_mem_free_arg_t mem_free;
	s3c_mfc_get_phys_addr_arg_t get_phys_addr;
} s3c_mfc_args;

typedef struct tag_mfc_args{
	MFC_ERROR_CODE ret_code; /* [OUT] error code */
	s3c_mfc_args args;
} s3c_mfc_common_args;

#define ENC_PROFILE_LEVEL(profile, level)      ((profile) | ((level) << 8))

#define ENC_PROFILE_MPEG4_SP                   0
#define ENC_PROFILE_MPEG4_ASP                  1
#define ENC_PROFILE_H264_BP                    0
#define ENC_PROFILE_H264_MAIN                  1
#define ENC_PROFILE_H264_HIGH                  2


#define ENC_RC_DISABLE                         0
#define ENC_RC_ENABLE_MACROBLOCK               1
#define ENC_RC_ENABLE_FRAME                    2

#define ENC_RC_QBOUND(min_qp, max_qp)          ((min_qp) | ((max_qp) << 8))
#define ENC_RC_MB_CTRL_DARK_DISABLE            (1 << 3)
#define ENC_RC_MB_CTRL_SMOOTH_DISABLE          (1 << 2)
#define ENC_RC_MB_CTRL_STATIC_DISABLE          (1 << 1)
#define ENC_RC_MB_CTRL_ACTIVITY_DISABLE        (1 << 0)


#endif /* _S3C_MFC_INTERFACE_H_ */
