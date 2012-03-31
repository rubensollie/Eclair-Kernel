#include <linux/kernel.h>
#include <linux/io.h>
#include <mach/map.h>
#include <mach/gpio.h>
#include <mach/hardware.h>
#include <plat/regs-serial.h>

#undef USE_SERIAL0
#undef USE_SERIAL1
#undef USE_SERIAL2

#define  USE_SERIAL1

/* console base settings. */
#if defined(USE_SERIAL0)
#define UART_CONSOLE_BASE	S3C_VA_UART0
#elif defined(USE_SERIAL1)
#define UART_CONSOLE_BASE	S3C_VA_UART1
#elif defined(USE_SERIAL2)
#define UART_CONSOLE_BASE	S3C_VA_UART2
#else
#define UART_CONSOLE_BASE	S3C_VA_UART0
#endif	/* console base. */

#define __REG_UART(x)		*(volatile unsigned int *)(UART_CONSOLE_BASE + x)

typedef struct {
    int (*init)(void);
    int (*read)(void);
    int (*write)(int);
    int (*poll)(void);
    int (*flush_input)(void);
    int (*flush_output)(void);
    void (*save)(void);
    void (*restore)(void);
} serial_driver_t;

/* flush serial input queue. returns 0 on success or negative error
 * number otherwise
 */
static int s3c_serial_flush_input(void)
{
    volatile unsigned int tmp;

    /* keep on reading as long as the receiver is not empty */
    while (__REG_UART(S3C_UTRSTAT) & 0x1) {
        tmp = __REG_UART(S3C_URXH);
    }

    return 0;
}


/* flush output queue. returns 0 on success or negative error number
 * otherwise.  This only waits for space in the tx fifo, NOT until the fifo
 * has completely emptied.
 */
static int s3c_serial_flush_output(void)
{
    /* wait until the transmitter is no longer busy */
    while (__REG_UART(S3C_UTRSTAT) & 0x2);

    return 0;
}


/* initialise serial port. Serial port has been setup during kernel booting. 
 * So clock and baud rate setting are unnecessary. 
 */
static int s3c_serial_init(void)
{
    /* switch receiver and transmitter off */
    __REG_UART(S3C_UCON) &= ~0xf;

    /* reset both Rx and Tx FIFO */
    __REG_UART(S3C_UFCON) = S3C_UFCON_RESETBOTH;
    //__REG_UART(S3C_UFCON) = 0x0;
    //__REG_UART(S3C_UMCON) = 0x0;
    //__REG_UART(S3C_ULCON) = 0x3;
  
    /* receiver and transmitter on. */
    __REG_UART(S3C_UCON) |= 0x5;

    return 0;
}


/* check if there is a character available to read. returns 1 if there
 * is a character available, 0 if not, and negative error number on
 * failure */
static int s3c_serial_poll(void)
{
    if (__REG_UART(S3C_UTRSTAT) & 0x1)
        return 1;
    else
        return 0;
}

/* read one character from the serial port. return character (between
 * 0 and 255) on success, or negative error number on failure. this
 * function is blocking */
static int s3c_serial_read(void)
{
    int rv;

    for(;;) {
        rv = s3c_serial_poll();

        if(rv < 0)
            return rv;

        if(rv > 0)
            return __REG_UART(S3C_URXH) & 0xff;
    }

    return -1;
}

/* write character to serial port. return 0 on success, or negative
 * error number on failure. this function is blocking
 */
static int s3c_serial_write(int c)
{
    /* wait for room in the transmit FIFO */
    /* wait for room in the tx FIFO */
    while (!(__REG_UART(S3C_UTRSTAT) & 0x2));

    /* if there is an character, console out! */
    __REG_UART(S3C_UTXH) = c;

    return 0;
}

struct sleep_save {
    void __iomem	*reg;
    unsigned long	val;
};

#define SAVE_ITEM(x) \
    { .reg = (x) }

#define SAVE_UART(va) \
    SAVE_ITEM((va) + S3C_ULCON), \
    SAVE_ITEM((va) + S3C_UCON), \
    SAVE_ITEM((va) + S3C_UFCON), \
    SAVE_ITEM((va) + S3C_UMCON)

static struct sleep_save uart_save[] = {
    SAVE_UART(UART_CONSOLE_BASE) 
};

static void s3c_serial_save(void)
{
    int count = ARRAY_SIZE(uart_save);
    int i = 0;

    for(i = 0; i < count; i++ ){
        uart_save[i].val = __raw_readl(uart_save[i].reg);
    }
}

static void s3c_serial_restore(void)
{   
    int count = ARRAY_SIZE(uart_save);
    int i = 0;

    /* reset both Rx and Tx FIFO */
    __REG_UART(S3C_UFCON) = S3C_UFCON_RESETBOTH;

    for(i = 0; i < count; i++ ){
        __raw_writel(uart_save[i].val, uart_save[i].reg);
    }    
}

/* export serial driver */
serial_driver_t s3c_serial_driver_gad = {
    init:		s3c_serial_init,
    read:		s3c_serial_read,
    write:		s3c_serial_write,
    poll:		s3c_serial_poll,
    flush_input:	s3c_serial_flush_input,
    flush_output:	s3c_serial_flush_output,
    save:       s3c_serial_save,
    restore:        s3c_serial_restore,
};
EXPORT_SYMBOL(s3c_serial_driver_gad);
