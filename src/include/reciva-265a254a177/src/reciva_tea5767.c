/*
 * TEA5767 tuner driver
 *
 * Copyright (c) 2006, 2007 Reciva Ltd
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#else
#include <asm/arch-bast/param.h>
#endif

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "reciva_tea5767.h"

// There needs to be a delay between setting the freq and reading back
// the status. This value seems reliable. 5ms also seems to work
// Anything less than about 3 or 4 ms is bad.
#define DEFAULT_TUNE_DELAY_MS 10
static int tune_delay_ms = DEFAULT_TUNE_DELAY_MS;
MODULE_PARM (tune_delay_ms, "i");


static int board_type;

static int tea5767_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int tea5767_poll (struct file *file, poll_table * wait);
static int tea5767_read (struct file *filp, char *buffer, size_t count, loff_t * ppos);
static int tea5767_open (struct inode *inode, struct file *file);
static int tea5767_release (struct inode *inode, struct file *file);
static void tea5767_int_handler (int irq, void *dev, struct pt_regs *regs);
static int tea5767_seek (struct i2c_client *clnt, int direction);
static void tea5767_timer_kevent (void *data);

//#define DEBUG

#ifdef DEBUG
#define DPRINTK printk
static void dump_status (struct i2c_client *clnt);
#else
#define DPRINTK(x, ...)	do { } while (0)
#define dump_status(x) do { } while (0)
#endif

#define TEA5767_NAME	"tea5767"
#define PREFIX        "tea5767:"

#define TEA5767_REG2_SUD	(1 << 7)
#define TEA5767_REG2_HLSI	(1 << 4)
#define TEA5767_REG2_MS		(1 << 3)
#define TEA5767_REG2_MR		(1 << 2)
#define TEA5767_REG2_ML		(1 << 1)
#define TEA5767_REG2_SWP1	(1 << 0)

enum injector_state {
	INJECTOR_IDLE,
	INJECTOR_TRY_HI,
	INJECTOR_TRY_LO,
	INJECTOR_SELECTED_HI_LO,
};

extern struct i2c_adapter *i2c_bit_s3c2410;

static struct i2c_driver tea5767;

static struct i2c_client *g_clnt;

struct tea5767_data {
	wait_queue_head_t wait_queue;	/* Used for blocking read */
	struct tea5767_packet output_buffer[16];	/* Stores data to write out of device */
	int inp, outp;		/* The number of bytes in the buffer */
	struct work_struct work;
	int irq;
	int f_ref;

	int injection;
	int scanning;
	int start_scan;
	int scan_wrapped;
	int scan_direction;	// 1=up 0=down

	int band_limit_low;
	int band_limit_high;

	unsigned char regs[5];
	unsigned char status[5];

	struct proc_dir_entry *procent;

	struct timer_list timer;
	struct work_struct timer_work;
	enum injector_state injector_state;
	struct {
		int freq;
		int iLevelHigh, iLevelLow;
		int iIFChigh, iIFClow;
		int sync;
	} injector_data;
};

#define MIN(a,b) ( (a)<(b) ? (a):(b) )

/***********************************************************************/

/* returned data starts at address 0xa */
static int
tea5767_chip_read (struct i2c_client *clnt, int nregs, unsigned char *data)
{
	int r;

	r = i2c_master_recv (clnt, (unsigned char *) data, nregs);
	if (r != nregs) {
		printk (KERN_ERR "tea5767: read failed, status %d\n", r);
		return r;
	}

	return 0;
}

/* write data starts at address 0x2 */
static int
tea5767_chip_write (struct i2c_client *clnt, int nregs, unsigned char *data)
{
	int r;

	r = i2c_master_send (clnt, (unsigned char *) data, nregs);
	if (r != nregs) {
		printk (KERN_ERR "tea5767: write failed, status %d\n", r);
		return r;
	}

	return 0;
}

static int
tea5767_write_sync (struct i2c_client *clnt)
{
	struct tea5767_data *tea = clnt->data;

	DPRINTK ("tea5767: writing %02x %02x %02x %02x %02x\n", tea->regs[0], tea->regs[1], tea->regs[2], tea->regs[3], tea->regs[4]);

	return tea5767_chip_write (clnt, 5, tea->regs);
}

