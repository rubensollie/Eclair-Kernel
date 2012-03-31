/*
 * drivers/media/video/samsung/mfc40/s3c_mfc_common.c
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


#include <linux/mm.h>

#include "s3c_mfc_buffer_manager.h"
#include "s3c_mfc_common.h"
#include "s3c_mfc_interface.h"
#include "s3c_mfc_memory.h"
#include "s3c_mfc_logmsg.h"
#include "s3c_mfc_fw.h"
#include "s3c_mfc_errorno.h"


static int s3c_mfc_mem_inst_no[MFC_MAX_INSTANCE_NUM];

/* Allocate buffers for decoder */
MFC_ERROR_CODE s3c_mfc_allocate_frame_buf(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_args *args, s3c_mfc_frame_buf_arg_t buf_size)
{
	s3c_mfc_dec_init_arg_t *init_arg;
	s3c_mfc_args		local_param;
	unsigned int		luma_size, chroma_size;
	MFC_ERROR_CODE 		ret_code = MFCINST_RET_OK;
	
	init_arg = (s3c_mfc_dec_init_arg_t *)args;
	luma_size = Align(buf_size.luma, 2*BUF_L_UNIT) * mfc_ctx->totalDPBCnt;
	chroma_size = Align(buf_size.chroma, 2*BUF_L_UNIT) * mfc_ctx->totalDPBCnt;	

	/* 
	 * Allocate chroma & (Mv in case of H264) buf 
	 */
	init_arg->out_frame_buf_size.chroma = chroma_size;	
	
	memset(&local_param, 0, sizeof(local_param));
	local_param.mem_alloc.buff_size = Align(chroma_size, 2*BUF_S_UNIT);
	local_param.mem_alloc.mapped_addr = init_arg->in_mapped_addr; 

	mfc_ctx->port_no = 0;		
	ret_code = s3c_mfc_get_virt_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;			
	init_arg->out_u_addr.chroma = local_param.mem_alloc.out_addr;

	memset(&local_param, 0, sizeof(local_param));
	local_param.get_phys_addr.u_addr = init_arg->out_u_addr.chroma;

	ret_code = s3c_mfc_get_phys_addr(mfc_ctx, &local_param);
	if (ret_code < 0) 
		return ret_code;	
	
	init_arg->out_p_addr.chroma = local_param.get_phys_addr.p_addr;	

	/* 
	 * Allocate luma buf 
	 */
	init_arg->out_frame_buf_size.luma = luma_size;	
	
	memset(&local_param, 0, sizeof(local_param));
	local_param.mem_alloc.buff_size = Align(luma_size, 2*BUF_S_UNIT);
	local_param.mem_alloc.mapped_addr = init_arg->in_mapped_addr; // peter, it chould be checked in related to luma cases			

	mfc_ctx->port_no = 1;		
	ret_code = s3c_mfc_get_virt_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;			
	init_arg->out_u_addr.luma = local_param.mem_alloc.out_addr;

	memset(&local_param, 0, sizeof(local_param));
	local_param.get_phys_addr.u_addr = init_arg->out_u_addr.luma;

	ret_code = s3c_mfc_get_phys_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;	
	
	init_arg->out_p_addr.luma = local_param.get_phys_addr.p_addr;	

	return ret_code;

}	

s3c_mfc_frame_buf_arg_t s3c_mfc_get_frame_buf_size(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_args *args)
{
	unsigned int luma_plane_sz, chroma_plane_sz, mv_plane_sz;
	s3c_mfc_frame_buf_arg_t buf_size;
	s3c_mfc_dec_init_arg_t *init_arg;

	init_arg = (s3c_mfc_dec_init_arg_t *)args;

	/* frameBufSize is sizes for LumaPlane, ChromaPlane & MvPlane */
	luma_plane_sz = Align(Align(init_arg->out_img_width, 4*BUF_S_UNIT)*Align(init_arg->out_img_height, BUF_S_UNIT), \
				64*BUF_L_UNIT);
	chroma_plane_sz = Align(Align(init_arg->out_img_width, 4*BUF_S_UNIT)*Align(init_arg->out_img_height/2, BUF_S_UNIT), \
				64*BUF_L_UNIT);;
	buf_size.luma = luma_plane_sz;
	buf_size.chroma = chroma_plane_sz;

	if (mfc_ctx->MfcCodecType == H264_DEC) {
		mv_plane_sz = Align(Align(init_arg->out_img_width, 4*BUF_S_UNIT)*Align(init_arg->out_img_height/4, BUF_S_UNIT), \
					64*BUF_L_UNIT);
		buf_size.chroma += mv_plane_sz;
	} 

	return buf_size;

}

