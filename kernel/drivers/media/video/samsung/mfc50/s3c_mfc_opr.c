/*
 * drivers/media/video/samsung/mfc40/s3c_mfc_opr.c
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver
 *
 * Jaeryul Oh, Copyright (c) 2009 Samsung Electronics
 * http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <plat/regs-mfc.h>

#include "s3c_mfc_common.h"
#include "s3c_mfc_opr.h"
#include "s3c_mfc_logmsg.h"
#include "s3c_mfc_memory.h"
#include "s3c_mfc_fw.h"
#include "s3c_mfc_buffer_manager.h"
#include "s3c_mfc_interface.h"
#include "s3c_mfc_intr.h"

extern void __iomem *s3c_mfc_sfr_virt_base;

extern unsigned int s3c_mfc_phys_buf, s3c_mfc_phys_dpb_luma_buf;

#define READL(offset)		readl(s3c_mfc_sfr_virt_base + (offset))
#define WRITEL(data, offset)	writel((data), s3c_mfc_sfr_virt_base + (offset))

static void s3c_mfc_cmd_reset(void);
static void s3c_mfc_cmd_frame_start(void);
static void s3c_mfc_cmd_sleep(void);
static void s3c_mfc_cmd_wakeup(void);
static void s3c_mfc_backup_context(s3c_mfc_inst_ctx  *mfc_ctx);
static void s3c_mfc_restore_context(s3c_mfc_inst_ctx  *mfc_ctx);
static void s3c_mfc_set_codec_firmware(s3c_mfc_inst_ctx  *mfc_ctx);
static void s3c_mfc_set_encode_init_param(int inst_no, MFC_CODEC_TYPE mfc_codec_type, s3c_mfc_args *args);
static int s3c_mfc_get_inst_no(MFC_CODEC_TYPE codec_type);
static MFC_ERROR_CODE s3c_mfc_set_dec_stream_buffer(int inst_no, int buf_addr, unsigned int buf_size);
static MFC_ERROR_CODE s3c_mfc_set_dec_frame_buffer(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_frame_buf_arg_t buf_addr, s3c_mfc_frame_buf_arg_t buf_size);
static MFC_ERROR_CODE s3c_mfc_set_risc_buffer(MFC_CODEC_TYPE codec_type, int inst_no);
static MFC_ERROR_CODE s3c_mfc_decode_one_frame(s3c_mfc_inst_ctx *mfc_ctx, s3c_mfc_dec_exe_arg_t *dec_arg, unsigned int *consumed_strm_size);


static void s3c_mfc_cmd_reset(void)  
{
	unsigned int mc_status;
	/* Stop procedure */
	WRITEL(0x3f7, S3C_FIMV_SW_RESET);	//  reset VI
	WRITEL(0x3f6, S3C_FIMV_SW_RESET);	//  reset RISC
	WRITEL(0x3e2, S3C_FIMV_SW_RESET);	//  All reset except for MC
	mdelay(10);

	/* Check MC status */
	do {
		mc_status = READL(S3C_FIMV_MC_STATUS);		
	} while(mc_status & 0x3);
	
	WRITEL(0x0, S3C_FIMV_SW_RESET);	

	WRITEL(0x3fe, S3C_FIMV_SW_RESET);
	
}

static void s3c_mfc_cmd_host2risc(s3c_mfc_facade_cmd cmd, int arg)
{
	s3c_mfc_facade_cmd cur_cmd;

	/* wait until host to risc command register becomes 'H2R_CMD_EMPTY' */
	do {	
		cur_cmd = READL(S3C_FIMV_HOST2RISC_CMD);		
	} while(cur_cmd != H2R_CMD_EMPTY);

	WRITEL(arg, S3C_FIMV_HOST2RISC_ARG1);
	#if 0	// peter, enable CRC data
	WRITEL(1, S3C_FIMV_HOST2RISC_ARG2);
	WRITEL(0, S3C_FIMV_HOST2RISC_ARG3);
	WRITEL(0, S3C_FIMV_HOST2RISC_ARG4);
	#endif
	WRITEL(cmd, S3C_FIMV_HOST2RISC_CMD);
	
}

/*
static void s3c_mfc_cmd_frame_start(void)
{
	WRITEL(1, S3C_FIMV_FRAME_START);
}

static void s3c_mfc_cmd_sleep()
{
	WRITEL(-1, S3C_FIMV_CH_ID);
	WRITEL(MFC_SLEEP, S3C_FIMV_COMMAND_TYPE);
}

static void s3c_mfc_cmd_wakeup()
{
	WRITEL(-1, S3C_FIMV_CH_ID);
	WRITEL(MFC_WAKEUP, S3C_FIMV_COMMAND_TYPE);
	mdelay(100);
}
*/

static void s3c_mfc_backup_context(s3c_mfc_inst_ctx  *mfc_ctx)
{
	memcpy(mfc_ctx->MfcSfr, s3c_mfc_sfr_virt_base, S3C_FIMV_REG_SIZE);
}

static void s3c_mfc_restore_context(s3c_mfc_inst_ctx  *mfc_ctx)
{
	/*
	memcpy(s3c_mfc_sfr_virt_base, mfc_ctx->MfcSfr, S3C_FIMV_REG_SIZE);
	*/
}

static MFC_ERROR_CODE s3c_mfc_set_dec_stream_buffer(int inst_no, int buf_addr, unsigned int buf_size)
{
	unsigned int fw_phybuf;	
	unsigned int risc_phy_buf, aligned_risc_phy_buf;

	mfc_debug("inst_no : %d, buf_addr : 0x%08x, buf_size : 0x%08x\n", inst_no, buf_addr, buf_size);
	
	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);
	
	risc_phy_buf = s3c_mfc_get_risc_buf_phys_addr(inst_no);
	aligned_risc_phy_buf = Align(risc_phy_buf, 2*BUF_L_UNIT);
	
	WRITEL((buf_addr & 0xfffffff8)-fw_phybuf, S3C_FIMV_SI_CH1_SB_ST_ADR);
	WRITEL(buf_size, S3C_FIMV_SI_CH1_SB_FRM_SIZE);	// peter, buf_size is '0' when last frame
	WRITEL(aligned_risc_phy_buf-fw_phybuf, S3C_FIMV_SI_CH1_DESC_ADR);
	WRITEL(CPB_BUF_SIZE, S3C_FIMV_SI_CH1_CPB_SIZE);
	WRITEL(DESC_BUF_SIZE, S3C_FIMV_SI_CH1_DESC_SIZE);

	mfc_debug("risc_phy_buf : 0x%08x, aligned_risc_phy_buf : 0x%08x\n", 
			risc_phy_buf, aligned_risc_phy_buf);

	return MFCINST_RET_OK;
}