static int
tea5767_read_sync (struct i2c_client *clnt)
{
	int r;
	struct tea5767_data *tea = clnt->data;

	r = tea5767_chip_read (clnt, 5, tea->status);

//      DPRINTK("tea5767: status %02x %02x %02x %02x %02x\n", tea->status[0], tea->status[1], tea->status[2], tea->status[3], tea->status[4]);

	return r;
}

/****************************************************************************
 * Converts PLL word to kHz
 ****************************************************************************/
static inline int
pll_to_khz (int pll, int f_ref, int injection)
{
	int khz;

	if (injection)
		khz = (pll * f_ref) / 4000 - 225;
	else
		khz = (pll * f_ref) / 4000 + 225;

	// Round to nearest 50kHz boundary
	khz = ((khz + 24) / 50) * 50;

	return khz;
}

/****************************************************************************
 * Converts kHz to PLL word
 ****************************************************************************/
static inline int
khz_to_pll (int khz, int f_ref, int injection)
{
	int pll;

	if (injection)
		pll = (4000 * (khz + 225)) / f_ref;
	else
		pll = (4000 * (khz - 225)) / f_ref;

	return pll;
}

/****************************************************************************
 * Ensure scan bits are cleared
 ****************************************************************************/
static void
tea5767_cancel_scan (struct tea5767_data *tea)
{
	tea->scanning = 0;
	tea->regs[0] &= 0x3f;
}

static int
tea5767_set_pll (struct tea5767_data *tea, int freq_khz)
{
	int i = khz_to_pll (freq_khz, tea->f_ref, tea->injection);

	DPRINTK ("tea5767: selected pll word %d for %dkHz\n", i, freq_khz);

	if (i > 0x3fff) {
		printk ("tea5767: pll value out of range\n");
		return -EINVAL;
	}

	tea->regs[0] = (tea->regs[0] & 0xc0) | (i >> 8);
	tea->regs[1] = i & 0xff;

	return 0;
}

static int
tea5767_init_chip (struct i2c_client *clnt)
{
	struct tea5767_data *tea = clnt->data;

	tea->f_ref = 32768;

	tea->injection = 1;
	tea->regs[0] = 0;	// not muted, not searching
	tea->regs[1] = 0;	// 
	tea->regs[2] = TEA5767_REG2_HLSI | TEA5767_REG2_SUD | 0x60;	// high side injection, serahc mode rssi=10(high)
	tea->regs[3] = 0x11;	// SW1 is ready flag
	tea->regs[4] = 0x40;	// 75us de-emph

	tea->band_limit_low = 87500;
	tea->band_limit_high = 108000;

	tea5767_set_pll (tea, 96000);

	tea5767_write_sync (clnt);
	tea5767_read_sync (clnt);

	return 0;
}

static int
tea5767_shutdown_chip (struct i2c_client *clnt)
{
	struct tea5767_data *tea = clnt->data;

	tea->regs[2] |= 0x80;	// mute
	tea->regs[3] |= 0x40;	// standby mode

	return tea5767_write_sync (clnt);
}

/******************************************************************************
 * Set frequency with specified HI/LO injection
 ******************************************************************************/
static void
set_frequency (struct i2c_client *clnt, int freq)
{
	struct tea5767_data *tea = clnt->data;

	// Set up freq
	tea5767_set_pll (tea, freq);

	// Set HI/LO injection
	if (tea->injection)
		tea->regs[2] |= TEA5767_REG2_HLSI;
	else
		tea->regs[2] &= ~(TEA5767_REG2_HLSI);

	// Write registers
	tea5767_write_sync (clnt);
}

/******************************************************************************
 * Calculate optimal HI/LO injection setting at specified freq
 ******************************************************************************/
static void
calculate_hi_lo_injection (struct i2c_client *clnt, int freq, int sync)
{
	struct tea5767_data *tea = clnt->data;

	tea->injector_data.freq = freq;
	tea->injection = 1;
	set_frequency (clnt, freq + 450);
	tea->injector_state = INJECTOR_TRY_HI;
	tea->injector_data.sync = sync;

	if (sync) {
		mdelay (tune_delay_ms);
		tea5767_timer_kevent (clnt);
		mdelay (tune_delay_ms);
		tea5767_timer_kevent (clnt);
		mdelay (tune_delay_ms);
		tea5767_timer_kevent (clnt);
	} else
		mod_timer (&tea->timer, jiffies + (tune_delay_ms * HZ) / 1000);
}

/******************************************************************************
 * Tune to specified frequency, working out correct HI/LO side injection
 ******************************************************************************/
