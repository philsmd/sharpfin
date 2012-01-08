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
#include <linux/version.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <asm/arch/audio.h>

#include <asm/arch/s3c24xx-dac.h>

#ifdef CONFIG_ARCH_S3C2410
#include <asm/arch/regs-gpio.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <asm/arch/regs-irq.h>
#else
#include <asm/arch-s3c2410/S3C2410-irq.h>
#endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM(x) module_param(x, int, S_IRUGO)
#else
#define RECIVA_MODULE_PARM(x) MODULE_PARM(x, "i")
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM_TYPE(x) module_param(x, uint, S_IRUGO)
#else
#define RECIVA_MODULE_PARM_TYPE(x) MODULE_PARM(x, "i")
#endif

#define MODULE_NAME "dummy_mixer"
#define PFX MODULE_NAME ":"

// Initial sample rate
static int initial_sample_rate = 48000;
RECIVA_MODULE_PARM_TYPE(initial_sample_rate);

// Static function prototypes
static void setup_sample_rate(int rate);


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
	printk("  initial_sample_rate=%d\n", initial_sample_rate);  

	if (initial_sample_rate >= 0)
		setup_sample_rate(initial_sample_rate);

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



// Sample rate indexes
typedef enum
{
  SAMPLE_RATE_INDEX_8000 = 0,
  SAMPLE_RATE_INDEX_11025 = 1,
  SAMPLE_RATE_INDEX_16000 = 2,
  SAMPLE_RATE_INDEX_22050 = 3,
  SAMPLE_RATE_INDEX_32000 = 4,
  SAMPLE_RATE_INDEX_44100 = 5,
  SAMPLE_RATE_INDEX_48000 = 6,
  SAMPLE_RATE_INDEX_88200 = 7,
  SAMPLE_RATE_INDEX_96000 = 8,

} sampe_rate_index_t;

// cf672 pin levels
//
// CLK0/CLK1/SAM0/SAM1
// 0000 - 96000
// 0001 - 24000 (n/a)
// 0010 - 48000
// 0011 - 12000 (n/a)
// 0100 - 32000
// 0101 - 8000
// 0110 - 16000
// 0111 - ??
// 1000 - 44100
// 1001 - 11025
// 1010 - 22050
// 1011 - ??
// 1100 - not used
// 1101 - not used
// 1110 - not used
// 1111 - not used
static unsigned int pin_levels_cf672[] =
{
  0x05, // SAMPLE_RATE_INDEX_8000
  0x09, // SAMPLE_RATE_INDEX_11025
  0x06, // SAMPLE_RATE_INDEX_16000
  0x0a, // SAMPLE_RATE_INDEX_22050
  0x04, // SAMPLE_RATE_INDEX_32000
  0x08, // SAMPLE_RATE_INDEX_44100
  0x02, // SAMPLE_RATE_INDEX_48000

  // XXX cant genearte 88.2 on current hardware
  0x08, // SAMPLE_RATE_INDEX_88200

  0x00, // SAMPLE_RATE_INDEX_96000
};

// cf672 - gpio pins used to select sample rate
// CLK0 = J3-29 (GPD3)
// CLK1 = J3-31 (GPD2)
// SAM0 = J3-25 (GPD5)
// SAM1 = J3-27 (GPD4)
#define NUM_RATE_PINS 4
static int rate_pins_cf672[NUM_RATE_PINS] =
{
  S3C2410_GPD4,
  S3C2410_GPD5,
  S3C2410_GPD2,
  S3C2410_GPD3,
};

static unsigned int *pin_levels = pin_levels_cf672;
static unsigned int *rate_pins = rate_pins_cf672;

/****************************************************************************
 * Convert a sample rate into a pin levels bitmap
 ****************************************************************************/
static unsigned int rate_to_pin_levels(int rate)
{
  unsigned int levels = 0;
  int index = SAMPLE_RATE_INDEX_8000;

  switch (rate)
  {
    case 8000:  index = SAMPLE_RATE_INDEX_8000; break;
    case 11025: index = SAMPLE_RATE_INDEX_11025; break;
    case 16000: index = SAMPLE_RATE_INDEX_16000; break;
    case 22050: index = SAMPLE_RATE_INDEX_22050; break;
    case 32000: index = SAMPLE_RATE_INDEX_32000; break;
    case 44100: index = SAMPLE_RATE_INDEX_44100; break;
    case 48000: index = SAMPLE_RATE_INDEX_48000; break;
    case 88200: index = SAMPLE_RATE_INDEX_88200; break;
    case 96000: index = SAMPLE_RATE_INDEX_96000; break;
  }

  levels = pin_levels[index];

  return levels;
}

