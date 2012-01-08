/*
 * Princeton AK4370 device driver
 *
 * Copyright (c) 2006, 2007 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <sound/driver.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/soundcard.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>


#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <asm/arch/audio.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-iis.h>
#include <asm/io.h>

#include <asm/arch/s3c24xx-dac.h>


struct i2c_gain {
	unsigned int  left:8;
	unsigned int  right:8;
};

struct ak4370 {
	struct i2c_client clnt;
  struct platform_device  *my_dev;
	struct i2c_gain	volume;
	int muted;

  int   (*claim)(struct ak4370 *);
  int   (*release)(struct ak4370 *);
  int   (*wr)(struct ak4370 *, int reg, int val);

  struct snd_card     *card;
  struct wm8721_cfg   *cfg;
  struct semaphore     lock;
};

static struct ak4370 *g_dac;

#include "../reciva_audio_mute.h"

/* Device ID and debug strings */
#define PREFIX "AK4370:"

/* DAC device I2C addresses
 * If you want to use 0x11 then set addr_lsb=1 rather than attempt to scan for 
 * range of I2C addresses due to potential address clash with FM tuner device */
#define DAC_I2C_ADDR_NORMAL       0x10

/* 0=MANGA (PDN: J3-2 GPD10, external clocking) 1=COBAL (PDN: J1-9 GPE7, S3C2410 clocking) 
   2=STARRY7 (PDN: J1-15 GPE5, S3C2410 clocking) 3=MX-200i (PDN: J1-20 GPB9, S3C2410 clocking)
   4=STARRY7bis (PDN: J1-15 GPE5, external clocking) */
static int board_type, deemphasis, addr_lsb;

/* Mute DAC bits in line with hard muting */
static int dynamic_muting;
static int pdn_pin;
static int log;

module_param(board_type, int, S_IRUGO);
module_param(addr_lsb, int, S_IRUGO);
module_param(deemphasis, int, S_IRUGO);
module_param(log, int, S_IRUGO);
module_param(dynamic_muting, int, S_IRUGO);


/****************************************************************************
 * Write to an AK4370 register
 ****************************************************************************/
static int ak4370_reg_write (struct i2c_client *clnt, int addr, int data)
{
	int r;
	unsigned char buffer[2];
	buffer[0] = addr & 0xff;
	buffer[1] = data & 0xff;
	
	r = i2c_master_send(clnt, buffer, 2);
	if (r != 2) {
		printk(PREFIX "write failed, status %d\n", r);
		return r;
	}
	return 0;
}

#if 0
static int ak4370_reg_read (struct i2c_client *clnt, int addr)
{
	int r;
	unsigned char buffer[1];
	buffer[0] = addr & 0xff;
	
	r = i2c_master_send(clnt, buffer, 1);
	if (r != 1) {
		printk(PREFIX "read failed, status %d\n", r);
		return r;
	}

	r = i2c_master_recv(clnt, buffer, 1);
	if (r != 1) {
		printk(PREFIX "read failed, status %d\n", r);
		return r;
	}
	return buffer[0];
}
#endif

/****************************************************************************
 * Convert volume range 0-100 to DAC attentuation value 0-31
 ****************************************************************************/
static int ak4370_volume_to_att (int vol)
{
	return (100 - vol) * 31 / 100;
}

/****************************************************************************
 * Set the volume
 ****************************************************************************/
static void dac_set_volume (struct i2c_client *clnt, struct ak4370 *dac)
{
  //printk("ak4370: setting volume %d (mute=%d)\n", dac->volume.left, dac->muted);

  int data = 0x40 | ak4370_volume_to_att (dac->volume.left);
  if (dac->muted)
    data |= 0x20;

  ak4370_reg_write (clnt, 0x0e, data);
}

/****************************************************************************
 * Set deemphasis
 ****************************************************************************/
