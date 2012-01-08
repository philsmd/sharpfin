/*
 * Si4704 tuner driver
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
typedef void *REG;
#else
typedef unsigned long REG;
#endif

#include "reciva_util.h"
#include "reciva_si4700.h"

static int si4704_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int si4704_poll (struct file *file, poll_table * wait);
static int si4704_read (struct file *filp, char *buffer, size_t count, loff_t * ppos);
static int si4704_open (struct inode *inode, struct file *file);
static int si4704_release (struct inode *inode, struct file *file);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t si4704_int_handler (int irq, void *dev);
#else
static void si4704_int_handler (int irq, void *dev, struct pt_regs *regs);
#endif

#define SI4704_NAME	"si4704"
#define DEFAULT_CHANNEL_SPACING		50	/* kHz */
#define DEFAULT_BASE_FREQ		87500	/* 87.5MHz */

#undef DEBUG_IRQ
#undef DEBUG_COMMAND
#undef DEBUG_TUNING
#undef DEBUG_RDS

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define GET_CLIENT_DATA i2c_get_clientdata(clnt)
#else
#define GET_CLIENT_DATA clnt->data
#endif
extern struct i2c_adapter *i2c_bit_s3c2410;

static struct i2c_driver si4704;

static struct i2c_client *g_clnt;

struct si4704_data {
	struct work_struct work;
	struct i2c_client *client;

	unsigned char cmdbuf[16];
	unsigned char rspbuf[16];

	wait_queue_head_t wait_queue;	/* Used for blocking read */
	struct si4700_packet output_buffer[16];	/* Stores data to write out of device */
	int inp, outp;		/* The number of bytes in the buffer */
	int channel_spacing;
	int base_freq;

	char use_rds;
	char powerdown;
	char seeking;
	char tuning;
        char async_tune;
        char tune_complete;
        char first_rds;

	wait_queue_head_t tune_queue;	/* Used to serialise tuning operations */

        struct semaphore lock;
};

#define Si4704_STC		(1 << 14)
#define Si4704_RDSR		(1 << 15)

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

static int sen = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (sen, int, S_IRUGO);
#else
MODULE_PARM (sen, "i");
#endif

// bit 0 = enable analog output
// bit 1 = enable digital output
// bit 2 = digital output format i2s
static int output_mode = 1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (output_mode, int, S_IRUGO);
#else
MODULE_PARM (output_mode, "i");
#endif

#define SI4704_POWERCFG_SEEK	(1 << 8)
#define SI4704_POWERCFG_SEEKUP	(1 << 9)
#define SI4704_CHAN_TUNE	(1 << 15)

#define MIN(a,b) ( (a)<(b) ? (a):(b) )

static int reset_pin, irqpin, irq;

#define SI4704_CMD_POWER_UP		0x01
#define SI4704_CMD_GET_REV		0x10
#define SI4704_CMD_POWER_DOWN		0x11
#define SI4704_CMD_SET_PROPERTY		0x12
#define SI4704_CMD_GET_PROPERTY		0x13
#define SI4704_CMD_GET_INT_STATUS	0x14
#define SI4704_CMD_PATCH_ARGS		0x15
#define SI4704_CMD_PATCH_DATA		0x16
#define SI4704_CMD_FM_TUNE_FREQ		0x20
#define SI4704_CMD_FM_SEEK_START	0x21
#define SI4704_CMD_FM_TUNE_STATUS	0x22
#define SI4704_CMD_FM_RSQ_STATUS	0x23
#define SI4704_CMD_FM_RDS_STATUS	0x24
#define SI4704_CMD_GPO_CTL		0x80
#define SI4704_CMD_GPO_SET		0x81

#define SI4704_PROP_GPO_IEN				0x0001
#define SI4704_PROP_DIGITAL_OUTPUT_FORMAT		0x0102
#define SI4704_PROP_DIGITAL_OUTPUT_SAMPLE_RATE		0x0104
#define SI4704_PROP_REFCLK_FREQ				0x0201
#define SI4704_PROP_REFCLK_PRESCALE			0x0202
#define SI4704_PROP_FM_DEEMPHASIS			0x1100
#define SI4704_PROP_FM_BLEND_STEREO_THRESHOLD		0x1105
#define SI4704_PROP_FM_BLEND_MONO_THRESHOLD		0x1106
#define SI4704_PROP_FM_ANTENNA_INPUT			0x1107
#define SI4704_PROP_FM_MAX_TUNE_ERROR			0x1108
#define SI4704_PROP_FM_RSQ_INT_SOURCE			0x1200
#define SI4704_PROP_FM_RSQ_SNR_HI_THRESHOLD		0x1201
#define SI4704_PROP_FM_RSQ_SNR_LO_THRESHOLD		0x1202
#define SI4704_PROP_FM_RSQ_RSSI_HI_THRESHOLD		0x1203
#define SI4704_PROP_FM_RSQ_RSSI_LO_THRESHOLD		0x1204
#define SI4704_PROP_FM_RSQ_BLEND_THRESHOLD		0x1207
#define SI4704_PROP_FM_SOFT_MUTE_MAX_ATTENUATION	0x1302
#define SI4704_PROP_FM_SOFT_MUTE_SNR_THRESHOLD		0x1303
#define SI4704_PROP_FM_SEEK_BAND_BOTTOM			0x1400
#define SI4704_PROP_FM_SEEK_BAND_TOP			0x1401
#define SI4704_PROP_FM_SEEK_FREQ_SPACING		0x1402
#define SI4704_PROP_FM_SEEK_TUNE_SNR_THRESHOLD		0x1403
#define SI4704_PROP_FM_SEEK_TUNE_RSSI_THRESHOLD		0x1404
#define SI4704_PROP_RDS_INT_SOURCE			0x1500
#define SI4704_PROP_RDS_INT_FIFO_COUNT			0x1501
#define SI4704_PROP_RDS_CONFIG				0x1502
#define SI4704_PROP_RX_VOLUME				0x4000
#define SI4704_PROP_RX_HARD_MUTE			0x4001

