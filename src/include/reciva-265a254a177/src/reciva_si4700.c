/*
 * Si4700 tuner driver
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
#include <linux/version.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/arch/regs-irq.h>
#endif

#include "reciva_util.h"
#include "reciva_si4700.h"

static int si4700_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int si4700_poll (struct file *file, poll_table * wait);
static int si4700_read (struct file *filp, char *buffer, size_t count, loff_t * ppos);
static int si4700_open (struct inode *inode, struct file *file);
static int si4700_release (struct inode *inode, struct file *file);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t si4700_int_handler (int irq, void *dev);
#else
static void si4700_int_handler (int irq, void *dev, struct pt_regs *regs);
#endif

#define SI4700_NAME	"si4700"
#define DEFAULT_CHANNEL_SPACING		50	/* kHz */
#define DEFAULT_BASE_FREQ		87500	/* 87.5MHz */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define GET_CLIENT_DATA i2c_get_clientdata(clnt)
#else
#define GET_CLIENT_DATA clnt->data
#endif
extern struct i2c_adapter *i2c_bit_s3c2410;

static struct i2c_driver si4700;

static struct i2c_client *g_clnt;

struct si4700_data {
	struct work_struct work;
	struct i2c_client *client;
	unsigned short regs[16];

	wait_queue_head_t wait_queue;	/* Used for blocking read */
	wait_queue_head_t tune_queue;	/* Used for blocking read */
	struct si4700_packet output_buffer[16];	/* Stores data to write out of device */
	int inp, outp;		/* The number of bytes in the buffer */
	int irq;
	int channel_spacing;
	int base_freq;
	int seek_snr_threshold;
	int seek_impulse_threshold;
	int seek_rssi;
	int irqpin;

	char use_rds;
	char powerdown;
	char scanning;
	char tuning;
        char async_tune;
        char tune_complete;
        char have_cp;

	wait_queue_head_t tune_wait_queue;	/* Used to serialise tuning operations */

	struct proc_dir_entry *procent;
};

#define Si4700_STC		(1 << 14)
#define Si4700_RDSR		(1 << 15)

#define DEFAULT_SNR_THRESHOLD		4
#define DEFAULT_IMPULSE_THRESHOLD	4
#define DEFAULT_RSSI_THRESHOLD		8

// Different boards use different GPIO pins:
// |   | nFMRESET      | RDSINT       |
// | 0 | J2-7 (GPG7)   | J2-3 (GPG5)  |
// | 1 | J1-20 (GPB9)  | J2-3 (GPG5)  |
// | 2 | J1-18 (GPB7)  | J2-5 (GPG6)  |
// | 3 | J1-18 (GPB7)  | J2-3 (GPG5)  |
// | 4 | J1-16 (GPB0)  | J2-3 (GPG5)  |
// | 5 | INT   (GPB6)  | INT  (GPF4)  |
// | 6 | J1-13 (GPE6)  | J2-3 (GPG5)  |
static int board_type;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (board_type, int, S_IRUGO);
#else
MODULE_PARM (board_type, "i");
#endif

static int xoscen;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (xoscen, int, S_IRUGO);
#else
MODULE_PARM (xoscen, "i");
#endif

static int blending_spread = 19;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (blending_spread, int, S_IRUGO);
#else
MODULE_PARM (blending_spread, "i");
#endif

#define SI4700_POWERCFG_SEEK	(1 << 8)
#define SI4700_POWERCFG_SEEKUP	(1 << 9)
#define SI4700_CHAN_TUNE	(1 << 15)

#define SI4700_CMD_SET_PROPERTY                 0x07
#define SI4700_CMD_GET_PROPERTY                 0x08
#define SI4700_CMD_VERIFY_COMMAND               0xff

#define SI4700_PROP_BLEND_STEREO_RSSI           0x301
#define SI4700_PROP_BLEND_MONO_RSSI             0x300

#define MIN(a,b) ( (a)<(b) ? (a):(b) )

static int reset_pin;

/***********************************************************************/

static int si4700_get_frequency (struct i2c_client *clnt, int *freq);

static void
si4700_hard_reset (void)
{
	s3c2410_gpio_setpin (reset_pin, 0);

	// must have SDIO low during rising edge on Si4700 nRST
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	unsigned int pin = s3c2410_gpio_getpin (S3C2410_GPE15);
	s3c2410_gpio_setpin (S3C2410_GPE15, 0);
	unsigned int pincfg = s3c2410_gpio_getcfg (S3C2410_GPE15);
	s3c2410_gpio_cfgpin (S3C2410_GPE15, S3C2410_GPE15_OUTP);
#else
	struct i2c_client dummy_client;
	dummy_client.adapter = i2c_bit_s3c2410;
	i2c_control (&dummy_client, I2C_BIT_LOCK, 0);
	i2c_control (&dummy_client, I2C_BIT_SETSDA, 0);
#endif

	mdelay (1);
	s3c2410_gpio_setpin (reset_pin, 1);
	mdelay (1);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	s3c2410_gpio_cfgpin (S3C2410_GPE15, pincfg);
	s3c2410_gpio_setpin (S3C2410_GPE15, pin);
#else
	i2c_control (&dummy_client, I2C_BIT_SETSDA, 1);
	i2c_control (&dummy_client, I2C_BIT_UNLOCK, 0);
#endif
}