static MFC_ERROR_CODE s3c_mfc_set_dec_frame_buffer(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_frame_buf_arg_t buf_addr, 
							s3c_mfc_frame_buf_arg_t buf_size)
{
	unsigned int width, height, frame_size, dec_dpb_addr;
	unsigned int fw_phybuf, dpb_luma_phybuf, i;
	s3c_mfc_frame_buf_arg_t dpb_buf_addr;
	unsigned int aligned_width, aligned_ch_height, aligned_mv_height;

	mfc_debug("luma_buf_addr : 0x%08x  luma_buf_size : %d\n", buf_addr.luma, buf_size.luma);
	mfc_debug("chroma_buf_addr : 0x%08x  chroma_buf_size : %d\n", buf_addr.chroma, buf_size.chroma);

	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);
	#if 0
	dpb_luma_phybuf = Align(s3c_mfc_get_dpb_luma_buf_phys_addr(), 128*BUF_L_UNIT);
	#else
	dpb_luma_phybuf = MFC_RESERVED_DRAM1_START;
	#endif

	width = Align(mfc_ctx->img_width, BUF_S_UNIT/2);
	height = Align(mfc_ctx->img_height, BUF_S_UNIT);
	frame_size = (width*height*3)>>1;

	mfc_debug("width : %d height : %d frame_size : %d mfc_ctx->DPBCnt :%d\n", \
					width, height, frame_size, mfc_ctx->DPBCnt);
	
	// peter, it should be corrected 	
	if ((buf_size.luma + buf_size.chroma) < frame_size*mfc_ctx->totalDPBCnt) {	
		mfc_err("MFCINST_ERR_FRM_BUF_SIZE\n");
		return MFCINST_ERR_FRM_BUF_SIZE;
	}

	aligned_width = Align(mfc_ctx->img_width, 4*BUF_S_UNIT); 	// 128B align 
	aligned_ch_height = Align(mfc_ctx->img_height/2, BUF_S_UNIT); 	// 32B align
	aligned_mv_height = Align(mfc_ctx->img_height/4, BUF_S_UNIT); 	// 32B align

	dpb_buf_addr.luma = Align(buf_addr.luma, 2*BUF_S_UNIT);
	dpb_buf_addr.chroma = Align(buf_addr.chroma, 2*BUF_S_UNIT);
	
	mfc_debug("DEC_LUMA_DPB_START_ADR : 0x%08x\n", dpb_buf_addr.luma);
	mfc_debug("DEC_CHROMA_DPB_START_ADR : 0x%08x\n", dpb_buf_addr.chroma);

	if (mfc_ctx->MfcCodecType == H264_DEC) {		
		
		for (i=0; i < mfc_ctx->totalDPBCnt; i++) {		
			/* set Luma address */
			WRITEL((dpb_buf_addr.luma-dpb_luma_phybuf)>>11, S3C_FIMV_H264_LUMA_ADR+(4*i));	
			dpb_buf_addr.luma += Align(aligned_width*height, 64*BUF_L_UNIT);
			/* set Chroma address */	
			WRITEL((dpb_buf_addr.chroma-fw_phybuf)>>11, S3C_FIMV_H264_CHROMA_ADR+(4*i));	
			dpb_buf_addr.chroma += Align(aligned_width*aligned_ch_height, 64*BUF_L_UNIT);
			/* set MV address */	
			WRITEL((dpb_buf_addr.chroma-fw_phybuf)>>11, S3C_FIMV_MV_ADR+(4*i));	
			dpb_buf_addr.chroma += Align(aligned_width*aligned_mv_height, 64*BUF_L_UNIT);			
		}	

	} else {
	
		for (i=0; i < mfc_ctx->totalDPBCnt; i++) {		
			/* set Luma address */
			WRITEL((dpb_buf_addr.luma-dpb_luma_phybuf)>>11, S3C_FIMV_LUMA_ADR+(4*i));	
			dpb_buf_addr.luma += Align(aligned_width*height, 64*BUF_L_UNIT);
			/* set Chroma address */	
			WRITEL((dpb_buf_addr.chroma-fw_phybuf)>>11, S3C_FIMV_CHROMA_ADR+(4*i));	
			dpb_buf_addr.chroma += Align(aligned_width*aligned_ch_height, 64*BUF_L_UNIT);			
		}	

	}

	mfc_debug("DEC_LUMA_DPB_END_ADR : 0x%08x\n", dpb_buf_addr.luma);
	mfc_debug("DEC_CHROMA_DPB_END_ADR : 0x%08x\n", dpb_buf_addr.chroma);


	return MFCINST_RET_OK;
}

// peter, MFC_SetBufAddrOfEncoder()
MFC_ERROR_CODE s3c_mfc_set_enc_ref_buffer(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_args *args)
{
	s3c_mfc_enc_init_mpeg4_arg_t *init_arg;
	unsigned int width, height, frame_size;
	unsigned int fw_phybuf, mv_ref_yc_phybuf, i;
	unsigned int aligned_width, aligned_height;
	unsigned int ref_y, mv_ref_yc;	

	init_arg = (s3c_mfc_enc_init_mpeg4_arg_t *)args;	
	
	mfc_debug("strm_ref_y_buf_addr : 0x%08x  strm_ref_y_buf_size : %d\n", 
			init_arg->out_p_addr.strm_ref_y, init_arg->out_buf_size.strm_ref_y);
	mfc_debug("mv_ref_yc_buf_addr : 0x%08x  mv_ref_yc_buf_size : %d\n", 
			init_arg->out_p_addr.mv_ref_yc, init_arg->out_buf_size.mv_ref_yc);		

	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);
	#if 0
	mv_ref_yc_phybuf = Align(s3c_mfc_get_dpb_luma_buf_phys_addr(), 128*BUF_L_UNIT);
	#else
	mv_ref_yc_phybuf = MFC_RESERVED_DRAM1_START;
	#endif

	width = Align(mfc_ctx->img_width, BUF_S_UNIT/2);
	height = Align(mfc_ctx->img_height, BUF_S_UNIT);
	frame_size = (width*height*3)>>1;

	mfc_debug("width : %d height : %d frame_size : %d\n",width, height, frame_size);	

	aligned_width = Align(mfc_ctx->img_width, 4*BUF_S_UNIT); // 128B align 
	aligned_height = Align(mfc_ctx->img_height, BUF_S_UNIT); // 32B align

	ref_y = init_arg->out_p_addr.strm_ref_y + STREAM_BUF_SIZE;	
	ref_y = Align(ref_y, 64*BUF_L_UNIT);
	mv_ref_yc = init_arg->out_p_addr.mv_ref_yc;	
	mv_ref_yc = Align(mv_ref_yc, 64*BUF_L_UNIT);

	for (i=0; i < 2; i++) {	
		// Set refY0,Y2
		WRITEL((ref_y-fw_phybuf)>>11, S3C_FIMV_ENC_REF0_LUMA_ADR+(4*i));	
		ref_y += Align(aligned_width*aligned_height, 64*BUF_L_UNIT);
		// Set refY1,Y3
		WRITEL((mv_ref_yc-mv_ref_yc_phybuf)>>11, S3C_FIMV_ENC_REF1_LUMA_ADR+(4*i));	
		mv_ref_yc += Align(aligned_width*aligned_height, 64*BUF_L_UNIT);	
	}
	for (i=0; i < 4; i++) {	
		// Set refC0~C3
		WRITEL((mv_ref_yc-mv_ref_yc_phybuf)>>11, S3C_FIMV_ENC_REF0_CHROMA_ADR+(4*i));	
		mv_ref_yc += Align(aligned_width*aligned_height/2, 64*BUF_L_UNIT); // Align size should be checked	
	}
	WRITEL((mv_ref_yc-mv_ref_yc_phybuf)>>11, S3C_FIMV_ENC_MV_BUF_ADR);	

	mfc_debug("ENC_REFY01_END_ADR : 0x%08x\n", ref_y);
	mfc_debug("ENC_REFYC_MV_END_ADR : 0x%08x\n", mv_ref_yc);

	return MFCINST_RET_OK;
		
}


