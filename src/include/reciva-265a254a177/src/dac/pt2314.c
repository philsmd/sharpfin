/*
 * Princeton PT2314 device driver
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

#include <asm/arch/s3c24xx-dac.h>

#define PT2314_NAME	"pt2314"
#define MIXER_INFO_ID "PT2314"
#define MIXER_INFO_NAME "PT2314"

#if 0
static int standalone;
MODULE_PARM(standalone, "i");
#endif

static int force_attach;
module_param(force_attach, int, S_IRUGO);

#define PT2314_AUD_SWITCH_REG		0x40
#define PT2314_AUD_SWITCH_LOUDNESS_OFF		0x04
#define PT2314_AUD_SWITCH_LOUDNESS_ON		0x00
#define PT2314_AUD_SWITCH_LEVEL_0DB		0x18
#define PT2314_AUD_SWITCH_LEVEL_3_75DB		0x10
#define PT2314_AUD_SWITCH_LEVEL_7_5DB		0x08
#define PT2314_AUD_SWITCH_LEVEL_11_25DB		0x00
#define PT2314_TREBLE_REG		0x70
#define PT2314_BASS_REG			0x60
#define PT2314_SPEAKER_LR_REG           0xC0
#define PT2314_SPEAKER_RR_REG           0xE0
#define PT2314_SPEAKER_LF_REG           0x80
#define PT2314_SPEAKER_RF_REG           0xA0

static int new_mux;

struct pt2314_eq
{
        /* all in mB */
        int bass; 
        int treble;
        int gain;
};

struct i2c_gain {
	unsigned int  left:8;
	unsigned int  right:8;
};

struct pt2314 {
	struct i2c_client clnt;
  struct platform_device  *my_dev;
	struct i2c_gain	volume;
	int source_reg;

  int   (*claim)(struct pt2314 *);
  int   (*release)(struct pt2314 *);
  int   (*wr)(struct pt2314 *, int reg, int val);

  struct snd_card     *card;
  struct wm8721_cfg   *cfg;
  struct semaphore     lock;

};

static int pt2314_write_reg(struct i2c_client *clnt, int data)
{
	char buffer[1];
	int r;

	buffer[0] = data & 0xff;

	r = i2c_master_send(clnt, buffer, 1);
	if (r != 1) {
		printk(KERN_ERR "pt2314: write failed, status %d\n", r);
		return -EIO;
	}

	return 0;
}


static int volume_to_pt2314(int vol)
{
	return 63 - ((vol * 63) / 100);
}

static const unsigned char eq_table[] =
{
	0,		// -14
	0,		// -13
	1,		// -12
	1,		// -11
	2,		// -10
	2,		// -9
	3,		// -8
	3,		// -7
	4,		// -6
	4,		// -5
	5,		// -4
	5,		// -3
	6,		// -2
	6,		// -1
	7,		// 0
	14,		// 1
	14,		// 2
	13,		// 3
	13,		// 4
	12,		// 5
	12,		// 6
	11,		// 7
	11,		// 8
	10,		// 9
	10,		// 10
	9,		// 11
	9,		// 12
	8,		// 13
	8,		// 14
};

static inline int eq_to_pt2314(int val)
{
	int r;

	if (val > 28) {
		printk("pt2314: eq %x out of range\n", val);
		return 0;
	}

	r = eq_table[val];

	return r;
}

static int pt2314_set_bass(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  unsigned int val = ev->value.integer.value[0] & 0xff;
	return pt2314_write_reg(&dac->clnt, PT2314_BASS_REG | eq_to_pt2314 (val));
}

static int pt2314_set_treble(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  unsigned int val = ev->value.integer.value[0] & 0xff;
	return pt2314_write_reg(&dac->clnt, PT2314_TREBLE_REG | eq_to_pt2314 (val));
}

