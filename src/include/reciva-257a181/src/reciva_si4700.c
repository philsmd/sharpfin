/*
 * Si4700 tuner driver
 *
 * Copyright (c) 2006 Reciva Ltd
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
#include <linux/soundcard.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "reciva_si4700.h"

static int si4700_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg);
static unsigned int si4700_poll (struct file *file, poll_table *wait);
static int si4700_read (struct file *filp, char *buffer, size_t count, loff_t *ppos);
static int si4700_open(struct inode * inode, struct file * file);
static int si4700_release(struct inode * inode, struct file * file);
static void si4700_int_handler(int irq, void *dev, struct pt_regs *regs);

#define SI4700_NAME	"si4700"

extern struct i2c_adapter *i2c_bit_s3c2410;

static struct i2c_driver si4700;

static struct i2c_client *g_clnt;

struct si4700_data {
	unsigned short regs[16];

	wait_queue_head_t wait_queue;		   /* Used for blocking read */
	struct si4700_packet output_buffer[16];    /* Stores data to write out of device */
	int inp, outp;                             /* The number of bytes in the buffer */
	struct work_struct work;
	int irq;

	int scanning;
	int start_scan;

	struct proc_dir_entry *procent;
};

#define Si4700_STC		(1 << 14)
#define Si4700_RDSR		(1 << 15)

#define BOARD_TYPE_APPB		0
#define BOARD_TYPE_IR6081	1

static int board_type;

MODULE_PARM(board_type, "i");

#define MIN(a,b) ( (a)<(b) ? (a):(b) )

/***********************************************************************/

/* returned data starts at address 0xa */
static int
si4700_chip_read (struct i2c_client *clnt, int nregs, unsigned short *data)
{
	int r, i;

	r = i2c_master_recv (clnt, (unsigned char *)data, nregs * 2);
	if (r != nregs * 2) {
		printk(KERN_ERR "si4700: read failed, status %d\n", r);
		return r;
	}

	for (i = 0; i < nregs; i++)
		data[i] = __be16_to_cpu (data[i]);

	return 0;
}

/* write data starts at address 0x2 */
static int
si4700_chip_write (struct i2c_client *clnt, int nregs, unsigned short *data)
{
	int r, i;
	unsigned short sdata[16];

	for (i = 0; i < nregs; i++)
		sdata[i] = __cpu_to_be16 (data[i]);

	r = i2c_master_send (clnt, (unsigned char *)sdata, nregs * 2);
	if (r != nregs * 2) {
		printk(KERN_ERR "si4700: write failed, status %d\n", r);
		return r;
	}

	return 0;
}

static int
si4700_read_regs (struct i2c_client *clnt, int nregs)
{
	unsigned short regs[16];
	int i, r;
	struct si4700_data *si = clnt->data;

	r = si4700_chip_read (clnt, nregs, regs);
	if (r)
		return r;

	for (i = 0; i < nregs; i++) {
		si->regs[(i + 0xa) % 16] = regs[i];
	}

	return 0;
}

static int
si4700_write_regs (struct i2c_client *clnt, int nregs)
{
	unsigned short regs[16];
	int i;
	struct si4700_data *si = clnt->data;

	for (i = 0; i < nregs; i++) {
		regs[i] = si->regs[(i + 2) % 16];
	}

	return si4700_chip_write (clnt, nregs, regs);
}

#define BASE_FREQ	87500	/* 87.5MHz */

static int
si4700_tune_to (struct i2c_client *clnt, int freq)
{
	struct si4700_data *si = clnt->data;
	int chan, r;

	if (freq < BASE_FREQ)
		return -EINVAL;

	chan = (freq - BASE_FREQ) / 100;

	printk("selecting channel %d\n", chan);

	si->regs[3] &= ~0x8000;
	r = si4700_write_regs (clnt, 2);
	if (r) 
		return r;
	si->regs[3] = 0x8000 | chan;
	return si4700_write_regs (clnt, 4);
}