// peter, set the desc, motion vector, overlap, bitplane0/1/2, etc
static MFC_ERROR_CODE s3c_mfc_set_risc_buffer(MFC_CODEC_TYPE codec_type, int inst_no)
{
	unsigned int fw_phybuf;	
	unsigned int risc_phy_buf, aligned_risc_phy_buf;
	
	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);
	
	risc_phy_buf = s3c_mfc_get_risc_buf_phys_addr(inst_no);
	aligned_risc_phy_buf = Align(risc_phy_buf, 2*BUF_L_UNIT) + DESC_BUF_SIZE;

	mfc_debug("inst_no : %d, risc_buf_start : 0x%08x\n", inst_no, risc_phy_buf);


	switch (codec_type) {
	case H264_DEC:		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_VERT_NB_MV_ADR);
		aligned_risc_phy_buf += 16*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_VERT_NB_IP_ADR);	
		aligned_risc_phy_buf += 32*BUF_L_UNIT;
		break;
		
	case MPEG4_DEC:	
	case MP4SH_DEC:	
	case H263_DEC:	
	case DIVX_DEC:
	case XVID_DEC:	
	case DIVX311_DEC:	
	case DIVX412_DEC:	
	case DIVX502_DEC:	
	case DIVX503_DEC:		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_NB_DCAC_ADR);		
		aligned_risc_phy_buf += 16*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_UP_NB_MV_ADR);	
		aligned_risc_phy_buf += 68*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_SA_MV_ADR);	
		aligned_risc_phy_buf += 136*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_OT_LINE_ADR);
		aligned_risc_phy_buf += 32*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_SP_ADR);	
		aligned_risc_phy_buf += 68*BUF_L_UNIT;			
		break;
		
	case VC1AP_DEC:	
	case VC1RCV_DEC:		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_UP_NB_MV_ADR);	
		aligned_risc_phy_buf += 68*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_SA_MV_ADR);	
		aligned_risc_phy_buf += 136*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_OT_LINE_ADR);
		aligned_risc_phy_buf += 32*BUF_L_UNIT;	
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_BITPLANE3_ADR);	
		aligned_risc_phy_buf += 2*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_BITPLANE2_ADR);	
		aligned_risc_phy_buf += 2*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_BITPLANE1_ADR);
		aligned_risc_phy_buf += 2*BUF_L_UNIT;		
		break;
		
	case MPEG1_DEC:	
	case MPEG2_DEC:	
		break;	

	case H264_ENC:
	case MPEG4_ENC:
	case H263_ENC:
		aligned_risc_phy_buf = Align(risc_phy_buf, 64*BUF_L_UNIT);
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_ENC_UP_MV_ADR);
		aligned_risc_phy_buf += 64*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_ENC_COZERO_FLAG_ADR);
		aligned_risc_phy_buf += 64*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_ENC_UP_INTRA_MD_ADR);
		aligned_risc_phy_buf += 64*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_ENC_UP_INTRA_PRED_ADR);
		aligned_risc_phy_buf += 64*BUF_L_UNIT;		
		WRITEL((aligned_risc_phy_buf-fw_phybuf)>>11, S3C_FIMV_ENC_NB_DCAC_ADR);
		aligned_risc_phy_buf += 64*BUF_L_UNIT;	
		break;	

	default:	 		
		break;		

	}	
	
	mfc_debug("inst_no : %d, risc_buf_end : 0x%08x\n", inst_no, aligned_risc_phy_buf);

	return MFCINST_RET_OK;
}