#define SI4704_POWER_UP_CTSIEN				0x80
#define SI4704_POWER_UP_GPO2OEN				0x40
#define SI4704_POWER_UP_PATCH_ENABLE			0x20
#define SI4704_POWER_UP_XOSC_ENABLE			0x10
#define SI4704_POWER_UP_FUNC_FM_RX			0x00
#define SI4704_POWER_UP_FUNC_AM_RX			0x01
#define SI4704_POWER_UP_FUNC_QUERY_LIBRARY		0x0f

#define SI4704_POWER_UP_OPMODE_DIGITAL			0xB0
#define SI4704_POWER_UP_OPMODE_ANALOGUE			0x05

#define SI4704_STAT_CTS					0x80
#define SI4704_STAT_ERR                                 0x40

#define SI4704_IEN_STCINT				0x0001
#define SI4704_IEN_RDSINT				0x0004
#define SI4704_IEN_STCREP                               0x0100
#define SI4704_IEN_RDSREP                               0x0400

#define SI4704_RDSIS_RDSRECV                            0x0001

/***********************************************************************/

static int si4704_get_frequency (struct i2c_client *clnt, int *freq);

static void
si4704_hard_reset (void)
{
	// irq pin needs to be low during reset for bustype selection to work
	s3c2410_gpio_pullup (irqpin, 1);

	s3c2410_gpio_setpin (reset_pin, 0);

	/* Must have SCL high during rising edge on Si4704 nRST.
	   Lock the bus so nobody else can transmit.  */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        // TODO Claim an I2C lock during this, so nothing else gets clobbered
        unsigned int pin = s3c2410_gpio_getpin (S3C2410_GPE14);
        unsigned int pincfg = s3c2410_gpio_getcfg (S3C2410_GPE14);

        s3c2410_gpio_cfgpin (S3C2410_GPE14, S3C2410_GPE14_OUTP);
        s3c2410_gpio_setpin (S3C2410_GPE14, 1);
#else
	struct i2c_client dummy_client;
	dummy_client.adapter = i2c_bit_s3c2410;
	i2c_control (&dummy_client, I2C_BIT_LOCK, 0);
#endif

	mdelay (1);
	s3c2410_gpio_setpin (reset_pin, 1);
	mdelay (1);

	s3c2410_gpio_pullup (irqpin, 0);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        s3c2410_gpio_cfgpin (S3C2410_GPE14, pincfg);
        s3c2410_gpio_setpin (S3C2410_GPE14, pin);
#else
	i2c_control (&dummy_client, I2C_BIT_UNLOCK, 0);
#endif
}

static int
si4704_chip_read (struct i2c_client *clnt, int nbytes, unsigned char *data)
{
	int r;

	r = i2c_master_recv (clnt, (unsigned char *) data, nbytes);
	if (r != nbytes) {
		printk (KERN_ERR "si4704: read failed, status %d\n", r);
		return r;
	}

	return 0;
}

static int
si4704_chip_write (struct i2c_client *clnt, int nbytes, unsigned char *data)
{
	int r;

	r = i2c_master_send (clnt, (unsigned char *) data, nbytes);
	if (r != nbytes) {
		printk (KERN_ERR "si4704: write failed, status %d\n", r);
		return r;
	}

	return 0;
}

static int 
si4704_write_cmd (struct si4704_data *si, int nbytes)
{
#ifdef DEBUG_COMMAND
	int i;
	printk ("si4704: command data ");
	for (i = 0; i < nbytes; i++)
		printk ("%02x ", si->cmdbuf[i]);
	printk("\n");
#endif
	return si4704_chip_write (si->client, nbytes, si->cmdbuf);
}

static int 
si4704_read_rsp (struct si4704_data *si, int nbytes)
{
	int n = si4704_chip_read (si->client, nbytes, si->rspbuf);
	if (n)
		return n;
#ifdef DEBUG_COMMAND
	int i;
	printk ("si4704: response data ");
	for (i = 0; i < nbytes; i++)
		printk ("%02x ", si->rspbuf[i]);
	printk("\n");
#endif
	return 0;
}

static int
si4704_await_cts (struct i2c_client *clnt)
{
	int r;
	struct si4704_data *si = GET_CLIENT_DATA;

	do {
		r = si4704_chip_read (clnt, 1, &si->rspbuf[0]);
		if (r)
			return r;
	} while ((si->rspbuf[0] & SI4704_STAT_CTS) == 0);

        if (si->rspbuf[0] & SI4704_STAT_ERR) {
                printk ("%s: error, status %x\n", __FUNCTION__, si->rspbuf[0]);
        }

	return 0;
}