static int pt2314_set_volume(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  unsigned int val = ev->value.integer.value[0];
  printk("pt2314_set_volume val=%d %08x", val, val);
  val &= 0xff;
  dac->volume.left    = val & 255;
  dac->volume.right   = val & 255;  //val >> 8;

  int newvol;
  newvol = volume_to_pt2314(dac->volume.left) & 0x3f;
  printk(" (%d, %02x)\n", dac->volume.left, newvol);
  if ( dac->volume.left == 0 )
  {
    printk("Volume 0. Muting speakers.\n");

    // set speaker att to MUTE
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_LF_REG | 0x1F);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_RF_REG | 0x1F);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_LR_REG | 0x1F);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_RR_REG | 0x1F);
  }
  else
  {
    // set speaker att to 0dB
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_LF_REG | 0);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_RF_REG | 0);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_LR_REG | 0);
    pt2314_write_reg (&dac->clnt, PT2314_SPEAKER_RR_REG | 0);
  }
  return pt2314_write_reg(&dac->clnt, 0 | newvol);		// volume	
}

#if 0
static int millibel_gain_to_pt2314 (int mb)
{
        int bels = (mb / 1000);
        int frac = mb % 1000;
        int val;

        if (bels > 0)
                bels = 0;
        if (bels < -7)
                bels = -7;

        val = (-bels) << 3;
        if (frac <= -875)
                val |= 0x7;
        else if (frac <= -750)
                val |= 0x6;
        else if (frac <= -625)
                val |= 0x5;
        else if (frac <= -500)
                val |= 0x4;
        else if (frac <= -375)
                val |= 0x3;
        else if (frac <= -250)
                val |= 0x2;
        else if (frac <= -125)
                val |= 0x1;

        return val;
}

static int millibel_eq_to_pt2314 (int mb)
{
        int db = mb / 100;
        if (db < -14)
                db = -14;
        if (db > 14)
                db = 14;
        db += 14;

        return eq_table[db];
}

static int pt2314_set_eq(struct i2c_client *clnt, struct pt2314_eq *eq)
{
        return -ENOSYS;
}
#endif

static int pt2314_update_source (struct i2c_client *clnt, struct pt2314 *dac)
{
	return pt2314_write_reg (clnt, PT2314_AUD_SWITCH_REG | dac->source_reg);
}

static int pt2314_set_source(struct pt2314 *dac, int source)
{
        printk("pt2314: set_source %d\n", source);
	
	dac->source_reg = (dac->source_reg & ~3) | (source & 3);
	return pt2314_update_source (&dac->clnt, dac);
}

// PRIVATE3
static int pt2314_set_loudness(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  int val;
  int ret = get_user(val, (int *)ev);
  if (ret)
    return -EFAULT;

  printk("pt2314: set_loudness %d\n", val);

	dac->source_reg &= ~(PT2314_AUD_SWITCH_LOUDNESS_OFF | PT2314_AUD_SWITCH_LOUDNESS_ON);
	dac->source_reg |= val ? PT2314_AUD_SWITCH_LOUDNESS_ON : PT2314_AUD_SWITCH_LOUDNESS_OFF;
	
	return pt2314_update_source (&dac->clnt, dac);
}

static void pt2314_cmd_init(struct i2c_client *clnt)
{
  // set speaker att to 0dB
  pt2314_write_reg (clnt, PT2314_SPEAKER_LF_REG | 0);
  pt2314_write_reg (clnt, PT2314_SPEAKER_RF_REG | 0);
  pt2314_write_reg (clnt, PT2314_SPEAKER_LR_REG | 0);
  pt2314_write_reg (clnt, PT2314_SPEAKER_RR_REG | 0);
}

static int pt2314_private1(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  pt2314_cmd_init (&dac->clnt);
  return 0;
}

static int pt2314_private4(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
    struct pt2314 *dac = kc->private_data;
		/* new style mux setting */
    int val;
    int ret = get_user(val, (int *)ev);
		if (ret)
			return -EFAULT;

		new_mux = 1;

		return pt2314_set_source (dac, val);
}

static int pt2314_source_req(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  struct pt2314 *dac = kc->private_data;
  if (new_mux)
    return -ENOSYS;

	return pt2314_set_source (dac, kc->id.index);
}

#if 0
/****************************************************************************
 * Handle mixer ioctl commands
 ****************************************************************************/