/* returned data starts at address 0xa */
static int
si4700_chip_read (struct i2c_client *clnt, int nregs, unsigned short *data)
{
	int r, i;

	r = i2c_master_recv (clnt, (unsigned char *) data, nregs * 2);
	if (r != nregs * 2) {
		printk (KERN_ERR "si4700: read failed, status %d\n", r);
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

	r = i2c_master_send (clnt, (unsigned char *) sdata, nregs * 2);
	if (r != nregs * 2) {
		printk (KERN_ERR "si4700: write failed, status %d\n", r);
		return r;
	}

	return 0;
}

static int
si4700_read_regs (struct i2c_client *clnt, int nregs)
{
	unsigned short regs[16];
	int i, r;
	struct si4700_data *si = GET_CLIENT_DATA;

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
	struct si4700_data *si = GET_CLIENT_DATA;

	for (i = 0; i < nregs; i++) {
		regs[i] = si->regs[(i + 2) % 16];
	}

	return si4700_chip_write (clnt, nregs, regs);
}

static int
wait_seek_complete (struct si4700_data *si)
{
	int status;

	printk("waiting for seek to complete\n");

	do {
		si4700_read_regs (si->client, 1);
		status = si->regs[0xa];
		printk("status %x\n", status);

		yield ();
	} while (status & Si4700_STC);

	printk("STC clear, continuing\n");

	return 0;
}

static inline void
si4700_set_command (struct si4700_data *si, u8 cmd, u8 arg0, u8 arg1, u8 arg2, u8 arg3, u8 arg4, u8 arg5, u8 arg6)
{
        si->regs[0xc] = (arg0 << 8) | arg1;
        si->regs[0xd] = (arg2 << 8) | arg3;
        si->regs[0xe] = (arg4 << 8) | arg5;
        si->regs[0xf] = (arg6 << 8) | cmd;
}

static int
si4700_set_property (struct i2c_client *clnt, int prop, int val)
{
	struct si4700_data *si = GET_CLIENT_DATA;
        int old_rds = 0;

        printk ("si4700: write property %x to %d\n", prop, val);
 
        if (si->have_cp == 0)
                return -ENOSYS;

        if (si->use_rds) {
                // Must turn off RDS to use the command processor
                old_rds = (si->regs[4] & (1 << 12)) ? 1 : 0;
                if (old_rds) {
                        si->regs[4] &= ~(1 << 12);
                        si4700_write_regs (clnt, 3);
                }
        }

        si4700_set_command (si, SI4700_CMD_VERIFY_COMMAND, 0, 0, 0, 0, 0, 0, 0);
        si4700_write_regs (clnt, 14);
        do {
                si4700_read_regs (clnt, 6);
                schedule ();
        } while (si->regs[0xf] & 0xff);
        
        si4700_set_command (si, SI4700_CMD_GET_PROPERTY, 0, 0, 0, 0, prop >> 8, prop & 0xff, 0);
        si4700_write_regs (clnt, 14);
        do {
                si4700_read_regs (clnt, 6);
                schedule ();
        } while (si->regs[0xf] & 0xff);
        printk ("si4700: before setting, regs %04x %04x %04x %04x\n", si->regs[0xc], si->regs[0xd], si->regs[0xe], si->regs[0xf]);

        si4700_set_command (si, SI4700_CMD_SET_PROPERTY, val >> 8, val & 0xff, 0, 0, prop >> 8, prop & 0xff, 0);
        si4700_write_regs (clnt, 14);
        do {
                si4700_read_regs (clnt, 6);
                schedule ();
        } while (si->regs[0xf] & 0xff);

        si4700_set_command (si, SI4700_CMD_GET_PROPERTY, 0, 0, 0, 0, prop >> 8, prop & 0xff, 0);
        si4700_write_regs (clnt, 14);
        do {
                si4700_read_regs (clnt, 6);
                schedule ();
        } while (si->regs[0xf] & 0xff);
        printk ("si4700: after setting, regs %04x %04x %04x %04x\n", si->regs[0xc], si->regs[0xd], si->regs[0xe], si->regs[0xf]);

        // Put RDS back
        if (old_rds) {
                si->regs[4] |= (1 << 12);
                si4700_write_regs (clnt, 3);
        }

        return 0;
}

static int
si4700_tune_to (struct i2c_client *clnt, int freq, int sync)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int chan, r;

#if 0
	si->regs[0x2] &= ~SI4700_POWERCFG_SEEK;
	si->regs[0x3] &= ~SI4700_CHAN_TUNE;
	r = si4700_write_regs (clnt, 2);

	r = wait_seek_complete (si);
	if (r)
		return r;
#endif

	if (freq < si->base_freq)
		return -EINVAL;

	chan = (freq - si->base_freq) / si->channel_spacing;
	if (chan > 0x3ff)
		return -EINVAL;

	si->tuning = 1;
        si->async_tune = sync ? 0 : 1;
        si->tune_complete = 0;

	si->regs[4] &= ~(1 << 12);	// RDS off
	si->regs[3] &= ~SI4700_CHAN_TUNE;
	r = si4700_write_regs (clnt, 4);
	if (r)
		return r;
	si->regs[3] = SI4700_CHAN_TUNE | chan;
	r = si4700_write_regs (clnt, 4);
        if (r)
                return r;
        
        if (sync) {
#if 1
                /* workaround for lost irq bug */
                while (!si->tune_complete) {
                        interruptible_sleep_on_timeout (&si->tune_queue, HZ / 10);

                        if (!si->tune_complete) {
                                int freq;
                                si4700_get_frequency (clnt, &freq);
                        }
                }
#else
                interruptible_sleep_on (&si->tune_queue);
#endif
        }
        
        return r;
}