static int ak4370_set_deemphasis (struct i2c_client *clnt, int freq)
{
        int val;
        switch (freq) {
        case 44100:
                val = 0;
                break;
        case 0:
                val = 0x1;
                break;
        case 48000:
                val = 0x2;
                break;
        case 32000:
                val = 0x3;
                break;
        default:
                return -EINVAL;
        }
        return ak4370_reg_write (clnt, 0x04, val);
}

/****************************************************************************
 * Mute/unmute the DAC
 ****************************************************************************/
static void mute_dac(int on)
{
  //printk("AK4370: mute_dac %d\n", on);
  if (dynamic_muting && g_dac)
  {
    //printk("AK4370: REALLY\n");
    g_dac->muted = on;
    dac_set_volume (&g_dac->clnt, g_dac);
  }
}

/****************************************************************************
 * Initialise the DAC registers
 ****************************************************************************/
static void dac_init(struct ak4370 *dac)
{
	struct i2c_client *clnt = &dac->clnt;

        if (board_type != 0 && board_type != 4) {
                ak4370_reg_write (clnt, 0x00, 0x1);	// power on vcom
                ak4370_reg_write (clnt, 0x02, 0x20);    // master mode
        }
        ak4370_reg_write (clnt, 0x00, 0x3f);	// power everything up
        ak4370_reg_write (clnt, 0x01, 0x80);	// MCKI = 256fs (this should be the default value)

	ak4370_reg_write (clnt, 0x03, 0x03);	// IIS
	ak4370_reg_write (clnt, 0x04, (deemphasis == 44100) ? 0x00 : 0x01);	// Unmute	
	ak4370_reg_write (clnt, 0x05, 0xff);	// full volume
	ak4370_reg_write (clnt, 0x06, 0xff);	// full volume
	ak4370_reg_write (clnt, 0x07, 0x03);	// DAC output to HP
	ak4370_reg_write (clnt, 0x08, log ? 0x43 : 0x03);	// DAC output to LOUT
	ak4370_reg_write (clnt, 0x09, 0x0f);	// line out 0dB

	/* Init volume */
	dac_set_volume (clnt, dac);

#if 0
        int i;
        printk("AK4370: register dump follows\n");
        for (i = 0; i < 0x12; i++) {
                printk("%02x: %02x\n", i, ak4370_reg_read (clnt, i));
        }
#endif
}


static int ak4370_private1(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct ak4370 *dac = kc->private_data;
  // wm8721 register write emulation
  int val;
  if (get_user(val, (int *)ev))
    return -EFAULT;
  int reg, data;
  reg = val >> 16;
  data = val & 0xffff;
  printk(PREFIX "%s %d %d\n", __FUNCTION__, reg, data);
  if (reg == 0x4) {
    // application trying to set aux-in
    if (data & 0x08) {
      ak4370_reg_write (&dac->clnt, 0x07, 0x0C);    // LIN1 output to HP
      ak4370_reg_write (&dac->clnt, 0x08, 0x0C);    // LIN2 output to LOUT
    } else {
      ak4370_reg_write (&dac->clnt, 0x07, 0x03);    // DAC output to HP
      ak4370_reg_write (&dac->clnt, 0x08, 0x03);    // DAC output to LOUT
    }
    return 0;
  }
  return -EINVAL;
}

static int ak4370_private2(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
    struct ak4370 *dac = kc->private_data;
		/* Set deemphasis */
    int val;
    if (get_user(val, (int *)ev))
			return -EFAULT;

		return ak4370_set_deemphasis(&dac->clnt, val);
}

static int ak4370_private3(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
    struct ak4370 *dac = kc->private_data;
		/* new style mux setting */
    int val;
    if (get_user(val, (int *)ev))
			return -EFAULT;
    int muxval = 0;
    switch (val) {
      case 0:         // PCM
        muxval = 0x03;
        break;
      case 1:         // line 1
        muxval = 0x0c;
        break;
      case 2:         // line 2
        muxval = 0x30;
        break;
      default:
        return -EINVAL;
    }
    printk(PREFIX "%s %d %d\n", __FUNCTION__, val, muxval);
    int ret = ak4370_reg_write (&dac->clnt, 0x07, muxval);
    if (ret == 0)
    {
      ret = ak4370_reg_write (&dac->clnt, 0x08, muxval);
    }
    return ret;
}

