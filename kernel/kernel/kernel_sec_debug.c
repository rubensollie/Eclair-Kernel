/* kernel_sec_debug.c
 *
 * Exception handling in kernel by SEC
 *
 * Copyright (c) 2010 Samsung Electronics
 *                http://www.samsung.com/
 *
 */

#ifdef CONFIG_KERNEL_DEBUG_SEC

#include <linux/kernel_sec_common.h>
#include <asm/cacheflush.h>           // cacheflush
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include <linux/file.h>
#include <mach/hardware.h>
#include <mach/param.h>

/*
 *  Variable
 */

const char* gkernel_sec_build_info_date_time[] =
{
	__DATE__,
	__TIME__   
};

#define DEBUG_LEVEL_FILE_NAME	"/mnt/.lfs/debug_level.inf"
#define DEBUG_LEVEL_RD	0
#define DEBUG_LEVEL_WR	1
static int debuglevel;

char gkernel_sec_build_info[100];
//extern unsigned int HWREV;  
unsigned char  kernel_sec_cause_str[KERNEL_SEC_DEBUG_CAUSE_STR_LEN];

/*
 *  Function
 */

void __iomem * kernel_sec_viraddr_wdt_reset_reg;
__used t_kernel_sec_arm_core_regsiters kernel_sec_core_reg_dump;
__used t_kernel_sec_mmu_info           kernel_sec_mmu_reg_dump;
__used kernel_sec_upload_cause_type     gkernel_sec_upload_cause;

#define DPRAM_START_ADDRESS_PHYS 		0x40000000
#define DPRAM_SHARED_BANK				0x05000000
#define DPRAM_SHARED_BANK_SIZE			0x01000000
#define DPRAM_SFR_SIZE					0x800

volatile void __iomem *dpram_sfr_base = 0;
volatile unsigned int *onedram_sem;
volatile unsigned int *onedram_mailboxAB;		//received mail
volatile unsigned int *onedram_mailboxBA;		//send mail
unsigned int received_cp_ack = 0;

void kernel_sec_set_cp_upload(void)
{
	unsigned int send_mail, wait_count;

	send_mail = KERNEL_SEC_DUMP_AP_DEAD_INDICATOR;

	*onedram_sem = 0x0;
	*onedram_mailboxBA = send_mail;

	printk(KERN_EMERG"[kernel_sec_dump_set_cp_upload] set cp upload mode, MailboxBA 0x%8x\n", send_mail);
    
       wait_count = 0;
	received_cp_ack = 0;
	while(1)
	{
		if(received_cp_ack == 1)
		{
			printk(KERN_EMERG"  - Done.\n");
			break;
		}
		mdelay(10);
		if(++wait_count > 500)
		{
			printk(KERN_EMERG"  - Fail to set CP uploadmode.\n");
			break;
		}	
	}
	printk(KERN_EMERG" modem_wait_count : %d \n", wait_count);
}
EXPORT_SYMBOL(kernel_sec_set_cp_upload);


void kernel_sec_set_cp_ack(void)   //is set by dpram - dpram_irq_handler
{
	received_cp_ack = 1;
}
EXPORT_SYMBOL(kernel_sec_set_cp_ack);


void kernel_sec_set_upload_magic_number(void)
{
	int *magic_virt_addr = (int*) LOKE_BOOT_USB_DWNLD_V_ADDR;

	if( (KERNEL_SEC_DEBUG_LEVEL_MID == kernel_sec_get_debug_level()) || 
		  (KERNEL_SEC_DEBUG_LEVEL_HIGH == kernel_sec_get_debug_level()) )
	{
		*magic_virt_addr = LOKE_BOOT_USB_DWNLDMAGIC_NO; // SET
		printk(KERN_EMERG"KERNEL:magic_number=0x%x SET_UPLOAD_MAGIC_NUMBER\n",*magic_virt_addr);
	}
	else
	{
		*magic_virt_addr = 0;	
		printk(KERN_EMERG"KERNEL:magic_number=0x%x DEBUG LEVEL low!!\n",*magic_virt_addr);
	}
}
EXPORT_SYMBOL(kernel_sec_set_upload_magic_number);