static int
si4700_seek (struct i2c_client *clnt)
{
	struct si4700_data *si = clnt->data;

	si->start_scan = 1;
	return si4700_tune_to (clnt, BASE_FREQ);
}

static int
si4700_set_volume (struct i2c_client *clnt, int vol)
{
	struct si4700_data *si = clnt->data;

	si->regs[5] &= ~15;
	si->regs[5] |= vol & 15;
	return si4700_write_regs (clnt, 4);
}

static int
si4700_init_chip (struct i2c_client *clnt)
{
	struct si4700_data *si = clnt->data;
	int r;

	si->regs[2] = 0x4801;
	si->regs[3] = 0x0000;
	si->regs[4] = 0xC804;
	si->regs[5] = 19 << 8;

	r = si4700_write_regs (clnt, 4);
	if (r)
		return r;

	mdelay (220);

	r = si4700_read_regs (clnt, 16);
	if (r)
		return r;

	printk("si4700: device id %04x, %04x\n", si->regs[0], si->regs[1]);

	return 0;
}

static int
si4700_power_down (struct i2c_client *clnt)
{
	struct si4700_data *si = clnt->data;

	si->regs[2] = 0x0041;

	return si4700_write_regs (clnt, 4);
}

/***********************************************************************/

static struct si4700_packet *
si4700_get_packet (struct si4700_data *si)
{
	struct si4700_packet *r;
	int new_inp = ((si->inp + 1) % 16);

	if (new_inp == si->outp)
		return NULL;

	r = &si->output_buffer[si->inp];

	return r;
}

static void
si4700_write_packet (struct si4700_data *si)
{
	si->inp = ((si->inp + 1) % 16);
	wake_up_interruptible (&si->wait_queue);
}

static void
si4700_int_handler (int irq, void *dev, struct pt_regs *regs)
{
	struct si4700_data *si = dev;

	schedule_work (&si->work);
}

static void 
si4700_kevent (void *data)
{
	struct i2c_client *clnt = data;
	struct si4700_data *si = clnt->data;
	int status;

	si4700_read_regs (clnt, 6);

	status = si->regs[0xa];

	if (status & Si4700_STC) {
		printk("si4700: seek complete\n");
		si->regs[2] &= ~(1 << 8);
		si->regs[3] &= ~(1 << 15);
		si->regs[4] |= 1 << 12;	// RDS on
		si4700_write_regs (clnt, 3);

		if (si->scanning) {
			if (status & (1 << 13)) {
				printk("seek complete\n");
				si->scanning = 0;
			} else {
				printk("station found at channel %d rssi %d\n",
				       si->regs[0xb] & 0x3ff,
				       si->regs[0xa] & 0xff);
				si4700_seek (clnt);
			}
		} else if (si->start_scan) {
			si->start_scan = 0;
			si->scanning = 1;

			si->regs[2] |= 1 << 8 | 1 << 9 | 1 << 10;
	
			si4700_write_regs (clnt, 1);
		}
	}

	if (status & Si4700_RDSR) {
		struct si4700_packet *outpkt;

		outpkt = si4700_get_packet (si);
		if (outpkt) {
			outpkt->code = SI4700_PACKET_RDS;
			outpkt->u.rds.a = si->regs[0xc];
			outpkt->u.rds.b = si->regs[0xd];
			outpkt->u.rds.c = si->regs[0xe];
			outpkt->u.rds.d = si->regs[0xf];

			if (si->regs[0xa] & 0x600)
				outpkt->u.rds.a |= 0x80000000;
			if (si->regs[0xb] & 0xc000)
				outpkt->u.rds.b |= 0x80000000;
			if (si->regs[0xb] & 0x3000)
				outpkt->u.rds.c |= 0x80000000;
			if (si->regs[0xb] & 0x0c00)
				outpkt->u.rds.d |= 0x80000000;

			si4700_write_packet (si);
		}
	}
}

/***********************************************************************/