static int ak4370_set_volume(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct ak4370 *dac = kc->private_data;
  unsigned int val = ev->value.integer.value[0] & 0xff;
  dac->volume.left    = val & 255;
  dac->volume.right   = val & 255;  //val >> 8;
  dac_set_volume(&dac->clnt, dac);
  return 0;
}

static int ak4370_ctl_vol_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  ei->value.integer.min = 0;
  ei->value.integer.max = 100;
  return 0;
}

static int ak4370_ctl_bool_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
  ei->value.integer.min = 0;
  ei->value.integer.max = 1;
  return 0;
}


static int ak4370_do_nothing(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  return 0;
}

static int ak4370_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  ev->value.integer.value[0] = 0;

  return 0;
}

static struct snd_kcontrol_new ak4370_ctl_template = {
  .iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .get  = ak4370_ctl_get,
  .index = 0,
};


/* list of mixer controls */
struct ak4370_ctl
{
  const char *name;
  int (*put)(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev);
  int (*info)(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei);
  int index;
};


static struct ak4370_ctl ctrls[] = {
  {
    .name   = "Master Playback Volume",
    .put    = ak4370_set_volume,
    .info   = ak4370_ctl_vol_info,
  },
  {
    .name   = "PRIVATE1",
    .put    = ak4370_private1,
    .info   = ak4370_ctl_bool_info,
  },
  {
    .name   = "PRIVATE2",
    .put    = ak4370_private2,
    .info   = ak4370_ctl_vol_info,
  },
  {
    .name   = "PRIVATE3",
    .put    = ak4370_private3,
    .info   = ak4370_ctl_bool_info,
  },
  {
    .name   = "PCM",
    .put    = ak4370_do_nothing,
    .info   = ak4370_ctl_bool_info,
  },
  {
    .name   = "Aux",
    .put    = ak4370_do_nothing,
    .info   = ak4370_ctl_bool_info,
  },
};

static int
ak4370_add_ctrl(struct ak4370 *dac, struct ak4370_ctl *ctl)
{
  struct snd_kcontrol *kctl;
  int ret = -ENOMEM;

  printk("%s: %p: ctl %p (%s)\n", __FUNCTION__, dac, ctl, ctl->name);

  kctl = snd_ctl_new1(&ak4370_ctl_template, dac);
  if (kctl) {
    strlcpy(kctl->id.name, ctl->name, sizeof(kctl->id.name));
    kctl->private_value = (unsigned long)dac;
    kctl->id.index = ctl->index;
    kctl->put = ctl->put;
    kctl->info = ctl->info;

    ret = snd_ctl_add(dac->card, kctl);
    if (ret < 0)
      snd_ctl_free_one(kctl);

    printk("%s: %s ret %d kctl %p numid %d\n", __FUNCTION__, ctl->name, ret, kctl, kctl->id.numid);
  }

  return ret;
}

static int ak4370_add_ctrls(struct ak4370 *dac)
{
  int ret;
  int i;

  for (i = 0; i < ARRAY_SIZE(ctrls); i++) {
    ret = ak4370_add_ctrl(dac, &ctrls[i]);
    if (ret)
      return ret;
  }

  return 0;
}

/****************************************************************************
 * Enable the clocks to the DAC.
 * They need to be enabled before sending any I2C commands
 ****************************************************************************/
