/*
 * linux/reciva/reciva_gpio_expander_pc19539.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Reciva Ltd. All Rights Reserved
 * 
 * GPIO expander driver - Texas Instruments PCA9539
 *
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
#include <linux/serial.h>
#include <linux/version.h>


#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

#include "reciva_gpio_expander_generic.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int pin_write(int pin, int data);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "PCA9539:"
#define MODULE_NAME "Reciva GPIO Expander PCA9539"

// Register addresses
#define PCA9539_REG_INPUT_PORT0       0x00
#define PCA9539_REG_INPUT_PORT1       0x01
#define PCA9539_REG_OUTPUT_PORT0      0x02
#define PCA9539_REG_OUTPUT_PORT1      0x03
#define PCA9539_REG_OUTPUT_POLARITY0  0x04
#define PCA9539_REG_OUTPUT_POLARITY1  0x05
#define PCA9539_REG_OUTPUT_CONFIG0    0x06
#define PCA9539_REG_OUTPUT_CONFIG1    0x07


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = MODULE_NAME;
static reciva_rge_driver_t driver =
{
  name:         acModuleName,
  pin_write:    pin_write,
  pin_read:     NULL,
};

/* Current output levels */
static int output_levels = 0;

/* Pins to be set as outputs */
static int output_bitmask = 0;


   /*************************************************************************/
   /***                      I2C Driver - START                           ***/
   /*************************************************************************/

static struct i2c_driver reciva_i2c_driver;
static int i2c_device_found = 0;
static int i2c_address = 116;
static struct i2c_client *reciva_i2c_client = NULL;

static struct i2c_client client_template = {
  name: "(unset)",
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  flags:  I2C_CLIENT_ALLOW_USE,
#endif
  driver: &reciva_i2c_driver
};


/****************************************************************************
 * This will get called for every I2C device that is found on the bus in the
 * address range specified in normal_i2c_range[]
 ****************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int reciva_i2c_attach(struct i2c_adapter *adap, int addr, int kind)
#else
static int reciva_i2c_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)   
#endif
{
  struct i2c_client *clnt;
  printk(PREFIX "I2C device found (address=0x%04x)\n", addr);  

  /* Just use the first device we find. 
   * There should only be 1 device connected, but just in case.. */
  if (i2c_device_found)
  {
    printk(PREFIX "Not using this device\n");  
    return 0;
  }    
  else
  {
    i2c_device_found = 1;    
    printk(PREFIX "Using this device\n");  
  } 
  
  clnt = kmalloc(sizeof(*clnt), GFP_KERNEL);
  memcpy(clnt, &client_template, sizeof(*clnt));
  clnt->adapter = adap;
  clnt->addr = addr;
  strcpy(clnt->name, MODULE_NAME);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  i2c_set_clientdata (clnt, NULL);
#else
  clnt->data = NULL;
#endif


  reciva_i2c_client = clnt;
  i2c_attach_client(clnt);

  return 0;
}

static int reciva_i2c_detach_client(struct i2c_client *clnt)
{
  printk(PREFIX "reciva_i2c_detach_client\n");
  i2c_detach_client(clnt);
  kfree(clnt);

  return 0;
}

/* Addresses to scan */
#define RECIVA_I2C_ADDR_NORMAL      117
#define RECIVA_I2C_ADDR_RANGE_END   117  