static int
tea5767_tune_to (struct i2c_client *clnt, int freq, int sync)
{
	struct tea5767_data *tea = clnt->data;
	DPRINTK (PREFIX "tune to %d\n", freq);

	if (tea->injector_state != INJECTOR_IDLE)
		return -EBUSY;

	tea5767_cancel_scan (tea);
	calculate_hi_lo_injection (clnt, freq, sync);

	return 0;
}

static int
tea5767_set_mono (struct i2c_client *clnt, int mono)
{
	struct tea5767_data *tea = clnt->data;

	tea->regs[2] &= ~TEA5767_REG2_MS;
	if (mono)
		tea->regs[2] |= TEA5767_REG2_MS;

	return tea5767_write_sync (clnt);
}

/***********************************************************************/

static struct tea5767_packet *
tea5767_get_packet (struct tea5767_data *tea)
{
	struct tea5767_packet *r;
	int new_inp = ((tea->inp + 1) % 16);

	if (new_inp == tea->outp)
		return NULL;

	r = &tea->output_buffer[tea->inp];

	return r;
}

static void
tea5767_write_packet (struct tea5767_data *tea)
{
	tea->inp = ((tea->inp + 1) % 16);
	wake_up_interruptible (&tea->wait_queue);
}

static void
tea5767_int_handler (int irq, void *dev, struct pt_regs *regs)
{
	struct tea5767_data *tea = dev;

	DPRINTK (PREFIX "IRQ (j=%ld)\n", jiffies);
	schedule_work (&tea->work);
}

/****************************************************************************
 * Returns the current frequency in kHz
 ****************************************************************************/
static int
get_freq_in_khz (struct tea5767_data *tea)
{
	unsigned int pll = (tea->status[0] << 8 | tea->status[1]) & 0x3fff;

	return pll_to_khz (pll, tea->f_ref, tea->injection);
}

/****************************************************************************
 * This gets scheduled when an interrupt triggers (via schedule_work)
 ****************************************************************************/
static void
tea5767_kevent (void *data)
{
	struct i2c_client *clnt = data;
	struct tea5767_data *tea = clnt->data;
	int iResult = 0;

	// Read registers 
	tea5767_read_sync (clnt);
	dump_status (clnt);

	int iReadyFlag = tea->status[0] & (1 << 7);
	int iBandLimitFlag = tea->status[0] & (1 << 6);;
	unsigned int uiRSSI = tea->status[3] >> 4;
	int iFrequency = get_freq_in_khz (tea);

	if (tea->scanning) {
		if (iReadyFlag) {
			if (iBandLimitFlag) {
				if (tea->scan_wrapped) {
					// This is the second time we hit the band limit.  Give up.
					iResult = 1;
					DPRINTK (PREFIX "Seek complete, no more stations (freq=%d)\n", iFrequency);
				} else {
					// Wrap to the other end of the band and try again
					DPRINTK (PREFIX "Seek hit band limit, wrapping and re-trying d=%d\n", tea->scan_direction);
					if (tea->scan_direction)
						tea5767_tune_to (clnt, tea->band_limit_low, 1);
					else
						tea5767_tune_to (clnt, tea->band_limit_high, 1);

					tea5767_seek (clnt, tea->scan_direction);
					tea->scan_wrapped = 1;
					return;
				}
			} else {
				iResult = 0;
				DPRINTK (PREFIX "Station found (freq=%d)\n", iFrequency);
			}
		}

		/* Send packet back to user space */
		struct tea5767_packet *outpkt;
		outpkt = tea5767_get_packet (tea);
		if (outpkt) {
			outpkt->code = TEA5767_PACKET_SEEK_STATUS;
			outpkt->u.seek.status = iResult;
			outpkt->u.seek.frequency = iFrequency;
			outpkt->u.seek.rssi = uiRSSI;
			tea5767_write_packet (tea);
		}

		/* Cancel scan and update freq */
		tea5767_cancel_scan (tea);
		tea->regs[0] = tea->status[0] & 0x3f;
		tea->regs[1] = tea->status[1];
		tea5767_write_sync (clnt);
	}
}

/****************************************************************************
 * Called by the timer to do hi/lo injection stuff
 ****************************************************************************/