void kernel_sec_get_debug_level_from_boot(void)
{
	unsigned int temp;
	temp = __raw_readl(S5P_INFORM6);
	temp &= KERNEL_SEC_DEBUG_LEVEL_MASK;
	temp = temp >> KERNEL_SEC_DEBUG_LEVEL_BIT;

	if( temp == 0x0 )  //low
		debuglevel = KERNEL_SEC_DEBUG_LEVEL_LOW;
	else if( temp == 0x1 )  //mid
		debuglevel = KERNEL_SEC_DEBUG_LEVEL_MID;
	else if( temp == 0x2 )  //high
		debuglevel = KERNEL_SEC_DEBUG_LEVEL_HIGH;
	else
	{
		printk(KERN_EMERG"KERNEL:kernel_sec_get_debug_level_from_boot (reg value is incorrect.)\n");
		debuglevel = KERNEL_SEC_DEBUG_LEVEL_LOW;
	}

	printk(KERN_EMERG"KERNEL:kernel_sec_get_debug_level_from_boot=0x%x\n",debuglevel);
}


void kernel_sec_clear_upload_magic_number(void)
{
	int *magic_virt_addr = (int*) LOKE_BOOT_USB_DWNLD_V_ADDR;

	*magic_virt_addr = 0;  // CLEAR
	printk(KERN_EMERG"KERNEL:magic_number=0x%x CLEAR_UPLOAD_MAGIC_NUMBER\n",*magic_virt_addr);
}
EXPORT_SYMBOL(kernel_sec_clear_upload_magic_number);

void kernel_sec_map_wdog_reg(void)
{
	/* Virtual Mapping of Watchdog register */
	kernel_sec_viraddr_wdt_reset_reg = ioremap_nocache(S3C_PA_WDT,0x400);

	if (kernel_sec_viraddr_wdt_reset_reg == NULL)
	{
		printk(KERN_EMERG"Failed to ioremap() region in forced upload keystring\n");
	}
}
EXPORT_SYMBOL(kernel_sec_map_wdog_reg);

void kernel_sec_set_upload_cause(kernel_sec_upload_cause_type uploadType)
{
	gkernel_sec_upload_cause=uploadType;
	unsigned int temp;
	
	temp = __raw_readl(S5P_INFORM6);
	temp |= uploadType;                // KERNEL_SEC_UPLOAD_CAUSE_MASK    0x000000FF
	__raw_writel( temp , S5P_INFORM6);
	
	printk(KERN_EMERG"(kernel_sec_set_upload_cause) : upload_cause set 0x%x\n",uploadType);	
}
EXPORT_SYMBOL(kernel_sec_set_upload_cause);

void kernel_sec_set_cause_strptr(unsigned char* str_ptr, int size)
{
	unsigned int temp;

	memset((void *)kernel_sec_cause_str, 0, sizeof(kernel_sec_cause_str));
	memcpy(kernel_sec_cause_str, str_ptr, size);
	
	temp = virt_to_phys(kernel_sec_cause_str);
	__raw_writel(temp, LOKE_BOOT_USB_DWNLD_V_ADDR+4); //loke read this ptr, display_aries_upload_image
}
EXPORT_SYMBOL(kernel_sec_set_cause_strptr);


void kernel_sec_set_autotest(void)
{
	unsigned int temp;

	temp = __raw_readl(S5P_INFORM6);
	temp |= 1<<KERNEL_SEC_UPLOAD_AUTOTEST_BIT;
	__raw_writel( temp , S5P_INFORM6 );	
}
EXPORT_SYMBOL(kernel_sec_set_autotest);

void kernel_sec_set_build_info(void)
{
	char * p = gkernel_sec_build_info;
	sprintf(p,"APOLLO KERNEL: ");
	strcat(p, " Date:");
	strcat(p, gkernel_sec_build_info_date_time[0]);
	strcat(p, " Time:");
	strcat(p, gkernel_sec_build_info_date_time[1]);
}
EXPORT_SYMBOL(kernel_sec_set_build_info);

void kernel_sec_init(void)
{
       // set the onedram mailbox virtual address
       dpram_sfr_base = ioremap_nocache(DPRAM_START_ADDRESS_PHYS + DPRAM_SHARED_BANK - DPRAM_SFR_SIZE, DPRAM_SFR_SIZE);
	onedram_sem =  dpram_sfr_base;
	onedram_mailboxAB = dpram_sfr_base + 0x20;
	onedram_mailboxBA = dpram_sfr_base + 0x40;

	kernel_sec_get_debug_level_from_boot();
#if 0 // Temporary block checking panic while kernel's init process
	kernel_sec_set_upload_magic_number();
	kernel_sec_set_upload_cause(UPLOAD_CAUSE_INIT);	
#endif
	kernel_sec_map_wdog_reg();
}
EXPORT_SYMBOL(kernel_sec_init);