/* This function sets the MFC SFR values according to the input arguments. */
static void s3c_mfc_set_encode_init_param(int inst_no, MFC_CODEC_TYPE mfc_codec_type, s3c_mfc_args *args)
{
	unsigned int		ms_size;

	s3c_mfc_enc_init_mpeg4_arg_t   *enc_init_mpeg4_arg;
	s3c_mfc_enc_init_h264_arg_t    *enc_init_h264_arg;

	enc_init_mpeg4_arg = (s3c_mfc_enc_init_mpeg4_arg_t *) args;
	enc_init_h264_arg  = (s3c_mfc_enc_init_h264_arg_t  *) args;

	mfc_debug("mfc_codec_type : %d\n", mfc_codec_type);	

	/* Set image size */
	WRITEL(enc_init_mpeg4_arg->in_width, S3C_FIMV_ENC_HSIZE_PX);	
	if ((mfc_codec_type == H264_ENC) && (enc_init_mpeg4_arg->in_interlace_mode)) {
		WRITEL(enc_init_mpeg4_arg->in_height>>1, S3C_FIMV_ENC_VSIZE_PX);
		WRITEL(enc_init_mpeg4_arg->in_interlace_mode, S3C_FIMV_ENC_PIC_STRUCT);
	}	
	else {
		WRITEL(enc_init_mpeg4_arg->in_height, S3C_FIMV_ENC_VSIZE_PX);
		WRITEL(enc_init_mpeg4_arg->in_interlace_mode, S3C_FIMV_ENC_PIC_STRUCT);
	}

	WRITEL(0, S3C_FIMV_ENC_PXL_CACHE_CTRL);
	WRITEL(1, S3C_FIMV_ENC_BF_MODE_CTRL);	// stream buf frame mode
	
	WRITEL(1, S3C_FIMV_ENC_SF_EPB_ON_CTRL);	// Auto EPB insertion on, only for h264	
	WRITEL((1<<18)|(enc_init_mpeg4_arg->in_BframeNum<<16)|enc_init_mpeg4_arg->in_gop_num, 
		S3C_FIMV_ENC_PIC_TYPE_CTRL); 

	/* Multi-slice options */
	if (enc_init_mpeg4_arg->in_MS_mode) {			
		ms_size = (mfc_codec_type == H263_ENC) ? 0 : enc_init_mpeg4_arg->in_MS_size;
		switch (enc_init_mpeg4_arg->in_MS_mode) {
		case 1:		
			WRITEL((0<<1)|0x1, S3C_FIMV_ENC_MSLICE_CTRL);			
			WRITEL(ms_size, S3C_FIMV_ENC_MSLICE_MB);			
			break;
		case 2:	
			WRITEL((1<<1)|0x1, S3C_FIMV_ENC_MSLICE_CTRL);
			WRITEL(ms_size, S3C_FIMV_ENC_MSLICE_BYTE);
			break;
		case 4:
			WRITEL(0x5, S3C_FIMV_ENC_MSLICE_CTRL);
			break;
		default:
			mfc_err("Invalid Multi-slice mode type\n");
			break;
		}	
	}
	else {
		WRITEL(0, S3C_FIMV_ENC_MSLICE_CTRL);
	}
	
	/* Set circular intra refresh MB count */
	WRITEL(enc_init_mpeg4_arg->in_mb_refresh, S3C_FIMV_ENC_CIR_CTRL);
	WRITEL(MEM_STRUCT_LINEAR, S3C_FIMV_ENC_MAP_FOR_CUR);	// linear mode
	/* Set padding control */
	WRITEL((enc_init_mpeg4_arg->in_pad_ctrl_on<<31)|(enc_init_mpeg4_arg->in_cr_pad_val<<16)|
		(enc_init_mpeg4_arg->in_cb_pad_val<<8)|(enc_init_mpeg4_arg->in_luma_pad_val<<0),
		S3C_FIMV_ENC_PADDING_CTRL);	
	WRITEL(0, S3C_FIMV_ENC_INT_MASK);	// mask interrupt	

	/* Set Rate Control */
	if (enc_init_mpeg4_arg->in_RC_framerate != 0)
		WRITEL(enc_init_mpeg4_arg->in_RC_framerate, S3C_FIMV_ENC_RC_FRAME_RATE);
	if (enc_init_mpeg4_arg->in_RC_bitrate != 0)
		WRITEL(enc_init_mpeg4_arg->in_RC_bitrate, S3C_FIMV_ENC_RC_BIT_RATE);
	WRITEL(enc_init_mpeg4_arg->in_RC_qbound, S3C_FIMV_ENC_RC_QBOUND);
	/* if in_RC_frm_enable is '1' */
	if (READL(S3C_FIMV_ENC_RC_CONFIG) & 0x0200)
		WRITEL(enc_init_mpeg4_arg->in_RC_rpara, S3C_FIMV_ENC_RC_RPARA);		

	switch (mfc_codec_type) {
	case H264_ENC:
		/* if in_RC_mb_enable is '1' */
		WRITEL((enc_init_mpeg4_arg->in_RC_frm_enable << 9)|(enc_init_h264_arg->in_RC_mb_enable << 8)|
			(enc_init_mpeg4_arg->in_vop_quant & 0x3F),
			S3C_FIMV_ENC_RC_CONFIG);
		if (READL(S3C_FIMV_ENC_RC_CONFIG) & 0x0100) {
			WRITEL((enc_init_h264_arg->in_RC_mb_dark_disable<<3)|
				(enc_init_h264_arg->in_RC_mb_smooth_disable<<2)| 
				(enc_init_h264_arg->in_RC_mb_static_disable<<1)| 
				(enc_init_h264_arg->in_RC_mb_activity_disable<<0),
				S3C_FIMV_ENC_RC_MB_CTRL);
		}	
		WRITEL((enc_init_mpeg4_arg->in_profile_level)|(1<<2)|
			(enc_init_h264_arg->in_transform8x8_mode<<4), 
			S3C_FIMV_ENC_PROFILE);
		WRITEL((enc_init_h264_arg->in_transform8x8_mode), S3C_FIMV_ENC_H264_TRANS_FLAG);		
		WRITEL((enc_init_h264_arg->in_symbolmode & 0x1), S3C_FIMV_ENC_ENTRP_MODE);
		WRITEL((enc_init_h264_arg->in_deblock_filt & 0x1), S3C_FIMV_ENC_LF_CTRL);
		WRITEL((enc_init_h264_arg->in_deblock_alpha_C0 & 0x1f)*2, S3C_FIMV_ENC_ALPHA_OFF);
		WRITEL((enc_init_h264_arg->in_deblock_beta & 0x1f)*2, S3C_FIMV_ENC_BETA_OFF);			
		if (enc_init_h264_arg->in_ref_num_p > enc_init_h264_arg->in_reference_num) {
			//mfc_info("Reference frame no of P should be equal or less than \
			//	Reference frame no");	
			//mfc_info("Reference frame no of P was set same as Reference frame no");		
			enc_init_h264_arg->in_ref_num_p = enc_init_h264_arg->in_reference_num;
		}	
		WRITEL((enc_init_h264_arg->in_ref_num_p<<5)|(enc_init_h264_arg->in_reference_num), 
			S3C_FIMV_ENC_H264_NUM_OF_REF);		
			
		WRITEL(enc_init_h264_arg->in_md_interweight_pps, S3C_FIMV_ENC_H264_MDINTER_WGT);		
		WRITEL(enc_init_h264_arg->in_md_intraweight_pps, S3C_FIMV_ENC_H264_MDINTRA_WGT);				
		break;
		
	case MPEG4_ENC:
		WRITEL((enc_init_mpeg4_arg->in_RC_frm_enable << 9)|(enc_init_mpeg4_arg->in_vop_quant & 0x3F),
			S3C_FIMV_ENC_RC_CONFIG);
		WRITEL(enc_init_mpeg4_arg->in_profile_level, S3C_FIMV_ENC_PROFILE);
		WRITEL(enc_init_mpeg4_arg->in_qpelME_enable, S3C_FIMV_ENC_MPEG4_QUART_PXL);		
		break;

	case H263_ENC:
		WRITEL((enc_init_mpeg4_arg->in_RC_frm_enable << 9)|(enc_init_mpeg4_arg->in_vop_quant & 0x3F),
			S3C_FIMV_ENC_RC_CONFIG);
		WRITEL(0x20, S3C_FIMV_ENC_PROFILE);
		break;

	default:
		mfc_err("Invalid MFC codec type\n");
	}	

}

int s3c_mfc_load_firmware()
{
	volatile unsigned char *fw_virbuf;

	mfc_debug("s3c_mfc_load_firmware++\n");

	fw_virbuf = Align((unsigned int)s3c_mfc_get_fw_buf_virt_addr(), 128*BUF_L_UNIT);
	memcpy((void *)fw_virbuf, s3c_mfc_fw_code, sizeof(s3c_mfc_fw_code));	

	mfc_debug("s3c_mfc_load_firmware--\n");
	
	return 0;
}