static void
tea5767_timer_kevent (void *data)
{
	struct i2c_client *clnt = data;
	struct tea5767_data *tea = clnt->data;

	switch (tea->injector_state) {
	case INJECTOR_TRY_HI:
		tea5767_read_sync (clnt);
		tea->injector_data.iLevelHigh = (tea->status[3] & 0xf0) >> 4;
		tea->injector_data.iIFChigh = tea->status[2] & 0x7f;
		set_frequency (clnt, tea->injector_data.freq - 450);
		tea->injector_state = INJECTOR_TRY_LO;
		if (tea->injector_data.sync == 0)
			mod_timer (&tea->timer, jiffies + (tune_delay_ms * HZ) / 1000);
		break;

	case INJECTOR_TRY_LO:
		tea5767_read_sync (clnt);
		tea->injector_data.iLevelLow = (tea->status[3] & 0xf0) >> 4;
		tea->injector_data.iIFClow = tea->status[2] & 0x7f;

		if (tea->injector_data.iLevelHigh < tea->injector_data.iLevelLow)
			tea->injection = 1;
		else
			tea->injection = 0;

		DPRINTK (PREFIX "  h=%d l=%d ifc[h=%02x l=%02x] i=%d\n", tea->injector_data.iLevelHigh, tea->injector_data.iLevelLow,
			 tea->injector_data.iIFChigh, tea->injector_data.iIFClow, tea->injection);
		set_frequency (clnt, tea->injector_data.freq);

		tea->injector_state = INJECTOR_SELECTED_HI_LO;
		if (tea->injector_data.sync == 0)
			mod_timer (&tea->timer, jiffies + (tune_delay_ms * HZ) / 1000);
		break;

	case INJECTOR_SELECTED_HI_LO:
		tea->injector_state = INJECTOR_IDLE;

		if (tea->injector_data.sync == 0) {
			/* Send packet back to user space */
			struct tea5767_packet *outpkt;
			outpkt = tea5767_get_packet (tea);
			if (outpkt) {
				outpkt->code = TEA5767_PACKET_TUNE_COMPLETE;
				outpkt->u.seek.frequency = tea->injector_data.freq;
				tea5767_write_packet (tea);
			}
		}
		break;
	default:
		break;
	}
}


static void
tea5767_timer_func (unsigned long data)
{
	struct tea5767_data *tea = (struct tea5767_data *) data;

	schedule_work (&tea->timer_work);
}

/***********************************************************************/

static struct file_operations reciva_tea5767_fops = {
	owner:		THIS_MODULE,
	ioctl:		tea5767_ioctl,
	read:		tea5767_read,
	poll:		tea5767_poll,
	open:		tea5767_open,
	release:	tea5767_release,
};

static struct miscdevice tea5767_miscdev = {
	MISC_DYNAMIC_MINOR,
	"tea5767",
	&reciva_tea5767_fops
};

static unsigned int
tea5767_poll (struct file *file, poll_table * wait)
{
	struct i2c_client *clnt = file->private_data;
	struct tea5767_data *tea = clnt->data;

	poll_wait (file, &tea->wait_queue, wait);
	if (tea->inp != tea->outp)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int
tea5767_read (struct file *filp, char *buffer, size_t count, loff_t * ppos)
{
	struct i2c_client *clnt = filp->private_data;
	struct tea5767_data *tea = clnt->data;
	int r, bcount;

	/* Wait until there is something in the buffer */
	while (tea->inp == tea->outp) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		if (signal_pending (current))
			return -EINTR;

		interruptible_sleep_on (&tea->wait_queue);
	}

	bcount = MIN (count, sizeof (struct tea5767_packet));

	/* Send data to user application */
	r = (copy_to_user (buffer, &tea->output_buffer[tea->outp], bcount)) ? -EFAULT : bcount;

	tea->outp = (tea->outp + 1) % 16;

	return r;
}

static int
tea5767_open (struct inode *inode, struct file *file)
{
	struct i2c_client *clnt = g_clnt;
	struct tea5767_data *tea = clnt->data;
	unsigned long flags;
	unsigned long x;
	int r;

	file->private_data = clnt;

	init_timer (&tea->timer);
	tea->timer.function = tea5767_timer_func;
	tea->timer.data = (unsigned long) tea;

	r = tea5767_init_chip (clnt);
	if (r)
		return r;

	tea->inp = tea->outp = 0;
	tea->injector_state = INJECTOR_IDLE;

	save_flags_cli (flags);
	x = __raw_readl (S3C2410_EXTINT1);
	x &= ~(7 << 20);	// rising edge trigger
	x |= 4 << 20;
	__raw_writel (x, S3C2410_EXTINT1);
	restore_flags (flags);

	if (request_irq (tea->irq, tea5767_int_handler, 0, "Tea5767", clnt->data)) {
		printk ("couldn't request Tea5767 interrupt line\n");
		return -EBUSY;
	}

	return 0;
}

