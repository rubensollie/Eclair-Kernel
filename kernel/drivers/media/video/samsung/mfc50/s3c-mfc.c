/*
 * drivers/media/video/samsung/mfc50/s3c-mfc.c
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


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <plat/media.h>

#include "s3c_mfc_interface.h"
#include "s3c_mfc_common.h"
#include "s3c_mfc_logmsg.h"
#include "s3c_mfc_opr.h"
#include "s3c_mfc_intr.h"
#include "s3c_mfc_memory.h"
#include "s3c_mfc_buffer_manager.h"

static int s3c_mfc_openhandle_count = 0;
static struct resource	*s3c_mfc_mem;
void __iomem		*s3c_mfc_sfr_virt_base;

unsigned int  		s3c_mfc_int_type = 0;

static struct mutex	s3c_mfc_mutex;

unsigned int		s3c_mfc_phys_buf, s3c_mfc_phys_dpb_luma_buf;
volatile unsigned char	*s3c_mfc_virt_buf, *s3c_mfc_virt_dpb_luma_buf;
int vir_mmap_size;

extern s3c_mfc_alloc_mem_t *s3c_mfc_alloc_mem_head[MFC_MAX_PORT_NUM];
extern s3c_mfc_alloc_mem_t *s3c_mfc_alloc_mem_tail[MFC_MAX_PORT_NUM];

DECLARE_WAIT_QUEUE_HEAD(s3c_mfc_wait_queue);

static int s3c_mfc_open(struct inode *inode, struct file *file)
{
	s3c_mfc_inst_ctx *mfc_ctx;
	int ret;

	mutex_lock(&s3c_mfc_mutex);

	s3c_mfc_openhandle_count++;
	if (s3c_mfc_openhandle_count == 1) {
		/* MFC Hardware Initialization */	
		if (s3c_mfc_init_hw() == FALSE)
			return -ENODEV;
	}
	
	mfc_ctx = (s3c_mfc_inst_ctx *) kmalloc(sizeof(s3c_mfc_inst_ctx), GFP_KERNEL);
	if (mfc_ctx == NULL) {
		mfc_err("MFCINST_MEMORY_ALLOC_FAIL\n");
		ret = -ENOMEM;
		goto out_open;
	}
	
	memset(mfc_ctx, 0, sizeof(s3c_mfc_inst_ctx)); 	

	/* get the inst no allocating some part of memory among reserved memory */
	mfc_ctx->mem_inst_no = s3c_mfc_get_mem_inst_no();
	if (mfc_ctx->mem_inst_no < 0) {
		kfree(mfc_ctx);
		mfc_err("MFCINST_INST_NUM_EXCEEDED\n");
		ret = -EPERM;
		goto out_open;
	}
	
	if (s3c_mfc_set_state(mfc_ctx, MFCINST_STATE_OPENED) < 0) {
		mfc_err("MFCINST_ERR_STATE_INVALID\n");
		kfree(mfc_ctx);
		ret = -ENODEV;
		goto out_open;
	}

	/* Decoder only */
	mfc_ctx->extraDPB = MFC_MAX_EXTRA_DPB;		
	mfc_ctx->FrameType = MFC_RET_FRAME_NOT_SET;	

	file->private_data = (s3c_mfc_inst_ctx *)mfc_ctx;
	
	ret = 0;

out_open:
	mutex_unlock(&s3c_mfc_mutex);
	return ret;
}

