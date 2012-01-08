/* sound/arm/wm8721.c
 *
 * WN8721 Audio codec driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <sound/driver.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>

#include <asm/mach-types.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include <asm/arch/s3c24xx-dac.h>
#include "wm8721-hw.h"

#if 1
#define DBG(x...) do { printk(KERN_DEBUG x); } while(0)
#else
#define DBG(x...)
#endif

// Enable master mode (wm8721 generates BCLK/DACLRC)
static int master_mode;
module_param(master_mode, int, S_IRUGO);


#define TLV_MAXRATES		(8)

struct wm8721 {
	struct snd_card			*card;
	struct wm8721_cfg		*cfg;
	struct wm8721_rates	*rates;
	struct wm8721_hw		*hw;
	struct semaphore		 lock;

	/* wm8721 state */

	unsigned char			active;
	unsigned short			regs[0xf];

	unsigned int			rate_list_data[TLV_MAXRATES];
	struct snd_pcm_hw_constraint_list	rate_list;
};

/* sample rates */

struct wm8721_rate {
	unsigned int	dac;
	unsigned int	sr;
};

struct wm8721_rates {
	unsigned int		 base;
	unsigned int		 nr_rates;
	struct wm8721_rate *rates;
};

/* sample rate tables */

static struct wm8721_rate rate_usb[] = {
	{
		.dac	= 96000,
		.sr	= (7 << 2),
	}, {
		.dac	= 88200,
		.sr	= (15 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 48000,
		.sr	= (0 << 2),
	}, {
		.dac	= 44100,
		.sr	= (8 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 32000,
		.sr	= (6 << 2), 
	}, {
		.dac	= 8000,
		.sr	= (1 << 2),
	}, {
		.dac	= 8021,
		.sr	= (9 << 2) | WM8721_SRATE_BOSR,
	},
};

static struct wm8721_rate rate_11289600[] = {
	{
		.dac	= 88200,
		.sr	= (15 << 2),
	}, {
		.dac	= 44100,
		.sr	= (8 << 2),
	}, {
		.dac	= 8021,
		.sr	= (9 << 2),
	},
};

static struct wm8721_rate rate_16934400[] = {
	{
		.dac	= 88200,
		.sr	= (15 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 44100,
		.sr	= (8 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 8021,
		.sr	= (9 << 2) | WM8721_SRATE_BOSR,
	},
};

static struct wm8721_rate rate_18432000[] = {
	{
		.dac	= 96000,
		.sr	= (7 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 48000,
		.sr	= (0 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 32000,
		.sr	= (6 << 2) | WM8721_SRATE_BOSR,
	}, {
		.dac	= 8000,
		.sr	= (1 << 2) | WM8721_SRATE_BOSR,
	},
};

static struct wm8721_rate rate_12288000[] = {
	{
		.dac	= 96000,
		.sr	= (7 << 2),
	}, {
		.dac    = 48000,
		.sr	= 0,
	}, {
		.dac	= 32000,
		.sr	= (6 << 2),
	}, {
		.dac	= 8000,
		.sr	= (1 << 2),
	},
};

static struct wm8721_rates rates[] = {
	{
		.base		= 12000000,
		.rates		= rate_usb,
		.nr_rates	= ARRAY_SIZE(rate_usb),
	}, {
		.base		= 12288000,
		.rates		= rate_12288000,
		.nr_rates	= ARRAY_SIZE(rate_12288000),
	}, {
		.base		= 11289600,
		.rates		= rate_11289600,
		.nr_rates	= ARRAY_SIZE(rate_11289600),
	}, {
		.base		= 18432000,
		.rates		= rate_18432000,
		.nr_rates	= ARRAY_SIZE(rate_18432000),
	}, {
		.base		= 16934400,
		.rates		= rate_16934400,
		.nr_rates	= ARRAY_SIZE(rate_16934400),
	},
	{ }
};

static struct wm8721_rates *wm8721_get_rates(struct wm8721 *wm_dev)
{
	struct wm8721_rates *ptr = rates;

	if (wm_dev->cfg == NULL)
		return NULL;

	DBG("getting rate for %ld\n", wm_dev->cfg->clkrate);

	while (ptr->base != 0 && ptr->base != wm_dev->cfg->clkrate)
		ptr++;

	return ptr;
}

/* sound controls */

struct wm8721_ctl {
	struct snd_kcontrol_new	*ctl;
	const char		*name;

	int			def;
	int			reg;
	int			mask;
	int			shift;
	int			min;
	int			max;
};

/* wm8721 register accesses code */

static inline int wm8721_wr(struct wm8721 *wm_dev, int reg, int val)
{
  int ret = 0;
	if (reg != WM8721_RESET)
		wm_dev->regs[reg] = val;

	ret = (wm_dev->hw->wr)(wm_dev->hw, reg, val);

  udelay(1000);

	return ret; 
}

static inline int wm8721_wr_changed(struct wm8721 *wm_dev, int reg, int val)
{
	if (reg == WM8721_RESET)
		return wm8721_wr(wm_dev, reg, val);

	if (wm_dev->regs[reg] != val) {
		DBG("%s: reg %02x changed %02x -> %02x\n", __FUNCTION__,
		    reg, wm_dev->regs[reg], val);

		return wm8721_wr(wm_dev, reg, val);
	}

	return 0;
}

/* sound control code */

#define kctl_to_tlvctl(kc) ((struct wm8721_ctl *) (kc)->private_value)

static int wm8721_ctl_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
	struct wm8721_ctl *ctl = kctl_to_tlvctl(kc);

	ei->count = 1;

	if (ctl->min == ctl->max) {
    ei->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
		ei->value.integer.min = 0;
		ei->value.integer.max = 1;
	} else {
    ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		ei->value.integer.min = ctl->min;
		ei->value.integer.max = ctl->max;
	}

	return 0;
}

static int wm8721_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
	struct wm8721 *wm_dev = kc->private_data;
	struct wm8721_ctl *ctl = kctl_to_tlvctl(kc);
	unsigned int val;

	down(&wm_dev->lock);
	val = wm_dev->regs[ctl->reg];
	val  &= ctl->mask;
	val >>= ctl->shift;
	up(&wm_dev->lock);

	ev->value.integer.value[0] = val;

	DBG("%s: got %d from 0x%x (0x%0x)\n", __FUNCTION__, val, ctl->reg,
	    wm_dev->regs[ctl->reg]);

	return 0;
}

static int wm8721_ctl_put(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
	struct wm8721 *wm_dev = kc->private_data;
	struct wm8721_ctl *ctl = kctl_to_tlvctl(kc);
	unsigned int val = ev->value.integer.value[0];
  unsigned short tmp = 0;

	DBG("%s: put %p, %d\n", __FUNCTION__, kc, val);
	DBG("\t %s reg %d shift %x mask %d\n", ctl->name, ctl->reg, ctl->shift, ctl->mask);

	down(&wm_dev->lock);

  tmp =  wm_dev->regs[ctl->reg] & ~ctl->mask;
	tmp |= val << ctl->shift;
	
	if (wm_dev->active || 1) {
		wm8721_wr_changed(wm_dev, ctl->reg, tmp);
	}

	up(&wm_dev->lock);
	return 0;
}

static struct snd_kcontrol_new wm8721_ctl = {
	.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
	.access	= SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info	= wm8721_ctl_info,
	.get	= wm8721_ctl_get,
	.put	= wm8721_ctl_put,
};

static int wm8721_reg_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
	ev->value.integer.value[0] = 0;

	return 0;
}

static int wm8721_reg_ctl_put(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
	struct wm8721 *wm_dev = kc->private_data;
  /* Write to CODEC register
   * Upper 16 bits : register offset
   * Lower 16 bits : register value */

  unsigned int tmp = 0;
  if (get_user(tmp, (int *)ev))
    return -EFAULT;
	int reg = (tmp >> 16) & 0xffff;
	int val = tmp & 0x7f;

	DBG("%s: put %p, %02x %04x\n", __FUNCTION__, kc, reg, val);

	down(&wm_dev->lock);

	if (wm_dev->active || 1) {
		wm8721_wr_changed(wm_dev, reg, val);
	}

	up(&wm_dev->lock);
	return 0;
}

static struct snd_kcontrol_new wm8721_reg_ctl = {
	.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
	.access	= SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info	= wm8721_ctl_info,
	.get	= wm8721_reg_ctl_get,
	.put	= wm8721_reg_ctl_put,
};

/* list of mixer controls */

struct wm8721_ctl ctrls[] = {
	{
		.ctl		= &wm8721_ctl,
		.name		= "Master Playback Volume",
		.reg		= WM8721_LOUT1V,
		.def		= 0,
		.shift	= 0,
		.mask		= (1<<7)-1,
		.min		= 0,
		.max		= 127,
  // JM TODO De-emphasis control, On/Off stuff
  },
	{
		.ctl		= &wm8721_reg_ctl,
		.name		= "PRIVATE1",
		.min		= 0,
		.max		= 0x7fffff,
  },
};

static int
wm8721_add_ctrl(struct wm8721 *wm_dev, struct wm8721_ctl *ctl)
{
	struct snd_kcontrol *kctl;
	int ret = -ENOMEM;

	DBG("%s: %p: ctl %p (%s)\n", __FUNCTION__, wm_dev, ctl, ctl->name);

	kctl = snd_ctl_new1(ctl->ctl, wm_dev);
	if (kctl) {
		strlcpy(kctl->id.name, ctl->name, sizeof(kctl->id.name));
		kctl->private_value = (unsigned long)ctl;
		
		// todo - ensure default value //

		ret = snd_ctl_add(wm_dev->card, kctl);
		if (ret < 0)
			snd_ctl_free_one(kctl);

    DBG("%s: %s ret %d kctl %p numid %d\n", __FUNCTION__, ctl->name, ret, kctl, kctl->id.numid);
	} 

	return ret;
}

static int wm8721_add_ctrls(struct wm8721 *wm_dev)
{
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(ctrls); i++) {
		ret = wm8721_add_ctrl(wm_dev, &ctrls[i]);
		if (ret)
			return ret;
	}
	
	return 0;
}

/* rate control */

static int wm8721_rates_to_list(struct wm8721_rates *rates,
				     struct snd_pcm_hw_constraint_list *list,
				     int dac)
{
       struct wm8721_rate *rp;
       int *ptr = list->list;
       int clk;

       DBG("%s: rates=%p, list=%p, dac=%d\n", __FUNCTION__, rates, list, dac);
       DBG("%s: rates->rates =%p, list->list=%p\n", __FUNCTION__, rates->rates, list->list);

       if (rates == NULL)
	       return -EINVAL;

       if (rates->rates == NULL)
	       return -EINVAL;

       rp = rates->rates;

       list->count = 0;
       list->mask = 0;

       for (clk = 0; clk < TLV_MAXRATES; clk++, rp++, ptr++) {
	       if (clk >= rates->nr_rates)
		       break;
	       
	       *ptr = rp->dac;
	       list->count++;
       }

       /* sort the resutls */

 restart_sort:
       for (clk = 0; clk < list->count-1; clk++) {
	       ptr = list->list + clk;
	       if (ptr[0] > ptr[1]) {
		       int tmp = ptr[1];
		       ptr[1] = ptr[0];
		       ptr[0] = tmp;
		       goto restart_sort;
	       }
       }

       return 0;
}

static int wm8721_find_sr(struct wm8721 *wm_dev, int rate)
{
	struct wm8721_rate *rp = wm_dev->rates->rates;
	int i;

	DBG("%s(%p,%d)\n", __FUNCTION__, wm_dev, rate);

	for (i = 0; i < wm_dev->rates->nr_rates; i++, rp++) {
		if (rp->dac == rate)
			return rp->sr;
	}
	
	return -1;
}


static int wm8721_reset(struct wm8721 *wm_dev)
{
	int ret;

	ret = wm8721_wr(wm_dev, WM8721_RESET, 0x00);
	if (ret)
		return ret;

	mdelay(1);
	return 0;
}

static int wm8721_configure(struct wm8721 *wm_dev)
{
	struct wm8721_cfg *cfg = wm_dev->cfg;
	int ret;
	int reg;

  
	/* set whether we are clock master or not */
	wm_dev->regs[WM8721_IFACE] &= ~WM8721_IFACE_MASTER;

	DBG("%s: master_mode=%d\n", __FUNCTION__, master_mode);
	if (cfg->master || master_mode)
	{
		wm_dev->regs[WM8721_IFACE] |= WM8721_IFACE_MASTER;
		wm_dev->regs[WM8721_PWR] &= ~(WM8721_PWR_OSCPD | WM8721_PWR_OUTPUT);
	}

	/* configure all registers */

	// for (reg = 0; reg < WM8721_RESET; reg++) {
	for (reg = 2; reg < 10; reg++) {
		ret = wm8721_wr(wm_dev, reg, wm_dev->regs[reg]);
	}

	/* check over the clock */
	
	wm_dev->rates = wm8721_get_rates(wm_dev);
	if (wm_dev->rates == NULL) {
		ret = -EINVAL;
		goto err;
	}

	return 0;

 err:
	return ret;
}

static int wm8721_free(struct snd_device *dev)
{
	struct wm8721 *wm_dev = dev->device_data;

	if (wm_dev->hw && wm_dev->hw->release)
		wm_dev->hw->release(wm_dev->hw);

	kfree(wm_dev);
	return 0;
}

static struct snd_device_ops wm8721_ops = {
	.dev_free	= wm8721_free,
};

static unsigned short wm8721_defaultregs[WM8721_RESET] = {
  // For WM8731 default line-in volume to 0dB, unmuted
	[WM8731_LIN1V] = WM8721_VOL_BOTH | 0x17,
	[WM8731_RIN1V] = WM8721_VOL_BOTH | 0x17,
	[WM8721_LOUT1V] = WM8721_VOL_ZEROCROSS | WM8721_VOL_BOTH | 0x69,
	[WM8721_ROUT1V] = WM8721_VOL_ZEROCROSS | WM8721_VOL_BOTH | 0x69,
	[WM8721_APANA]	= WM8721_APANA_DAC,
	[WM8721_APDIGI]	= WM8721_APDIGI_DE_44K1,
	[WM8721_PWR]		= (0xff &
                          ~(WM8721_PWR_ON | WM8721_PWR_DAC
                            | WM8721_PWR_OUTPUT | WM8731_PWR_LINEIN)),
	[WM8721_IFACE]	= WM8721_IFACE_I2S | WM8721_IFACE_16BITS,
	[WM8721_SRATE]	= 0x20, /* 44.1kHz for a 11.2896Mhz clock */
	[WM8721_ACTIVE]	= 1,
};

void *wm8721_attach(struct snd_card *card,
				       struct device *dev,
				       struct wm8721_cfg *cfg)
{
  struct s3c24xx_dac *s3c_dev = dev->platform_data;
	struct wm8721 *wm_dev;
	int ret = 0;
	
	DBG("%s: card=%p, cfg=%p\n", __FUNCTION__, card, cfg);

	wm_dev = kmalloc(sizeof(*wm_dev), GFP_KERNEL);
	if (wm_dev == NULL)
		return ERR_PTR(-ENOMEM);

	memset(wm_dev, 0, sizeof(*wm_dev));
	memcpy(&wm_dev->regs, wm8721_defaultregs, sizeof(wm_dev->regs));
	init_MUTEX(&wm_dev->lock);

	wm_dev->card	= card;
	wm_dev->cfg	= cfg;
	wm_dev->hw		= s3c_dev->dac;

	if (wm_dev->hw == NULL) {
		dev_err(dev, "no platform data\n");
		ret = -ENOENT;
		goto exit_err;
	}

	if (wm_dev->hw->claim) {
		ret = (wm_dev->hw->claim)(wm_dev->hw);
	
		if (ret)
			goto exit_err;
	}

	/* create a new sound device and attach controls */

	ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, wm_dev, &wm8721_ops);
	if (ret)
		goto exit_err;

	/* initialise variables */

	wm_dev->rate_list.list = wm_dev->rate_list_data;

	wm8721_reset(wm_dev);

	/* attach the mixer controls */

	ret = wm8721_add_ctrls(wm_dev);
	if (ret)
		goto exit_err;

	/* configure the device */
	wm8721_configure(wm_dev);

	/* ok, return our new client */

	return wm_dev;

 exit_err:
	return ERR_PTR(ret);
}

EXPORT_SYMBOL(wm8721_attach);

void wm8721_detach(void *wm_dev)
{
	/* nothing to do here */
}

EXPORT_SYMBOL(wm8721_detach);

/* routines to deal with the sound system */
int wm8721_prepare(void *pw,
			struct snd_pcm_substream *substream,
			struct snd_pcm_runtime *runtime)
{
  struct wm8721 *wm_dev = pw;
	//int sr = wm8721_find_sr(wm_dev, runtime->rate);
	int tmp;

