/* drivers/rtc/rtc-s3c.c
 *
 * Copyright (c) 2004,2006 Simtec Electronics
 *	Ben Dooks, <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410/S3C2440/S3C24XX Internal RTC Driver
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/log2.h>

#include <mach/hardware.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/time.h>
#include <linux/delay.h>

#include <plat/regs-rtc.h>

#define SET_RTC_DEFAULT_RESET_TIME

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
#include <linux/kernel_stat.h>
#endif /* CONFIG_SYNC_RTC */


#ifdef SET_RTC_DEFAULT_RESET_TIME
#define DEFAULT_RESET_TIME_YEAR 	(2000)
#define DEFAULT_RESET_TIME_MON	 	(1)
#define DEFAULT_RESET_TIME_DATE	 	(1)
#define DEFAULT_RESET_TIME_HOUR 	(0)
#define DEFAULT_RESET_TIME_MIN 		(0)
#define DEFAULT_RESET_TIME_SEC	 	(0)
#endif /* SET_RTC_DEFAULT_RESET_TIME */

/* I have yet to find an S3C implementation with more than one
 * of these rtc blocks in */

static struct resource *s3c_rtc_mem;

static void __iomem *s3c_rtc_base;
static int s3c_rtc_alarmno = NO_IRQ;
static int s3c_rtc_tickno  = NO_IRQ;
static int s3c_rtc_freq    = 1;

static DEFINE_SPINLOCK(s3c_rtc_pie_lock);
static unsigned int tick_count;

#ifdef CONFIG_RTC_DRV_MAX8998
int max8998_rtc_set_time(struct rtc_time *tm);
int max8998_rtc_read_time(struct rtc_time *tm);
#endif

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
#define CONFIG_RTC_S3C_SYNC_SYSTEM_TIME_DEBUG

static int s3c_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm);

#define RTC_SYNC_SAVE_DELTA_INTERVAL	(HZ*30)		/* 30 seconds */
#define RTC_SYNC_ADJUST_INTERVAL	(HZ*60*60)	/* 1 hour */
#define RTC_SYNC_DETECT_IDLE_INTERVAL 	(HZ*5)		/* 5 seconds */
#define RTC_SYNC_AFTER_BUSY_INTERVAL	(HZ*55)		/* 55 seconds */

#define RTC_SYNC_FORCE_INTERVAL		(HZ*60*60*4)	/* 4 hours */
#define RTC_SYNC_MAX_BUSY_COUNT		(RTC_SYNC_FORCE_INTERVAL / (RTC_SYNC_DETECT_IDLE_INTERVAL + RTC_SYNC_AFTER_BUSY_INTERVAL))
#define RTC_SYNC_IDLE_PERCENT		(80)

enum rtc_sync_state {RS_SAVE_DELTA, RS_WAIT_ADJUST_TIME, RS_WAIT_ADJUST_TIME_AFTER_BUSY, RS_DETECT_IDLE, RS_TRY_ADJUST};

static void rtc_sync_work_handler(struct work_struct * __unused);
static DECLARE_DELAYED_WORK(rtc_sync_work, rtc_sync_work_handler);
static void rtc_sync_start (void);
static void rtc_sync_start_save_delta(void);

static struct timespec		ts_saved_delta;
static enum rtc_sync_state   	rtc_sync_state;

static inline int detect_cpu_idle (unsigned int old_idle_tick)
{
	unsigned int idle_tick = kstat_cpu(0).cpustat.idle + kstat_cpu(0).cpustat.iowait;
	unsigned int unit_idle_tick = idle_tick - old_idle_tick;
	int	     state = false;
	
	old_idle_tick = idle_tick;

	if ( unit_idle_tick > (RTC_SYNC_DETECT_IDLE_INTERVAL * RTC_SYNC_IDLE_PERCENT / 100))
	{
		state = true;
	}

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME_DEBUG
	printk ("RTC_SYNC: %s idle_tick:%d\n", (state == true)?"<idle>":"<busy>",  unit_idle_tick);
#endif
	return state;
}