static int s3c_mfc_release(struct inode *inode, struct file *file)
{
	s3c_mfc_inst_ctx *mfc_ctx;
	s3c_mfc_alloc_mem_t *node, *tmp_node;
	int port_no = 0;
	int ret;

	mfc_debug("MFC Release..\n");
	mutex_lock(&s3c_mfc_mutex);

	mfc_ctx = (s3c_mfc_inst_ctx *)file->private_data;
	if (mfc_ctx == NULL) {
		mfc_err("MFCINST_ERR_INVALID_PARAM\n");
		ret = -EIO;
		goto out_release;
	}			
	for (port_no=0; port_no < MFC_MAX_PORT_NUM; port_no++) {			
		for(node = s3c_mfc_alloc_mem_head[port_no]; node != s3c_mfc_alloc_mem_tail[port_no]; 
			node = node->next) {
			if (node->inst_no == mfc_ctx->mem_inst_no) {
				tmp_node = node;
				node = node->prev;
				mfc_ctx->port_no = port_no;
				s3c_mfc_release_alloc_mem(mfc_ctx, tmp_node);		
			}	
		}				
	}	
	s3c_mfc_merge_frag(mfc_ctx->mem_inst_no);
	
	s3c_mfc_return_mem_inst_no(mfc_ctx->mem_inst_no);
	s3c_mfc_return_inst_no(mfc_ctx->InstNo, mfc_ctx->MfcCodecType);

	s3c_mfc_openhandle_count--;
	kfree(mfc_ctx);

	ret = 0;

out_release:
	mutex_unlock(&s3c_mfc_mutex);
	return ret;
}