static struct file_operations reciva_si4700_fops =
{
	owner:    THIS_MODULE,
	ioctl:    si4700_ioctl,
	read:	  si4700_read,
	poll:	  si4700_poll,	
	open:	  si4700_open,
	release:  si4700_release,
};

static struct miscdevice si4700_miscdev =
{
	MISC_DYNAMIC_MINOR,
	"si4700",
	&reciva_si4700_fops
};

static unsigned int 
si4700_poll (struct file *file, poll_table *wait)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = clnt->data;

	poll_wait(file, &si->wait_queue, wait);
	if (si->inp != si->outp)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int 
si4700_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	struct i2c_client *clnt = filp->private_data;
	struct si4700_data *si = clnt->data;
	int r, bcount;

	/* Wait until there is something in the buffer */
	while (si->inp == si->outp) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (signal_pending (current))
			return -EINTR;
		
		interruptible_sleep_on (&si->wait_queue);
	}

	bcount = MIN (count, sizeof (struct si4700_packet));
	
	/* Send data to user application */
	r = (copy_to_user (buffer, &si->output_buffer[si->outp], bcount)) ? -EFAULT : bcount;
	
	si->outp = (si->outp + 1) % 16;
	
	return r;
}

static int 
si4700_open(struct inode * inode, struct file * file)
{
	struct i2c_client *clnt = g_clnt;
	struct si4700_data *si = clnt->data;
	unsigned long flags;
	unsigned long x;
	int r;

	file->private_data = clnt;

	r = si4700_init_chip (clnt);
	if (r)
		return r;

	si->inp = si->outp = 0;

	save_flags_cli (flags);
	x = __raw_readl (S3C2410_EXTINT1);
	x &= ~(7 << 20);	// falling edge trigger
	x |= 2 << 20;
	__raw_writel (x, S3C2410_EXTINT1);
	restore_flags (flags);

	if (request_irq (si->irq, si4700_int_handler, 0, "Si4700", clnt->data)) {
		printk ("couldn't request Si4700 interrupt line\n");
		si4700_power_down (clnt);
		return -EBUSY;
	}

	return 0;
}

static int 
si4700_release(struct inode * inode, struct file * file)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = clnt->data;

	free_irq (si->irq, clnt->data);

	si4700_power_down (clnt);
	
	return 0;
}

static int
si4700_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = clnt->data;
	int freq, vol;

	switch (cmd) {
	case IOC_SI4700_TUNE:
		if (get_user (freq, (int *)arg))
			return -EFAULT;
		return si4700_tune_to (clnt, freq);

	case IOC_SI4700_STATUS:
		si4700_read_regs (clnt, 1);
		return put_user (si->regs[0xa] & 0x1ff, (int *)arg);

	case IOC_SI4700_VOLUME:
		if (get_user (vol, (int *)arg))
			return -EFAULT;
		return si4700_set_volume (clnt, vol);
	}

	return -ENODEV;
}

/***********************************************************************/

static struct i2c_client client_template = {
	name: "(unset)",
	flags:  I2C_CLIENT_ALLOW_USE,
	driver: &si4700
};

/*****************************************************************************
 * Provide data to file in /proc about current status
 *****************************************************************************/
static int 
read_procmem(char *buf, char **start, off_t offset, 
                        int count, int *eof, void *data)
{
	struct i2c_client *clnt = data;
	struct si4700_data *si = clnt->data;
	int i, o = 0;
	
	si4700_read_regs (clnt, 16);

	for (i = 0; i < 16; i++) {
	  o += sprintf(buf + o, "%02x: %08x\n", i, si->regs[i]);
	}

	if (o <= offset + count)
	  *eof = 1;

	*start = buf + offset;

	o -= offset;
	if (o > count) o = count;
	if (o < 0) o = 0;

	return o;
}

