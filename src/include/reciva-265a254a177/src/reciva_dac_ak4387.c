/*
 * linux/reciva/reciva_dac_ak4387.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2007 Reciva Ltd. All Rights Reserved
 * 
 * Simplistic initialisation of the AK4387 DAC
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/version.h>

#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static int reciva_ioctl(struct inode *inode, struct file *file,
                        unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define AK4387_SPI  S3C2410_GPGDAT
#define AK4387_CSN  (1 << 3)
#define AK4387_CCLK (1 << 5)
#define AK4387_CDTI (1 << 7)

#define AK4387_RSTN_PORT S3C2410_GPBDAT
#define AK4387_RSTN (1 << 7)

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva DAC AK4387";

/* IOCTL related */
#define RECIVA_LINE_OUT_IOCTL_BASE  'J'
#define IOC_RLO_POWER          _IOW(RECIVA_LINE_OUT_IOCTL_BASE, 0, int)

static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_line_out",
  &reciva_fops
};

static int dac_on;


typedef struct 
{
  unsigned int csn;
  unsigned int cclk;
  unsigned int cdti;
  unsigned int rstn;

} pin_info_t;

static pin_info_t pin_info;

// 0:
//   CSN = GPG3 (J2-1)
//   CCLK = GPG5 (J2-3)
//   CDTI = GPG7 (J2-7)
//   RSTN = GPB7 (J1-18)
// 1:
//   CSN = GPF1 (J2-10)
//   CCLK = GPG7 (J2-7)
//   CDTI = GPF2 (J2-12)
//   RSTN = GPB0 (J1-16)
static int board_type;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(board_type, int, S_IRUGO);
#else
MODULE_PARM(board_type, "i");
#endif

// Turn DAC on when module is loaded rather than via ioctl
static int default_to_on;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(default_to_on, int, S_IRUGO);
#else
MODULE_PARM(default_to_on, "i");
#endif

// Delay after each set pin (us)
static int set_pin_delay_us;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
module_param(set_pin_delay_us, int, S_IRUGO);
#else
MODULE_PARM(set_pin_delay_us, "i");
#endif


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Set pin with delay
 ****************************************************************************/
static void vSetPinWithDelay(int pin, int data)
{
  s3c2410_gpio_setpin (pin, data);
  udelay(set_pin_delay_us);
}

/****************************************************************************
 * Reset DAC
 ****************************************************************************/
static void ak4387_off(void)
{
  printk("AK4387: %s\n", __FUNCTION__);

  if (dac_on)
  {
    vSetPinWithDelay (pin_info.rstn, 0);
    // Wait for reset to complete (>150ns)
    udelay(1);
    dac_on = 0;
  }
}

/****************************************************************************
 * Power on DAC and write audio format for register 00
 * (all the other register default's should be OK)
 *
 * Note: device is reset every time RSTN goes low, so we have to set the
 *       format every time we switch it on.
 ****************************************************************************/
static void ak4387_on(void)
{
  printk("AK4387: %s\n", __FUNCTION__);

  if (dac_on)
  {
    return;
  }

  dac_on = 1;

  // According to the spec, MCLK, BICK and LRCK must be present when
  // the chip is out of reset. For now, I'll enable MCLK at least.
  // (Register writing is inhibited if MCLK is not present.)
  s3c2410_gpio_cfgpin(S3C2410_GPE2, S3C2410_GPE2_CDCLK);
  s3c2410_gpio_pullup(S3C2410_GPE2, 1); // disable pull-up

  vSetPinWithDelay (pin_info.rstn, 1);
  udelay(1);
  
  // Select chip, prepare clock
  vSetPinWithDelay (pin_info.csn, 0);
  vSetPinWithDelay (pin_info.cclk, 0);
  udelay(1);

  // Register 00: MCLK frequency "Manual Setting Mode"
  //              Audio data Mode 0 - 16-bit LSB justified
  unsigned int uiData = 0x6003;
  int i;
  int level;
  for (i=0; i<16; i++)
  {
    // Set-up bit
    if (uiData & 0x8000)
      level = 1;
    else
      level = 0;;
    vSetPinWithDelay (pin_info.cdti, level);
    udelay(1);

    // Clock data
    vSetPinWithDelay (pin_info.cclk, 1);
    udelay(1);
    vSetPinWithDelay (pin_info.cclk, 0);
    udelay(1);
    
    uiData <<= 1;
  }
  
  // Unselect chip
  vSetPinWithDelay (pin_info.csn, 1);
}

/****************************************************************************
 * Command handler
 * Parameters : Standard ioctl params
 * Returns : 0 == success, otherwise error code
 ****************************************************************************/
static int
reciva_ioctl(struct inode *inode, struct file *file,
             unsigned int cmd, unsigned long arg)
{
  int i;

  switch (cmd)
  {
    case IOC_RLO_POWER:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      if (i)
      {
        ak4387_on();
      }
      else
      {
        ak4387_off();
      }
      return 0;

    default:
      return -ENODEV;
  }

  return 0;
}
  
   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 *
 * Set-up IO lines. Leave DAC off.
 ****************************************************************************/
static int __init 
ak4387_init(void)
{
  printk("%s module: loaded\n", acModuleName);
  printk("AK4387: board_type=%d\n", board_type);
  printk("AK4387: default_to_on=%d\n", default_to_on);
  printk("AK4387: set_pin_delay_us=%d\n", set_pin_delay_us);

  /* Work out which GPIO pins we're using for the control lines */
  switch (board_type)
  {
    case 0:
    default:
      pin_info.csn = S3C2410_GPG3;
      pin_info.cclk = S3C2410_GPG5;
      pin_info.cdti = S3C2410_GPG7;
      pin_info.rstn = S3C2410_GPB7;
      break;

    case 1:
      pin_info.csn = S3C2410_GPF1;

      if ( machine_is_rirm2() )
        pin_info.cclk = S3C2410_GPG7;
      else
        pin_info.cclk = S3C2410_GPG2;

      pin_info.cdti = S3C2410_GPF2;
      pin_info.rstn = S3C2410_GPB0;
      break;
  }

  /* Set up the gpio pins */

  /* Set up data */
  vSetPinWithDelay (pin_info.csn, 1);
  vSetPinWithDelay (pin_info.cclk, 1);
  vSetPinWithDelay (pin_info.cdti, 1);
  vSetPinWithDelay (pin_info.rstn, 0);

  /* Disable pullups */
  s3c2410_gpio_pullup (pin_info.csn, 1);
  s3c2410_gpio_pullup (pin_info.cclk, 1);
  s3c2410_gpio_pullup (pin_info.cdti, 1);
  s3c2410_gpio_pullup (pin_info.rstn, 1);

  /* Set as ouput */
  s3c2410_gpio_cfgpin (pin_info.csn, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_cfgpin (pin_info.cclk, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_cfgpin (pin_info.cdti, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_cfgpin (pin_info.rstn, S3C2410_GPIO_OUTPUT);

  // Wait for reset to complete (>150ns)
  udelay(1);

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);

  /* Turn it on and init registers */
  if (default_to_on)
    ak4387_on();

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
ak4387_exit(void)
{
  // Power down chip
  ak4387_off();
  printk("%s module: unloaded\n", acModuleName);
}


module_init(ak4387_init);
module_exit(ak4387_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva DAC AK4387 driver");
MODULE_LICENSE("GPL");