static int s3c_mfc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int 			ret, ex_ret;
	s3c_mfc_frame_buf_arg_t frame_buf_size;
	s3c_mfc_inst_ctx	*mfc_ctx = NULL;
	s3c_mfc_common_args	in_param;
	s3c_mfc_alloc_mem_t *node;
	int port_no = 0;
	int matched_u_addr = 0;	
	unsigned char	*start;
	int size;

	mutex_lock(&s3c_mfc_mutex);

	ret = copy_from_user(&in_param, (s3c_mfc_common_args *)arg, sizeof(s3c_mfc_common_args));
	if (ret < 0) {
		mfc_err("Inparm copy error\n");
		ret = -EIO;
		in_param.ret_code = MFCINST_ERR_INVALID_PARAM;
		goto out_ioctl;
	}

	mfc_ctx = (s3c_mfc_inst_ctx *)file->private_data;
	mutex_unlock(&s3c_mfc_mutex);
	
	switch (cmd) {	
	case IOCTL_MFC_ENC_INIT:
		
		mutex_lock(&s3c_mfc_mutex);
		mfc_debug("IOCTL_MFC_ENC_INIT\n");
		if (s3c_mfc_set_state(mfc_ctx, MFCINST_STATE_ENC_INITIALIZE) < 0) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}

		/* MFC encode init */
		in_param.ret_code = s3c_mfc_init_encode(mfc_ctx, &(in_param.args));
		if (in_param.ret_code < 0) {
			ret = in_param.ret_code;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}		

		/* Allocate MFC buffer(stream, refYC, MV) */
		in_param.ret_code = s3c_mfc_allocate_stream_ref_buf(mfc_ctx, &(in_param.args));
		if (in_param.ret_code < 0) {
			ret = in_param.ret_code;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}

		/* cache clean */			

		/* Set Ref YC0~3 & MV */
		in_param.ret_code = s3c_mfc_set_enc_ref_buffer(mfc_ctx, &(in_param.args));
		if (in_param.ret_code < 0) {
			ret = in_param.ret_code;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}	

		/* cache clean */
		
		mutex_unlock(&s3c_mfc_mutex);
		break;
	
	case IOCTL_MFC_ENC_EXE:
		
		mutex_lock(&s3c_mfc_mutex);
		if (s3c_mfc_set_state(mfc_ctx, MFCINST_STATE_ENC_EXE) < 0) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}
		#if 0	// peter for debug
		/* cache clean for Luma */
		start = (unsigned char *)in_param.args.enc_exe.in_Y_addr_vir;
		size = (int)(mfc_ctx->img_width * mfc_ctx->img_height);
		printk("start addr = 0x%08x, size = 0x%08x in IOCTL_MFC_ENC_EXE\n",start, size);
		dma_cache_maint(start, size, DMA_TO_DEVICE);

		/* cache clean for Chroma */
		start = (unsigned char *)in_param.args.enc_exe.in_CbCr_addr_vir;
		size = (int)((mfc_ctx->img_width * mfc_ctx->img_height)>>1);
		printk("start addr = 0x%08x, size = 0x%08x in IOCTL_MFC_ENC_EXE\n",start, size);
		dma_cache_maint(start, size, DMA_TO_DEVICE);
		#endif

		in_param.ret_code = s3c_mfc_exe_encode(mfc_ctx, &(in_param.args));
		//mfc_debug("InParm->ret_code : %d\n", in_param.ret_code);
		ret = in_param.ret_code;
		mutex_unlock(&s3c_mfc_mutex);
		break;
	
	case IOCTL_MFC_DEC_INIT:
		
		mutex_lock(&s3c_mfc_mutex);
		mfc_debug("IOCTL_MFC_DEC_INIT\n");
		if (s3c_mfc_set_state(mfc_ctx, MFCINST_STATE_DEC_INITIALIZE) < 0) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}
		
		/* MFC decode init */
		in_param.ret_code = s3c_mfc_init_decode(mfc_ctx, &(in_param.args));
		if (in_param.ret_code < 0) {
			ret = in_param.ret_code;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}
		
		if (in_param.args.dec_init.out_dpb_cnt <= 0) {
			mfc_err("MFC out_dpb_cnt error\n");
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}

		/* Get frame buf size */
		frame_buf_size = s3c_mfc_get_frame_buf_size(mfc_ctx, &(in_param.args));	

		/* Allocate MFC buffer(Y, C, MV) */
		in_param.ret_code = s3c_mfc_allocate_frame_buf(mfc_ctx, &(in_param.args), frame_buf_size);
		if (in_param.ret_code < 0) {
			ret = in_param.ret_code;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}		

		mutex_unlock(&s3c_mfc_mutex);
		break;

	case IOCTL_MFC_DEC_EXE:
		
		mutex_lock(&s3c_mfc_mutex);
		mfc_debug("IOCTL_MFC_DEC_EXE\n");
		if (s3c_mfc_set_state(mfc_ctx, MFCINST_STATE_DEC_EXE) < 0) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}

		in_param.ret_code = s3c_mfc_exe_decode(mfc_ctx, &(in_param.args));
		ret = in_param.ret_code;
		mutex_unlock(&s3c_mfc_mutex);
		break;

	case IOCTL_MFC_GET_CONFIG:
		
		mutex_lock(&s3c_mfc_mutex);
		if (mfc_ctx->MfcState < MFCINST_STATE_DEC_INITIALIZE) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
			mutex_unlock(&s3c_mfc_mutex);
			break;
		}

		in_param.ret_code = s3c_mfc_get_config(mfc_ctx, &(in_param.args));
		ret = in_param.ret_code;
		mutex_unlock(&s3c_mfc_mutex);
		break;

	case IOCTL_MFC_SET_CONFIG:
		
		mutex_lock(&s3c_mfc_mutex);
		in_param.ret_code = s3c_mfc_set_config(mfc_ctx, &(in_param.args));
		ret = in_param.ret_code;
		mutex_unlock(&s3c_mfc_mutex);
		break;

	case IOCTL_MFC_GET_IN_BUF:
	
		if (mfc_ctx->MfcState < MFCINST_STATE_OPENED) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;

			break;
		}		
		/* allocate stream buf for decoder & current YC buf for encoder */
		if ((in_param.args.mem_alloc.codec_type == H264_ENC) || 
			(in_param.args.mem_alloc.codec_type == H263_ENC) ||
			(in_param.args.mem_alloc.codec_type == MPEG4_ENC)) 
			mfc_ctx->port_no = 1;
		
		else 
			mfc_ctx->port_no = 0;		
		
		in_param.args.mem_alloc.buff_size = Align(in_param.args.mem_alloc.buff_size, 2*BUF_L_UNIT);
		in_param.ret_code = s3c_mfc_get_virt_addr(mfc_ctx, &(in_param.args));
		
		ret = in_param.ret_code;

		break;

	case IOCTL_MFC_FREE_BUF:

		if (mfc_ctx->MfcState < MFCINST_STATE_OPENED) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;

			break;
		}	

		for (port_no=0; port_no < MFC_MAX_PORT_NUM; port_no++) {			
			for(node = s3c_mfc_alloc_mem_head[port_no]; node != s3c_mfc_alloc_mem_tail[port_no]; 
				node = node->next) {
				if(node->u_addr == (unsigned char *)in_param.args.mem_free.u_addr) {
					matched_u_addr = 1;
					break;
				}	
			}
			if (matched_u_addr)
				break;			
		}	
		if (node == s3c_mfc_alloc_mem_tail[port_no]) {
			mfc_err("invalid virtual address(0x%x)\r\n", in_param.args.mem_free.u_addr);
			ret = MFCINST_MEMORY_INVAILD_ADDR;			
		}
		mfc_ctx->port_no = port_no;
		
		in_param.ret_code = s3c_mfc_release_alloc_mem(mfc_ctx, node);
		ret = in_param.ret_code;

		break;

	case IOCTL_MFC_GET_PHYS_ADDR:

		if (mfc_ctx->MfcState < MFCINST_STATE_OPENED) {
			mfc_err("MFCINST_ERR_STATE_INVALID\n");
			in_param.ret_code = MFCINST_ERR_STATE_INVALID;
			ret = -EINVAL;
	
			break;
		}

		in_param.ret_code = s3c_mfc_get_phys_addr(mfc_ctx, &(in_param.args));
		ret = in_param.ret_code;

		break;

	default:
		
		mfc_err("Requested ioctl command is not defined. (ioctl cmd=0x%08x)\n", cmd);
		in_param.ret_code  = MFCINST_ERR_INVALID_PARAM;
		ret = -EINVAL;

	}


