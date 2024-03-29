/* linux/arch/arm/plat-s5p64xx/dev-uart.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * Base S5P64XX UART resource and device definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/platform_device.h>

#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/devs.h>

/* Serial port registrations */

/* 64xx uarts are closer together */

static struct resource s5p64xx_uart0_resource[] = {
	[0] = {
		.start	= S3C_PA_UART0,
		.end	= S3C_PA_UART0 + S3C_SZ_UART,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S3CUART_RX0,
		.end	= IRQ_S3CUART_RX0,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S3CUART_TX0,
		.end	= IRQ_S3CUART_TX0,
		.flags	= IORESOURCE_IRQ,

	},
	[3] = {
		.start	= IRQ_S3CUART_ERR0,
		.end	= IRQ_S3CUART_ERR0,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct resource s5p64xx_uart1_resource[] = {
	[0] = {
		.start = S3C_PA_UART1,
		.end   = S3C_PA_UART1 + S3C_SZ_UART,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S3CUART_RX1,
		.end	= IRQ_S3CUART_RX1,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S3CUART_TX1,
		.end	= IRQ_S3CUART_TX1,
		.flags	= IORESOURCE_IRQ,

	},
	[3] = {
		.start	= IRQ_S3CUART_ERR1,
		.end	= IRQ_S3CUART_ERR1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource s5p64xx_uart2_resource[] = {
	[0] = {
		.start = S3C_PA_UART2,
		.end   = S3C_PA_UART2 + S3C_SZ_UART,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_S3CUART_RX2,
		.end	= IRQ_S3CUART_RX2,
		.flags	= IORESOURCE_IRQ,
	},
	[2] = {
		.start	= IRQ_S3CUART_TX2,
		.end	= IRQ_S3CUART_TX2,
		.flags	= IORESOURCE_IRQ,

	},
	[3] = {
		.start	= IRQ_S3CUART_ERR2,
		.end	= IRQ_S3CUART_ERR2,
		.flags	= IORESOURCE_IRQ,
	},
};

struct s3c24xx_uart_resources s5p64xx_uart_resources[] __initdata = {
	[0] = {
		.resources	= s5p64xx_uart0_resource,
		.nr_resources	= ARRAY_SIZE(s5p64xx_uart0_resource),
	},
	[1] = {
		.resources	= s5p64xx_uart1_resource,
		.nr_resources	= ARRAY_SIZE(s5p64xx_uart1_resource),
	},
	[2] = {
		.resources	= s5p64xx_uart2_resource,
		.nr_resources	= ARRAY_SIZE(s5p64xx_uart2_resource),
	},
};

/* uart devices */

static struct platform_device s3c24xx_uart_device0 = {
	.id		= 0,
	.dev = {
		.bus = &platform_bus_type,
        },
};

static struct platform_device s3c24xx_uart_device1 = {
	.id		= 1,
	.dev = {
		.bus = &platform_bus_type,
        },
};

static struct platform_device s3c24xx_uart_device2 = {
	.id		= 2,
	.dev = {
		.bus = &platform_bus_type,
        },
};

struct platform_device *s3c24xx_uart_src[3] = {
	&s3c24xx_uart_device0,
	&s3c24xx_uart_device1,
	&s3c24xx_uart_device2,
};

struct platform_device *s3c24xx_uart_devs[3] = {
};