static int
si4700_seek_stop (struct i2c_client *clnt)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int r;

	si->regs[0x2] &= ~SI4700_POWERCFG_SEEK;

	r = si4700_write_regs (clnt, 4);

	if (r == 0)
		si->scanning = 0;

	return r;
}

static int
si4700_seek (struct i2c_client *clnt, int direction)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int r;

	si->regs[0x2] &= ~SI4700_POWERCFG_SEEK;
	si->regs[0x3] &= ~SI4700_CHAN_TUNE;
	r = si4700_write_regs (clnt, 2);

	r = wait_seek_complete (si);
	if (r)
		return r;

	if (direction)
		si->regs[0x2] |= SI4700_POWERCFG_SEEKUP;
	else
		si->regs[0x2] &= ~SI4700_POWERCFG_SEEKUP;

	si->scanning = 1;

	si->regs[0x2] |= SI4700_POWERCFG_SEEK;	// seek on

	// Update seek thresholds also
	return si4700_write_regs (clnt, 5);
}

static int
si4700_set_seek_params (struct i2c_client *clnt, int snr_threshold, int impulse_threshold, int rssi_threshold)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	si->seek_snr_threshold = snr_threshold;
	si->seek_impulse_threshold = impulse_threshold;
	si->seek_rssi = rssi_threshold;

	si->regs[5] &= ~0xff00;
	si->regs[5] |= (si->seek_rssi << 8);
	si->regs[6] = ((si->seek_snr_threshold & 0xf) << 4) | (si->seek_impulse_threshold & 0xf);

	return si4700_write_regs (clnt, 5);
}

static int
si4700_set_volume (struct i2c_client *clnt, int vol)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	si->regs[5] &= ~15;
	si->regs[5] |= vol & 15;
	return si4700_write_regs (clnt, 4);
}

static int
si4700_set_channel_spacing (struct i2c_client *clnt, int spacing)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int newval;

	switch (spacing) {
	case 50:
		newval = 0x20;
		break;
	case 100:
		newval = 0x10;
		break;
	case 200:
		newval = 0x00;
		break;
	default:
		return -EINVAL;
	}

	si->regs[5] &= ~0x30;
	si->regs[5] |= newval;

	si->channel_spacing = spacing;

	return si4700_write_regs (clnt, 4);
}

static int
si4700_set_band (struct i2c_client *clnt, int band)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	switch (band) {
	case 0:		// US/Europe
		si->base_freq = 87500;
		break;
	case 1:		// Japan wideband
	case 2:		// Japan
		si->base_freq = 76000;
		break;
	default:
		return -EINVAL;
	}
	si->regs[0x5] &= ~(3 << 6);
	si->regs[0x5] |= band << 6;

	return si4700_write_regs (clnt, 4);
}

static int
si4700_set_stereo_blending (struct i2c_client *clnt, int blending)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	if (blending < 0 || blending > 3)
		return -EINVAL;

	si->regs[4] &= ~0xc0;
	si->regs[4] |= (blending << 6);

	return si4700_write_regs (clnt, 4);
}

static int
si4700_set_stereo_blending_db (struct i2c_client *clnt, int blending)
{
	struct si4700_data *si = GET_CLIENT_DATA;

        if (si->have_cp == 0) {
                /* This is a rev B or older chip with no command processor.  */
                int blendval = 0;
                switch (blending) {
                case 31:        blendval = 0; break;    /* 0dB */
                case 37:        blendval = 1; break;    /* +6dB */
                case 19:        blendval = 2; break;    /* -12dB */
                case 25:        blendval = 3; break;    /* -6dB */
                default:        return -EINVAL;         /* Anything else */
                }

                return si4700_set_stereo_blending (clnt, blendval);
        }

        if (blending < 0 || blending > 255)
                return -EINVAL;

        int r;
        r = si4700_set_property (clnt, SI4700_PROP_BLEND_MONO_RSSI, blending);
        if (r)
                return r;
        blending += blending_spread;
        if (blending > 255)
                blending = 255;
        return si4700_set_property (clnt, SI4700_PROP_BLEND_STEREO_RSSI, blending);
}