/****************************************************************************
 * Set up the hardware to generate the required sample rate
 * rate - sample rate in hz
 ****************************************************************************/
static void setup_sample_rate(int rate)
{
  printk(PFX "%s r=%d\n", __FUNCTION__, rate);
  int i;
  unsigned int levels = rate_to_pin_levels(rate);
  int level;
  printk("  levels=0x%08x\n", levels);

  for (i=0; i<NUM_RATE_PINS; i++)
  {
    if ((levels >> i) & 0x01)
      level = 1;
    else
      level = 0;

    s3c2410_gpio_cfgpin (rate_pins[i], S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_setpin (rate_pins[i], level);
    s3c2410_gpio_pullup (rate_pins[i], 1); // off 
  }
}

/****************************************************************************
 * Print some debug
 ****************************************************************************/
static void print_struct_runtime(struct snd_pcm_runtime *runtime)
{
  if (runtime)
  {
    printk("  (runtime)");
    printk(" rate=%d", runtime->rate);
    printk(" channels=%d", runtime->channels);
    printk(" frame_bits=%d", runtime->frame_bits);
    printk(" sample_bits=%d\n", runtime->sample_bits);

    struct snd_pcm_hardware *hw = &runtime->hw;
    if (hw)
    {
      printk("  (hw)");
      printk(" channels_min=%d", hw->channels_min);
      printk(" channels_max=%d", hw->channels_max);
      printk(" rates=0x%x", hw->rates);
      printk(" rate_min=0x%x", hw->rate_min);
      printk(" rate_max=0x%x", hw->rate_max);
      printk(" formats=0x%llx\n", hw->formats);
    }
  }
}

/****************************************************************************
 * Think this is just a request for device capabilities
 * (see include/sound/pcm.h for structs)
 ****************************************************************************/
static int reciva_iis_op_open(void *pw, struct snd_pcm_substream *substream)
{
  struct snd_pcm_runtime *runtime = substream->runtime;
  struct snd_pcm_hardware *hw = &runtime->hw;

  printk(PFX "%s\n", __FUNCTION__);
  print_struct_runtime(runtime);

  if (substream == NULL || hw == NULL)
    return -EINVAL;

  // see include/sound/pcm.h
  hw->channels_min = 2;
  hw->channels_max = 2;
  hw->rates  = 
               SNDRV_PCM_RATE_8000 |
               SNDRV_PCM_RATE_11025 |
               SNDRV_PCM_RATE_16000 |
               SNDRV_PCM_RATE_22050 |
               SNDRV_PCM_RATE_32000 |
               SNDRV_PCM_RATE_44100 |
               SNDRV_PCM_RATE_48000 |
               SNDRV_PCM_RATE_88200 |
               SNDRV_PCM_RATE_96000 |
               0;
  hw->rate_min = 8000;
  hw->rate_max = 96000;
  hw->formats     &= (  SNDRV_PCM_FMTBIT_S16_LE
                      | SNDRV_PCM_FORMAT_S20_3LE
                      | SNDRV_PCM_FMTBIT_S24_3LE);

  return 0;
}

/****************************************************************************
 * Notification of device configuration
 ****************************************************************************/
static int reciva_iis_op_prepare(void *pw,
      struct snd_pcm_substream *substream,
      struct snd_pcm_runtime *runtime)
{
  printk(PFX "%s\n", __FUNCTION__);
  print_struct_runtime(runtime);

  // Set up the required sample rate
  if (runtime)
    setup_sample_rate(runtime->rate);

  return 0;
}


static struct s3c24xx_iis_ops s3c24xx_dummy_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup, // see s3c24xx-wm8721.c
  .open     = reciva_iis_op_open,
  .close    = s3c24xx_iis_op_close,   // see s3c24xx-wm8721.c
  .prepare  = reciva_iis_op_prepare,
};