int dac_mixer_ioctl(struct i2c_client *clnt, int cmd, void *arg)
{
	struct pt2314 *dac = clnt->data;
	int val, nr = _IOC_NR(cmd), ret = 0;
	
	if (cmd == SOUND_MIXER_INFO) {
		struct mixer_info mi;
		strncpy(mi.id, MIXER_INFO_ID, sizeof(mi.id));
		strncpy(mi.name, MIXER_INFO_NAME, sizeof(mi.name));
		mi.modify_counter = 0;
		return copy_to_user(arg, &mi, sizeof(mi));
	}

        if (cmd == SOUND_MIXER_PRIVATE1) {
                pt2314_cmd_init (clnt);
                return 0;
        }
	
        if (cmd == SOUND_MIXER_PRIVATE2) {
                struct pt2314_eq eq;

                if (copy_from_user (arg, &eq, sizeof (eq)))
                        return -EFAULT;

                return pt2314_set_eq (clnt, &eq);
        }

	if (cmd == SOUND_MIXER_PRIVATE3) {
		ret = get_user(val, (int *)arg);
		if (ret)
			goto out;
		
		return pt2314_set_loudness (clnt, val);
	}

	if (cmd == SOUND_MIXER_PRIVATE4) {
		/* new style mux setting */
		ret = get_user(val, (int *)arg);
		if (ret)
			goto out;

		new_mux = 1;

		return pt2314_set_source (clnt, val);
	}

	/* Handle write commands */
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = get_user(val, (int *)arg);
		if (ret)
			goto out;
		
		switch (nr) {
		case SOUND_MIXER_VOLUME:
			dac->volume.left    = val & 255;
			dac->volume.right   = val & 255;  //val >> 8;
			pt2314_set_volume (clnt, dac);
			break;
		case SOUND_MIXER_PCM:
			if (new_mux)
				return -ENOSYS;
			pt2314_set_source (clnt, 0);
			break;
		case SOUND_MIXER_LINE1:
			if (new_mux)
				return -ENOSYS;
			pt2314_set_source (clnt, 1);
			break;
		case SOUND_MIXER_LINE2:
			if (new_mux)
				return -ENOSYS;
			pt2314_set_source (clnt, 2);
			break;
		case SOUND_MIXER_LINE3:
			if (new_mux)
				return -ENOSYS;
			pt2314_set_source (clnt, 3);
			break;
		case SOUND_MIXER_BASS:
			pt2314_set_bass (clnt, val & 0xff);
			break;
		case SOUND_MIXER_TREBLE:
			pt2314_set_treble (clnt, val & 0xff);
			break;
		default:
			ret = -EINVAL;
		}
	}
	
	/* Handle read commands */
	if (ret == 0 && _IOC_DIR(cmd) & _IOC_READ) {
		int nr = _IOC_NR(cmd);
		ret = 0;
		
		switch (nr) {
		case SOUND_MIXER_VOLUME:     val = 0;   break;
		case SOUND_MIXER_CAPS:       val = 0;	break;
		case SOUND_MIXER_STEREODEVS: val = 0;	break;
		case SOUND_MIXER_PCM:        val = 0;   break;
		case SOUND_MIXER_LINE1:      val = 0;   break;
		case SOUND_MIXER_LINE2:      val = 0;   break;
		case SOUND_MIXER_LINE3:      val = 0;   break;
		case SOUND_MIXER_DEVMASK:
			val = SOUND_MASK_VOLUME | SOUND_MIXER_PCM | SOUND_MIXER_LINE1 | SOUND_MIXER_LINE2 | SOUND_MIXER_LINE3 | SOUND_MIXER_BASS | SOUND_MIXER_TREBLE;
			break;
		default:	val = 0;     ret = -EINVAL;	break;
		}
		
		if (ret == 0)
			ret = put_user(val, (int *)arg);
	}
out:
	return ret;
}
#endif

static int pt2314_ctl_vol_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
  ei->value.integer.min = 0;
  ei->value.integer.max = 100;
  return 0;
}

static int pt2314_ctl_bool_info(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei)
{
  ei->count = 1;
  ei->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
  ei->value.integer.min = 0;
  ei->value.integer.max = 1;
  return 0;
}


