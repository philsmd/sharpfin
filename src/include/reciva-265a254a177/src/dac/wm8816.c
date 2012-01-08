/*
 * WM8816 driver
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

#include <asm/arch/audio.h>
#include <asm/arch/s3c24xx-dac.h>

#include "../reciva_data_bus.h"

/* Device ID and debug strings */
#define PREFIX "WM8816:"
#define MIXER_INFO_ID "WM8816 MIXER"
#define MIXER_INFO_NAME MIXER_INFO_ID

struct wm8816 {
  struct snd_card     *card;
};

/****************************************************************************
 * Converts a volume in range 0 to 100 into a value to be programmed into
 * wm8816 volume control register
 ****************************************************************************/
static int volume0to100_to_wm8816(int vol)
{  
  if (vol > 100)
    vol = 100;  

  // Limit max vol to 0dB (0xe0)
  vol = (vol * 0xe0)/100; 
  return vol;
}

/****************************************************************************
 * Set the volume
 * volume = actual value to be programmed into gain register
 ****************************************************************************/
static void set_volume_direct(int reg_val)
{
  //printk("%s %d\n", __FUNCTION__, reg_val);
  char temp[2];
  temp[0] = 0xcb; // R5 - both gain registers
  temp[1] = reg_val;

  reciva_data_bus_write(temp, 2);
}

/****************************************************************************
 * Set the volume (range 0 to 100)
 ****************************************************************************/
static void set_volume0to100(int volume)
{
  //printk("%s %d\n", __FUNCTION__, volume);
  char reg_val = (char)volume0to100_to_wm8816(volume);
  set_volume_direct(reg_val);
}

static int wm8816_set_volume(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  //printk("%s\n", __FUNCTION__);
  unsigned int val = ev->value.integer.value[0] & 0xff;
  set_volume0to100(val);
  return 0;
}

static int wm8816_private1(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
    //printk("%s\n", __FUNCTION__);
    int val;
    int ret = get_user(val, (int *)ev);
    if (ret)
      return -EFAULT;

    set_volume_direct(val);

    return 0;
}

static int wm8816_ctl_vol_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  ei->value.integer.min = 0;
  ei->value.integer.max = 100;
  return 0;
}

static int wm8816_ctl_reg_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  ei->value.integer.min = 0;
  ei->value.integer.max = 255;
  return 0;
}

static int wm8816_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  ev->value.integer.value[0] = 0;

  return 0;
}

static struct snd_kcontrol_new wm8816_ctl_template = {
  .iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .get  = wm8816_ctl_get,
};


/* list of mixer controls */
struct wm8816_ctl
{
  const char *name;
  int (*put)(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev);
  int (*info)(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei);
};

static struct wm8816_ctl ctrls[] = {
  {
    .name   = "Master Playback Volume",
    .put    = wm8816_set_volume,
    .info   = wm8816_ctl_vol_info,
  },
  {
    .name   = "PRIVATE1",
    .put    = wm8816_private1,
    .info   = wm8816_ctl_reg_info,
  },
};

static int
wm8816_add_ctrl(struct wm8816 *dac, struct wm8816_ctl *ctl)
{
  struct snd_kcontrol *kctl;
  int ret = -ENOMEM;

  printk("%s: ctl %p (%s)\n", __FUNCTION__, ctl, ctl->name);

  kctl = snd_ctl_new1(&wm8816_ctl_template, NULL);
  if (kctl) {
    strlcpy(kctl->id.name, ctl->name, sizeof(kctl->id.name));
    kctl->put = ctl->put;
    kctl->info = ctl->info;

    ret = snd_ctl_add(dac->card, kctl);
    if (ret < 0)
      snd_ctl_free_one(kctl);

    printk("%s: %s ret %d kctl %p numid %d\n", __FUNCTION__, ctl->name, ret, kctl, kctl->id.numid);
  }

  return ret;
}

static int wm8816_add_ctrls(struct wm8816 *dac)
{
  int ret;
  int i;

  for (i = 0; i < ARRAY_SIZE(ctrls); i++) {
    ret = wm8816_add_ctrl(dac, &ctrls[i]);
    if (ret)
      return ret;
  }

  return 0;
}

/****************************************************************************
 * Handle mixer ioctl commands
 ****************************************************************************/