static int
__si4704_set_property (struct si4704_data *si, int prop, int val)
{
	si->cmdbuf[0] = SI4704_CMD_SET_PROPERTY;
	si->cmdbuf[1] = 0;
	si->cmdbuf[2] = (prop & 0xff00) >> 8;
	si->cmdbuf[3] = (prop & 0x00ff) >> 0;
	si->cmdbuf[4] = (val & 0xff00) >> 8;
	si->cmdbuf[5] = (val & 0x00ff) >> 0;

	si4704_await_cts (si->client);

	si4704_write_cmd (si, 6);

	return si4704_await_cts (si->client);
}

static int
__si4704_get_property (struct si4704_data *si, int prop, int *val)
{
	si->cmdbuf[0] = SI4704_CMD_GET_PROPERTY;
	si->cmdbuf[1] = 0;
	si->cmdbuf[2] = (prop & 0xff00) >> 8;
	si->cmdbuf[3] = (prop & 0x00ff) >> 0;

	si4704_await_cts (si->client);

	si4704_write_cmd (si, 4);

	si4704_await_cts (si->client);

	si4704_read_rsp (si, 4);

	*val = (si->rspbuf[2] << 8) | si->rspbuf[3];

	return 0;
}

static int
si4704_set_property (struct si4704_data *si, int prop, int val)
{
        down (&si->lock);
	int ret = __si4704_set_property (si, prop, val);
        up (&si->lock);
        return ret;
}

static int
si4704_get_property (struct si4704_data *si, int prop, int *val)
{
        down (&si->lock);
	int ret = __si4704_get_property (si, prop, val);
        up (&si->lock);
        return ret;
}

static int
si4704_tune_to (struct i2c_client *clnt, int freq, int sync)
{
	struct si4704_data *si = GET_CLIENT_DATA;

        down (&si->lock);

        // disable rds
        __si4704_set_property (si, SI4704_PROP_RDS_CONFIG, 0x0);

	si->tuning = 1;
        si->async_tune = sync ? 0 : 1;
        si->tune_complete = 0;

	freq /= 10;

	si->cmdbuf[0] = SI4704_CMD_FM_TUNE_FREQ;
	si->cmdbuf[1] = 0;
	si->cmdbuf[2] = (freq >> 8) & 0xff;
	si->cmdbuf[3] = freq & 0xff;
	si->cmdbuf[4] = 0;

	si4704_write_cmd (si, 5);

        /* nb, no await_cts here, next caller will do that */

        up (&si->lock);

        if (sync) {
#if 1
                /* workaround for lost irq bug */
                while (!si->tune_complete) {
                        interruptible_sleep_on_timeout (&si->tune_queue, HZ / 10);
                        
                        if (!si->tune_complete) {
                                int freq;
                                si4704_get_frequency (clnt, &freq);
                        }
                }
#else
                /* wait for keventd to say we're finished */
                interruptible_sleep_on (&si->tune_queue);
#endif
        }

	return 0;
}

#define SI4704_TUNE_STATUS_CANCEL	0x02
#define SI4704_TUNE_STATUS_INTACK	0x01

static int
si4704_seek_stop (struct i2c_client *clnt)
{
	struct si4704_data *si = GET_CLIENT_DATA;
        down (&si->lock);
	si4704_await_cts (clnt);
	si->cmdbuf[0] = SI4704_CMD_FM_TUNE_STATUS;
	si->cmdbuf[1] = SI4704_TUNE_STATUS_CANCEL;
	si4704_write_cmd (si, 2);
	si4704_await_cts (clnt);
	si4704_read_rsp (si, 8);
        up (&si->lock);
	return 0;
}

#define SI4704_SEEK_SEEKUP		0x08
#define SI4704_SEEK_WRAP		0x04

static int
si4704_seek (struct i2c_client *clnt, int direction)
{
	struct si4704_data *si = GET_CLIENT_DATA;

        down (&si->lock);

        // disable rds
        __si4704_set_property (si, SI4704_PROP_RDS_CONFIG, 0x0);

	si->cmdbuf[0] = SI4704_CMD_FM_SEEK_START;
	si->cmdbuf[1] = (direction ? SI4704_SEEK_SEEKUP : 0x00) | SI4704_SEEK_WRAP;

	si4704_write_cmd (si, 2);

	si4704_await_cts (clnt);

	si->seeking = 1;

        up (&si->lock);

	return 0;
}

static int
si4704_set_seek_params (struct i2c_client *clnt, int snr_threshold, int impulse_threshold, int rssi_threshold)
{
        struct si4704_data *si = GET_CLIENT_DATA;
        int r;

        r = si4704_set_property (si, SI4704_PROP_FM_SEEK_TUNE_RSSI_THRESHOLD, rssi_threshold);
        if (r)
                return r;

        r = si4704_set_property (si, SI4704_PROP_FM_SEEK_TUNE_SNR_THRESHOLD, snr_threshold);
        if (r)
                return r;

        /* impulse level is not configurable anymore in Si4704 */

	return 0;
}