static void enable_dac_clocks(void)
{
  void __iomem *regs = ioremap(S3C2410_PA_IIS, 0x100);
  if (regs == NULL) {
    printk("%s couldn't get io memory\n", __FUNCTION__);
    return;
  }

  if (machine_is_rirm2())
  {
        /* set the s3c2410 to slave, and 16bit iis */
        __raw_writel(S3C2410_IISMOD_TXMODE | S3C2410_IISMOD_32FS | S3C2410_IISMOD_16BIT | S3C2410_IISMOD_LR_LLOW | S3C2410_IISMOD_SLAVE, regs + S3C2410_IISMOD);

        __raw_writel(0x63, regs + S3C2410_IISPSR);

        __raw_writel(S3C2410_IISCON_TXDMAEN | S3C2410_IISCON_RXIDLE | S3C2410_IISCON_TXIDLE | S3C2410_IISCON_IISEN, regs + S3C2410_IISCON);

        __raw_writel(S3C2410_IISFCON_TXENABLE | S3C2410_IISFCON_TXDMA, regs + S3C2410_IISFCON);
  }
  else
  {
        __raw_writel(S3C2412_IISMOD_TXMODE | S3C2412_IISMOD_32FS | S3C2412_IISMOD_16BIT | S3C2412_IISMOD_LR_LLOW | S3C2412_IISMOD_SLAVE, regs + S3C2412_IISMOD);
        __raw_writel(S3C2412_IISCON_TXDMACTIVE | S3C2412_IISCON_RXDMAPAUSE | S3C2412_IISCON_TXDMAPAUSE | S3C2412_IISCON_IISEN, regs + S3C2412_IISCON);
  }

  iounmap(regs);

        unsigned long flags;
        local_irq_save (flags);
        /* update pin config to ensure CDCLK is routed to the io pins */
        s3c2410_gpio_cfgpin (S3C2410_GPE2, S3C2410_GPE2_CDCLK);
        local_irq_restore (flags);
}


static struct i2c_driver ak4370_driver;
static struct s3c24xx_iis_ops s3c24xx_ak4370_ops;
static void *ak4370_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg);

static int ak4370_i2c_claim(struct ak4370 *dac)
{
  return i2c_use_client(&dac->clnt);
}

static int ak4370_i2c_release(struct ak4370 *dac)
{
  return i2c_release_client(&dac->clnt);
}

/****************************************************************************
 * This will get called for every I2C device that is found on the bus in the
 * address range specified in normal_i2c_range[]
 ****************************************************************************/
static int ak4370_attach(struct i2c_adapter *adap, int addr, int kind)		
{
	struct ak4370 *dac;
	struct s3c24xx_dac *plat_dac;
  int err;

	printk("%s I2C device found (address=%04x)\n", __FUNCTION__, addr);  

  dac = kzalloc(sizeof(*dac), GFP_KERNEL);
  if (dac == NULL) {
    err = -ENOMEM;
    goto exit_err;
  }
  plat_dac = kzalloc(sizeof(*plat_dac), GFP_KERNEL);
  if (plat_dac == NULL) {
    err = -ENOMEM;
    goto exit_err;
  }
	printk("plat_dac = %p ak4370 = %p\n", plat_dac, dac);  

  g_dac = dac;

  dac->volume.left = 0;
  dac->volume.right = 0;

  i2c_set_clientdata(&dac->clnt, dac);
  dac->clnt.addr  = addr;
  dac->clnt.adapter = adap;
  dac->clnt.driver  = &ak4370_driver;

  dac->claim   = ak4370_i2c_claim;
  dac->release   = ak4370_i2c_release;

  dac->my_dev = platform_device_alloc("s3c24xx-codec", -1);
  strcpy(plat_dac->name, "ak4370");
  plat_dac->dac = dac;
  plat_dac->attach_dac = ak4370_attach_codec;
  plat_dac->ops = &s3c24xx_ak4370_ops;
  dac->my_dev->dev.platform_data  = plat_dac;
  dac->my_dev->dev.parent   = &adap->dev;

  strlcpy(dac->clnt.name, "ak4370", I2C_NAME_SIZE);

  err = i2c_attach_client(&dac->clnt);
  if (err)
    goto exit_err;

  /* registered ok */

  platform_device_register(dac->my_dev);

  return 0;

 exit_err:
  return err;
}