out_ioctl:
	ex_ret = copy_to_user((s3c_mfc_common_args *)arg, &in_param, sizeof(s3c_mfc_common_args));
	if (ex_ret < 0) {
		mfc_err("Outparm copy to user error\n");
		ret = -EIO;
	}

	mfc_debug("---------------IOCTL return = %d ---------------\n", ret);
	
	return ret;
}

static int s3c_mfc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long vir_size	= vma->vm_end - vma->vm_start;
	unsigned long phy_size, offset;
	unsigned long mem0_size, mem1_size;
	unsigned long pageFrameNo = 0;

	mfc_debug("vma->vm_start = 0x%08x, vma->vm_end = 0x%08x\n", vma->vm_start, vma->vm_end);
	mfc_debug("vma->vm_end - vma->vm_start = %d\n", vir_size);

	offset = s3c_mfc_get_data_buf_phys_addr() - s3c_mfc_phys_buf;
	offset = Align(offset, 4*BUF_L_UNIT);

	phy_size = (unsigned long)(s3c_get_media_memsize(S3C_MDEV_MFC) - offset
					+ s3c_get_media_memsize_node(S3C_MDEV_MFC, 1)); 
					
	/* if memory size required from appl. mmap() is bigger than max data memory 
 	 * size allocated in the driver
	 */ 
	if (vir_size > phy_size) {
		mfc_err("virtual requested mem(%d) is bigger than physical mem(%d)\n", 
			vir_size, phy_size);
		return -EINVAL; 
	}
	vir_mmap_size = vir_size;
	
	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);		
	
	/*
	 * port0 mapping for stream buf & frame buf (chroma + MV)
	 */
	pageFrameNo = __phys_to_pfn(s3c_mfc_get_data_buf_phys_addr());		
	if( remap_pfn_range(vma, vma->vm_start, pageFrameNo, vir_size/2, vma->vm_page_prot) ) {
		mfc_err("mfc remap port0 error\n");
		return -EAGAIN;
	}	

	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);	

	/*
	 * port1 mapping for frame buf (luma)
	 */
	pageFrameNo = __phys_to_pfn(s3c_mfc_get_dpb_luma_buf_phys_addr());
	if( remap_pfn_range(vma, vma->vm_start+vir_size/2, pageFrameNo, vir_size/2, vma->vm_page_prot) ) {
		mfc_err("mfc remap port1 error\n");
		return -EAGAIN;
	}	

	mfc_debug("virtual requested mem = %d, physical reserved data mem = %d\n", vir_size, phy_size);

	return 0;

}