static void rtc_sync_adjust(void)
{
	struct rtc_time		rtc_time;
	static time_t		rtc_time_t;
	struct timespec		ts_system, ts_rtc, ts_delta, ts_delta_delta;

	getnstimeofday(&ts_system);
	s3c_rtc_gettime(NULL, &rtc_time);

	rtc_tm_to_time(&rtc_time, &rtc_time_t);
	/* RTC precision is 1 second; adjust delta for avg 1/2 sec err */
	set_normalized_timespec(&ts_rtc, rtc_time_t, NSEC_PER_SEC>>1);
	
	ts_delta = timespec_sub(ts_system, ts_rtc);
	ts_delta_delta = timespec_sub (ts_saved_delta, ts_delta);

	if (ts_delta_delta.tv_sec < -2 || ts_delta_delta.tv_sec >= 2)
	{
		/* 
		 * A differential beteen system time and rtc is over 2 second
		 * , let's adjust system time and save time delta
		 */
		set_normalized_timespec(&ts_system, rtc_time_t + ts_saved_delta.tv_sec, ts_saved_delta.tv_nsec);
		do_settimeofday(&ts_system);
		printk ("RTC_SYNC: adjust system time from rtc\n");
	}
}

static void rtc_sync_save_delta(void)
{
	struct rtc_time		rtc_time;
	static time_t		rtc_time_t;
	struct timespec		ts_system, ts_rtc;

	getnstimeofday(&ts_system);
	s3c_rtc_gettime(NULL, &rtc_time);

	rtc_tm_to_time(&rtc_time, &rtc_time_t);
	/* RTC precision is 1 second; adjust delta for avg 1/2 sec err */
	set_normalized_timespec(&ts_rtc, rtc_time_t, NSEC_PER_SEC>>1);

	ts_saved_delta = timespec_sub(ts_system, ts_rtc);

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME_DEBUG
	printk ("RTC_SYNC: save delta sec:%d nsec:%d\n", ts_saved_delta.tv_sec, ts_saved_delta.tv_nsec);
#endif
}

static void rtc_sync_work_handler(struct work_struct * __unused)
{
	static unsigned int 	old_idle_tick, busy_count;
	int 			next_interval;
	int 			cpu_idle;

	if (rtc_sync_state == RS_SAVE_DELTA)
	{
		rtc_sync_save_delta();
		rtc_sync_start();
		return;
	}

	switch (rtc_sync_state)
	{
	case RS_WAIT_ADJUST_TIME:
		/* start adjust service */
		busy_count = 0;
	case RS_WAIT_ADJUST_TIME_AFTER_BUSY:
		/* prepare detect cpu idle */
		old_idle_tick = kstat_cpu(0).cpustat.idle + kstat_cpu(0).cpustat.iowait;
		rtc_sync_state = RS_DETECT_IDLE;
		next_interval = RTC_SYNC_DETECT_IDLE_INTERVAL;
		break;
	case RS_DETECT_IDLE:
		cpu_idle = detect_cpu_idle(old_idle_tick);

		/* when cpu idle or passing the adjust force time */
		if (cpu_idle || ++busy_count > RTC_SYNC_MAX_BUSY_COUNT)
		{
			rtc_sync_state = RS_TRY_ADJUST;
			rtc_sync_adjust();
			rtc_sync_state = RS_WAIT_ADJUST_TIME;
			next_interval = RTC_SYNC_ADJUST_INTERVAL;
		}
		else
		{
			rtc_sync_state = RS_WAIT_ADJUST_TIME_AFTER_BUSY;
			next_interval = RTC_SYNC_AFTER_BUSY_INTERVAL;
		}
		break;
	default:
		return;
	}
	schedule_delayed_work(&rtc_sync_work, next_interval);
}

static inline void  rtc_sync_start ()
{
	rtc_sync_state = RS_WAIT_ADJUST_TIME;
	schedule_delayed_work(&rtc_sync_work, RTC_SYNC_ADJUST_INTERVAL);
}