MFC_ERROR_CODE s3c_mfc_init_hw()
{
	unsigned int fw_phybuf;
	unsigned int dpb_luma_phybuf;
	int fw_buf_size;
	unsigned int fw_version;	

	mfc_debug("s3c_mfc_init_hw++\n");	
	
	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);	
	#if 0
	dpb_luma_phybuf = Align(s3c_mfc_get_dpb_luma_buf_phys_addr(), 128*BUF_L_UNIT);
	#else
	dpb_luma_phybuf = MFC_RESERVED_DRAM1_START;
	#endif
	
	/*
	 * 0. MFC reset
	 */
	s3c_mfc_cmd_reset();

	/*
	 * 1. Set DRAM base Addr
	 */
	WRITEL(fw_phybuf, S3C_FIMV_MC_DRAMBASE_ADR_A); 		// channelA, port0 
	WRITEL(dpb_luma_phybuf, S3C_FIMV_MC_DRAMBASE_ADR_B); 	// channelB, port1
	WRITEL(0, S3C_FIMV_MC_RS_IBASE); 			// FW location sel : 0->A, 1->B
	WRITEL(1, S3C_FIMV_NUM_MASTER);				// 0->1master, 1->2master	
	
	/*
	 * 2. Initialize registers of stream I/F for decoder
	 */
	WRITEL(0xffff, S3C_FIMV_SI_CH1_INST_ID);
	WRITEL(0xffff, S3C_FIMV_SI_CH2_INST_ID);
	
	WRITEL(0, S3C_FIMV_RISC2HOST_CMD);
	WRITEL(0, S3C_FIMV_HOST2RISC_CMD);

	/*
	 * 3. Release reset signal to the RISC.
	 */
	WRITEL(0x3ff, S3C_FIMV_SW_RESET);	

	if(s3c_mfc_wait_for_done(R2H_CMD_FW_STATUS_RET) == 0){
		mfc_err("MFCINST_ERR_FW_LOAD_FAIL\n");
		return MFCINST_ERR_FW_LOAD_FAIL;
	}

	/*
	 * 4. Initialize firmware
	 */
	fw_buf_size = MFC_FW_TOTAL_BUF_SIZE;
	s3c_mfc_cmd_host2risc(H2R_CMD_SYS_INIT, fw_buf_size);

	if(s3c_mfc_wait_for_done(R2H_CMD_SYS_INIT_RET) == 0){
		mfc_err("R2H_CMD_SYS_INIT_RET FAIL\n");
		return MFCINST_ERR_FW_INIT_FAIL;
	}	
	
	fw_version = READL(S3C_FIMV_FW_VERSION);
	mfc_info("MFC FW version : %02xyy, %02xmm, %02xdd\n", 
		(fw_version>>16)&0xff, (fw_version>>8)&0xff, (fw_version)&0xff);

	mfc_debug("FW_PHY_BUFFER : 0x%08x\n", READL(S3C_FIMV_MC_DRAMBASE_ADR_A));
	mfc_debug("DPB_LUMA_BUFFER : 0x%08x\n", READL(S3C_FIMV_MC_DRAMBASE_ADR_B));
	mfc_debug("s3c_mfc_init_hw--\n");
	
	return MFCINST_RET_OK;
}

static unsigned int s3c_mfc_get_codec_arg(MFC_CODEC_TYPE codec_type)
{
	unsigned int codec_no=99;

	switch (codec_type) {
	case H264_DEC:
		codec_no = 0;
		break;
	case VC1AP_DEC:
		codec_no = 1;
		break;
	case MPEG4_DEC:	
	case DIVX_DEC:
	case XVID_DEC:
		codec_no = 2;
		break;
	case MPEG1_DEC:
	case MPEG2_DEC:
		codec_no = 3;
		break;
	case H263_DEC:
	case MP4SH_DEC:		
		codec_no = 4;
		break;
	case VC1RCV_DEC:
		codec_no = 5;
		break;
	case DIVX311_DEC:
		codec_no = 6;
		break;
	case DIVX412_DEC:
		codec_no = 7;
		break;
	case DIVX502_DEC:
		codec_no = 8;
		break;
	case DIVX503_DEC:
		codec_no = 9;
		break;	
	case H264_ENC:
		codec_no = 16;
		break;	
	case MPEG4_ENC:
		codec_no = 17;
		break;	
	case H263_ENC:
		codec_no = 18;
		break;	
	default:
		break;
		
	}

	return (codec_no);
	
}		

static int s3c_mfc_get_inst_no(MFC_CODEC_TYPE codec_type)
{
	unsigned int codec_no;
	int inst_no;

	codec_no = (unsigned int)s3c_mfc_get_codec_arg(codec_type);

	s3c_mfc_cmd_host2risc(H2R_CMD_OPEN_INSTANCE, codec_no);

	if(s3c_mfc_wait_for_done(R2H_CMD_OPEN_INSTANCE_RET) == 0){
		mfc_err("R2H_CMD_OPEN_INSTANCE_RET FAIL\n"); // peter, check this status
		return MFCINST_ERR_FW_INIT_FAIL;
	}

	inst_no = READL(S3C_FIMV_RISC2HOST_ARG1);

	mfc_debug("INSTANCE NO : %d, CODEC_TYPE : %d --\n", inst_no, codec_no);

	/* peter, when is impossible to get instance_no ??? */
	if (inst_no >= MFC_MAX_INSTANCE_NUM)
		return -1;
	else
		// Get the instance no
		return (inst_no);
	
}

void s3c_mfc_return_inst_no(int inst_no, MFC_CODEC_TYPE codec_type)
{
	unsigned int codec_no;

	codec_no = (unsigned int)s3c_mfc_get_codec_arg(codec_type);

	s3c_mfc_cmd_host2risc(H2R_CMD_CLOSE_INSTANCE, inst_no);

	if(s3c_mfc_wait_for_done(R2H_CMD_CLOSE_INSTANCE_RET) == 0) {
		mfc_err("R2H_CMD_CLOSE_INSTANCE_RET FAIL\n"); // peter, check this status
		return MFCINST_ERR_FW_INIT_FAIL;
	}

	mfc_debug("INSTANCE NO : %d, CODEC_TYPE : %d --\n", READL(S3C_FIMV_RISC2HOST_ARG1), codec_no);

/*
	// Get the instance no
	return (READL(S3C_FIMV_RISC2HOST_ARG1));
	if ((inst_no >= 0) && (inst_no < MFC_MAX_INSTANCE_NUM))
		s3c_mfc_inst_no[inst_no] = 0;
*/
}

MFC_ERROR_CODE s3c_mfc_init_encode(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	s3c_mfc_enc_init_mpeg4_arg_t   *enc_init_mpeg4_arg;	
		
	enc_init_mpeg4_arg = (s3c_mfc_enc_init_mpeg4_arg_t *) args;

	mfc_debug("++\n");
	mfc_ctx->MfcCodecType = enc_init_mpeg4_arg->in_codec_type;
	mfc_ctx->img_width = (unsigned int)enc_init_mpeg4_arg->in_width;
	mfc_ctx->img_height = (unsigned int)enc_init_mpeg4_arg->in_height;
	mfc_ctx->interlace_mode = enc_init_mpeg4_arg->in_interlace_mode;

	/* OPEN CHANNEL
	 * 	- set open instance using codec_type
	 * 	- get the instance no
	 */
	 
	// peter, MFC_CreateInstance() & 
	mfc_ctx->InstNo = s3c_mfc_get_inst_no(mfc_ctx->MfcCodecType);
	if (mfc_ctx->InstNo < 0) {
		kfree(mfc_ctx);
		mfc_err("MFCINST_INST_NUM_EXCEEDED\n");
		return MFCINST_INST_NUM_EXCEEDED;		
	}
	
	/* INIT CODEC
	 *  - set init parameter
	 *  - set init sequence done command
	 *  - set input risc buffer
	 */
	 
	/* peter, MFC_SetConfigParameters() & MFC_CalculateTotalFramesToEnc() & 
	   MFC_SetBufAddrOfEncoder() & MFC_GetEncBufEndAddr() */
	s3c_mfc_set_encode_init_param(mfc_ctx->InstNo, mfc_ctx->MfcCodecType, args);

	/* peter, init_codec command will be inserted in the next MFC fw version */

	s3c_mfc_set_risc_buffer(mfc_ctx->MfcCodecType, mfc_ctx->InstNo);	

	//s3c_mfc_backup_context(mfc_ctx);
	
	mfc_debug("--\n");
	
	return MFCINST_RET_OK;
}