static int
si4700_force_mono (struct i2c_client *clnt, int mono)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	if (mono)
		si->regs[2] |= 1 << 13;
	else
		si->regs[2] &= ~(1 << 13);

	return si4700_write_regs (clnt, 2);
}

static int
si4700_mute (struct i2c_client *clnt, int mute)
{
	struct si4700_data *si = GET_CLIENT_DATA;

  // Control reg 2 bit 14 = Mute disable
	if (mute)
		si->regs[2] &= ~(1 << 14);
	else
		si->regs[2] |= 1 << 14;

	return si4700_write_regs (clnt, 2);
}

static int
si4700_set_deemphasis (struct i2c_client *clnt, int usa_mode)
{
	struct si4700_data *si = GET_CLIENT_DATA;

	if (usa_mode)
		si->regs[4] &= ~(1 << 11);
	else
		si->regs[4] |= 1 << 11;

	return si4700_write_regs (clnt, 4);
}

static int
si4700_get_frequency (struct i2c_client *clnt, int *freq)
{
	struct si4700_data *si = GET_CLIENT_DATA;

        si4700_read_regs (clnt, 2);
        *freq = ((si->regs[0xb] & 0x3ff) * si->channel_spacing) + si->base_freq;

	if (si->regs[0xa] & Si4700_STC) {
#ifdef DEBUG_IRQ
                printk ("requesting softirq\n");
#endif
                schedule_work (&si->work);
        }
        return 0;
}

static int
si4700_init_chip (struct i2c_client *clnt)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int r;

        if (xoscen) {
                si->regs[2] = 0x0000;
                si->regs[3] = 0x0000;
                si->regs[4] = 0x0000;
                si->regs[5] = 0x0000;
                si->regs[6] = 0x0000;
                si->regs[7] = 0x8100;
                r = si4700_write_regs (clnt, 6);
                if (r)
                        return r;
                
                mdelay (500);
        }

	si->regs[2] = 0x4801;
	si->regs[3] = 0x0000;
	si->regs[4] = 0xC804;
	si->regs[5] = (si->seek_rssi << 8) | 0x20;
	si->regs[6] = ((si->seek_snr_threshold & 0xf) << 4) | (si->seek_impulse_threshold & 0xf);

	r = si4700_write_regs (clnt, 5);
	if (r)
		return r;

	mdelay (220);

	r = si4700_read_regs (clnt, 16);
	if (r)
		return r;

	printk ("si4700: device id %04x, %04x\n", si->regs[0], si->regs[1]);

        if (si->regs[0] == 0x1242) {
                /* This is Si4702/03.  Check the chip revision.  */
                int rev = ((si->regs[1] & 0xFC00) >> 10);
                if (rev >= 0x4) {
                        printk ("Found Si4702/03 revision C or later.  Command processor enabled.\n");
                        si->have_cp = 1;
                } else {
                        printk ("Found Si4702/03 revision A/B (%d).  No command processor in this version.\n", rev);
                }
        }

	return 0;
}

