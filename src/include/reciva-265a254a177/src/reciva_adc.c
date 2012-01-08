/*
 * ADC input driver
 *
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>
#include <asm/arch-s3c2410/S3C2410-clock.h>

#include "reciva_adc.h"

#define ADCCON		(S3C2410_VA_ADC + 0)
#define ADCDAT0		(S3C2410_VA_ADC + 0xc)

#define ADC_PSEN		(1 << 14)
#define ADC_ECFLG		(1 << 15)
#define ADC_ENABLE_START	(1 << 0)

static int adc_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static int adc_open (struct inode *inode, struct file *file);
static int adc_release (struct inode *inode, struct file *file);

static struct file_operations reciva_adc_fops = {
	owner:		THIS_MODULE,
	ioctl:		adc_ioctl,
	open:		adc_open,
	release:	adc_release,
};

static struct miscdevice adc_miscdev = {
	MISC_DYNAMIC_MINOR,
	"reciva_adc",
	&reciva_adc_fops
};

static void
update_clk (int set, int mask)
{
	unsigned long flags;
	int val;
	
	save_flags_cli (flags);
	val = __raw_readl (S3C2410_CLKCON);
	val = (val & ~mask) ^ set;
	restore_flags (flags);
}

static int
adc_read (int *val)
{
	int mux = 0;
	int cval = ADC_PSEN | (38 << 6) | (mux << 3);

	update_clk (1 << 15, 0);

	__raw_writel (cval, ADCCON);
	__raw_writel (cval | ADC_ENABLE_START, ADCCON);
	while ((__raw_readl (ADCCON) & ADC_ECFLG) == 0)
		;
	*val = __raw_readl (ADCDAT0);

	update_clk (0, 1 << 15);

	return 0;
}

static int
adc_open (struct inode *inode, struct file *file)
{
	return 0;
}

static int
adc_release (struct inode *inode, struct file *file)
{
	return 0;
}

static int
adc_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int v, r;

	switch (cmd) {
	case IOC_ADC_READ:
		r = adc_read (&v);
		if (r)
			return r;
		return put_user (v, (int *)arg);
	}

	return -ENOSYS;
}

static int __init
adc_init (void)
{
	/* Register the device */
	misc_register (&adc_miscdev);

	return 0;
}

static void __exit
adc_exit (void)
{
	/* Unregister the device */
	misc_deregister (&adc_miscdev);
}

module_init (adc_init);
module_exit (adc_exit);

MODULE_AUTHOR ("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION ("ADC driver");
MODULE_LICENSE ("GPL");