static MFC_ERROR_CODE s3c_mfc_encode_one_frame(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	s3c_mfc_enc_exe_arg          *enc_arg;
	unsigned int fw_phybuf, mv_ref_yc_phybuf;

	enc_arg = (s3c_mfc_enc_exe_arg *) args;
	
	mfc_debug("++ enc_arg->in_strm_st : 0x%08x enc_arg->in_strm_end :0x%08x \r\n", \
								enc_arg->in_strm_st, enc_arg->in_strm_end);
	mfc_debug("enc_arg->in_Y_addr : 0x%08x enc_arg->in_CbCr_addr :0x%08x \r\n",   \
								enc_arg->in_Y_addr, enc_arg->in_CbCr_addr);

	s3c_mfc_restore_context(mfc_ctx);

	fw_phybuf = Align(s3c_mfc_get_fw_buf_phys_addr(), 128*BUF_L_UNIT);
	#if 0
	mv_ref_yc_phybuf = Align(s3c_mfc_get_dpb_luma_buf_phys_addr(), 128*BUF_L_UNIT);
	#else
	mv_ref_yc_phybuf = MFC_RESERVED_DRAM1_START;
	#endif
	
	
	/* Set stream buffer addr */
	WRITEL((enc_arg->in_strm_st-fw_phybuf)>>11, S3C_FIMV_ENC_SI_CH1_SB_U_ADR);
	WRITEL((enc_arg->in_strm_st-fw_phybuf)>>11, S3C_FIMV_ENC_SI_CH1_SB_L_ADR);
	WRITEL(STREAM_BUF_SIZE, S3C_FIMV_ENC_SI_CH1_SB_SIZE);

	/* Set current frame buffer addr */
	WRITEL((enc_arg->in_Y_addr-mv_ref_yc_phybuf)>>11, S3C_FIMV_ENC_SI_CH1_CUR_Y_ADR);
	WRITEL((enc_arg->in_CbCr_addr-mv_ref_yc_phybuf)>>11, S3C_FIMV_ENC_SI_CH1_CUR_C_ADR);
	
	/* peter, if SsbSipMfcEncSetConfig() in appl, 2 reg. should be set */
	//WRITEL(enc_init_mpeg4_arg->in_vop_quant, S3C_FIMV_ENC_SI_CH1_FRAME_QP);
	//WRITEL(enc_init_mpeg4_arg->in_MS_size, S3C_FIMV_ENC_SI_CH1_SLICE_ARG);

	WRITEL(1, S3C_FIMV_ENC_STR_BF_U_EMPTY);
	WRITEL(1, S3C_FIMV_ENC_STR_BF_L_EMPTY);
	/* buf reset command if stream buffer is frame mode */
	WRITEL(0x1<<1, S3C_FIMV_ENC_SF_BUF_CTRL);

	WRITEL(mfc_ctx->InstNo, S3C_FIMV_SI_CH1_INST_ID);
	
	if (s3c_mfc_wait_for_done(R2H_CMD_FRAME_DONE_RET) == 0) {
		mfc_err("MFCINST_ERR_ENC_ENCODE_DONE_FAIL\n");
		return MFCINST_ERR_ENC_ENCODE_DONE_FAIL;
	}	

	enc_arg->out_frame_type = READL(S3C_FIMV_ENC_SI_SLICE_TYPE);
	enc_arg->out_encoded_size = READL(S3C_FIMV_ENC_SI_STRM_SIZE);	
	//EncExeArg->out_header_size  = READL(S3C_FIMV_ENC_HEADER_SIZE);	

	mfc_debug("-- frame type(%d) encodedSize(%d)\r\n", \
		enc_arg->out_frame_type, enc_arg->out_encoded_size);
	
	return MFCINST_RET_OK;

}

MFC_ERROR_CODE s3c_mfc_exe_encode(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	MFC_ERROR_CODE ret;
	s3c_mfc_enc_exe_arg          *enc_arg;

	/* 
	 * 5. Encode Frame	 */

	enc_arg = (s3c_mfc_enc_exe_arg *) args;
	ret = s3c_mfc_encode_one_frame(mfc_ctx, enc_arg);

	//if (mfc_ctx->interlace_mode)
	//	ret = s3c_mfc_encode_one_frame(mfc_ctx, enc_arg);

	mfc_debug("--\n");

	return ret;	
	
}