static inline void  rtc_sync_start_save_delta ()
{
	rtc_sync_state = RS_SAVE_DELTA;
	schedule_delayed_work(&rtc_sync_work, RTC_SYNC_SAVE_DELTA_INTERVAL);
}

#endif	/* CONFIG_RTC_S3C_SYNC_SYSTEM_TIME */

/* IRQ Handlers */

static irqreturn_t s3c_rtc_alarmirq(int irq, void *id)
{
	struct rtc_device *rdev = id;

	rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);

	writeb(S3C_INTP_ALM, s3c_rtc_base + S3C_INTP);

	return IRQ_HANDLED;
}

static irqreturn_t s3c_rtc_tickirq(int irq, void *id)
{
	struct rtc_device *rdev = id;

	rtc_update_irq(rdev, 1, RTC_PF | RTC_IRQF);

	writeb(S3C_INTP_TIC, s3c_rtc_base + S3C_INTP);

	return IRQ_HANDLED;
}

/* Update control registers */
static void s3c_rtc_setaie(int to)
{
	unsigned int tmp;

	pr_debug("%s: aie=%d\n", __func__, to);

	tmp = readb(s3c_rtc_base + S3C2410_RTCALM) & ~S3C2410_RTCALM_ALMEN;

	if (to)
		tmp |= S3C2410_RTCALM_ALMEN;

	writeb(tmp, s3c_rtc_base + S3C2410_RTCALM);
}

static int s3c_rtc_setpie(struct device *dev, int enabled)
{
	unsigned int tmp;

	pr_debug("%s: pie=%d\n", __func__, enabled);

	spin_lock_irq(&s3c_rtc_pie_lock);
	tmp = readw(s3c_rtc_base + S3C2410_RTCCON) & ~S3C_RTCCON_TICEN;
	tmp |= S3C2410_RTCCON_CLKEN;

	if (enabled)
		tmp |= S3C_RTCCON_TICEN;

	writew(tmp, s3c_rtc_base + S3C2410_RTCCON);
	spin_unlock_irq(&s3c_rtc_pie_lock);

	return 0;
}

static int s3c_rtc_setfreq(struct device *dev, int freq)
{
	unsigned int tmp;

	if (!is_power_of_2(freq))
		return -EINVAL;

	spin_lock_irq(&s3c_rtc_pie_lock);
#if 0
	tmp = readw(s3c_rtc_base + S3C2410_RTCCON) & 
			(S3C_RTCCON_TICEN | S3C2410_RTCCON_RTCEN | S3C2410_RTCCON_CLKEN);
	writew(tmp , s3c_rtc_base + S3C2410_RTCCON);
#endif
	tmp = (32768 / freq)-1;
	writel(tmp, s3c_rtc_base + S3C2410_TICNT);
	spin_unlock_irq(&s3c_rtc_pie_lock);

	return 0;
}

/* Time read/write */

static int s3c_rtc_gettime(struct device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = s3c_rtc_base;
	int year_110;

 retry_get_time:
	rtc_tm->tm_sec  = readb(base + S3C2410_RTCSEC);
	rtc_tm->tm_min  = readb(base + S3C2410_RTCMIN);
	rtc_tm->tm_hour = readb(base + S3C2410_RTCHOUR);
	rtc_tm->tm_mday = readb(base + S3C2410_RTCDATE);
	rtc_tm->tm_mon  = readb(base + S3C2410_RTCMON);
	
#if defined(CONFIG_CPU_S5PC110)
	year_110 = readl(base + S3C2410_RTCYEAR);	// read long type
	rtc_tm->tm_year = (0x00000fff & year_110);
#else
	rtc_tm->tm_year = readb(base + S3C2410_RTCYEAR);
#endif
	

	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}

	pr_debug("read time %02x.%02x.%02x %02x/%02x/%02x\n",
		 rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		 rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon);
	rtc_tm->tm_year = bcd2bin(rtc_tm->tm_year);

	rtc_tm->tm_year += 100;


	rtc_tm->tm_mon -= 1;

	return 0;
}

