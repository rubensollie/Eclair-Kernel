/*
 * Copyright (C) 2008 Google, Inc.
 *     Author: Nick Pelly <npelly@google.com>
 * Copyright 2010 Samsung Electronics Co. Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* Control bluetooth power for Apollo platform */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>

#include <linux/wakelock.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <mach/gpio.h>
#include <mach/hardware.h>

#include <plat/gpio-cfg.h>
#include <plat/irqs.h>

#define BT_SLEEP_ENABLE

extern void s5p_config_gpio_alive_table(int array_size, int (*gpio_table)[4]);

static struct rfkill *bt_rfkill;
static struct wake_lock bt_host_wakelock;

#ifdef BT_SLEEP_ENABLE
static struct rfkill *bt_sleep;
static struct wake_lock bt_wakelock;
#endif /* BT_SLEEP_ENABLE */

unsigned int host_wake_irq = IRQ_EINT(21);	

static int bt_gpio_table[][4] = {
	{GPIO_WLAN_BT_EN, GPIO_WLAN_BT_EN_STATE, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_BT_nRST, GPIO_BT_nRST_STATE, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	{GPIO_BT_HOST_WAKE, GPIO_BT_HOST_WAKE_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
	{GPIO_BT_WAKE, GPIO_BT_WAKE_STATE, GPIO_LEVEL_LOW, S3C_GPIO_PULL_NONE},
	/* need to check wlan status */
	{GPIO_WLAN_nRST, GPIO_WLAN_nRST_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static int bt_uart_on_table[][4] = {
	{GPIO_BT_UART_RXD, GPIO_BT_UART_RXD_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_UART_TXD, GPIO_BT_UART_TXD_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_UART_CTS, GPIO_BT_UART_CTS_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
	{GPIO_BT_UART_RTS, GPIO_BT_UART_RTS_STATE, GPIO_LEVEL_NONE, S3C_GPIO_PULL_NONE},
};

static int bt_uart_off_table[][4] = {
	{GPIO_BT_UART_RXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
	{GPIO_BT_UART_TXD, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
	{GPIO_BT_UART_CTS, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
	{GPIO_BT_UART_RTS, 0, GPIO_LEVEL_NONE, S3C_GPIO_PULL_DOWN},
};

static void bt_host_wake_work_func(struct work_struct *ignored)
{
	int gpio_val;

	gpio_val = gpio_get_value(GPIO_BT_HOST_WAKE);
	printk(KERN_DEBUG "[BT] GPIO_BT_HOST_WAKE = %d\n", gpio_val);
	printk(KERN_INFO "[BT] bt_host_wakelock timeout = 5 sec\n");
	wake_lock_timeout(&bt_host_wakelock, 5*HZ);
	enable_irq(host_wake_irq);
}

static DECLARE_WORK(bt_host_wake_work, bt_host_wake_work_func);
irqreturn_t bt_host_wake_isr(int irq, void *dev_id)
{
	disable_irq(host_wake_irq);
	schedule_work(&bt_host_wake_work);

	return IRQ_HANDLED;
}

int bluetooth_enable_irq(int enable, struct platform_device *pdev)
{
	int ret;
	if (enable) {
		set_irq_type(host_wake_irq , IRQ_TYPE_EDGE_RISING);	
		ret = request_irq(host_wake_irq , bt_host_wake_isr, 0, pdev->name, NULL);
		if (ret < 0)
			return ret;
		set_irq_wake(host_wake_irq, 1);
	}
	else {
		set_irq_wake(host_wake_irq, 0);
		free_irq(host_wake_irq, NULL);
	}
	return ret;
}

static int bluetooth_set_power(void *data, enum rfkill_state state)
{
	struct platform_device *pdev = (struct platform_device *)data;
	switch (state) {
		case RFKILL_STATE_UNBLOCKED:
			printk(KERN_INFO "[BT] Device powering ON\n");
			s5p_config_gpio_alive_table(ARRAY_SIZE(bt_uart_on_table), bt_uart_on_table);
			
			/* need delay between v_bat & reg_on for 2 cycle @ 38.4MHz */
			udelay(5);
			
			gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_HIGH);		
			s3c_gpio_pdn_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT1);
			
			gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_HIGH);			
			s3c_gpio_pdn_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT1);

			printk(KERN_DEBUG "[BT] GPIO_WLAN_BT_EN = %d, GPIO_BT_nRST = %d\n", 
			gpio_get_value(GPIO_WLAN_BT_EN), gpio_get_value(GPIO_BT_nRST));

			bluetooth_enable_irq(1, pdev);
			break;

		case RFKILL_STATE_SOFT_BLOCKED:
			printk(KERN_INFO "[BT] Device powering OFF\n");		
			bluetooth_enable_irq(0, pdev);
			
			gpio_set_value(GPIO_BT_nRST, GPIO_LEVEL_LOW);		
			s3c_gpio_pdn_cfgpin(GPIO_BT_nRST, S3C_GPIO_SLP_OUT0);
			
			/* wlan status should be checked before you set GPIO_WLAN_BT_EN to LOW */
			if (gpio_get_value(GPIO_WLAN_nRST) == 0) {
				gpio_set_value(GPIO_WLAN_BT_EN, GPIO_LEVEL_LOW);
				s3c_gpio_pdn_cfgpin(GPIO_WLAN_BT_EN, S3C_GPIO_SLP_OUT0);
			}			
			
			s5p_config_gpio_alive_table(ARRAY_SIZE(bt_uart_off_table), bt_uart_off_table);
			
			printk(KERN_DEBUG "[BT] GPIO_WLAN_BT_EN = %d, GPIO_BT_nRST = %d\n", 
			gpio_get_value(GPIO_WLAN_BT_EN), gpio_get_value(GPIO_BT_nRST));

			break;

		default:
			printk(KERN_ERR "[BT] Bad bluetooth rfkill state %d\n", state);
	}
	return 0;
}

#ifdef BT_SLEEP_ENABLE
static int bluetooth_set_sleep(void *data, enum rfkill_state state)
{	
	switch (state) {
		case RFKILL_STATE_UNBLOCKED:
			printk (KERN_INFO "[BT] wake_unlock(bt_wake_lock)\n");
			gpio_set_value(GPIO_BT_WAKE, GPIO_LEVEL_LOW);
			printk(KERN_DEBUG "[BT] GPIO_BT_WAKE = %d\n", gpio_get_value(GPIO_BT_WAKE) );
			wake_unlock(&bt_wakelock);
			break;

		case RFKILL_STATE_SOFT_BLOCKED:
			printk (KERN_INFO "[BT] wake_lock(bt_wake_lock)\n");
			gpio_set_value(GPIO_BT_WAKE, GPIO_LEVEL_HIGH);
			printk(KERN_DEBUG "[BT] GPIO_BT_WAKE = %d\n", gpio_get_value(GPIO_BT_WAKE) );
			wake_lock(&bt_wakelock);
			break;

		default:
			printk(KERN_ERR "[BT] bad bluetooth rfkill state %d\n", state);
	}
	return 0;
}
#endif /* BT_SLEEP_ENABLE */

static int __init apollo_rfkill_probe(struct platform_device *pdev)
{
	int ret;

	s5p_config_gpio_alive_table(ARRAY_SIZE(bt_gpio_table), bt_gpio_table);
	
	/* Host Wake IRQ */	
	wake_lock_init(&bt_host_wakelock, WAKE_LOCK_SUSPEND, "bt_host_wake");

	bt_rfkill = rfkill_allocate(&pdev->dev, RFKILL_TYPE_BLUETOOTH);
	if (!bt_rfkill)
		goto err_rfkill;

	bt_rfkill->name = "bt_rfkill";
	/* userspace cannot take exclusive control */	
	bt_rfkill->user_claim_unsupported = 1;
	bt_rfkill->user_claim = 0;
	bt_rfkill->data = pdev;	/* user data */
	bt_rfkill->toggle_radio = bluetooth_set_power;	
	/* set bt_rfkill default state to off */
	rfkill_set_default(RFKILL_TYPE_BLUETOOTH, RFKILL_STATE_SOFT_BLOCKED);

	ret = rfkill_register(bt_rfkill);
	if (ret) {
		rfkill_free(bt_rfkill);
		goto err_rfkill;
	}

#ifdef BT_SLEEP_ENABLE
	wake_lock_init(&bt_wakelock, WAKE_LOCK_SUSPEND, "bt_wake");
	
	bt_sleep = rfkill_allocate(&pdev->dev, RFKILL_TYPE_BLUETOOTH);
	if (!bt_sleep)
		goto err_sleep;

	bt_sleep->name = "bt_sleep";
	/* userspace cannot take exclusive control */
	bt_sleep->user_claim_unsupported = 1;
	bt_sleep->user_claim = 0;
	bt_sleep->data = NULL;	/* user data */
	bt_sleep->toggle_radio = bluetooth_set_sleep;

	ret = rfkill_register(bt_sleep);
	if (ret) {
		rfkill_free(bt_sleep);
		goto err_sleep;
	}

	/* set bt_sleep default state to wake_unlock */
	rfkill_force_state(bt_sleep, RFKILL_STATE_UNBLOCKED);
#endif /* BT_SLEEP_ENABLE */

	return 0;
	
err_sleep:
	wake_lock_destroy(&bt_wakelock);
	rfkill_unregister(bt_rfkill);

err_rfkill:
	wake_lock_destroy(&bt_host_wakelock);		
	return ret;

}

static struct platform_driver apollo_rfkill_driver = {
	.probe = apollo_rfkill_probe,
	.driver = {
		.name = "bcm4329_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init apollo_rfkill_init(void)
{
	return platform_driver_register(&apollo_rfkill_driver);
}

module_init(apollo_rfkill_init);
MODULE_DESCRIPTION("apollo rfkill");
MODULE_LICENSE("GPL");