static unsigned short normal_i2c[] = { RECIVA_I2C_ADDR_NORMAL, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = {I2C_CLIENT_END, I2C_CLIENT_END};   

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static struct i2c_client_address_data addr_data = {
  .normal_i2c = normal_i2c,
  .probe = normal_i2c_range,
  .ignore = normal_i2c_range,
};
#else
static unsigned short probe[] =       { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[] = {I2C_CLIENT_END,  I2C_CLIENT_END};   
static unsigned short ignore[]       = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[] =        { I2C_CLIENT_END, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
  normal_i2c, normal_i2c_range, 
  probe, probe_range, 
  ignore, ignore_range, 
  force
};
#endif

static int reciva_i2c_attach_adapter(struct i2c_adapter *adap)
{
  printk(PREFIX "reciva_i2c_attach_adapter\n");
  return i2c_probe(adap, &addr_data, reciva_i2c_attach);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static void reciva_i2c_inc_use(struct i2c_client *clnt)
{
  printk(PREFIX "reciva_i2c_inc_use\n");
  MOD_INC_USE_COUNT;
}

static void reciva_i2c_dec_use(struct i2c_client *clnt)
{
  printk(PREFIX "reciva_i2c_dec_use\n");
  MOD_DEC_USE_COUNT;
}
#endif

static struct i2c_driver reciva_i2c_driver = {
  attach_adapter: reciva_i2c_attach_adapter,
  detach_client:  reciva_i2c_detach_client,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	id:   I2C_DRIVERID_SI4700,
	driver: {
		name:	MODULE_NAME,
		owner:	THIS_MODULE
	},
#else
	id:   I2C_DRIVERID_WM8721,
	name:		MODULE_NAME,
	flags:		I2C_DF_NOTIFY,
	inc_use:	reciva_i2c_inc_use,
	dec_use:	reciva_i2c_dec_use
#endif

};

   /*************************************************************************/
   /***                      I2C Driver - END                             ***/
   /*************************************************************************/

/****************************************************************************
 * Read from device
 * data - read data gets dumped here
 * count - number of bytes to read
 ****************************************************************************/
static int
reciva_chip_read (struct i2c_client *clnt, unsigned char *data, int count)
{
  if (clnt == NULL)
    return -1;

  int r = i2c_master_recv (clnt, (unsigned char *)data, count);
  if (r != count) 
  {
    printk(PREFIX "read failed, status %d\n", r);
    return r;
  }

  return 0;
}

/****************************************************************************
 * Write to device
 * data - data to write
 * count - number of bytes to write
 ****************************************************************************/
static int
reciva_chip_write (struct i2c_client *clnt, const char *data, int count)
{
  if (clnt == NULL)
    return -1;

  int r = i2c_master_send (clnt, (unsigned char *)data, count);
  if (r != count) 
  {
    printk(PREFIX "write failed, status %d\n", r);
    return r;
  }

  return 0;
}

/****************************************************************************
 * Dump contents of all registers
 ****************************************************************************/
static int dump_registers(void)
{
  unsigned char temp[8];
  int r = reciva_chip_read (reciva_i2c_client, temp, 8);
  if (r == 0)
  {
    int i;
    printk(PREFIX "Register Dump\n");
    for (i=0; i<8; i++)
      printk("%02x ", temp[i]);
    printk("\n");
  }
  else
  {
    printk(PREFIX "ERROR - failed to read from device\n");
  }

  return r;
}

/****************************************************************************
 * Write data to specified pin
 * Set pin to -1 to force a rewrite of current levels
 ****************************************************************************/
static int pin_write(int pin, int data)
{
  //printk("%s pin=%d data=%d\n", __FUNCTION__, pin, data);
  if (pin <= 15)
  {  
    if (data) 
      output_levels |= (1 << pin);
    else
      output_levels &= ~(1 << pin);
  }
  //printk("  output_levels = %08x\n", output_levels);

  /* Set up address of register we're going to read from */
  char temp[3];
  temp[0] = PCA9539_REG_OUTPUT_PORT0;
  temp[1] = output_levels & 0xff;
  temp[2] = (output_levels >> 8) & 0xff;
  int r = reciva_chip_write (reciva_i2c_client, temp, 3);

  return r;
}

/****************************************************************************
 * Set up pin directions and output levels
 ****************************************************************************/
static void setup_gpio(void)
{
  /* Set levels of output pins */
  pin_write(16, 0); // 16 = force output levels to be written

  /* Configure as outputs */
  char temp[3];
  temp[0] = PCA9539_REG_OUTPUT_CONFIG0;
  temp[1] = (~output_bitmask) & 0x0f;
  temp[2] = (~output_bitmask >> 8) & 0x0f;
  reciva_chip_write (reciva_i2c_client, temp, 3);

  /* Read back register contents */
  dump_registers();
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  printk(PREFIX "  i2c_address=%d\n", i2c_address);
  printk(PREFIX "  output_bitmask=%08x\n", output_bitmask);
  printk(PREFIX "  output_levels=%08x\n", output_levels);

  /* Set up I2C address and add driver */
  normal_i2c[0] = i2c_address;
  normal_i2c[1] = i2c_address;
  int r = i2c_add_driver(&reciva_i2c_driver);

  /* Setup GPIO directions and levels */
  setup_gpio();

  /* Register this driver with generic module */
  reciva_rge_register_driver(&driver);

  return r;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_exit(void)
{
  printk("%s module: unloaded\n", acModuleName);
  i2c_del_driver(&reciva_i2c_driver);
}


module_init(reciva_init);
module_exit(reciva_exit);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param (i2c_address, int, S_IRUGO);
module_param (output_bitmask, int, S_IRUGO);
module_param (output_levels, int, S_IRUGO);
#else
MODULE_PARM(i2c_address, "i");
MODULE_PARM(output_bitmask, "i");
MODULE_PARM(output_levels, "i");
#endif

MODULE_DESCRIPTION("Reciva GPIO Expander PCA9539");
MODULE_LICENSE("GPL");