MFC_ERROR_CODE s3c_mfc_init_decode(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	//MFC_ERROR_CODE   ret;
	s3c_mfc_dec_init_arg_t *init_arg;
	//s3c_mfc_dec_type dec_type = SEQ_HEADER;
	//unsigned int FWPhyBuf;

	mfc_debug("++\n");
	init_arg = (s3c_mfc_dec_init_arg_t *)args;
	//FWPhyBuf = s3c_mfc_get_fw_buf_phys_addr();

	/* Context setting from input param */
	mfc_ctx->MfcCodecType = init_arg->in_codec_type;
	mfc_ctx->IsPackedPB = init_arg->in_packed_PB;
	
	/* OPEN CHANNEL
	 * 	- set open instance using codec_type
	 * 	- get the instance no
	 */
	// peter, MFC_InitProcessForDecoding()
	mfc_ctx->InstNo = s3c_mfc_get_inst_no(mfc_ctx->MfcCodecType);
	if (mfc_ctx->InstNo < 0) {
		kfree(mfc_ctx);
		mfc_err("MFCINST_INST_NUM_EXCEEDED\n");
		return MFCINST_INST_NUM_EXCEEDED;		
	}

	// peter, Does it need to set for decoder ?
	//WRITEL(mfc_ctx->postEnable, S3C_FIMV_ENC_LF_CTRL);
	//WRITEL(0, S3C_FIMV_ENC_PXL_CACHE_CTRL);		

	/* INIT CODEC
	 * 	- set input stream buffer 
	 * 	- set sequence done command
	 * 	- set input risc buffer
	 * 	- set NUM_EXTRA_DPB
	 */	
	// peter, MFC_ParseStreamHeader() 
	
	s3c_mfc_set_dec_stream_buffer(mfc_ctx->InstNo, init_arg->in_strm_buf, init_arg->in_strm_size); 
	WRITEL((SEQ_HEADER<<16 & 0x30000)|(mfc_ctx->InstNo), S3C_FIMV_SI_CH1_INST_ID);

	if (s3c_mfc_wait_for_done(R2H_CMD_SEQ_DONE_RET) == 0) {
		mfc_err("MFCINST_ERR_DEC_HEADER_DECODE_FAIL\n");
		return MFCINST_ERR_DEC_HEADER_DECODE_FAIL;
	}
		
	s3c_mfc_set_risc_buffer(mfc_ctx->MfcCodecType, mfc_ctx->InstNo);	

	/* out param & context setting from header decoding result */
	mfc_ctx->img_width = READL(S3C_FIMV_SI_HRESOL);
	mfc_ctx->img_height = READL(S3C_FIMV_SI_VRESOL);	

	init_arg->out_img_width = READL(S3C_FIMV_SI_HRESOL);
	init_arg->out_img_height = READL(S3C_FIMV_SI_VRESOL);

	/* in the case of VC1 interlace, height will be the multiple of 32
	 * otherwise, height and width is the mupltiple of 16
	 */
	init_arg->out_buf_width = (READL(S3C_FIMV_SI_HRESOL)+15)/16*16;
	init_arg->out_buf_height = (READL(S3C_FIMV_SI_VRESOL)+31)/32*32;

	if (mfc_ctx->MfcCodecType == DIVX311_DEC) {
		mfc_ctx->img_width = READL(S3C_FIMV_SI_DIVX311_HRESOL);
		mfc_ctx->img_height = READL(S3C_FIMV_SI_DIVX311_VRESOL);	
		init_arg->out_img_width = READL(S3C_FIMV_SI_DIVX311_HRESOL);
		init_arg->out_img_height = READL(S3C_FIMV_SI_DIVX311_VRESOL);	
		init_arg->out_buf_width = (READL(S3C_FIMV_SI_DIVX311_HRESOL)+15)/16*16;
		init_arg->out_buf_height = (READL(S3C_FIMV_SI_DIVX311_VRESOL)+31)/32*32;		
	}	

	/* peter, It should have extraDPB to protect tearing in the display
	 */
	init_arg->out_dpb_cnt = READL(S3C_FIMV_SI_BUF_NUMBER); 
	mfc_ctx->DPBCnt = READL(S3C_FIMV_SI_BUF_NUMBER);

/*
	switch (mfc_ctx->MfcCodecType) {
	case H264_DEC: 
		init_arg->out_dpb_cnt = (READL(S3C_FIMV_SI_BUF_NUMBER)*3)>>1; 
		mfc_ctx->DPBCnt = READL(S3C_FIMV_SI_BUF_NUMBER);
		break;

	case MPEG4_DEC:
	case MPEG2_DEC: 
	case DIVX_DEC: 
	case XVID_DEC:
		init_arg->out_dpb_cnt = ((NUM_MPEG4_DPB * 3) >> 1) + NUM_POST_DPB + mfc_ctx->extraDPB;
		mfc_ctx->DPBCnt = NUM_MPEG4_DPB;
		break;

	case VC1_DEC:
		init_arg->out_dpb_cnt = ((NUM_VC1_DPB * 3) >> 1)+ MfcCtx->extraDPB;
		mfc_ctx->DPBCnt = NUM_VC1_DPB + mfc_ctx->extraDPB;
		break;

	default:
		init_arg->out_dpb_cnt = ((NUM_MPEG4_DPB * 3) >> 1)+ NUM_POST_DPB + mfc_ctx->extraDPB;
		mfc_ctx->DPBCnt = NUM_MPEG4_DPB;
	}
*/
	mfc_ctx->totalDPBCnt = init_arg->out_dpb_cnt;

	mfc_debug("buf_width : %d buf_height : %d out_dpb_cnt : %d mfc_ctx->DPBCnt : %d\n", \
				init_arg->out_img_width, init_arg->out_img_height, init_arg->out_dpb_cnt, mfc_ctx->DPBCnt);
	mfc_debug("img_width : %d img_height : %d\n", \
				init_arg->out_img_width, init_arg->out_img_height);

	//s3c_mfc_backup_context(mfc_ctx);

	mfc_debug("--\n");
	
	return MFCINST_RET_OK;
}

static MFC_ERROR_CODE s3c_mfc_decode_one_frame(s3c_mfc_inst_ctx *mfc_ctx,  s3c_mfc_dec_exe_arg_t *dec_arg, 
						unsigned int *consumed_strm_size)
{
	int ret;
	unsigned int frame_type;
	static int count = 0;

	count++;
	
	mfc_debug("++ IntNo%d(%d)\r\n", mfc_ctx->InstNo, count);

	s3c_mfc_restore_context(mfc_ctx);	

	s3c_mfc_set_dec_stream_buffer(mfc_ctx->InstNo, dec_arg->in_strm_buf, dec_arg->in_strm_size);
	
	s3c_mfc_set_dec_frame_buffer(mfc_ctx, dec_arg->in_frm_buf, dec_arg->in_frm_size);

	/* Set RISC buffer */
	s3c_mfc_set_risc_buffer(mfc_ctx->MfcCodecType, mfc_ctx->InstNo);

	if(mfc_ctx->endOfFrame) {
		WRITEL((LAST_FRAME<<16 & 0x30000)|(mfc_ctx->InstNo), S3C_FIMV_SI_CH1_INST_ID);
		mfc_ctx->endOfFrame = 0;
	} else {
		WRITEL((FRAME<<16 & 0x30000)|(mfc_ctx->InstNo), S3C_FIMV_SI_CH1_INST_ID);
	}	

	if (s3c_mfc_wait_for_done(R2H_CMD_FRAME_DONE_RET) == 0) {
		mfc_err("MFCINST_ERR_DEC_DECODE_DONE_FAIL\n");
		return MFCINST_ERR_DEC_DECODE_DONE_FAIL;
	}		

	if ((READL(S3C_FIMV_SI_DISPLAY_STATUS) & 0x3) == DECODING_ONLY) {
		dec_arg->out_display_Y_addr = 0;
		dec_arg->out_display_C_addr = 0;
		mfc_debug("DECODING_ONLY frame decoded \n");
	} else {
		dec_arg->out_display_Y_addr = READL(S3C_FIMV_SI_DISPLAY_Y_ADR);
		dec_arg->out_display_C_addr = READL(S3C_FIMV_SI_DISPLAY_C_ADR);		
	}	

	if ((READL(S3C_FIMV_SI_DISPLAY_STATUS) & 0x3) == DECODING_EMPTY) {
		dec_arg->out_display_status = 0; 
	} else if ((READL(S3C_FIMV_SI_DISPLAY_STATUS) & 0x3) == DECODING_DISPLAY)  {
		dec_arg->out_display_status = 1; 
	} else if ((READL(S3C_FIMV_SI_DISPLAY_STATUS) & 0x3) == DISPLAY_ONLY)  { 	
		dec_arg->out_display_status = 2; 
	} else
		dec_arg->out_display_status = 3; 
	
	frame_type = READL(S3C_FIMV_SI_FRAME_TYPE); 
	mfc_ctx->FrameType = (s3c_mfc_frame_type)(frame_type & 0x3);

	//s3c_mfc_backup_context(mfc_ctx);

	mfc_debug("(Y_ADDR : 0x%08x  C_ADDR : 0x%08x)\r\n", \
		dec_arg->out_display_Y_addr , dec_arg->out_display_C_addr);  
	//mfc_debug("(in_strmsize : 0x%08x  consumed byte : 0x%08x)\r\n", \
	//		dec_arg->in_strm_size, READL(S3C_FIMV_RET_VALUE));      

	//*consumed_strm_size = READL(S3C_FIMV_RET_VALUE); // peter, find out related reg. for C110
	*consumed_strm_size = 0x0;
	
	return MFCINST_RET_OK;
}