#if 0
int dac_mixer_ioctl(struct i2c_client *clnt, int cmd, void *arg)
{
  int val, nr = _IOC_NR(cmd), ret = 0;

  //printk(PREFIX " %s %d\n", __FUNCTION__, nr);

  if (cmd == SOUND_MIXER_INFO) {
    struct mixer_info mi;
    strncpy(mi.id, MIXER_INFO_ID, sizeof(mi.id));
    strncpy(mi.name, MIXER_INFO_NAME, sizeof(mi.name));
    mi.modify_counter = 0;
    return copy_to_user(arg, &mi, sizeof(mi));
  }
  else if (cmd == SOUND_MIXER_PRIVATE1)
  {
    if (get_user(val, (int *)arg))
      return -EFAULT;

    set_volume_direct(val);
    return 0;
  }

  /* Handle write commands */
  if (_IOC_DIR(cmd) & _IOC_WRITE) 
  {
    ret = get_user(val, (int *)arg);
    if (ret)
      goto out;

    switch (nr) 
    {
      case SOUND_MIXER_VOLUME:
        // Remember this is both left and right channel volume stored as 0xllrr
        set_volume0to100(val & 0xff);
        break;
      case SOUND_MIXER_BASS:
      case SOUND_MIXER_TREBLE:
      case SOUND_MIXER_PCM:
      case SOUND_MIXER_LINE1:
        break;
      default:
        ret = -EINVAL;
    }
  }

  /* Handle read commands */
  if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) 
  {
    int nr = _IOC_NR(cmd);
    ret = 0;

    switch (nr) 
    {
    case SOUND_MIXER_VOLUME:     val = 0;   break;
    case SOUND_MIXER_BASS:       val = 0;   break;
    case SOUND_MIXER_TREBLE:     val = 0;   break;
    case SOUND_MIXER_CAPS:       val = 0;		break;
    case SOUND_MIXER_STEREODEVS: val = 0;		break;
    case SOUND_MIXER_PCM:        val = 0;   break;
    case SOUND_MIXER_LINE1:      val = 0;   break;
    case SOUND_MIXER_DEVMASK:
      val = SOUND_MASK_VOLUME | SOUND_MASK_BASS | SOUND_MIXER_TREBLE |
            SOUND_MIXER_PCM | SOUND_MIXER_LINE1;
      break;
    default: val = 0; ret = -EINVAL; break;
    }

    if (ret == 0)
      ret = put_user(val, (int *)arg);
  }

out:
  return ret;
}
#endif

static struct s3c24xx_iis_ops s3c24xx_wm8816_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup,
  .open   = s3c24xx_iis_op_open,
  .close    = s3c24xx_iis_op_close,
  .prepare  = s3c24xx_iis_op_prepare,
};

static struct snd_device_ops wm8816_ops = {
};

static void *wm8816_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg)
{
  struct s3c24xx_dac *s3c_dev = dev->platform_data;
  struct wm8816 *wm_dev = s3c_dev->dac;
  int ret = 0;

  printk("%s: card=%p, cfg=%p platform=%p\n", __FUNCTION__, card, cfg, dev->platform_data);

  if (wm_dev == NULL)
    return ERR_PTR(-ENOMEM);

  wm_dev->card  = card;

  /* create a new sound device and attach controls */

  ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, wm_dev, &wm8816_ops);
  if (ret)
    goto exit_err;

  /* attach the mixer controls */
  ret = wm8816_add_ctrls(wm_dev);
  if (ret)
    goto exit_err;

  /* ok, return our new client */
  return wm_dev;

 exit_err:
  return ERR_PTR(ret);
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init this_module_init(void)
{
  printk(PREFIX "init\n");  
  int err = 0;
  struct wm8816 *dac;
  struct s3c24xx_dac *plat_dac;

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
  printk("plat_dac = %p", plat_dac);

  struct platform_device *my_dev = platform_device_alloc("s3c24xx-codec", -1);
  strcpy(plat_dac->name, "wm8816");
  plat_dac->dac = dac;
  plat_dac->attach_dac = wm8816_attach_codec;
  plat_dac->ops = &s3c24xx_wm8816_ops;
  my_dev->dev.platform_data  = plat_dac;
  my_dev->dev.parent   = NULL;

  platform_device_register(my_dev);

  return 0;

 exit_err:
  return err;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit this_module_exit(void)
{
  printk(PREFIX "exit\n");  
}


module_init(this_module_init);
module_exit(this_module_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva WM8816 DAC driver");
MODULE_LICENSE("GPL");