static struct file_operations s3c_mfc_fops = {
	.owner		= THIS_MODULE,
	.open		= s3c_mfc_open,
	.release	= s3c_mfc_release,
	.ioctl		= s3c_mfc_ioctl,
	.mmap		= s3c_mfc_mmap
};


static struct miscdevice s3c_mfc_miscdev = {
	.minor		= 252,
	.name		= "s3c-mfc",
	.fops		= &s3c_mfc_fops,
};

static irqreturn_t s3c_mfc_irq(int irq, void *dev_id)
{
	unsigned int	intReason;

	intReason = readl(s3c_mfc_sfr_virt_base + S3C_FIMV_RISC2HOST_CMD) & 0x1FFFF;
	mfc_debug("Interrupt !! : %d\n", intReason);
	
	if (((intReason & R2H_CMD_FRAME_DONE_RET) == R2H_CMD_FRAME_DONE_RET)
		||((intReason & R2H_CMD_SEQ_DONE_RET) == R2H_CMD_SEQ_DONE_RET)
		||((intReason & R2H_CMD_SYS_INIT_RET) == R2H_CMD_SYS_INIT_RET)
		||((intReason & R2H_CMD_OPEN_INSTANCE_RET) == R2H_CMD_OPEN_INSTANCE_RET)
		||((intReason & R2H_CMD_CLOSE_INSTANCE_RET) == R2H_CMD_CLOSE_INSTANCE_RET)) {
		writel(0, s3c_mfc_sfr_virt_base + S3C_FIMV_RISC_HOST_INT);
		writel(0, s3c_mfc_sfr_virt_base + S3C_FIMV_RISC2HOST_CMD);
		s3c_mfc_int_type = intReason;
		wake_up_interruptible(&s3c_mfc_wait_queue);
	}		
	writel(0, s3c_mfc_sfr_virt_base + S3C_FIMV_RISC_HOST_INT);
	writel(0, s3c_mfc_sfr_virt_base + S3C_FIMV_RISC2HOST_CMD);

	if (((intReason & R2H_CMD_FRAME_DONE_RET) == R2H_CMD_FRAME_DONE_RET)
		||((intReason & R2H_CMD_SEQ_DONE_RET) == R2H_CMD_SEQ_DONE_RET)
		||((intReason & R2H_CMD_DECODE_ERR_RET) == R2H_CMD_DECODE_ERR_RET)) {
		
		writel(0xffff, s3c_mfc_sfr_virt_base + S3C_FIMV_SI_RTN_CHID);
		
	}	

	return IRQ_HANDLED;
}