/* core reg dump function*/
void kernel_sec_get_core_reg_dump(t_kernel_sec_arm_core_regsiters* regs)
{
	asm(
		// we will be in SVC mode when we enter this function. Collect SVC registers along with cmn registers.
		"str r0, [%0,#0] \n\t"		// R0
		"str r1, [%0,#4] \n\t"		// R1
		"str r2, [%0,#8] \n\t"		// R2
		"str r3, [%0,#12] \n\t"		// R3
		"str r4, [%0,#16] \n\t"		// R4
		"str r5, [%0,#20] \n\t"		// R5
		"str r6, [%0,#24] \n\t"		// R6
		"str r7, [%0,#28] \n\t"		// R7
		"str r8, [%0,#32] \n\t"		// R8
		"str r9, [%0,#36] \n\t"		// R9
		"str r10, [%0,#40] \n\t"	// R10
		"str r11, [%0,#44] \n\t"	// R11
		"str r12, [%0,#48] \n\t"	// R12

		/* SVC */
		"str r13, [%0,#52] \n\t"	// R13_SVC
		"str r14, [%0,#56] \n\t"	// R14_SVC
		"mrs r1, spsr \n\t"			// SPSR_SVC
		"str r1, [%0,#60] \n\t"

		/* PC and CPSR */
		"sub r1, r15, #0x4 \n\t"	// PC
		"str r1, [%0,#64] \n\t"	
		"mrs r1, cpsr \n\t"			// CPSR
		"str r1, [%0,#68] \n\t"

		/* SYS/USR */
		"mrs r1, cpsr \n\t"			// switch to SYS mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1f \n\t"
		"msr cpsr,r1 \n\t"

		"str r13, [%0,#72] \n\t"	// R13_USR
		"str r14, [%0,#76] \n\t"	// R13_USR

		/*FIQ*/
		"mrs r1, cpsr \n\t"			// switch to FIQ mode
		"and r1,r1,#0xFFFFFFE0\n\t"
		"orr r1,r1,#0x11\n\t"
		"msr cpsr,r1 \n\t"

		"str r8, [%0,#80] \n\t"		// R8_FIQ
		"str r9, [%0,#84] \n\t"		// R9_FIQ
		"str r10, [%0,#88] \n\t"	// R10_FIQ
		"str r11, [%0,#92] \n\t"	// R11_FIQ
		"str r12, [%0,#96] \n\t"	// R12_FIQ
		"str r13, [%0,#100] \n\t"	// R13_FIQ
		"str r14, [%0,#104] \n\t"	// R14_FIQ
		"mrs r1, spsr \n\t"			// SPSR_FIQ
		"str r1, [%0,#108] \n\t"

		/*IRQ*/
		"mrs r1, cpsr \n\t"			// switch to IRQ mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x12\n\t"
		"msr cpsr,r1 \n\t"

		"str r13, [%0,#112] \n\t"	// R13_IRQ
		"str r14, [%0,#116] \n\t"	// R14_IRQ
		"mrs r1, spsr \n\t"			// SPSR_IRQ
		"str r1, [%0,#120] \n\t"

		/*MON*/
		"mrs r1, cpsr \n\t"			// switch to monitor mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x16\n\t"
		"msr cpsr,r1 \n\t"

		"str r13, [%0,#124] \n\t"	// R13_MON
		"str r14, [%0,#128] \n\t"	// R14_MON
		"mrs r1, spsr \n\t"			// SPSR_MON
		"str r1, [%0,#132] \n\t"

		/*ABT*/
		"mrs r1, cpsr \n\t"			// switch to Abort mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x17\n\t"
		"msr cpsr,r1 \n\t"

		"str r13, [%0,#136] \n\t"	// R13_ABT
		"str r14, [%0,#140] \n\t"	// R14_ABT
		"mrs r1, spsr \n\t"			// SPSR_ABT
		"str r1, [%0,#144] \n\t"

		/*UND*/
		"mrs r1, cpsr \n\t"			// switch to undef mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x1B\n\t"
		"msr cpsr,r1 \n\t"

		"str r13, [%0,#148] \n\t"	// R13_UND
		"str r14, [%0,#152] \n\t"	// R14_UND
		"mrs r1, spsr \n\t"			// SPSR_UND
		"str r1, [%0,#156] \n\t"

		/* restore to SVC mode */
		"mrs r1, cpsr \n\t"			// switch to undef mode
		"and r1, r1, #0xFFFFFFE0\n\t"
		"orr r1, r1, #0x13\n\t"
		"msr cpsr,r1 \n\t"
		
		:				/* output */
        :"r"(regs)    	/* input */
        :"%r1"     		/* clobbered register */
        );

	return;	
}
EXPORT_SYMBOL(kernel_sec_get_core_reg_dump);

