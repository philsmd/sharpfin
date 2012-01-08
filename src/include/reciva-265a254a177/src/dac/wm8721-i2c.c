/* sound/arm/wm8721-i2c.c
 *
 * WM8721 Audio codec driver - I2C interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include "wm8721-hw.h"
#include "wm8721.h"

#include <asm/arch/audio.h>

#include <asm/arch/s3c24xx-dac.h>

#define PREFIX "wm8721-i2c:"

// Force detection of device. 
// Note you might also need to set forced_i2c_address
static int force_attach;
module_param(force_attach, int, S_IRUGO);

// Forced i2c address
// wm8711 - 0x1a (CSB=0) OR 0x1b (CSB=1)
static int forced_i2c_address = 0x1a;
module_param(forced_i2c_address, int, S_IRUGO);


static struct s3c24xx_iis_ops s3c24xx_wm8721_ops = {
  .owner    = THIS_MODULE,
  .startup  = s3c24xx_iis_op_startup,
  .open   = s3c24xx_iis_op_open,
  .close    = s3c24xx_iis_op_close,
  .prepare  = wm8721_prepare,
};

static unsigned short normal_i2c[] = { 0x1A,  I2C_CLIENT_END };
static unsigned short ignore_i2c[] = { I2C_CLIENT_END };
static unsigned short force_i2c[] = { ANY_I2C_BUS, 0x00, I2C_CLIENT_END };
static unsigned short *force_list[] = { force_i2c, NULL };

static struct i2c_client_address_data addr_data = {
 .normal_i2c = normal_i2c,
 .probe          = ignore_i2c,
 .ignore   = ignore_i2c,
};

struct wm8721_data {
	struct i2c_client	client;
	struct platform_device	*my_dev;
	struct wm8721_hw	hw;
};

static struct i2c_driver wm8721_driver;

static inline struct i2c_client *hw_to_client(struct wm8721_hw *wm_hw)
{
	struct wm8721_data *dp;

	dp = container_of(wm_hw, struct wm8721_data, hw);
	return &dp->client;
}

static int wm8721_i2c_claim(struct wm8721_hw *wm_hw)
{
	struct i2c_client *client = hw_to_client(wm_hw);
	return i2c_use_client(client);
}

static int wm8721_i2c_release(struct wm8721_hw *wm_hw)
{
	struct i2c_client *client = hw_to_client(wm_hw);
	return i2c_release_client(client);
}

static int wm8721_wr_i2c(struct wm8721_hw *wm_hw, int reg, int val)
{
	struct i2c_client *client = hw_to_client(wm_hw);
	unsigned char data[2];
	struct i2c_msg msg[1];
	int done;

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = data;

	data[0] = reg << 1;
	data[1] = val;

	if (val & (1<<8))
		data[0] |= 1;

	done = i2c_transfer(client->adapter, msg, 1);
	
	printk(KERN_DEBUG "%s: (%02x,%02x) wr %02x,%02x ret %d\n", 
	       __FUNCTION__, reg, val, data[0], data[1], done);

	return done == 1 ? 0 : -EIO;
}


static int wm8721_detect(struct i2c_adapter *adap, int addr, int kind)
{
	struct wm8721_data *wm_data;
  struct s3c24xx_dac *plat_dac;
	int err;

	wm_data = kzalloc(sizeof(*wm_data), GFP_KERNEL);
	if (wm_data == NULL) {
		err = -ENOMEM;
		goto exit_err;
	}
  plat_dac = kzalloc(sizeof(*plat_dac), GFP_KERNEL);
  if (plat_dac == NULL) {
    err = -ENOMEM;
    goto exit_err;
  }
	
	i2c_set_clientdata(&wm_data->client, wm_data);
	wm_data->client.addr	= addr;
	wm_data->client.adapter	= adap;
	wm_data->client.driver	= &wm8721_driver;
	// wm_data->client.id		= wm8721_id++;

	wm_data->hw.dev		= &adap->dev;
	wm_data->hw.owner		= THIS_MODULE;
	wm_data->hw.wr		= wm8721_wr_i2c;
	wm_data->hw.claim		= wm8721_i2c_claim;
	wm_data->hw.release		= wm8721_i2c_release;

	wm_data->my_dev = platform_device_alloc("s3c24xx-codec", -1);
  strcpy(plat_dac->name, "pt2314");
  plat_dac->dac = &wm_data->hw;
  plat_dac->attach_dac = wm8721_attach;
  plat_dac->ops = &s3c24xx_wm8721_ops;
	wm_data->my_dev->dev.platform_data	= plat_dac;
	wm_data->my_dev->dev.parent		= &adap->dev;

	strlcpy(wm_data->client.name, "wm8721", I2C_NAME_SIZE);
	
	err = i2c_attach_client(&wm_data->client);
	if (err)
		goto exit_err;

	/* registered ok */

	platform_device_register(wm_data->my_dev);

	return 0;
	
 exit_err:
	return err;
}

static int wm8721_attach_adapter(struct i2c_adapter *adapter)
{
	int s;
	printk(PREFIX "attach_adapter ENTRY\n");
	s = i2c_probe(adapter, &addr_data, wm8721_detect);
	printk(PREFIX "attach_adapter EXIT s=%d\n", s);
	return s;
}

static int wm8721_detach_client(struct i2c_client *client)
{
	int err;

	if ((err = i2c_detach_client(client))) {
		//dev_err(&client->dev, "Client deregistration failed, " "client not detached.\n");
		printk("Client deregistration failed, " "client not detached.\n");
		return err;
	}

	kfree(i2c_get_clientdata(client));
	return 0;
}


static struct i2c_driver wm8721_driver = {
	.driver = {
    .name		= "wm8721",
    .owner		= THIS_MODULE,
  },
//	.flags		= I2C_DF_NOTIFY,
	.attach_adapter	= wm8721_attach_adapter,
	.detach_client	= wm8721_detach_client,
};


static int __init wm8721_init(void)
{
	int s;
	printk(PREFIX "init ENTRY\n");
	printk(PREFIX "  force_attach = %d\n", force_attach);
	printk(PREFIX "  forced_i2c_address = 0x%02x\n", forced_i2c_address);

	if (force_attach)
	{
		force_i2c[1] = forced_i2c_address;
		addr_data.forces = force_list;
	}

	s = i2c_add_driver(&wm8721_driver);
	printk(PREFIX "init EXIT s=%d\n", s);
	return s;
}

static void __exit wm8721_exit(void)
{
	i2c_del_driver(&wm8721_driver);
}

module_init(wm8721_init);
module_exit(wm8721_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WM8721 I2C Audio driver");
MODULE_AUTHOR("Jonathan Miles");
