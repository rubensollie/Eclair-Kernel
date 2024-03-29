/* linux/arch/arm/plat-s5p64xx/setup-post.c
 *
 * Copyright 2009 Samsung Electronics
 *	Jinsung Yang
 *	http://samsungsemi.com/
 *
 * Base Post Processor gpio configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-bank-b.h>
#include <plat/gpio-bank-f.h>

struct platform_device; /* don't need the contents */

void s3c_post_cfg_gpio(struct platform_device *dev)
{
	/* nothing to do */
}