static int
tea5767_release (struct inode *inode, struct file *file)
{
	struct i2c_client *clnt = file->private_data;
	struct tea5767_data *tea = clnt->data;

	del_timer (&tea->timer);

	flush_scheduled_work ();

	tea5767_shutdown_chip (clnt);

	free_irq (tea->irq, clnt->data);

	return 0;
}

#ifdef DEBUG
static void
dump_status (struct i2c_client *clnt)
{
	struct tea5767_data *tea = clnt->data;
	DPRINTK (PREFIX "STATUS ");

	tea5767_read_sync (clnt);
	int iReadyFlag = tea->status[0] & (1 << 7);
	int iBandLimitFlag = tea->status[0] & (1 << 6);;
	unsigned int uiRSSI = tea->status[3] >> 4;
	int iFrequency = get_freq_in_khz (tea);
	DPRINTK ("rf=%d ", iReadyFlag);
	DPRINTK ("bf=%d ", iBandLimitFlag);
	DPRINTK ("r=%d ", uiRSSI);
	DPRINTK ("f=%d ", iFrequency);
	DPRINTK ("[%02x %02x %02x %02x %02x]\n", tea->status[0], tea->status[1], tea->status[2], tea->status[3], tea->status[4]);
}
#endif

static int
tea5767_seek (struct i2c_client *clnt, int direction)
{
	// Seek tune on this chip doesn't work reliably
	return 0;

	DPRINTK (PREFIX "seek up=%d\n", direction);
	struct tea5767_data *tea = clnt->data;
	dump_status (clnt);

	/* Ensure we start at the current frequency */
	tea5767_read_sync (clnt);
	tea->regs[0] = tea->status[0] & 0x3f;
	tea->regs[1] = tea->status[1];

	if (direction)
		tea->regs[2] |= (1 << 7);
	else
		tea->regs[2] &= ~(1 << 7);

	tea->scan_direction = direction;
	tea->scan_wrapped = 0;
	tea->scanning = 1;

	tea->regs[0] |= (1 << 6);	// seek on
	tea->regs[0] |= (1 << 7);	// mute

	int s = tea5767_write_sync (clnt);
	return s;
}

/******************************************************************************
 * Toggles the hi/lo bit and retunes to same freq
 ******************************************************************************/
static int
tea5767_toggle_hi_lo_bit (struct i2c_client *clnt)
{
	DPRINTK (PREFIX "toggle HI/LO\n");
	struct tea5767_data *tea = clnt->data;
	int freq = get_freq_in_khz (tea);

	tea->injection ^= 1;
	set_frequency (clnt, freq);
	mdelay (tune_delay_ms);
	return 0;
}

static int
tea5767_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *clnt = file->private_data;
	struct tea5767_data *tea = clnt->data;
	int data, dir, freq;

	switch (cmd) {
	case IOC_TEA5767_TUNE:
		if (get_user (data, (int *) arg))
			return -EFAULT;
		return tea5767_tune_to (clnt, data, 1);

	case IOC_TEA5767_TUNE_ASYNC:
		if (get_user (data, (int *) arg))
			return -EFAULT;
		return tea5767_tune_to (clnt, data, 0);

	case IOC_TEA5767_STATUS:
		tea5767_read_sync (clnt);
		return put_user (tea->status[0] | (tea->status[1] << 8) | (tea->status[2] << 16) | (tea->status[3] << 24), (int *) arg);

	case IOC_TEA5767_GET_FREQ:
		freq = get_freq_in_khz (tea);
		return put_user (freq, (int *) arg);

	case IOC_TEA5767_SEEK:
		if (get_user (dir, (int *) arg))
			return -EFAULT;
		return tea5767_seek (clnt, dir);

	case IOC_TEA5767_MONO:
		if (get_user (data, (int *) arg))
			return -EFAULT;
		return tea5767_set_mono (clnt, data);

	case IOC_TEA5767_TOGGLE_HI_LO_BIT:
		return tea5767_toggle_hi_lo_bit (clnt);
	}

	return -ENODEV;
}