static int
si4704_read_seek_params (struct i2c_client *clnt, int *arg)
{
        struct si4704_data *si = GET_CLIENT_DATA;
        int seek_params = 0;
        int r, v;

        r = si4704_get_property (si, SI4704_PROP_FM_SEEK_TUNE_RSSI_THRESHOLD, &v);
        if (r)
                return r;

        seek_params = (v << 8);

        r = si4704_get_property (si, SI4704_PROP_FM_SEEK_TUNE_SNR_THRESHOLD, &v);
        if (r)
                return r;

        seek_params |= v;

        return put_user (seek_params, (int *) arg);
}

static int
si4704_set_volume (struct i2c_client *clnt, int vol)
{
	struct si4704_data *si = GET_CLIENT_DATA;

  // Assume 0-15 range of SI4700 and convert to 0-63 range of the SI4704.
	return si4704_set_property (si, SI4704_PROP_RX_VOLUME, (vol << 2) + (vol >> 2));
}

static int
si4704_set_channel_spacing (struct i2c_client *clnt, int spacing)
{
	struct si4704_data *si = GET_CLIENT_DATA;

	return si4704_set_property (si, SI4704_PROP_FM_SEEK_FREQ_SPACING,
				    spacing / 10);
}

static int
si4704_set_band (struct i2c_client *clnt, int band)
{
	struct si4704_data *si = GET_CLIENT_DATA;

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

	return -ENOSYS;

}

static int
si4704_set_stereo_blending (struct i2c_client *clnt, int blending)
{
	struct si4704_data *si = GET_CLIENT_DATA;

	int val = 49;
	switch (blending) {
	case 1:
		val += 6;
		break;
	case 2:
		val -= 6;
		break;
	case 3:
		val += 12;
		break;
	default: 
		break;
	}

	return si4704_set_property (si, SI4704_PROP_FM_BLEND_STEREO_THRESHOLD, val);
}

static int
si4704_force_mono (struct i2c_client *clnt, int mono)
{
	struct si4704_data *si = GET_CLIENT_DATA;
	
	return si4704_set_property (si, SI4704_PROP_FM_BLEND_MONO_THRESHOLD, mono ? 127 : 30);
}

static int
si4704_mute (struct i2c_client *clnt, int mute)
{
	struct si4704_data *si = GET_CLIENT_DATA;

	return si4704_set_property (si, SI4704_PROP_RX_HARD_MUTE, mute ? 0x03 : 0x00);
}

static int
si4704_set_deemphasis (struct i2c_client *clnt, int usa_mode)
{
	struct si4704_data *si = GET_CLIENT_DATA;

	int val = usa_mode ? 0x02 : 0x01;
	
	return si4704_set_property (si, SI4704_PROP_FM_DEEMPHASIS, val);
}

static int
si4704_get_frequency (struct i2c_client *clnt, int *freq)
{
	struct si4704_data *si = GET_CLIENT_DATA;
	si4704_await_cts (clnt);
	si->cmdbuf[0] = SI4704_CMD_FM_TUNE_STATUS;
	si->cmdbuf[1] = 0;
	si4704_write_cmd (si, 2);
	si4704_await_cts (clnt);
	si4704_read_rsp (si, 8);
        if (si->rspbuf[0] & 1) {
#ifdef DEBUG_IRQ
                printk ("requesting softirq\n");
#endif
                schedule_work (&si->work);
        }
	int f = (si->rspbuf[2] << 8) | si->rspbuf[3];
	*freq = f * 10;
	return 0;
}

static int
si4704_get_status (struct i2c_client *clnt, int *data)
{
	struct si4704_data *si = GET_CLIENT_DATA;
	si4704_await_cts (clnt);
	si->cmdbuf[0] = SI4704_CMD_FM_RSQ_STATUS;
	si->cmdbuf[1] = 0;
	si4704_write_cmd (si, 2);
	si4704_await_cts (clnt);
	si4704_read_rsp (si, 8);

  //printk("%s Flags %02x Blend %d RSSI %d SNR %d\n", __FUNCTION__,
  //       si->rspbuf[2], si->rspbuf[3] &0x7f, si->rspbuf[4], si->rspbuf[5]);

  *data = si->rspbuf[4];  // rssi
  *data |= si->rspbuf[5] << 16;  // snr
  if ((si->rspbuf[3] & 0x7f) > 10) // stereo blend above 10%
    *data |= 1 << 8;
  *data |= (si->rspbuf[2] & 1) << 12; // valid channel

	return 0;
}