static int
si4700_power_down (struct i2c_client *clnt)
{
	struct si4700_data *si = GET_CLIENT_DATA;
	int r;

	si->regs[2] &= ~SI4700_POWERCFG_SEEK;
	si->regs[3] &= ~SI4700_CHAN_TUNE;
	si4700_write_regs (clnt, 2);

	r = wait_seek_complete (si);
	if (r)
		return r;

        if (si->use_rds) {
		si->regs[4] &= ~1 << 12;	// make sure RDS is off
		r = si4700_write_regs (clnt, 4);
		if (r)
			return r;
	}

	si->regs[2] = 0x0041;

	r = si4700_write_regs (clnt, 4);
	if (r)
		return r;

	// turn pullup on to stop pin floating
	s3c2410_gpio_pullup (si->irqpin, 0);

	si->powerdown = 1;

	return 0;
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t
si4700_int_handler (int irq, void *dev)
#else
static void
si4700_int_handler (int irq, void *dev, struct pt_regs *regs)
#endif
{
	struct si4700_data *si = dev;

	schedule_work (&si->work);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	return IRQ_HANDLED;
#endif
}

static void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
si4700_kevent (struct work_struct *work)
{
	struct si4700_data *si = container_of (work, struct si4700_data, work);
	struct i2c_client *clnt = si->client;
#else
si4700_kevent (void *data)
{
	struct i2c_client *clnt = data;
	struct si4700_data *si = GET_CLIENT_DATA;
#endif
	int status;

	si4700_read_regs (clnt, 6);

	status = si->regs[0xa];

	if (status & Si4700_STC) {
		si->regs[2] &= ~SI4700_POWERCFG_SEEK;
		si->regs[3] &= ~SI4700_CHAN_TUNE;
		if (si->use_rds) {
      si->regs[4] |= 1 << 12;	// RDS on
      struct si4700_packet *outpkt;
      outpkt = si4700_get_packet (si);
      if (outpkt) {
        outpkt->code = SI4700_PACKET_RDS_START;
        si4700_write_packet (si);
      }
    }
		si4700_write_regs (clnt, 3);

		if (si->scanning) {
			struct si4700_packet *outpkt;
			int result, channel, rssi;

			result = (status & (1 << 13)) ? 1 : 0;
			channel = si->regs[0xb] & 0x3ff;
			rssi = si->regs[0xa] & 0xff;

			if (result) {
				printk ("seek complete, no more stations\n");
			} else {
				printk ("station found at channel %d rssi %d\n", channel, rssi);
			}

			outpkt = si4700_get_packet (si);
			if (outpkt) {
				outpkt->code = SI4700_PACKET_SEEK_STATUS;
				outpkt->u.seek.status = result;
				outpkt->u.seek.frequency = (channel * si->channel_spacing) + si->base_freq;
				outpkt->u.seek.rssi = rssi;
				si4700_write_packet (si);
			}

			si->scanning = 0;
		} else if (si->tuning) {
                        printk ("si4700: tuning complete\n");
                        if (si->async_tune) {
                                struct si4700_packet *outpkt;
                                outpkt = si4700_get_packet (si);
                                if (outpkt) {
                                        outpkt->code = SI4700_PACKET_TUNE_COMPLETE;
                                        si4700_write_packet (si);
                                }
                        } else {
                                si->tune_complete = 1;
                                wake_up_interruptible (&si->tune_queue);
                        }
                        si->tuning = 0;
                }
	}

	if (status & Si4700_RDSR) {
		struct si4700_packet *outpkt;

		outpkt = si4700_get_packet (si);
		if (outpkt) {
			static const unsigned long rds_error_map[] = {
				0,	/* 0 errors */
				2 << 16,	/* 1-2 errors */
				5 << 16,	/* 3-5 errors */
				6 << 16,	/* 6 errors */
			};
			outpkt->code = SI4700_PACKET_RDS;
			outpkt->u.rds.a = si->regs[0xc];
			outpkt->u.rds.b = si->regs[0xd];
			outpkt->u.rds.c = si->regs[0xe];
			outpkt->u.rds.d = si->regs[0xf];

			if ((si->regs[0xa] & 0x600) == 0x600)
				outpkt->u.rds.a |= 0x80000000;
			if ((si->regs[0xb] & 0xc000) == 0xc000)
				outpkt->u.rds.b |= 0x80000000;
			if ((si->regs[0xb] & 0x3000) == 0x3000)
				outpkt->u.rds.c |= 0x80000000;
			if ((si->regs[0xb] & 0x0c00) == 0xc00)
				outpkt->u.rds.d |= 0x80000000;

			outpkt->u.rds.a |= rds_error_map[(si->regs[0xa] & 0x600) >> 9];
			outpkt->u.rds.b |= rds_error_map[(si->regs[0xb] & 0xc000) >> 14];
			outpkt->u.rds.c |= rds_error_map[(si->regs[0xb] & 0x3000) >> 12];
			outpkt->u.rds.d |= rds_error_map[(si->regs[0xb] & 0xc00) >> 10];

			si4700_write_packet (si);
		}
	}
}

/***********************************************************************/

static struct file_operations reciva_si4700_fops = {
	owner:		THIS_MODULE,
	ioctl:		si4700_ioctl,
	read:		si4700_read,
	poll:		si4700_poll,
	open:		si4700_open,
	release:	si4700_release,
};

static struct miscdevice si4700_miscdev = {
	MISC_DYNAMIC_MINOR,
	"si4700",
	&reciva_si4700_fops
};

static unsigned int
si4700_poll (struct file *file, poll_table * wait)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = GET_CLIENT_DATA;

	poll_wait (file, &si->wait_queue, wait);
	if (si->inp != si->outp)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int
si4700_read (struct file *filp, char *buffer, size_t count, loff_t * ppos)
{
	struct i2c_client *clnt = filp->private_data;
	struct si4700_data *si = GET_CLIENT_DATA;
	int r, bcount;

	/* Wait until there is something in the buffer */
	while (si->inp == si->outp) {
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		interruptible_sleep_on (&si->wait_queue);
	}

	bcount = MIN (count, sizeof (struct si4700_packet));

	/* Send data to user application */
	r = (copy_to_user (buffer, &si->output_buffer[si->outp], bcount)) ? -EFAULT : bcount;

	si->outp = (si->outp + 1) % 16;

	return r;
}

static spinlock_t init_lock = SPIN_LOCK_UNLOCKED;

static int
si4700_open (struct inode *inode, struct file *file)
{
	struct i2c_client *clnt = g_clnt;
	struct si4700_data *si = GET_CLIENT_DATA;
	unsigned long flags;
	unsigned long x;
	unsigned long extintreg = 0;
	int extintbit = -1, intpincfg = -1;
	int r;

  rutl_set_dab_fm_filter(DAB_FM_FILTER_FM);

	if (si->powerdown) {
		si4700_hard_reset ();
	}

	si->use_rds = 0;

	file->private_data = clnt;

	switch (si->irqpin) {
	case S3C2410_GPG0:
    extintreg = S3C2410_EXTINT1 + 0x10;
    extintbit = 0;
    intpincfg = S3C2410_GPG0_EINT8;
    break;
	case S3C2410_GPG1:
    extintreg = S3C2410_EXTINT1 + 0x10;
    extintbit = 4;
    intpincfg = S3C2410_GPG1_EINT9;
    break;
	case S3C2410_GPG5:
    extintreg = S3C2410_EXTINT1;
    extintbit = 20;
    intpincfg = S3C2410_GPG5_EINT13;
		break;
	case S3C2410_GPG6:
    extintreg = S3C2410_EXTINT1;
    extintbit = 24;
    intpincfg = S3C2410_GPG6_EINT14;
		break;
	case S3C2410_GPF4:
		// FIXME: S3C2412 has its EXTINT registers in a different location.
		// Need to fix this in the kernel support functions eventually
		extintreg = (S3C2410_EXTINT0 + 0x10);
		extintbit = 16;
		intpincfg = S3C2410_GPF4_EINT4;
		break;
	default:
		BUG ();
		break;
	}

	s3c2410_gpio_cfgpin (si->irqpin, intpincfg);

	r = si4700_init_chip (clnt);
	if (r)
		return r;

	si->powerdown = 0;

	// pullup off since Si470x uses totem pole drive
	s3c2410_gpio_pullup (si->irqpin, 1);

	si->inp = si->outp = 0;

	spin_lock_irqsave (init_lock, flags);

	// S3C2412 likes no pending interrupts before the request_irq
	// TODO: Needs more investigation
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (machine_is_rirm3()) {
		__raw_writel ((1 << 4), S3C2412_EINTPEND);
	}
#endif

	x = __raw_readl (extintreg);
	x &= ~(7 << extintbit);	// falling edge trigger
	x |= S3C2410_EXTINT_FALLEDGE << extintbit;
	__raw_writel (x, extintreg);
	spin_unlock_irqrestore (init_lock, flags);

	if (request_irq (si->irq, si4700_int_handler, 0, "Si4700", si)) {
		printk ("couldn't request Si4700 interrupt line\n");
		si4700_power_down (clnt);
		s3c2410_gpio_pullup (si->irqpin, 0);
		return -EBUSY;
	}

	return 0;
}

static int
si4700_release (struct inode *inode, struct file *file)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = GET_CLIENT_DATA;

	flush_scheduled_work ();

	s3c2410_gpio_pullup (si->irqpin, 0);
	free_irq (si->irq, si);

	si4700_power_down (clnt);

  rutl_set_dab_fm_filter(DAB_FM_FILTER_NONE);

	return 0;
}

static int
si4700_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *clnt = file->private_data;
	struct si4700_data *si = GET_CLIENT_DATA;
	int freq, vol, spacing, dir, seek_params, blending, deemph, band, rssi, mono, mute;
        int chip_id;

	switch (cmd) {
	case IOC_SI4700_TUNE:
		if (get_user (freq, (int *) arg))
			return -EFAULT;
		return si4700_tune_to (clnt, freq, 1);

	case IOC_SI4700_TUNE_ASYNC:
		if (get_user (freq, (int *) arg))
			return -EFAULT;
		return si4700_tune_to (clnt, freq, 0);

	case IOC_SI4700_SEEK:
		if (get_user (dir, (int *) arg))
			return -EFAULT;
		return si4700_seek (clnt, dir);

	case IOC_SI4700_STATUS:
		si4700_read_regs (clnt, 9);

		// Check POWERCFG::ENABLE
		// If clear then the chip has glitched and needs to be reset.
		if ((si->regs[0x2] & 1) == 0)
		{
			printk("si4700: Appears to have glitched. Resetting...\n");
			si4700_hard_reset();

			si->regs[2] = 0x4801;

			// Re-write registers 2 to 6 with their shadowed values
			si4700_write_regs (clnt, 5);
		}

    // AFC bit (indicating lock) is active low so flip bit 12 accordingly
		return put_user ((si->regs[0xa] & 0x11ff) ^ 0x1000, (int *) arg);

	case IOC_SI4700_VOLUME:
		if (get_user (vol, (int *) arg))
			return -EFAULT;
		return si4700_set_volume (clnt, vol);

	case IOC_SI4700_SPACING:
		if (get_user (spacing, (int *) arg))
			return -EFAULT;
		return si4700_set_channel_spacing (clnt, spacing);

	case IOC_SI4700_SEEK_PARAMS:
		if (get_user (seek_params, (int *) arg))
			return -EFAULT;
		return si4700_set_seek_params (clnt, seek_params & 0xf, (seek_params >> 4) & 0xf, (seek_params >> 8) & 0xff);

	case IOC_SI4700_READ_SEEK_PARAMS:
		seek_params = (si->seek_snr_threshold) | (si->seek_impulse_threshold << 4) | (si->seek_rssi << 8);
		return put_user (seek_params, (int *) arg);

	case IOC_SI4700_STEREO_BLENDING:
		if (get_user (blending, (int *) arg))
			return -EFAULT;
		return si4700_set_stereo_blending (clnt, blending);

	case IOC_SI4700_DEEMPHASIS:
		if (get_user (deemph, (int *) arg))
			return -EFAULT;
		return si4700_set_deemphasis (clnt, deemph);

	case IOC_SI4700_READ_FREQUENCY:
                si4700_get_frequency(clnt, &freq);
		return put_user (freq, (int *) arg);

	case IOC_SI4700_BAND:
		if (get_user (band, (int *) arg))
			return -EFAULT;
		return si4700_set_band (clnt, band);

	case IOC_SI4700_SEEK_STOP:
		return si4700_seek_stop (clnt);

	case IOC_SI4700_READ_RSSI:
		si4700_read_regs (clnt, 1);
		rssi = si->regs[0xa] & 0xff;
		return put_user (rssi, (int *) arg);

	case IOC_SI4700_FORCE_MONO:
		if (get_user (mono, (int *) arg))
			return -EFAULT;
		return si4700_force_mono (clnt, mono);

	case IOC_SI4700_MUTE:
		if (get_user (mute, (int *) arg))
			return -EFAULT;
		return si4700_mute (clnt, mute);

	case IOC_SI4700_ENABLE_RDS:
		if (get_user (si->use_rds, (int *) arg))
			return -EFAULT;
		return 0;
                
        case IOC_SI4700_STEREO_BLENDING_DB:
                if (get_user (blending, (int *) arg))
                        return -EFAULT;
                return si4700_set_stereo_blending_db (clnt, blending);

        case IOC_SI4700_HW_VERSION:
                chip_id = (si->regs[0] << 16) | si->regs[1];
                if (put_user (chip_id, (int *) arg))
                        return -EFAULT;
                return 0;

	case IOC_SI4704_START:
    return 0;
	}

	return -ENODEV;
}