MFC_ERROR_CODE s3c_mfc_exe_decode(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	MFC_ERROR_CODE ret;
	s3c_mfc_dec_exe_arg_t *dec_arg;
	unsigned int consumed_strm_size;
	
	/* 6. Decode Frame */
	mfc_debug("++\n");

	dec_arg = (s3c_mfc_dec_exe_arg_t *)args;
	ret = s3c_mfc_decode_one_frame(mfc_ctx, dec_arg, &consumed_strm_size);

	if((mfc_ctx->IsPackedPB) && (mfc_ctx->FrameType == MFC_RET_FRAME_P_FRAME) \
		&& (dec_arg->in_strm_size - consumed_strm_size > 4)) {
		mfc_debug("Packed PB\n");
		dec_arg->in_strm_buf += consumed_strm_size;
		dec_arg->in_strm_size -= consumed_strm_size;

		ret = s3c_mfc_decode_one_frame(mfc_ctx, dec_arg, &consumed_strm_size);
	}
	mfc_debug("--\n");

	return ret; 
}

MFC_ERROR_CODE s3c_mfc_deinit_hw(s3c_mfc_inst_ctx  *mfc_ctx)
{
	s3c_mfc_restore_context(mfc_ctx);

	return MFCINST_RET_OK;
}

MFC_ERROR_CODE s3c_mfc_get_config(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	s3c_mfc_get_config_arg_t *get_cnf_arg;
	get_cnf_arg = (s3c_mfc_get_config_arg_t *)args;

	switch (get_cnf_arg->in_config_param) {
	case MFC_DEC_GETCONF_CRC_DATA:
		if (mfc_ctx->MfcState != MFCINST_STATE_DEC_EXE) {
			mfc_err("MFC_DEC_GETCONF_CRC_DATA : state is invalid\n");
			return MFC_DEC_GETCONF_CRC_DATA;
		}
		get_cnf_arg->out_config_value[0] = READL(S3C_FIMV_CRC_LUMA0);
		get_cnf_arg->out_config_value[1] = READL(S3C_FIMV_CRC_CHROMA0);
		
		break;		
		
	default:
		mfc_err("invalid config param\n");
		return MFCINST_ERR_SET_CONF; // peter, it should be mod.
	}
	
	return MFCINST_RET_OK;
}


MFC_ERROR_CODE s3c_mfc_set_config(s3c_mfc_inst_ctx  *mfc_ctx,  s3c_mfc_args *args)
{
	s3c_mfc_set_config_arg_t *set_cnf_arg;
	set_cnf_arg = (s3c_mfc_set_config_arg_t *)args;

	switch (set_cnf_arg->in_config_param) {
	case MFC_DEC_SETCONF_POST_ENABLE:
		if (mfc_ctx->MfcState >= MFCINST_STATE_DEC_INITIALIZE) {
			mfc_err("MFC_DEC_SETCONF_POST_ENABLE : state is invalid\n");
			return MFCINST_ERR_STATE_INVALID;
		}

		if((set_cnf_arg->in_config_value[0] == 0) || (set_cnf_arg->in_config_value[0] == 1))
			mfc_ctx->postEnable = set_cnf_arg->in_config_value[0];
		else {
			mfc_warn("POST_ENABLE should be 0 or 1\n");
			mfc_ctx->postEnable = 0;
		}
		break;	
		
	case MFC_DEC_SETCONF_EXTRA_BUFFER_NUM:
		if (mfc_ctx->MfcState >= MFCINST_STATE_DEC_INITIALIZE) {
			mfc_err("MFC_DEC_SETCONF_EXTRA_BUFFER_NUM : state is invalid\n");
			return MFCINST_ERR_STATE_INVALID;
		}
		if ((set_cnf_arg->in_config_value[0] >= 0) || (set_cnf_arg->in_config_value[0] <= MFC_MAX_EXTRA_DPB))
			mfc_ctx->extraDPB = set_cnf_arg->in_config_value[0];
		else {
			mfc_warn("EXTRA_BUFFER_NUM should be between 0 and 5...It will be set 5 by default\n");
			mfc_ctx->extraDPB = MFC_MAX_EXTRA_DPB;
		}
		break;

	// peter, check whether C110 has this function	
	/*	
	case MFC_DEC_SETCONF_DISPLAY_DELAY:
		if (mfc_ctx->MfcState >= MFCINST_STATE_DEC_INITIALIZE) {
			mfc_err("MFC_DEC_SETCONF_DISPLAY_DELAY : state is invalid\n");
			return MFCINST_ERR_STATE_INVALID;
		}
		if (mfc_ctx->MfcCodecType == H264_DEC) {
			if ((set_cnf_arg->in_config_value[0] >= 0) || (set_cnf_arg->in_config_value[0] < 16))
				mfc_ctx->displayDelay = set_cnf_arg->in_config_value[0];
			else {
				mfc_warn("DISPLAY_DELAY should be between 0 and 16\n");
				mfc_ctx->displayDelay = 0;
			}
		} else {
			mfc_warn("MFC_DEC_SETCONF_DISPLAY_DELAY is only valid for H.264\n");
			mfc_ctx->displayDelay = 0;
		}
		break;
	*/	
	case MFC_DEC_SETCONF_IS_LAST_FRAME:
		if (mfc_ctx->MfcState != MFCINST_STATE_DEC_EXE) {
			mfc_err("MFC_DEC_SETCONF_IS_LAST_FRAME : state is invalid\n");
			return MFCINST_ERR_STATE_INVALID;
		}

		if ((set_cnf_arg->in_config_value[0] == 0) || (set_cnf_arg->in_config_value[0] == 1))
			mfc_ctx->endOfFrame = set_cnf_arg->in_config_value[0];
		else {
			mfc_warn("IS_LAST_FRAME should be 0 or 1\n");
			mfc_ctx->endOfFrame = 0;
		}
		break;
			
	case MFC_ENC_SETCONF_FRAME_TYPE:
		if ((mfc_ctx->MfcState < MFCINST_STATE_ENC_INITIALIZE) || (mfc_ctx->MfcState > MFCINST_STATE_ENC_EXE)) {
			mfc_err("MFC_ENC_SETCONF_FRAME_TYPE : state is invalid\n");
			return MFCINST_ERR_STATE_INVALID;
		}

		if ((set_cnf_arg->in_config_value[0] < DONT_CARE) || (set_cnf_arg->in_config_value[0] > NOT_CODED))
			mfc_ctx->forceSetFrameType = set_cnf_arg->in_config_value[0];
		else {
			mfc_warn("FRAME_TYPE should be between 0 and 2\n");
			mfc_ctx->forceSetFrameType = DONT_CARE;
		}
		break;
		
	default:
		mfc_err("invalid config param\n");
		return MFCINST_ERR_SET_CONF;
	}
	
	return MFCINST_RET_OK;
}