static int
si4704_init_chip (struct i2c_client *clnt, int mode)
{
	struct si4704_data *si = GET_CLIENT_DATA;
	int r;
	int opmode = 0;

	r = si4704_await_cts (clnt);
	if (r)
		return r;

	if (output_mode & 1)
		opmode |= SI4704_POWER_UP_OPMODE_ANALOGUE;

	if (output_mode & 2)
		opmode |= SI4704_POWER_UP_OPMODE_DIGITAL;

	si->cmdbuf[0] = SI4704_CMD_POWER_UP;
	si->cmdbuf[1] = mode | SI4704_POWER_UP_GPO2OEN;
	si->cmdbuf[2] = opmode;
	r = si4704_write_cmd (si, 3);
	if (r)
		return r;

	r = si4704_await_cts (clnt);
	if (r)
		return r;

	si->cmdbuf[0] = SI4704_CMD_GET_REV;
	r = si4704_write_cmd (si, 1);
	if (r)
		return r;
	
	r = si4704_await_cts (clnt);
	if (r)
		return r;
	
	r = si4704_read_rsp (si, 9);
	if (r)
		return r;

	printk ("found Si47%02x rev %c (%c.%c), firmware %c.%c, patch %02x%02x\n",
		si->rspbuf[1], si->rspbuf[8], si->rspbuf[6], si->rspbuf[7],
		si->rspbuf[2], si->rspbuf[3], si->rspbuf[4], si->rspbuf[5]);


	__si4704_set_property (si, SI4704_PROP_GPO_IEN, SI4704_IEN_STCINT | SI4704_IEN_RDSINT | SI4704_IEN_STCREP | SI4704_IEN_RDSREP);

	if (output_mode & 2)
	{
		if (output_mode & 4)
			__si4704_set_property (si, SI4704_PROP_DIGITAL_OUTPUT_FORMAT, 0x0 << 3); // i2s
		else
			__si4704_set_property (si, SI4704_PROP_DIGITAL_OUTPUT_FORMAT, 0x6 << 3); // msb_first

		__si4704_set_property (si, SI4704_PROP_DIGITAL_OUTPUT_SAMPLE_RATE, 48000);
	}

	return 0;
}

static int
si4704_power_down (struct i2c_client *clnt)
{
	struct si4704_data *si = GET_CLIENT_DATA;

	si4704_await_cts (clnt);

	si->cmdbuf[0] = SI4704_CMD_POWER_DOWN;
	si4704_write_cmd (si, 1);

	si4704_await_cts (clnt);

	si->powerdown = 1;

	s3c2410_gpio_setpin (reset_pin, 0);

        // release pullup on irq pin, Si4704 will pull it low
	s3c2410_gpio_pullup (irqpin, 1);

	return 0;
}

/***********************************************************************/

static struct si4700_packet *
si4704_get_packet (struct si4704_data *si)
{
	struct si4700_packet *r;
	int new_inp = ((si->inp + 1) % 16);

	if (new_inp == si->outp)
		return NULL;

	r = &si->output_buffer[si->inp];

	return r;
}