int kernel_sec_get_mmu_reg_dump(t_kernel_sec_mmu_info *mmu_info)
{
	asm("mrc    p15, 0, r1, c1, c0, 0 \n\t"	//SCTLR
		"str r1, [%0] \n\t"
		"mrc    p15, 0, r1, c2, c0, 0 \n\t"	//TTBR0
		"str r1, [%0,#4] \n\t"
		"mrc    p15, 0, r1, c2, c0,1 \n\t"	//TTBR1
		"str r1, [%0,#8] \n\t"
		"mrc    p15, 0, r1, c2, c0,2 \n\t"	//TTBCR
		"str r1, [%0,#12] \n\t"
		"mrc    p15, 0, r1, c3, c0,0 \n\t"	//DACR
		"str r1, [%0,#16] \n\t"
		"mrc    p15, 0, r1, c5, c0,0 \n\t"	//DFSR
		"str r1, [%0,#20] \n\t"
		"mrc    p15, 0, r1, c6, c0,0 \n\t"	//DFAR
		"str r1, [%0,#24] \n\t"
		"mrc    p15, 0, r1, c5, c0,1 \n\t"	//IFSR
		"str r1, [%0,#28] \n\t"
		"mrc    p15, 0, r1, c6, c0,2 \n\t"	//IFAR
		"str r1, [%0,#32] \n\t"
		/*Dont populate DAFSR and RAFSR*/
		"mrc    p15, 0, r1, c10, c2,0 \n\t"	//PMRRR
		"str r1, [%0,#44] \n\t"
		"mrc    p15, 0, r1, c10, c2,1 \n\t"	//NMRRR
		"str r1, [%0,#48] \n\t"
		"mrc    p15, 0, r1, c13, c0,0 \n\t"	//FCSEPID
		"str r1, [%0,#52] \n\t"
		"mrc    p15, 0, r1, c13, c0,1 \n\t"	//CONTEXT
		"str r1, [%0,#56] \n\t"
		"mrc    p15, 0, r1, c13, c0,2 \n\t"	//URWTPID
		"str r1, [%0,#60] \n\t"
		"mrc    p15, 0, r1, c13, c0,3 \n\t"	//UROTPID
		"str r1, [%0,#64] \n\t"
		"mrc    p15, 0, r1, c13, c0,4 \n\t"	//POTPIDR
		"str r1, [%0,#68] \n\t"
		:					/* output */
        :"r"(mmu_info)    /* input */
        :"%r1","memory"         /* clobbered register */
        ); 
	return 0;
}
EXPORT_SYMBOL(kernel_sec_get_mmu_reg_dump);

void kernel_sec_save_final_context(void)
{
	if(	kernel_sec_get_mmu_reg_dump(&kernel_sec_mmu_reg_dump) < 0)
	{
		printk(KERN_EMERG"(kernel_sec_save_final_context) kernel_sec_get_mmu_reg_dump faile.\n");
	}
	kernel_sec_get_core_reg_dump(&kernel_sec_core_reg_dump);

	printk(KERN_EMERG "(kernel_sec_save_final_context) Final context was saved before the system reset.\n");
}
EXPORT_SYMBOL(kernel_sec_save_final_context);


/*
 *  bSilentReset
 *    TRUE  : Silent reset - clear the magic code.
 *    FALSE : Reset to upload mode - not clear the magic code.
 *
 *  TODO : DebugLevel consideration should be added.
 */