static int s3c_rtc_settime(struct device *dev, struct rtc_time *tm)
{
	void __iomem *base = s3c_rtc_base;

	int year = tm->tm_year - 100;

	
	pr_debug("set time %02d.%02d.%02d %02d/%02d/%02d\n",
		 tm->tm_year, tm->tm_mon, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
	cancel_delayed_work(&rtc_sync_work);
	rtc_sync_start_save_delta ();
#endif	/* CONFIG_RTC_S3C_SYNC_SYSTEM_TIME */

	/* we get around y2k by simply not supporting it */
	
	if (year < 0 || year >= 100) {
		dev_err(dev, "rtc only supports 100 years\n");
		return -EINVAL;
	}

#ifdef CONFIG_RTC_DRV_MAX8998
	max8998_rtc_set_time(tm);
#endif

	writeb(bin2bcd(tm->tm_sec),  base + S3C2410_RTCSEC);
	writeb(bin2bcd(tm->tm_min),  base + S3C2410_RTCMIN);
	writeb(bin2bcd(tm->tm_hour), base + S3C2410_RTCHOUR);
	writeb(bin2bcd(tm->tm_mday), base + S3C2410_RTCDATE);
	writeb(bin2bcd(tm->tm_mon + 1), base + S3C2410_RTCMON);
	
#if defined(CONFIG_CPU_S5PC110)
	year = (0x00000fff & year);
	writel(bin2bcd(year), base + S3C2410_RTCYEAR);
#else
	writeb(bin2bcd(year), base + S3C2410_RTCYEAR);
#endif


	return 0;
}

static int s3c_rtc_getalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	void __iomem *base = s3c_rtc_base;
	unsigned int alm_en;

	alm_tm->tm_sec  = readb(base + S3C2410_ALMSEC);
	alm_tm->tm_min  = readb(base + S3C2410_ALMMIN);
	alm_tm->tm_hour = readb(base + S3C2410_ALMHOUR);
	alm_tm->tm_mon  = readb(base + S3C2410_ALMMON);
	alm_tm->tm_mday = readb(base + S3C2410_ALMDATE);
#if defined(CONFIG_CPU_S5PC110)
	alm_tm->tm_year = readl(base + S3C2410_ALMYEAR);
	alm_tm->tm_year = (0x00000fff & alm_tm->tm_year);
#else
	alm_tm->tm_year = readb(base + S3C2410_ALMYEAR);
#endif


	alm_en = readb(base + S3C2410_RTCALM);

	alrm->enabled = (alm_en & S3C2410_RTCALM_ALMEN) ? 1 : 0;

	pr_debug("read alarm %02x %02x.%02x.%02x %02x/%02x/%02x\n",
		 alm_en,
		 alm_tm->tm_year, alm_tm->tm_mon, alm_tm->tm_mday,
		 alm_tm->tm_hour, alm_tm->tm_min, alm_tm->tm_sec);


	/* decode the alarm enable field */

	if (alm_en & S3C2410_RTCALM_SECEN)
		alm_tm->tm_sec = bcd2bin(alm_tm->tm_sec);
	else
		alm_tm->tm_sec = 0xff;

	if (alm_en & S3C2410_RTCALM_MINEN)
		alm_tm->tm_min = bcd2bin(alm_tm->tm_min);
	else
		alm_tm->tm_min = 0xff;

	if (alm_en & S3C2410_RTCALM_HOUREN)
		alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
	else
		alm_tm->tm_hour = 0xff;

	if (alm_en & S3C2410_RTCALM_DAYEN)
		alm_tm->tm_mday = bcd2bin(alm_tm->tm_mday);
	else
		alm_tm->tm_mday = 0xff;

	if (alm_en & S3C2410_RTCALM_MONEN) {
		alm_tm->tm_mon = bcd2bin(alm_tm->tm_mon);
		alm_tm->tm_mon -= 1;
	} else {
		alm_tm->tm_mon = 0xff;
	}

	if (alm_en & S3C2410_RTCALM_YEAREN)
		alm_tm->tm_year = bcd2bin(alm_tm->tm_year);
	else
		alm_tm->tm_year = 0xffff;

	return 0;
}