static int pt2314_ctl_get(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev)
{
  ev->value.integer.value[0] = 0;

  return 0;
}

static struct snd_kcontrol_new pt2314_ctl_template = {
  .iface  = SNDRV_CTL_ELEM_IFACE_MIXER,
  .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
  .get  = pt2314_ctl_get,
};


/* list of mixer controls */
struct pt2314_ctl
{
  const char *name;
  int (*put)(struct snd_kcontrol *kc, struct snd_ctl_elem_value *ev);
  int (*info)(struct snd_kcontrol *kc, struct snd_ctl_elem_info *ei);
  int index;
};


static struct pt2314_ctl ctrls[] = {
  {
    .name   = "Master Playback Volume",
    .put    = pt2314_set_volume,
    .info   = pt2314_ctl_vol_info,
    .index  = 0,
  },
  {
    .name   = "PRIVATE1",
    .put    = pt2314_private1,
    .info   = pt2314_ctl_bool_info,
    .index  = 0,
  },
  {
    .name   = "PRIVATE3",
    .put    = pt2314_set_loudness,
    .info   = pt2314_ctl_vol_info,
    .index  = 0,
  },
  {
    .name   = "PRIVATE4",
    .put    = pt2314_private4,
    .info   = pt2314_ctl_bool_info,
    .index  = 0,
  },
  {
    .name   = "PCM",
    .put    = pt2314_source_req,
    .info   = pt2314_ctl_bool_info,
    .index  = 0,
  },
  {
    .name   = "Aux",
    .put    = pt2314_source_req,
    .info   = pt2314_ctl_bool_info,
    .index  = 1,
  },
  {
    .name   = "Aux",
    .put    = pt2314_source_req,
    .info   = pt2314_ctl_bool_info,
    .index  = 2,
  },
  {
    .name   = "Aux",
    .put    = pt2314_source_req,
    .info   = pt2314_ctl_bool_info,
    .index  = 3,
  },
  {
    .name   = "Tone Control - Bass Volume",
    .put    = pt2314_set_bass,
    .info   = pt2314_ctl_vol_info,
    .index  = 0,
  },
  {
    .name   = "Tone Control - Treble Volume",
    .put    = pt2314_set_treble,
    .info   = pt2314_ctl_vol_info,
    .index  = 0,
  },
};

static int
pt2314_add_ctrl(struct pt2314 *dac, struct pt2314_ctl *ctl)
{
  struct snd_kcontrol *kctl;
  int ret = -ENOMEM;

  printk("%s: %p: ctl %p (%s)\n", __FUNCTION__, dac, ctl, ctl->name);

  kctl = snd_ctl_new1(&pt2314_ctl_template, dac);
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

static int pt2314_add_ctrls(struct pt2314 *dac)
{
  int ret;
  int i;

  for (i = 0; i < ARRAY_SIZE(ctrls); i++) {
    ret = pt2314_add_ctrl(dac, &ctrls[i]);
    if (ret)
      return ret;
  }

  return 0;
}







static struct i2c_driver pt2314_driver;
static struct s3c24xx_iis_ops s3c24xx_pt2314_ops;
static void *pt2314_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg);

static int pt2314_i2c_claim(struct pt2314 *dac)
{
  return i2c_use_client(&dac->clnt);
}

static int pt2314_i2c_release(struct pt2314 *dac)
{
  return i2c_release_client(&dac->clnt);
}

/****************************************************************************
 * This will get called for every I2C device that is found on the bus in the
 * address range specified in normal_i2c_range[]
 ****************************************************************************/