extern void usb_switch_mode(int usb_mode);
extern void arch_reset(char mode);
void kernel_sec_hw_reset(bool bSilentReset)
{

	if (bSilentReset || (KERNEL_SEC_DEBUG_LEVEL_LOW == kernel_sec_get_debug_level()))
	{
		kernel_sec_clear_upload_magic_number();
		printk(KERN_EMERG "(kernel_sec_hw_reset) Upload Magic Code is cleared for silet reset.\n");
	} else {
		usb_switch_mode(1); // 1 : AP ,  2: CP
		kernel_sec_set_upload_magic_number();
	}

	printk(KERN_EMERG "(kernel_sec_hw_reset) %s\n", gkernel_sec_build_info);
	
	printk(KERN_EMERG "(kernel_sec_hw_reset) The forced reset was called. The system will be reset !!\n");

	/* flush cache back to ram */
	flush_cache_all();
	
	__raw_writel(0x8000,kernel_sec_viraddr_wdt_reset_reg +0x4 );
	__raw_writel(0x1,   kernel_sec_viraddr_wdt_reset_reg +0x4 );
	__raw_writel(0x8,   kernel_sec_viraddr_wdt_reset_reg +0x8 );
	__raw_writel(0x8021,kernel_sec_viraddr_wdt_reset_reg);

	arch_reset('r');
    /* Never happened because the reset will occur before this. */
	while(1);	
}
EXPORT_SYMBOL(kernel_sec_hw_reset);

#if 0
int kernel_sec_lfs_debug_level_op(int dir, int flags)
{
	struct file *filp;
	mm_segment_t fs;

	int ret;

	filp = filp_open(DEBUG_LEVEL_FILE_NAME, flags, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __FUNCTION__,
				PTR_ERR(filp));

		return -1;
	}

	fs = get_fs();
	set_fs(get_ds());

	if (dir == DEBUG_LEVEL_RD)
		ret = filp->f_op->read(filp, (char __user *)&debuglevel,
				sizeof(int), &filp->f_pos);
	else
		ret = filp->f_op->write(filp, (char __user *)&debuglevel,
				sizeof(int), &filp->f_pos);

	set_fs(fs);
	filp_close(filp, NULL);

	return ret;
}
#endif

bool kernel_sec_set_debug_level(int level)
{
	int ret;
	
		if( ( level == KERNEL_SEC_DEBUG_LEVEL_LOW ) ||
				( level == KERNEL_SEC_DEBUG_LEVEL_MID ) ||
				( level == KERNEL_SEC_DEBUG_LEVEL_HIGH ) )
		{
			debuglevel = level;
			/* write to param */
			SEC_SET_PARAM(__KERNEL_SEC_DEBUG_LEVEL, debuglevel);

			/* write to regiter (magic code) */
			kernel_sec_set_upload_magic_number();
				
			printk(KERN_NOTICE "(kernel_sec_set_debug_level) The debug value is 0x%x !!\n", level);	
			return 1;
		}
		else
		{
			printk(KERN_NOTICE "(kernel_sec_set_debug_level) The debug value is \
					invalid(0x%x)!! Set default level(LOW)\n", level);	
			debuglevel = KERNEL_SEC_DEBUG_LEVEL_LOW;
			return 0;
		}
	}
EXPORT_SYMBOL(kernel_sec_set_debug_level);

int kernel_sec_get_debug_level_from_param()
{
	int ret;

	SEC_GET_PARAM(__KERNEL_SEC_DEBUG_LEVEL, debuglevel);
	
	if( ( debuglevel == KERNEL_SEC_DEBUG_LEVEL_LOW ) ||
			( debuglevel == KERNEL_SEC_DEBUG_LEVEL_MID ) ||
			( debuglevel == KERNEL_SEC_DEBUG_LEVEL_HIGH ) )
	{
		/* return debug level */
		printk(KERN_NOTICE "(kernel_sec_get_debug_level_from_param) kernel debug level is 0x%x !!\n", debuglevel);
		return debuglevel;
	}
	else
	{
		/*In case of invalid debug level, default (debug level low)*/
		printk(KERN_NOTICE "(kernel_sec_get_debug_level_from_param) The debug value is \
				invalid(0x%x)!! Set default level(LOW)\n", debuglevel);	
		debuglevel = KERNEL_SEC_DEBUG_LEVEL_LOW;
	}
	return debuglevel;
}
EXPORT_SYMBOL(kernel_sec_get_debug_level_from_param);

int kernel_sec_get_debug_level()
{
	return debuglevel;
}
EXPORT_SYMBOL(kernel_sec_get_debug_level);

int kernel_sec_check_debug_level_high(void)
{
	if( KERNEL_SEC_DEBUG_LEVEL_HIGH == kernel_sec_get_debug_level() )
		return 1;
	return 0;
}
EXPORT_SYMBOL(kernel_sec_check_debug_level_high);

#endif // CONFIG_KERNEL_DEBUG_SEC