  if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
    return -EINVAL;
  
#if 0
	/* update sample rate */

	tmp = wm_dev->regs[WM8721_SRATE];
	tmp &= ~(WM8721_SRATE_BOSR | WM8721_SRATE_USB | WM8721_SRATE_MASK);

	if (wm_dev->cfg->clkrate == 12000000)
		tmp |= WM8721_SRATE_USB;

	if (sr == -1) {
		printk(KERN_ERR "cannot get rate for %d\n", runtime->rate);
		return -EINVAL;
	}

	wm8721_wr_changed(wm_dev, WM8721_SRATE, tmp | sr);
#endif

	/* update format */

	tmp = wm_dev->regs[WM8721_IFACE];
	tmp &= ~WM8721_IFACE_FORMATMASK;		

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		tmp |= WM8721_IFACE_16BITS;
		break;
  case SNDRV_PCM_FORMAT_S20_3LE:
		tmp |= WM8721_IFACE_20BITS;
    break;
  case SNDRV_PCM_FORMAT_S24_3LE:
		tmp |= WM8721_IFACE_24BITS;
    break;

	default:
		printk(KERN_ERR "unknown data format\n");
		return -EINVAL;
	}
	
	wm8721_wr_changed(wm_dev, WM8721_IFACE, tmp);

	return 0;
}

EXPORT_SYMBOL(wm8721_prepare);