static int s3c_rtc_setalarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *tm = &alrm->time;
	void __iomem *base = s3c_rtc_base;
	unsigned int alrm_en;
	
	int year = tm->tm_year - 100;

	
	pr_debug("s3c_rtc_setalarm: %d, %02x/%02x/%02x %02x.%02x.%02x\n",
		 alrm->enabled,
		 tm->tm_mday & 0xff, tm->tm_mon & 0xff, tm->tm_year & 0xff,
		 tm->tm_hour & 0xff, tm->tm_min & 0xff, tm->tm_sec);


	alrm_en = readb(base + S3C2410_RTCALM) & S3C2410_RTCALM_ALMEN;
	writeb(0x00, base + S3C2410_RTCALM);

	if (tm->tm_sec < 60 && tm->tm_sec >= 0) {
		alrm_en |= S3C2410_RTCALM_SECEN;
		writeb(bin2bcd(tm->tm_sec), base + S3C2410_ALMSEC);
	}

	if (tm->tm_min < 60 && tm->tm_min >= 0) {
		alrm_en |= S3C2410_RTCALM_MINEN;
		writeb(bin2bcd(tm->tm_min), base + S3C2410_ALMMIN);
	}

	if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
		alrm_en |= S3C2410_RTCALM_HOUREN;
		writeb(bin2bcd(tm->tm_hour), base + S3C2410_ALMHOUR);
	}

	if (tm->tm_mday >= 0) {
		alrm_en |= S3C2410_RTCALM_DAYEN;
		writeb(bin2bcd(tm->tm_mday), base + S3C2410_ALMDATE);
	}

	if (tm->tm_mon < 13 && tm->tm_mon >= 0) {
		alrm_en |= S3C2410_RTCALM_MONEN;
		writeb(bin2bcd(tm->tm_mon + 1), base + S3C2410_ALMMON);
	}
	
#if defined(CONFIG_CPU_S5PC110)
	if (year < 100 && year >= 0) {
		alrm_en |= S3C2410_RTCALM_YEAREN;
		year = (0x00000fff & year);
		writel(bin2bcd(year), base + S3C2410_ALMYEAR);
	}
#else
	if (year < 100 && year >= 0) {
		alrm_en |= S3C2410_RTCALM_YEAREN;
		writeb(bin2bcd(year), base + S3C2410_ALMYEAR);
	}
#endif

	pr_debug("setting S3C2410_RTCALM to %08x\n", alrm_en);

	writeb(alrm_en, base + S3C2410_RTCALM);

	s3c_rtc_setaie(alrm->enabled);

	return 0;
}

static int s3c_rtc_ioctl(struct device *dev,
			 unsigned int cmd, unsigned long arg)
{
	unsigned int ret = -ENOIOCTLCMD;

	switch (cmd) {
	case RTC_AIE_OFF:
	case RTC_AIE_ON:
		s3c_rtc_setaie((cmd == RTC_AIE_ON) ? 1 : 0);
		ret = 0;
		break;

	case RTC_PIE_OFF:
	case RTC_PIE_ON:
		tick_count = 0;
		s3c_rtc_setpie(dev,(cmd == RTC_PIE_ON) ? 1 : 0);
		ret = 0;
		break;

	case RTC_IRQP_READ:
		ret = put_user(s3c_rtc_freq, (unsigned long __user *)arg);
		break;

	case RTC_IRQP_SET:
		/* check for power of 2 */

		if ((arg & (arg-1)) != 0 || arg < 1) {
			ret = -EINVAL;
			goto exit;
		}

		pr_debug("s3c2410_rtc: setting frequency %ld\n", arg);

		s3c_rtc_setfreq(dev, arg);
		ret = 0;
		break;

	case RTC_UIE_ON:
	case RTC_UIE_OFF:
		ret = -EINVAL;
	}

 exit:
	return ret;
}
static int s3c_rtc_proc(struct device *dev, struct seq_file *seq)
{
	unsigned int ticnt = readb(s3c_rtc_base + S3C2410_TICNT);

	seq_printf(seq, "periodic_IRQ\t: %s\n",
		     (ticnt & S3C2410_TICNT_ENABLE) ? "yes" : "no" );
	return 0;
}