static void
si4704_write_packet (struct si4704_data *si)
{
	si->inp = ((si->inp + 1) % 16);
	wake_up_interruptible (&si->wait_queue);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static irqreturn_t
si4704_int_handler (int irq, void *dev)
#else
static void
si4704_int_handler (int irq, void *dev, struct pt_regs *regs)
#endif
{
	struct si4704_data *si = dev;

#ifdef DEBUG_IRQ
	printk ("si4704: hard interrupt\n");
#endif

	schedule_work (&si->work);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	return IRQ_HANDLED;
#endif
}

static void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
si4704_kevent (struct work_struct *work)
{
	struct si4704_data *si = container_of (work, struct si4704_data, work);
	struct i2c_client *clnt = si->client;
#else
si4704_kevent (void *data)
{
	struct i2c_client *clnt = data;
	struct si4704_data *si = GET_CLIENT_DATA;
#endif

        down (&si->lock);

	si4704_await_cts (clnt);
	si->cmdbuf[0] = SI4704_CMD_GET_INT_STATUS;
	si4704_write_cmd (si, 1);
	si4704_await_cts (clnt);
	int intstat = si->rspbuf[0];

#ifdef DEBUG_IRQ
	printk("interrupt, status %x\n", intstat);
#endif

	if (intstat & 0x1) {
                si->cmdbuf[0] = SI4704_CMD_FM_TUNE_STATUS;
                si->cmdbuf[1] = 1;	// INTACK
                si4704_write_cmd (si, 2);
                si4704_await_cts (clnt);

		if (si->seeking) {
                        si4704_read_rsp (si, 8);

			int freq = ((si->rspbuf[2] << 8) | si->rspbuf[3]) * 10;
			int result = (si->rspbuf[1] & 0x80) ? 1 : 0;
			int rssi = si->rspbuf[4];
			printk("seek result %08x, freq %d, rssi %d\n", si->rspbuf[1], freq, rssi);

			struct si4700_packet *outpkt = si4704_get_packet (si);
			if (outpkt) {
				outpkt->code = SI4700_PACKET_SEEK_STATUS;
				outpkt->u.seek.status = result;
				outpkt->u.seek.frequency = freq;
				outpkt->u.seek.rssi = rssi;
				si4704_write_packet (si);
			}

			si->seeking = 0;
		} else if (si->tuning) {
#ifdef DEBUG_TUNING
                        printk ("si4700: tuning complete\n");
#endif
                        if (si->async_tune) {
                                struct si4700_packet *outpkt;
                                outpkt = si4704_get_packet (si);
                                if (outpkt) {
                                        outpkt->code = SI4700_PACKET_TUNE_COMPLETE;
                                        si4704_write_packet (si);
                                }
                        } else {
                                si->tune_complete = 1;
                                wake_up_interruptible (&si->tune_queue);
                        }
                        si->tuning = 0;
                } else
                        printk ("%s: unexpected STC\n", __FUNCTION__);

                if (si->use_rds) {
                        __si4704_set_property (si, SI4704_PROP_RDS_INT_SOURCE, SI4704_RDSIS_RDSRECV);
                        __si4704_set_property (si, SI4704_PROP_RDS_INT_FIFO_COUNT, 0x0004);
                        __si4704_set_property (si, SI4704_PROP_RDS_CONFIG, 0xef01);
                        struct si4700_packet *outpkt;
                        outpkt = si4704_get_packet (si);
                        if (outpkt) {
                                outpkt->code = SI4700_PACKET_RDS_START;
                                si4704_write_packet (si);
                        }
                        si->first_rds = 1;
                }
	}

        if (intstat & 0x4) {
                int more = 0;

                static const unsigned long rds_error_map[] = {
                        0,	/* 0 errors */
                        2 << 16,	/* 1-2 errors */
                        5 << 16,	/* 3-5 errors */
                        (1 << 31) | 6 << 16,	/* 6 errors (uncorrectable) */
                };

                do {
                        si->cmdbuf[0] = SI4704_CMD_FM_RDS_STATUS;
                        si->cmdbuf[1] = 1;              // INTACK

                        si4704_write_cmd (si, 2);
                        si4704_await_cts (clnt);
                        si4704_read_rsp (si, 13);
                        more = si->rspbuf[3];

                        // First RDS packet may contain blocks from previous
                        // channel, so ignore it
                        if (si->first_rds == 1)
                        {
                          si->first_rds = 0;
                          continue;
                        }
                        if (more == 0)
                          break;

#ifdef DEBUG_RDS
                        int i=0;
                        printk ("si4704: RDS ");
                        for (i = 0; i < 4; i++)
                          printk ("%02x ", si->rspbuf[i]);
                        for ( ; i < 12; i+=2)
                          printk ("%02x%02x ", si->rspbuf[i],  si->rspbuf[i+1]);
                        printk("%02x\n", si->rspbuf[12]);
#endif

                        struct si4700_packet *outpkt;

                        outpkt = si4704_get_packet (si);
                        if (outpkt) {
                                outpkt->code = SI4700_PACKET_RDS;
                                outpkt->u.rds.a = si->rspbuf[5] | (si->rspbuf[4] << 8);
                                outpkt->u.rds.b = si->rspbuf[7] | (si->rspbuf[6] << 8);
                                outpkt->u.rds.c = si->rspbuf[9] | (si->rspbuf[8] << 8);
                                outpkt->u.rds.d = si->rspbuf[11] | (si->rspbuf[10] << 8);

                                outpkt->u.rds.a |= rds_error_map[(si->rspbuf[12] & 0xc0) >> 6];
                                outpkt->u.rds.b |= rds_error_map[(si->rspbuf[12] & 0x30) >> 4];
                                outpkt->u.rds.c |= rds_error_map[(si->rspbuf[12] & 0xc) >> 2];
                                outpkt->u.rds.d |= rds_error_map[(si->rspbuf[12] & 0x3) >> 0];

                                si4704_write_packet (si);
                        }

                        // give other tasks a chance to run
                        up (&si->lock);
                        down (&si->lock);
                } while (more);                
        }

        up (&si->lock);
}

/***********************************************************************/

static struct file_operations reciva_si4704_fops = {
	owner:		THIS_MODULE,
	ioctl:		si4704_ioctl,
	read:		si4704_read,
	poll:		si4704_poll,
	open:		si4704_open,
	release:	si4704_release,
};

static struct miscdevice si4704_miscdev = {
	MISC_DYNAMIC_MINOR,
	"si4700",
	&reciva_si4704_fops
};

static unsigned int
si4704_poll (struct file *file, poll_table * wait)
{
	struct i2c_client *clnt = file->private_data;
	struct si4704_data *si = GET_CLIENT_DATA;

	poll_wait (file, &si->wait_queue, wait);
	if (si->inp != si->outp)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int
si4704_read (struct file *filp, char *buffer, size_t count, loff_t * ppos)
{
	struct i2c_client *clnt = filp->private_data;
	struct si4704_data *si = GET_CLIENT_DATA;
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
si4704_open (struct inode *inode, struct file *file)
{
	file->private_data = g_clnt;
        return 0;
}

static int
si4704_start(struct i2c_client *clnt, int arg)
{
	struct si4704_data *si = GET_CLIENT_DATA;
	unsigned long flags;
	unsigned long x;
	REG extintreg = 0;
	int extintbit = -1, intpincfg = -1;
	int r;
        output_mode = arg;

  printk("%s %d\n", __FUNCTION__, output_mode);

	si->seeking = si->tuning = 0;

        rutl_set_dab_fm_filter(DAB_FM_FILTER_FM);

	if (si->powerdown) {
		si4704_hard_reset ();
	}

	si->use_rds = 1;

	switch (irqpin) {
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
        case S3C2410_GPG2:
                extintreg = S3C2410_EXTINT1 + 0x10;
                extintbit = 8;
                intpincfg = S3C2410_GPG2_EINT10;
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
	case S3C2410_GPG7:
		extintreg = S3C2410_EXTINT1;
		extintbit = 25;
		intpincfg = S3C2410_GPG7_EINT15;
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

	s3c2410_gpio_cfgpin (irqpin, intpincfg);

	r = si4704_init_chip (clnt, SI4704_POWER_UP_FUNC_FM_RX);
	if (r)
		return r;

	si->powerdown = 0;

	// pullup off since Si470x uses totem pole drive
	s3c2410_gpio_pullup (irqpin, 1);

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

	if (request_irq (irq, si4704_int_handler, 0, "Si4704", si)) {
		printk ("couldn't request Si4704 interrupt line\n");
		si4704_power_down (clnt);
		s3c2410_gpio_pullup (irqpin, 0);
		return -EBUSY;
	}

	return 0;
}

static int
si4704_release (struct inode *inode, struct file *file)
{
        printk("%s\n", __FUNCTION__);

	struct i2c_client *clnt = file->private_data;
	struct si4704_data *si = GET_CLIENT_DATA;

	down (&si->lock);

        __si4704_set_property (si, SI4704_PROP_DIGITAL_OUTPUT_SAMPLE_RATE, 0);

	s3c2410_gpio_pullup (irqpin, 0);
	free_irq (irq, si);

	flush_scheduled_work ();

	si4704_power_down (clnt);

        rutl_set_dab_fm_filter(DAB_FM_FILTER_NONE);

	up (&si->lock);

	return 0;
}

static int
si4704_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct i2c_client *clnt = file->private_data;
	struct si4704_data *si = GET_CLIENT_DATA;
	int freq, vol, spacing, dir, seek_params, blending, deemph, band, mono, mute, r, prop, val;

	switch (cmd) {
	case IOC_SI4700_TUNE:
		if (get_user (freq, (int *) arg))
			return -EFAULT;
		return si4704_tune_to (clnt, freq, 1);

	case IOC_SI4700_TUNE_ASYNC:
		if (get_user (freq, (int *) arg))
			return -EFAULT;
		return si4704_tune_to (clnt, freq, 0);

	case IOC_SI4700_SEEK:
		if (get_user (dir, (int *) arg))
			return -EFAULT;
		return si4704_seek (clnt, dir);

	case IOC_SI4700_STATUS:
		r = si4704_get_status (clnt, &val);
		if (r)
			return r;
		return put_user (val, (int *) arg);

	case IOC_SI4700_VOLUME:
		if (get_user (vol, (int *) arg))
			return -EFAULT;
		return si4704_set_volume (clnt, vol);

	case IOC_SI4700_SPACING:
		if (get_user (spacing, (int *) arg))
			return -EFAULT;
		return si4704_set_channel_spacing (clnt, spacing);

	case IOC_SI4700_SEEK_PARAMS:
		if (get_user (seek_params, (int *) arg))
			return -EFAULT;
		return si4704_set_seek_params (clnt, seek_params & 0xf, (seek_params >> 4) & 0xf, (seek_params >> 8) & 0xff);

	case IOC_SI4700_READ_SEEK_PARAMS:
                return si4704_read_seek_params (clnt, (int *)arg);

	case IOC_SI4700_STEREO_BLENDING:
		if (get_user (blending, (int *) arg))
			return -EFAULT;
		return si4704_set_stereo_blending (clnt, blending);

	case IOC_SI4700_DEEMPHASIS:
		if (get_user (deemph, (int *) arg))
			return -EFAULT;
		return si4704_set_deemphasis (clnt, deemph);

	case IOC_SI4700_READ_FREQUENCY:
		r = si4704_get_frequency (clnt, &freq);
		if (r)
			return r;
		return put_user (freq, (int *) arg);

	case IOC_SI4700_BAND:
		if (get_user (band, (int *) arg))
			return -EFAULT;
		return si4704_set_band (clnt, band);

	case IOC_SI4700_SEEK_STOP:
		return si4704_seek_stop (clnt);

	case IOC_SI4700_READ_RSSI:
		return 0;

	case IOC_SI4700_FORCE_MONO:
		if (get_user (mono, (int *) arg))
			return -EFAULT;
		return si4704_force_mono (clnt, mono);

	case IOC_SI4700_MUTE:
		if (get_user (mute, (int *) arg))
			return -EFAULT;
		return si4704_mute (clnt, mute);

	case IOC_SI4700_ENABLE_RDS:
		if (get_user (si->use_rds, (int *) arg))
			return -EFAULT;
		return 0;

        case IOC_SI4700_SET_PROPERTY:
		if (get_user (prop, (int *) arg))
			return -EFAULT;
                val = (prop >> 16) & 0xffff;
                prop &= 0xffff;
                return si4704_set_property (si, prop, val);

        case IOC_SI4700_GET_PROPERTY:
		if (get_user (prop, (int *) arg))
			return -EFAULT;
                prop &= 0xffff;
                r = si4704_get_property (si, prop, &val);
                if (r)
                        return r;
                prop |= (val << 16);
                if (put_user (prop, (int *) arg))
                        return -EFAULT;
                return 0;

	case IOC_SI4704_START:
		return si4704_start(clnt, arg);

	}

	return -ENODEV;
}

/***********************************************************************/

static struct i2c_client client_template = {
	name:	"(unset)",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	flags:	I2C_CLIENT_ALLOW_USE,
#endif
	driver:	&si4704
};

static int
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
si4704_attach (struct i2c_adapter *adap, int addr, int kind)
#else
si4704_attach (struct i2c_adapter *adap, int addr, unsigned short flags, int kind)
#endif
{
	struct si4704_data *si;
	struct i2c_client *clnt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	if (strcmp (adap->name, "s3c2410-i2c")) {
#else
	if (adap != i2c_bit_s3c2410) {
#endif
		printk ("si4704: can't attach to unknown adapter\n");
		return -ENODEV;
	}

	clnt = kmalloc (sizeof (*clnt), GFP_KERNEL);
	memcpy (clnt, &client_template, sizeof (*clnt));
	clnt->adapter = adap;
	clnt->addr = addr;
	strcpy (clnt->name, "si4704");

	si = kmalloc (sizeof (*si), GFP_KERNEL);
	if (!si)
		return -ENOMEM;

	memset (si, 0, sizeof (*si));

        init_MUTEX (&si->lock);
	init_waitqueue_head (&si->wait_queue);
	init_waitqueue_head (&si->tune_queue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	INIT_WORK (&si->work, si4704_kevent);
#else
	INIT_WORK (&si->work, si4704_kevent, clnt);
#endif

	si->client = clnt;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	i2c_set_clientdata (clnt, si);
#else
	clnt->data = si;
#endif

	si->channel_spacing = DEFAULT_CHANNEL_SPACING;
	si->base_freq = DEFAULT_BASE_FREQ;

	i2c_attach_client (clnt);

	printk ("si4704: attached at address %x\n", addr);

	/* Register the device */
	misc_register (&si4704_miscdev);

	g_clnt = clnt;

	return 0;
}

static int
si4704_detach_client (struct i2c_client *clnt)
{
	g_clnt = NULL;

	/* Unregister the device */
	misc_deregister (&si4704_miscdev);

	i2c_detach_client (clnt);

	kfree (GET_CLIENT_DATA);
	kfree (clnt);

	return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x63, I2C_CLIENT_END };
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
si4704_attach_adapter (struct i2c_adapter *adap)
{
	return i2c_probe (adap, &addr_data, si4704_attach);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static void
si4704_inc_use (struct i2c_client *clnt)
{
	MOD_INC_USE_COUNT;
}

static void
si4704_dec_use (struct i2c_client *clnt)
{
	MOD_DEC_USE_COUNT;
}
#endif


static struct i2c_driver si4704 = {
	id:		I2C_DRIVERID_SI4700,
	attach_adapter:	si4704_attach_adapter,
	detach_client:	si4704_detach_client,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	driver: {
		name:	SI4704_NAME, 
		owner:	THIS_MODULE
	},
#else
	name:		SI4704_NAME,
	flags:		I2C_DF_NOTIFY,
	inc_use:	si4704_inc_use,
	dec_use:	si4704_dec_use
#endif
};

static int __init
si4704_init (void)
{
	if (sen == 0)		// alternate i2c address
		normal_i2c[0] = 0x11;

	// apply si4704 reset
	switch (board_type) {
	case 0:
		// (J2-7 GPG7)
                if (machine_is_rirm3())
                        reset_pin = S3C2410_GPG2;
                else
                        reset_pin = S3C2410_GPG7;
		break;
	case 1:
		// (J1-20 GPB9)
		reset_pin = S3C2410_GPB9;
		break;
	case 2:
	case 3:
		// (J1-18 GPB7)
		reset_pin = S3C2410_GPB7;
		break;
	case 4:
		// (J1-16 GPB0)
		reset_pin = S3C2410_GPB0;
		break;
	case 5:
		// (INT GPB6)
		// This is Stingray, so we also need to turn on the RF power
		reset_pin = S3C2410_GPB6;
		break;
	case 6:
		// (J1-13 GPE6)
		reset_pin = S3C2410_GPE6;
		break;
	case 7:
		// (J2-3 GPG5)
                if (machine_is_rirm3())
                        reset_pin = S3C2410_GPG0;
                else
                        reset_pin = S3C2410_GPG5;
		break;
	default:
		printk ("reciva_si700: unknown board type\n");
		return -EINVAL;
	}

	// figure out irq routing
	switch (board_type) {
	case 0:
	case 1:
	case 3:
	case 4:
	case 6:
                if (machine_is_rirm3()) {
                        irqpin = S3C2410_GPG0;
                        irq = IRQ_EINT8;
                } else {
                        irqpin = S3C2410_GPG5;
                        irq = IRQ_EINT13;
                }
		break;
	case 2:
                if (machine_is_rirm3()) {
                        irqpin = S3C2410_GPG1;
                        irq = IRQ_EINT9;
                } else {
                        irqpin = S3C2410_GPG6;
                        irq = IRQ_EINT14;
                }
		break;
	case 5:
		irqpin = S3C2410_GPF4;
		irq = IRQ_EINT4;
		break;
	case 7:
                if (machine_is_rirm3()) {
                        irqpin = S3C2410_GPG2;
                        irq = IRQ_EINT10;
                } else {
                        irqpin = S3C2410_GPG7;
                        irq = IRQ_EINT15;
                }
		break;
	default:
		BUG ();
	}

	s3c2410_gpio_cfgpin (reset_pin, S3C2410_GPIO_OUTPUT);

	s3c2410_gpio_pullup (reset_pin, 1);

	si4704_hard_reset ();

	return i2c_add_driver (&si4704);
}

static void __exit
si4704_exit (void)
{
	i2c_del_driver (&si4704);
}

module_init (si4704_init);
module_exit (si4704_exit);

MODULE_AUTHOR ("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION ("SI4704 FM tuner driver");
MODULE_LICENSE ("GPL");