static int 
si4700_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)		
{
	struct si4700_data *si;
	struct i2c_client *clnt;

	if (adap != i2c_bit_s3c2410) {
		printk ("si4700: can't attach to unknown adapter\n");
		return -ENODEV;
	}

	clnt = kmalloc(sizeof(*clnt), GFP_KERNEL);
	memcpy(clnt, &client_template, sizeof(*clnt));
	clnt->adapter = adap;
	clnt->addr = addr;
	strcpy(clnt->name, "si4700");

	si = kmalloc(sizeof(*si), GFP_KERNEL);
	if (!si)
		return -ENOMEM;

	memset(si, 0, sizeof(*si));

	init_waitqueue_head (&si->wait_queue);
	INIT_WORK(&si->work, si4700_kevent, clnt);

	si->irq = IRQ_EINT13;
	
	clnt->data = si;

	i2c_attach_client (clnt);

	printk ("si4700: attached at address %x\n", addr);

	/* Register the device */
	misc_register (&si4700_miscdev);	

	g_clnt = clnt;

	/* Initialise proc entry */
	si->procent = create_proc_read_entry("driver/si4700_regs", 0, NULL, read_procmem, clnt);

	return 0;
}

static int 
si4700_detach_client(struct i2c_client *clnt)
{
	g_clnt = NULL;
	
	remove_proc_entry ("driver/si4700/regs", NULL);

	/* Unregister the device */
	misc_deregister (&si4700_miscdev);

	i2c_detach_client (clnt);

	kfree (clnt->data);
	kfree (clnt);

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x10, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { I2C_CLIENT_END };
static unsigned short probe[]        = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[]  = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[]       = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[]        = { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c, normal_i2c_range, 
	probe, probe_range, 
	ignore, ignore_range, 
	force
};

static int si4700_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, si4700_attach);
}

static void si4700_inc_use(struct i2c_client *clnt)
{
	MOD_INC_USE_COUNT;
}

static void si4700_dec_use(struct i2c_client *clnt)
{
	MOD_DEC_USE_COUNT;
}

static struct i2c_driver si4700 = {
	name:		SI4700_NAME,
	id:		I2C_DRIVERID_SI4700,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	si4700_attach_adapter,
	detach_client:	si4700_detach_client,
	inc_use:	si4700_inc_use,
	dec_use:	si4700_dec_use
};

static int __init si4700_init(void)
{
	struct i2c_client dummy_client;
	int reset_pin;

	// apply si4700 reset
	switch (board_type) {
	case 0:
		// (J2-7 GPG7)
		s3c2410_gpio_cfgpin (S3C2410_GPG7, S3C2410_GPG7_OUTP);
		reset_pin = S3C2410_GPG7;
		break;
	case 1:
		// (J1-20 GPB9)
		s3c2410_gpio_cfgpin (S3C2410_GPB9, S3C2410_GPB9_OUTP);
		reset_pin = S3C2410_GPB9;
		break;
	default:
		printk("reciva_si700: unknown board type\n");
		return -EINVAL;
	}

	s3c2410_gpio_pullup (reset_pin, 1);
	s3c2410_gpio_setpin (reset_pin, 0);

	s3c2410_gpio_cfgpin (S3C2410_GPG5, S3C2410_GPG5_EINT13);
	s3c2410_gpio_pullup (S3C2410_GPG5, 1);

	// must have SDIO low during rising edge on Si4700 nRST
	dummy_client.adapter = i2c_bit_s3c2410;
	i2c_control (&dummy_client, I2C_BIT_LOCK, 0);
	i2c_control (&dummy_client, I2C_BIT_SETSDA, 0);

	mdelay (1);
	s3c2410_gpio_setpin (reset_pin, 1);
	mdelay (1);

	i2c_control (&dummy_client, I2C_BIT_SETSDA, 1);
	i2c_control (&dummy_client, I2C_BIT_UNLOCK, 0);

	return i2c_add_driver(&si4700);
}

static void __exit si4700_exit(void)
{
	i2c_del_driver(&si4700);
}

module_init(si4700_init);
module_exit(si4700_exit);

MODULE_AUTHOR("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION("SI4700 FM tuner driver");
MODULE_LICENSE("GPL");