static int s3c_rtc_open(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc_dev = platform_get_drvdata(pdev);
	int ret;

	ret = request_irq(s3c_rtc_alarmno, s3c_rtc_alarmirq,
			  IRQF_DISABLED,  "s3c2410-rtc alarm", rtc_dev);

	if (ret) {
		dev_err(dev, "IRQ%d error %d\n", s3c_rtc_alarmno, ret);
		return ret;
	}

	ret = request_irq(s3c_rtc_tickno, s3c_rtc_tickirq,
			  IRQF_DISABLED,  "s3c2410-rtc tick", rtc_dev);

	if (ret) {
		dev_err(dev, "IRQ%d error %d\n", s3c_rtc_tickno, ret);
		goto tick_err;
	}

	return ret;

 tick_err:
	free_irq(s3c_rtc_alarmno, rtc_dev);
	return ret;
}

static void s3c_rtc_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc_dev = platform_get_drvdata(pdev);

	/* do not clear AIE here, it may be needed for wake */

	s3c_rtc_setpie(dev, 0);
	free_irq(s3c_rtc_alarmno, rtc_dev);
	free_irq(s3c_rtc_tickno, rtc_dev);
}

static const struct rtc_class_ops s3c_rtcops = {
	.open		= s3c_rtc_open,
	.release	= s3c_rtc_release,
	.ioctl		= s3c_rtc_ioctl,
	.read_time	= s3c_rtc_gettime,
	.set_time	= s3c_rtc_settime,
	.read_alarm	= s3c_rtc_getalarm,
	.set_alarm	= s3c_rtc_setalarm,
	.irq_set_freq	= s3c_rtc_setfreq,
	.irq_set_state	= s3c_rtc_setpie,
	.proc	        = s3c_rtc_proc,
};

static void s3c_rtc_enable(struct platform_device *pdev, int en)
{
	void __iomem *base = s3c_rtc_base;
	unsigned int tmp;

	if (s3c_rtc_base == NULL)
		return;

	if (!en) {
		tmp = readw(base + S3C2410_RTCCON);
		writew(tmp & ~(S3C2410_RTCCON_RTCEN | S3C_RTCCON_TICEN | S3C2410_RTCCON_CLKEN), base + S3C2410_RTCCON);
	} else {
		/* re-enable the device, and check it is ok */

		if ((readb(base+S3C2410_RTCCON) & S3C2410_RTCCON_RTCEN) == 0){
			dev_dbg(&pdev->dev, "rtc disabled, re-enabling\n");

			tmp = readb(base + S3C2410_RTCCON);
			writeb(tmp|S3C2410_RTCCON_RTCEN, base+S3C2410_RTCCON);
		}

		if ((readb(base + S3C2410_RTCCON) & S3C2410_RTCCON_CNTSEL)){
			dev_dbg(&pdev->dev, "removing RTCCON_CNTSEL\n");

			tmp = readb(base + S3C2410_RTCCON);
			writeb(tmp& ~S3C2410_RTCCON_CNTSEL, base+S3C2410_RTCCON);
		}

		if ((readb(base + S3C2410_RTCCON) & S3C2410_RTCCON_CLKRST)){
			dev_dbg(&pdev->dev, "removing RTCCON_CLKRST\n");

			tmp = readb(base + S3C2410_RTCCON);
			writeb(tmp & ~S3C2410_RTCCON_CLKRST, base+S3C2410_RTCCON);
		}

		tmp = readw(base + S3C2410_RTCCON);
		writew(tmp|S3C2410_RTCCON_CLKEN, base+S3C2410_RTCCON);
	}
}