/* Allocate buffers for encoder */
MFC_ERROR_CODE s3c_mfc_allocate_stream_ref_buf(s3c_mfc_inst_ctx  *mfc_ctx, s3c_mfc_args *args)
{
	s3c_mfc_enc_init_mpeg4_arg_t *init_arg;
	s3c_mfc_args		local_param;
	unsigned int		buf_width, buf_height;
	MFC_ERROR_CODE 		ret_code = MFCINST_RET_OK;
	
	init_arg = (s3c_mfc_enc_init_mpeg4_arg_t *)args;	

	/* 
	 * Allocate stream & ref Y0, Y2 buf 
	 */
	buf_width = (mfc_ctx->img_width+15)/16*16;
	buf_height = (mfc_ctx->img_height+31)/32*32;
	
	init_arg->out_buf_size.strm_ref_y = STREAM_BUF_SIZE + Align(buf_width*buf_height, 64*BUF_L_UNIT)*2;
	
	memset(&local_param, 0, sizeof(local_param));
	local_param.mem_alloc.buff_size = Align(init_arg->out_buf_size.strm_ref_y, 2*BUF_S_UNIT);
	local_param.mem_alloc.mapped_addr = init_arg->in_mapped_addr; 

	mfc_ctx->port_no = 0;		
	ret_code = s3c_mfc_get_virt_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;			
	init_arg->out_u_addr.strm_ref_y = local_param.mem_alloc.out_addr;

	memset(&local_param, 0, sizeof(local_param));
	local_param.get_phys_addr.u_addr = init_arg->out_u_addr.strm_ref_y;

	ret_code = s3c_mfc_get_phys_addr(mfc_ctx, &local_param);
	if (ret_code < 0) 
		return ret_code;	
	
	init_arg->out_p_addr.strm_ref_y = local_param.get_phys_addr.p_addr;	

	/* 
	 * Allocate ref C0,C2,YC1,YC3 & MV buf 
	 */
	init_arg->out_buf_size.mv_ref_yc = MV_BUF_SIZE + Align(buf_width*buf_height, 64*BUF_L_UNIT)*2
							+ Align(buf_width*buf_height/2, 64*BUF_L_UNIT)*4;	
	
	memset(&local_param, 0, sizeof(local_param));
	/* In IOCTL_MFC_GET_IN_BUF(), Cur Y/C buf start addr should be 64KB aligned */
	local_param.mem_alloc.buff_size = Align(init_arg->out_buf_size.mv_ref_yc, 64*BUF_L_UNIT);
	local_param.mem_alloc.mapped_addr = init_arg->in_mapped_addr; // peter, it chould be checked in related to luma cases			

	mfc_ctx->port_no = 1;		
	ret_code = s3c_mfc_get_virt_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;			
	init_arg->out_u_addr.mv_ref_yc = local_param.mem_alloc.out_addr;

	memset(&local_param, 0, sizeof(local_param));
	local_param.get_phys_addr.u_addr = init_arg->out_u_addr.mv_ref_yc;

	ret_code = s3c_mfc_get_phys_addr(mfc_ctx, &(local_param));
	if (ret_code < 0) 
		return ret_code;	
	
	init_arg->out_p_addr.mv_ref_yc = local_param.get_phys_addr.p_addr;	

	return ret_code;

}

void  s3c_mfc_init_mem_inst_no(void)
{
	memset(&s3c_mfc_mem_inst_no, 0x00, sizeof(s3c_mfc_mem_inst_no));
}

int s3c_mfc_get_mem_inst_no(void)
{
	unsigned int i;

	for(i = 0; i < MFC_MAX_INSTANCE_NUM; i++)
		if (s3c_mfc_mem_inst_no[i] == 0) {
			s3c_mfc_mem_inst_no[i] = 1;
			return i;
		}

	return -1;   
}

void s3c_mfc_return_mem_inst_no(int inst_no)
{
	if ((inst_no >= 0) && (inst_no < MFC_MAX_INSTANCE_NUM))
		s3c_mfc_mem_inst_no[inst_no] = 0;

}

/*
BOOL s3c_mfc_is_running(void)
{
	unsigned int    i;
	BOOL ret = FALSE;

	for(i = 1; i < MFC_MAX_INSTANCE_NUM; i++)
		if(s3c_mfc_mem_inst_no[i] == 1)
			ret = TRUE;

	return ret;  
}
*/

int s3c_mfc_set_state(s3c_mfc_inst_ctx *ctx, s3c_mfc_inst_state state)
{

	if(ctx->MfcState > state)
		return -1;

	ctx->MfcState = state;
	return  0;

}