static int s3c_mfc_probe(struct platform_device *pdev)
{
	struct resource *res;
	size_t		size;
	int 		ret;

	/* mfc clock enable should be here */	

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		ret = -ENOENT;
		goto probe_out;
	}

	// 60K is required for mfc register (0x0 ~ 0xe008)
	size = (res->end - res->start) + 1;
	s3c_mfc_mem = request_mem_region(res->start, size, pdev->name);
	if (s3c_mfc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto probe_out;
	}

	s3c_mfc_sfr_virt_base = ioremap(s3c_mfc_mem->start, s3c_mfc_mem->end - s3c_mfc_mem->start + 1);
	if (s3c_mfc_sfr_virt_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address region\n");
		ret = -ENOENT;
		goto probe_out;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto probe_out;
	}

	ret = request_irq(res->start, s3c_mfc_irq, IRQF_DISABLED, pdev->name, pdev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to install irq (%d)\n", ret);
		goto probe_out;
	}

	mutex_init(&s3c_mfc_mutex);

	/* 
	 * buffer memory secure 
	 */
	s3c_mfc_phys_buf = s3c_get_media_memory(S3C_MDEV_MFC);
	s3c_mfc_phys_buf = Align(s3c_mfc_phys_buf, 128*BUF_L_UNIT);
	s3c_mfc_virt_buf = phys_to_virt(s3c_mfc_phys_buf);
	if (s3c_mfc_virt_buf == NULL) {
		mfc_err("fail to mapping port0 buffer\n");
		ret = -EPERM;
		goto probe_out;
	}
	
	s3c_mfc_phys_dpb_luma_buf = s3c_get_media_memory_node(S3C_MDEV_MFC, 1);
	s3c_mfc_phys_dpb_luma_buf = Align(s3c_mfc_phys_dpb_luma_buf, 128*BUF_L_UNIT);
	s3c_mfc_virt_dpb_luma_buf = phys_to_virt(s3c_mfc_phys_dpb_luma_buf);
	if (s3c_mfc_virt_dpb_luma_buf == NULL) {
		mfc_err("fail to mapping port1 buffer\n");
		ret = -EPERM;
		goto probe_out;
	}

	printk("s3c_mfc_phys_buf = 0x%08x, s3c_mfc_phys_dpb_luma_buf = 0x%08x <<\n", 
		s3c_mfc_phys_buf, s3c_mfc_phys_dpb_luma_buf);
	printk("s3c_mfc_virt_buf = 0x%08x, s3c_mfc_virt_dpb_luma_buf = 0x%08x <<\n", 
		s3c_mfc_virt_buf, s3c_mfc_virt_dpb_luma_buf);
	
	/*
	 * MFC FW downloading
	 */	
	if (s3c_mfc_load_firmware() < 0) {
		mfc_err("MFCINST_ERR_FW_INIT_FAIL\n");
		ret = -EPERM;
		goto probe_out;
	}	

	s3c_mfc_init_mem_inst_no();

	s3c_mfc_init_buffer_manager();

	ret = misc_register(&s3c_mfc_miscdev);
	
	return 0;

probe_out:
	dev_err(&pdev->dev, "not found (%d). \n", ret);
	return ret;
}

static int s3c_mfc_remove(struct platform_device *pdev)
{
	iounmap(s3c_mfc_sfr_virt_base);
	iounmap(s3c_mfc_virt_buf);

	/* remove memory region */
	if (s3c_mfc_mem != NULL) {
		release_resource(s3c_mfc_mem);
		kfree(s3c_mfc_mem);
		s3c_mfc_mem = NULL;
	}

	free_irq(IRQ_MFC, pdev);

	mutex_destroy(&s3c_mfc_mutex);

	misc_deregister(&s3c_mfc_miscdev);

	return 0;
}

static int s3c_mfc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int s3c_mfc_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver s3c_mfc_driver = {
	.probe		= s3c_mfc_probe,
	.remove		= s3c_mfc_remove,
	.shutdown	= NULL,
	.suspend	= s3c_mfc_suspend,
	.resume		= s3c_mfc_resume,

	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c-mfc",
	},
};

static char banner[] __initdata = KERN_INFO "S5PC110 MFC Driver, (c) 2009 Samsung Electronics\n";

static int __init s3c_mfc_init(void)
{
	mfc_info("%s", banner);

	if (platform_driver_register(&s3c_mfc_driver) != 0) {
		printk(KERN_ERR "platform device registration failed.. \n");
		return -1;
	}

	return 0;
}

static void __exit s3c_mfc_exit(void)
{
	platform_driver_unregister(&s3c_mfc_driver);
	mfc_info("S5PC110 MFC Driver exit.\n");
}

module_init( s3c_mfc_init );
module_exit( s3c_mfc_exit );

MODULE_AUTHOR("Jaeryul, Oh");
MODULE_DESCRIPTION("S3C MFC (Multi Function Codec - FIMV5.0) Device Driver");
MODULE_LICENSE("GPL");