static int __devexit s3c_rtc_remove(struct platform_device *dev)
{
	struct rtc_device *rtc = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);
	rtc_device_unregister(rtc);

	s3c_rtc_setpie(&dev->dev, 0);
	s3c_rtc_setaie(0);

	iounmap(s3c_rtc_base);
	release_resource(s3c_rtc_mem);
	kfree(s3c_rtc_mem);

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
	cancel_delayed_work(&rtc_sync_work);
#endif	/* CONFIG_RTC_S3C_SYNC_SYSTEM_TIME */

	return 0;
}

static int __devinit s3c_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct resource *res;
        unsigned char bcd_tmp,bcd_loop;
	int ret;
#ifdef CONFIG_RTC_DRV_MAX8998
	struct rtc_time tm;
#endif

	pr_debug("%s: probe=%p\n", __func__, pdev);

	/* find the IRQs */

	s3c_rtc_tickno = platform_get_irq(pdev, 1);
	if (s3c_rtc_tickno < 0) {
		dev_err(&pdev->dev, "no irq for rtc tick\n");
		return -ENOENT;
	}

	s3c_rtc_alarmno = platform_get_irq(pdev, 0);
	if (s3c_rtc_alarmno < 0) {
		dev_err(&pdev->dev, "no irq for alarm\n");
		return -ENOENT;
	}

	pr_debug("s3c2410_rtc: tick irq %d, alarm irq %d\n",
		 s3c_rtc_tickno, s3c_rtc_alarmno);

	/* get the memory region */

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		return -ENOENT;
	}

	s3c_rtc_mem = request_mem_region(res->start,
					 res->end-res->start+1,
					 pdev->name);

	if (s3c_rtc_mem == NULL) {
		dev_err(&pdev->dev, "failed to reserve memory region\n");
		ret = -ENOENT;
		goto err_nores;
	}

	s3c_rtc_base = ioremap(res->start, res->end - res->start + 1);
	if (s3c_rtc_base == NULL) {
		dev_err(&pdev->dev, "failed ioremap()\n");
		ret = -EINVAL;
		goto err_nomap;
	}

	/* check to see if everything is setup correctly */

	s3c_rtc_enable(pdev, 1);

 	pr_debug("s3c2410_rtc: RTCCON=%02x\n",
		 readb(s3c_rtc_base + S3C2410_RTCCON));

	s3c_rtc_setfreq(&pdev->dev, 1);

	device_init_wakeup(&pdev->dev, 1);

#ifdef CONFIG_RTC_DRV_MAX8998
	max8998_rtc_read_time(&tm);
#endif

	/* register RTC and exit */

	rtc = rtc_device_register("s3c", &pdev->dev, &s3c_rtcops,
				  THIS_MODULE);

	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "cannot attach rtc\n");
		ret = PTR_ERR(rtc);
		goto err_nortc;
	}

	rtc->max_user_freq = S3C_MAX_CNT;

#ifdef CONFIG_RTC_DRV_MAX8998
	s3c_rtc_settime(rtc, &tm);  //update from pmic
#endif

#ifdef SET_RTC_DEFAULT_RESET_TIME
	{
		struct rtc_time tm;

		s3c_rtc_gettime (pdev, &tm);
		if (rtc_valid_tm (&tm) != 0)
		{
			struct rtc_time reset_tm = {
				.tm_sec = DEFAULT_RESET_TIME_SEC,
				.tm_min = DEFAULT_RESET_TIME_MIN,
				.tm_hour = DEFAULT_RESET_TIME_HOUR,
				.tm_mday = DEFAULT_RESET_TIME_DATE,
				.tm_mon = DEFAULT_RESET_TIME_MON - 1,
				.tm_year = DEFAULT_RESET_TIME_YEAR - 1900,
				};

			s3c_rtc_settime (pdev, &reset_tm);
			#ifdef CONFIG_RTC_DRV_MAX8998
			max8998_rtc_set_time(&reset_tm); // also update pmic rtc as default 
			#endif
		}
	}
