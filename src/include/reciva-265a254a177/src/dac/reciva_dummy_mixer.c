/*
 * Reciva dummy mixer driver
 *
 * Copyright (c) 2008 Reciva Ltd
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

#include <asm/arch/s3c24xx-dac.h>

struct dummy_mixer {
  struct platform_device  *my_dev;

  struct snd_card     *card;
  struct wm8721_cfg   *cfg;

};

static int dummy_ctl_vol_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  ei->value.integer.min = 0;
  ei->value.integer.max = 255;
  return 0;
}

static int dummy_ctl_bool_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
  ei->value.integer.min = 0;
  ei->value.integer.max = 1;
  return 0;
}


static int dummy_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  ev->value.integer.value[0] = 0;

  return 0;
}

static int dummy_ctl_put(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  return 0;
}

static struct snd_kcontrol_new ctl_template = {
  .iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .get  = dummy_ctl_get,
  .put  = dummy_ctl_put,
};


/* list of mixer controls */
struct dummy_ctl
{
  const char *name;
  int (*info)(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei);
};


static struct dummy_ctl ctrls[] = {
  {
    .name   = "Master Playback Volume",
    .info   = dummy_ctl_vol_info,
  },
  {
    .name   = "PCM",
    .info   = dummy_ctl_bool_info,
  },
  {
    .name   = "Aux",
    .info   = dummy_ctl_bool_info,
  },
  {
    .name   = "Tone Control - Bass Volume",
    .info   = dummy_ctl_vol_info,
  },
  {
    .name   = "Tone Control - Treble Volume",
    .info   = dummy_ctl_vol_info,
  },
};

static int
dummy_add_ctrl(struct dummy_mixer *dac, struct dummy_ctl *ctl)
{
  struct snd_kcontrol *kctl;
  int ret = -ENOMEM;

  printk("%s: %p: ctl %p (%s)\n", __FUNCTION__, dac, ctl, ctl->name);

  kctl = snd_ctl_new1(&ctl_template, dac);
  if (kctl) {
    strlcpy(kctl->id.name, ctl->name, sizeof(kctl->id.name));
    kctl->private_value = (unsigned long)dac;
    kctl->id.index = 0;
    kctl->info = ctl->info;

    ret = snd_ctl_add(dac->card, kctl);
    if (ret < 0)
      snd_ctl_free_one(kctl);

    printk("%s: %s ret %d kctl %p numid %d\n", __FUNCTION__, ctl->name, ret, kctl, kctl->id.numid);
  }

  return ret;
}

static int dummy_add_ctrls(struct dummy_mixer *dac)
{
  int ret;
  int i;

  for (i = 0; i < ARRAY_SIZE(ctrls); i++) {
    ret = dummy_add_ctrl(dac, &ctrls[i]);
    if (ret)
      return ret;
  }

  return 0;
}







static struct s3c24xx_iis_ops s3c24xx_dummy_ops;
static void *dummy_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg);

static int __init dummy_mixer_init(void)
{
	printk("%s\n", __FUNCTION__);  
	struct dummy_mixer *dac;
	struct s3c24xx_dac *plat_dac;
  int err;

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
	printk("plat_dac = %p dummy_mixer = %p\n", plat_dac, dac);  

  dac->my_dev = platform_device_alloc("s3c24xx-codec", -1);
  strcpy(plat_dac->name, "dummy_mixer");
  plat_dac->dac = dac;
  plat_dac->attach_dac = dummy_attach_codec;
  plat_dac->ops = &s3c24xx_dummy_ops;
  dac->my_dev->dev.platform_data  = plat_dac;
  //dac->my_dev->dev.parent   = &adap->dev;

  /* registered ok */
  platform_device_register(dac->my_dev);

  return 0;

 exit_err:
  return err;
}

static void __exit dummy_mixer_exit(void)
{
	printk("%s\n", __FUNCTION__);  
}

module_init(dummy_mixer_init);
module_exit(dummy_mixer_exit);

MODULE_AUTHOR("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION("Dummy Mixer DAC driver");
MODULE_LICENSE("GPL");

static int dummy_free(struct snd_device *dev)
{
  struct dummy_mixer *pt_dev = dev->device_data;

  kfree(pt_dev);
  return 0;
}

static struct snd_device_ops dummy_ops = {
  .dev_free = dummy_free,
};

static void *dummy_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg)
{
  struct s3c24xx_dac *s3c_dev = dev->platform_data;
  struct dummy_mixer *pt_dev = s3c_dev->dac;
  int ret = 0;

  printk("%s: card=%p, cfg=%p dummy_mixer=%p\n", __FUNCTION__, card, cfg, dev->platform_data);

  if (pt_dev == NULL)
    return ERR_PTR(-ENOMEM);

  pt_dev->card  = card;

  /* create a new sound device and attach controls */

  ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pt_dev, &dummy_ops);
  if (ret)
    goto exit_err;

  /* attach the mixer controls */

  ret = dummy_add_ctrls(pt_dev);
  if (ret)
    goto exit_err;

  /* ok, return our new client */

  return pt_dev;

 exit_err:
  return ERR_PTR(ret);
}

static struct s3c24xx_iis_ops s3c24xx_dummy_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup,
  .open   = s3c24xx_iis_op_open,
  .close    = s3c24xx_iis_op_close,
  .prepare  = s3c24xx_iis_op_prepare,
};