/***********************************************************************/

static struct i2c_client client_template = {
	name:	"(unset)",
	flags:	I2C_CLIENT_ALLOW_USE,
	driver:	&tea5767
};

/*****************************************************************************
 * Provide data to file in /proc about current status
 *****************************************************************************/
static int
read_procmem (char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	struct i2c_client *clnt = data;
	struct tea5767_data *tea = clnt->data;
	int i, o = 0;

	tea5767_read_sync (clnt);

	for (i = 0; i < 5; i++) {
		o += sprintf (buf + o, "%02x: %08x\n", i, tea->status[i]);
	}

	if (o <= offset + count)
		*eof = 1;

	*start = buf + offset;

	o -= offset;
	if (o > count)
		o = count;
	if (o < 0)
		o = 0;

	return o;
}

static int
tea5767_attach (struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
{
	struct tea5767_data *tea;
	struct i2c_client *clnt;

	if (adap != i2c_bit_s3c2410) {
		printk ("tea5767: can't attach to unknown adapter\n");
		return -ENODEV;
	}

	clnt = kmalloc (sizeof (*clnt), GFP_KERNEL);
	memcpy (clnt, &client_template, sizeof (*clnt));
	clnt->adapter = adap;
	clnt->addr = addr;
	strcpy (clnt->name, "tea5767");

	tea = kmalloc (sizeof (*tea), GFP_KERNEL);
	if (!tea)
		return -ENOMEM;

	memset (tea, 0, sizeof (*tea));

	init_waitqueue_head (&tea->wait_queue);
	INIT_WORK (&tea->work, tea5767_kevent, clnt);
	INIT_WORK (&tea->timer_work, tea5767_timer_kevent, clnt);

	switch (board_type) {
	case 0:
	case 1:
		tea->irq = IRQ_EINT13;
		break;
	case 2:
		tea->irq = IRQ_EINT14;
		break;
	}

	clnt->data = tea;

	i2c_attach_client (clnt);

	printk ("tea5767: attached at address %x\n", addr);

	/* Register the device */
	misc_register (&tea5767_miscdev);

	g_clnt = clnt;

	/* Initialise proc entry */
	tea->procent = create_proc_read_entry ("driver/tea5767", 0, NULL, read_procmem, clnt);

	return 0;
}

static int
tea5767_detach_client (struct i2c_client *clnt)
{
	g_clnt = NULL;

	remove_proc_entry ("driver/tea5767", NULL);

	/* Unregister the device */
	misc_deregister (&tea5767_miscdev);

	i2c_detach_client (clnt);

	kfree (clnt->data);
	kfree (clnt);

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x60, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { I2C_CLIENT_END };
static unsigned short probe[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[] = { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c, normal_i2c_range,
	probe, probe_range,
	ignore, ignore_range,
	force
};

static int
tea5767_attach_adapter (struct i2c_adapter *adap)
{
	return i2c_probe (adap, &addr_data, tea5767_attach);
}

static void
tea5767_inc_use (struct i2c_client *clnt)
{
	MOD_INC_USE_COUNT;
}

static void
tea5767_dec_use (struct i2c_client *clnt)
{
	MOD_DEC_USE_COUNT;
}

static struct i2c_driver tea5767 = {
	name:		TEA5767_NAME,
	id:		I2C_DRIVERID_TEA5767,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	tea5767_attach_adapter,
	detach_client:	tea5767_detach_client,
	inc_use:	tea5767_inc_use,
	dec_use:	tea5767_dec_use
};

static void
setup_gpio (void)
{
	// RDSINT
	// (J2-3 GPG5)
	s3c2410_gpio_cfgpin (S3C2410_GPG5, S3C2410_GPG5_EINT13);
	s3c2410_gpio_pullup (S3C2410_GPG5, 1);
}

static int __init
tea5767_init (void)
{
	printk ("tea5767: init\n");
	printk ("tea5767:   tune_delay_ms=%d\n", tune_delay_ms);

	setup_gpio ();

	return i2c_add_driver (&tea5767);
}

static void __exit
tea5767_exit (void)
{
	i2c_del_driver (&tea5767);
}

module_init (tea5767_init);
module_exit (tea5767_exit);

MODULE_AUTHOR ("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION ("TEA5767 FM tuner driver");
MODULE_LICENSE ("GPL");