#else

	/* check rtc time */
	for (bcd_loop = S3C2410_RTCSEC ; bcd_loop <= S3C2410_RTCYEAR ; bcd_loop +=0x4)
	{
		bcd_tmp = readb(s3c_rtc_base + bcd_loop);
		if(((bcd_tmp & 0xf) > 0x9) || ((bcd_tmp & 0xf0) > 0x90))
			writeb(0, s3c_rtc_base + bcd_loop);
	}
#endif /* SET_RTC_DEFAULT_RESET_TIME */

	platform_set_drvdata(pdev, rtc);

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
	rtc_sync_start_save_delta();
#endif	/* CONFIG_RTC_S3C_SYNC_SYSTEM_TIME */

	return 0;

 err_nortc:
	s3c_rtc_enable(pdev, 0);
	iounmap(s3c_rtc_base);

 err_nomap:
	release_resource(s3c_rtc_mem);

 err_nores:
	return ret;
}

#ifdef CONFIG_PM

/* RTC Power management control */

static struct timespec s3c_rtc_delta;
static int ticnt_save;

static int s3c_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rtc_time tm;
	struct timespec time;

	time.tv_nsec = 0;
	/* save TICNT for anyone using periodic interrupts */
	ticnt_save = readb(s3c_rtc_base + S3C2410_TICNT);

	s3c_rtc_gettime(&pdev->dev, &tm);
	rtc_tm_to_time(&tm, &time.tv_sec);
	save_time_delta(&s3c_rtc_delta, &time);

	if (gpio_get_value(GPIO_WLAN_BT_EN) == 0) /* BCM4329 isnt working */
		s3c_rtc_enable(pdev, 0);
	
#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
	cancel_delayed_work(&rtc_sync_work);
#endif	/* CONFIG_RTC_S3C_SYNC_SYSTEM_TIME */

	return 0;
}

static int s3c_rtc_resume(struct platform_device *pdev)
{
	struct rtc_time tm;
	struct timespec time;

	time.tv_nsec = 0;

	if (gpio_get_value(GPIO_WLAN_BT_EN) == 0) /* BCM4329 isnt working */
		s3c_rtc_enable(pdev, 1);

	s3c_rtc_gettime(&pdev->dev, &tm);
	rtc_tm_to_time(&tm, &time.tv_sec);
	restore_time_delta(&s3c_rtc_delta, &time);
	writeb(ticnt_save, s3c_rtc_base + S3C2410_TICNT);

#ifdef CONFIG_RTC_S3C_SYNC_SYSTEM_TIME
	rtc_sync_start ();
#endif

	return 0;
}
#else
#define s3c_rtc_suspend NULL
#define s3c_rtc_resume  NULL
#endif

static struct platform_driver s3c2410_rtc_driver = {
	.probe		= s3c_rtc_probe,
	.remove		= __devexit_p(s3c_rtc_remove),
	.suspend	= s3c_rtc_suspend,
	.resume		= s3c_rtc_resume,
	.driver		= {
		.name	= "s3c2410-rtc",
		.owner	= THIS_MODULE,
	},
};

static char __initdata banner[] = "S3C24XX RTC, (c) 2004,2006 Simtec Electronics\n";

static int __init s3c_rtc_init(void)
{
	printk(banner);
	return platform_driver_register(&s3c2410_rtc_driver);
}

static void __exit s3c_rtc_exit(void)
{
	platform_driver_unregister(&s3c2410_rtc_driver);
}

module_init(s3c_rtc_init);
module_exit(s3c_rtc_exit);

MODULE_DESCRIPTION("Samsung S3C RTC Driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:s3c2410-rtc");