static int ak4370_detach_client(struct i2c_client *clnt)
{
  int err;

  if ((err = i2c_detach_client(clnt))) {
    printk("Client deregistration failed, " "client not detached.\n");
    return err;
  }

  kfree(i2c_get_clientdata(clnt));
  g_dac = NULL;
  return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x10, I2C_CLIENT_END };
static unsigned short ignore_i2c[] = { I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_i2c,
	.probe = ignore_i2c,
	.ignore = ignore_i2c, 
};

static int ak4370_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, ak4370_attach);
}

static struct i2c_driver ak4370_driver = {
  .driver = {
    name:		"ak4370",
    owner:  THIS_MODULE,
  },
	.attach_adapter =	ak4370_attach_adapter,
	.detach_client =	ak4370_detach_client,
};

static int __init ak4370_init(void)
{
	printk("%s\n", __FUNCTION__);  

        switch (board_type) {
        case 0:
                pdn_pin = S3C2410_GPD10;
                break;

        case 1:
                pdn_pin = S3C2410_GPE7;
                break;

        case 2:
        case 4:
                pdn_pin = S3C2410_GPE5;
                break;

        case 3:
                pdn_pin = S3C2410_GPB9;
                break;

        default:
                printk("unknown board type %d\n", board_type);
                return -EINVAL;
        }

  /* turn on dac */
  if (board_type != 0 && board_type != 4)
    enable_dac_clocks ();

  normal_i2c[0] += addr_lsb;
	int res = i2c_add_driver(&ak4370_driver);
  reciva_register_mute_function(mute_dac);
  return res;
}

static void __exit ak4370_exit(void)
{
	printk("%s\n", __FUNCTION__);  
	i2c_del_driver(&ak4370_driver);
  reciva_unregister_mute_function(mute_dac);
}

module_init(ak4370_init);
module_exit(ak4370_exit);

MODULE_AUTHOR("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION("AK4370 DAC driver");
MODULE_LICENSE("GPL");

static int ak4370_free(struct snd_device *dev)
{
  struct ak4370 *pt_dev = dev->device_data;

  if (pt_dev->release)
    pt_dev->release(pt_dev);

  kfree(pt_dev);
  return 0;
}

static struct snd_device_ops ak4370_ops = {
  .dev_free = ak4370_free,
};

static void *ak4370_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg)
{
  struct s3c24xx_dac *s3c_dev = dev->platform_data;
  struct ak4370 *pt_dev = s3c_dev->dac;
  int ret = 0;

  printk("%s: card=%p, cfg=%p ak4370=%p\n", __FUNCTION__, card, cfg, dev->platform_data);

  if (pt_dev == NULL)
    return ERR_PTR(-ENOMEM);

  init_MUTEX(&pt_dev->lock);

  pt_dev->card  = card;
  pt_dev->cfg = cfg;

  if (pt_dev->claim) {
    ret = (pt_dev->claim)(pt_dev);

    if (ret)
      goto exit_err;
  }

  /* create a new sound device and attach controls */

  ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pt_dev, &ak4370_ops);
  if (ret)
    goto exit_err;


        s3c2410_gpio_cfgpin (pdn_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_pullup (pdn_pin, 1);
  s3c2410_gpio_setpin (pdn_pin, 0);

  udelay (10);

        s3c2410_gpio_setpin (pdn_pin, 1);

  udelay (10);

  /* initialise variables */
  dac_init (pt_dev);


  /* attach the mixer controls */
  ret = ak4370_add_ctrls(pt_dev);
  if (ret)
    goto exit_err;

  /* ok, return our new client */

  return pt_dev;

 exit_err:
  return ERR_PTR(ret);
}

static struct s3c24xx_iis_ops s3c24xx_ak4370_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup,
  .open   = s3c24xx_iis_op_open,
  .close    = s3c24xx_iis_op_close,
  .prepare  = s3c24xx_iis_op_prepare,
};