static int pt2314_attach(struct i2c_adapter *adap, int addr, int kind)		
{
	struct pt2314 *dac;
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
	printk("plat_dac = %p pt2314 = %p\n", plat_dac, dac);  

  dac->source_reg = PT2314_AUD_SWITCH_LOUDNESS_OFF | PT2314_AUD_SWITCH_LEVEL_0DB;
  dac->volume.left = 0;
  dac->volume.right = 0;

  i2c_set_clientdata(&dac->clnt, dac);
  dac->clnt.addr  = addr;
  dac->clnt.adapter = adap;
  dac->clnt.driver  = &pt2314_driver;

  dac->claim   = pt2314_i2c_claim;
  dac->release   = pt2314_i2c_release;

  dac->my_dev = platform_device_alloc("s3c24xx-codec", -1);
  strcpy(plat_dac->name, "pt2314");
  plat_dac->dac = dac;
  plat_dac->attach_dac = pt2314_attach_codec;
  plat_dac->ops = &s3c24xx_pt2314_ops;
  dac->my_dev->dev.platform_data  = plat_dac;
  dac->my_dev->dev.parent   = &adap->dev;

  strlcpy(dac->clnt.name, "pt2314", I2C_NAME_SIZE);

  err = i2c_attach_client(&dac->clnt);
  if (err)
    goto exit_err;

  /* registered ok */

  platform_device_register(dac->my_dev);

  return 0;

 exit_err:
  return err;
}

static int pt2314_detach_client(struct i2c_client *clnt)
{
  int err;

  if ((err = i2c_detach_client(clnt))) {
    printk("Client deregistration failed, " "client not detached.\n");
    return err;
  }

  kfree(i2c_get_clientdata(clnt));
  return 0;
}

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x44, I2C_CLIENT_END };
static unsigned short ignore_i2c[] = { I2C_CLIENT_END };
static unsigned short force_i2c[] = { ANY_I2C_BUS, 0x44, I2C_CLIENT_END };
static unsigned short *force_list[] = { force_i2c, NULL };

static struct i2c_client_address_data addr_data = {
	.normal_i2c = normal_i2c,
	.probe = ignore_i2c,
	.ignore = ignore_i2c, 
};

static int pt2314_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, pt2314_attach);
}

static struct i2c_driver pt2314_driver = {
  .driver = {
    name:		PT2314_NAME,
    owner:  THIS_MODULE,
  },
	.attach_adapter =	pt2314_attach_adapter,
	.detach_client =	pt2314_detach_client,
};

static int __init pt2314_init(void)
{
	printk("%s\n", __FUNCTION__);  

  if (force_attach)
  {
    addr_data.forces = force_list;
  }

	return i2c_add_driver(&pt2314_driver);
}

static void __exit pt2314_exit(void)
{
	printk("%s\n", __FUNCTION__);  
	i2c_del_driver(&pt2314_driver);
}

module_init(pt2314_init);
module_exit(pt2314_exit);

MODULE_AUTHOR("Phil Blundell <pb@reciva.com>");
MODULE_DESCRIPTION("PT2314 DAC driver");
MODULE_LICENSE("GPL");

static int pt2314_free(struct snd_device *dev)
{
  struct pt2314 *pt_dev = dev->device_data;

  if (pt_dev->release)
    pt_dev->release(pt_dev);

  kfree(pt_dev);
  return 0;
}

static struct snd_device_ops pt2314_ops = {
  .dev_free = pt2314_free,
};

static void *pt2314_attach_codec(struct snd_card *card,
               struct device *dev,
               struct wm8721_cfg *cfg)
{
  struct s3c24xx_dac *s3c_dev = dev->platform_data;
  struct pt2314 *pt_dev = s3c_dev->dac;
  int ret = 0;

  printk("%s: card=%p, cfg=%p pt2314=%p\n", __FUNCTION__, card, cfg, dev->platform_data);

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

  ret = snd_device_new(card, SNDRV_DEV_LOWLEVEL, pt_dev, &pt2314_ops);
  if (ret)
    goto exit_err;

  /* initialise variables */
  if (!force_attach)
  {
    pt2314_cmd_init (&pt_dev->clnt);
  }


  /* attach the mixer controls */

  ret = pt2314_add_ctrls(pt_dev);
  if (ret)
    goto exit_err;

  /* ok, return our new client */

  return pt_dev;

 exit_err:
  return ERR_PTR(ret);
}

static struct s3c24xx_iis_ops s3c24xx_pt2314_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup,
  .open   = s3c24xx_iis_op_open,
  .close    = s3c24xx_iis_op_close,
  .prepare  = s3c24xx_iis_op_prepare,
};