/***********************************************************************/

static struct i2c_client client_template = {
	name:	"(unset)",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	flags:	I2C_CLIENT_ALLOW_USE,
#endif
	driver:	&si4700
};

/*****************************************************************************
 * Provide data to file in /proc about current status
 *****************************************************************************/
static int
read_procmem (char *buf, char **start, off_t offset, int count, int *eof, void *data)
{
	struct i2c_client *clnt = data;
	struct si4700_data *si = GET_CLIENT_DATA;
	int i, o = 0;

	si4700_read_regs (clnt, 16);

	for (i = 0; i < 16; i++) {
		o += sprintf (buf + o, "%02x: %08x\n", i, si->regs[i]);
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
si4700_attach (struct i2c_adapter *adap, int addr, int kind)
#else
si4700_attach (struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
#endif
{
	struct si4700_data *si;
	struct i2c_client *clnt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (strcmp (adap->name, "s3c2410-i2c")) {
#else
	if (adap != i2c_bit_s3c2410) {
#endif
		printk ("si4700: can't attach to unknown adapter\n");
		return -ENODEV;
	}

	clnt = kmalloc (sizeof (*clnt), GFP_KERNEL);
	memcpy (clnt, &client_template, sizeof (*clnt));
	clnt->adapter = adap;
	clnt->addr = addr;
	strcpy (clnt->name, "si4700");

	si = kmalloc (sizeof (*si), GFP_KERNEL);
	if (!si)
		return -ENOMEM;

	memset (si, 0, sizeof (*si));

	init_waitqueue_head (&si->wait_queue);
	init_waitqueue_head (&si->tune_queue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	INIT_WORK (&si->work, si4700_kevent);
#else
	INIT_WORK (&si->work, si4700_kevent, clnt);
#endif

	// figure out irq routing
	switch (board_type) {
	case 0:
	case 1:
	case 3:
	case 4:
	case 6:
    if (machine_is_rirm3())
    {
      si->irqpin = S3C2410_GPG0;
      si->irq = IRQ_EINT8;
    }
    else
    {
      si->irqpin = S3C2410_GPG5;
      si->irq = IRQ_EINT13;
    }

		break;
	case 2:
    if (machine_is_rirm3())
    {
      si->irqpin = S3C2410_GPG1;
      si->irq = IRQ_EINT9;
    }
    else
    {
      si->irqpin = S3C2410_GPG6;
      si->irq = IRQ_EINT14;
    }
		break;
	case 5:
		si->irqpin = S3C2410_GPF4;
		si->irq = IRQ_EINT4;
		break;
	default:
		BUG ();
	}

	si->client = clnt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	i2c_set_clientdata (clnt, si);
#else
	clnt->data = si;
#endif

	si->channel_spacing = DEFAULT_CHANNEL_SPACING;
	si->base_freq = DEFAULT_BASE_FREQ;
	si->seek_snr_threshold = DEFAULT_SNR_THRESHOLD;
	si->seek_impulse_threshold = DEFAULT_IMPULSE_THRESHOLD;
	si->seek_rssi = DEFAULT_RSSI_THRESHOLD;

	i2c_attach_client (clnt);

	printk ("si4700: attached at address %x\n", addr);

	/* Register the device */
	misc_register (&si4700_miscdev);

	g_clnt = clnt;

	/* Initialise proc entry */
	si->procent = create_proc_read_entry ("driver/si4700_regs", 0, NULL, read_procmem, clnt);

	return 0;
}

static int
si4700_detach_client (struct i2c_client *clnt)
{
	g_clnt = NULL;

	remove_proc_entry ("driver/si4700_regs", NULL);

	/* Unregister the device */
	misc_deregister (&si4700_miscdev);

	i2c_detach_client (clnt);

	kfree (GET_CLIENT_DATA);
	kfree (clnt);

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x10, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = { I2C_CLIENT_END };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_i2c,
	.probe = normal_i2c_range,
	.ignore = normal_i2c_range,
};
#else
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
#endif

static int
si4700_attach_adapter (struct i2c_adapter *adap)
{
	return i2c_probe (adap, &addr_data, si4700_attach);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static void
si4700_inc_use (struct i2c_client *clnt)
{
	MOD_INC_USE_COUNT;
}

static void
si4700_dec_use (struct i2c_client *clnt)
{
	MOD_DEC_USE_COUNT;
}
#endif


static struct i2c_driver si4700 = {
	id:		I2C_DRIVERID_SI4700,
	attach_adapter:	si4700_attach_adapter,
	detach_client:	si4700_detach_client,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	driver: {
		name:	SI4700_NAME, 
		owner:	THIS_MODULE
	},
#else
	name:		SI4700_NAME,
	flags:		I2C_DF_NOTIFY,
	inc_use:	si4700_inc_use,
	dec_use:	si4700_dec_use
#endif
};

static int __init
si4700_init (void)
{
	// apply si4700 reset
	switch (board_type) {
	case 0:
		// (J2-7 GPG7)
    if (machine_is_rirm3())
    {
      s3c2410_gpio_cfgpin (S3C2410_GPG2, S3C2410_GPG2_OUTP);
      reset_pin = S3C2410_GPG2;
    }
    else
    {
      s3c2410_gpio_cfgpin (S3C2410_GPG7, S3C2410_GPG7_OUTP);
      reset_pin = S3C2410_GPG7;
    }
		break;
	case 1:
		// (J1-20 GPB9)
		s3c2410_gpio_cfgpin (S3C2410_GPB9, S3C2410_GPB9_OUTP);
		reset_pin = S3C2410_GPB9;
		break;
	case 2:
	case 3:
		// (J1-18 GPB7)
		s3c2410_gpio_cfgpin (S3C2410_GPB7, S3C2410_GPB7_OUTP);
		reset_pin = S3C2410_GPB7;
		break;
	case 4:
		// (J1-16 GPB0)
		s3c2410_gpio_cfgpin (S3C2410_GPB0, S3C2410_GPB0_OUTP);
		reset_pin = S3C2410_GPB0;
		break;
	case 5:
		// (INT GPB6)
		// This is Stingray, so we also need to turn on the RF power
		s3c2410_gpio_cfgpin (S3C2410_GPB6, S3C2410_GPB6_OUTP);
		reset_pin = S3C2410_GPB6;
		break;
	case 6:
		// (J1-13 GPE6)
		s3c2410_gpio_cfgpin (S3C2410_GPE6, S3C2410_GPE6_OUTP);
		reset_pin = S3C2410_GPE6;
		break;
	default:
		printk ("reciva_si700: unknown board type\n");
		return -EINVAL;
	}

	s3c2410_gpio_pullup (reset_pin, 1);

	si4700_hard_reset ();

	return i2c_add_driver (&si4700);
}

static void __exit
si4700_exit (void)
{
	i2c_del_driver (&si4700);
}

module_init (si4700_init);
module_exit (si4700_exit);

MODULE_AUTHOR ("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION ("SI4700 FM tuner driver");
MODULE_LICENSE ("GPL");
