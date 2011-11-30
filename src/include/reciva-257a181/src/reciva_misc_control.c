/*
 * Reciva Misc Control
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * Description:
 * Control GPIO pins via an ioctl
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include "reciva_misc_control.h"
#include "reciva_util.h"
#include "reciva_gpio.h"

/* Deprecated stuff for the old mode of using this kernel module */
static int set_standby_pin(int on);
static int set_ethernet_power(int on);

typedef enum
{
  RMC_STANDBY_NONE = 0,
  RMC_STANDBY_GPB9 = 1,
  RMC_STANDBY_GPG3 = 2,
  RMC_STANDBY_GPH7 = 3,
} standby_pin_t;

static standby_pin_t standby_pin = 0;
MODULE_PARM(standby_pin, "i");

/* Ethernet IC power control */
typedef enum
{
  RMC_ETHERNET_IC_POWER_NONE    = 0,
  RMC_ETHERNET_IC_POWER_nGPG9   = 1,
} ethernet_ic_power_t;
static ethernet_ic_power_t ethernet_ic_power = RMC_ETHERNET_IC_POWER_NONE;
MODULE_PARM(ethernet_ic_power, "i");


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

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* NB x should be an upper case letter and a should be one of CON, DAT, and UP
 */
#define GP(x,a) GPIO_REG(0x10 * ((x) - 'A') + (a))

#define CON 0x00
#define DAT 0x04
#define UP  0x08

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

struct gpio_channel {
  char c;
  int n;
};

/* Our numbering of pins starts at 0 whereas all the app data sheets start at
 * 1.  (unusual formatting to correspond to data sheet).  c of 0
 * corresponds to 'not a gpio pin'.  Just to be even more confusing our
 * numbering of pins in these arrays starts at 0.  Everywhere else in the code
 * our numbering of pins starts at 1.  I hope that's clear. */
/* Notes: 
 * GPG8 also connected to GPF0
 * KBDROW1 sometimes used for ethernet power enable 
 */
static const struct gpio_channel connector1[] =
 {
   {  0,   0 }, /* VDD3v3 */    {  0,   0 }, /* Gnd */
   { 'E', 10 }, /* SDATA3 */    { 'E',  2 }, /* CDCLK */
   { 'E',  9 }, /* SDATA2 */    { 'E',  4 }, /* I2SSDO */
   { 'E',  8 }, /* SDATA1 */    { 'E',  1 }, /* I2SCLK */
   { 'E',  7 }, /* SDATA0 */    { 'E',  0 }, /* I2SLRCLK */
   {  0,   0 }, /* Gnd */       { 'E',  3 }, /* I2SSDI */
   { 'E',  6 }, /* SDCMD */     {  0,   0 }, /* Gnd */
   { 'E',  5 }, /* SDCLK */     { 'B',  0 }, /* L3CLOCK */
   {  0,   0 }, /* VDD3v3 */    { 'B',  7 }, /* L3MODE */
   { 'E', 14 }, /* SCL */       { 'B',  9 }, /* L3DATA */
 };

static const struct gpio_channel connector2[] =
 {
   { 'G',  3 }, /* nSS1 */      {  0,   0 }, /* VSIN */
   { 'G',  5 }, /* SPIMISO1 */  {  0,   0 }, /* VSIN */
   { 'G',  6 }, /* SPIMISI1 */  {  0,   0 }, /* EN1v8 */
   { 'G',  7 }, /* SPICLK1 */   { 'E', 15 }, /* SDA */
   {  0,   0 }, /* Gnd */       { 'F',  1 }, /* SDnWP */
   { 'G', 11 }, /* KBDROW3 */   { 'F',  2 }, /* SDnCD */
   { 'G', 10 }, /* KBDROW2 */   {  0,   0 }, /* VDD3v3 */
   { 'G',  9 }, /* KBDROW1 */   {  0,   0 }, /* Gnd */
   { 'G',  8 }, /* KBDROW0 */   {  0,   0 }, /* USBDP */
   { 'G', 14 }, /* SHAFT0 */    {  0,   0 }, /* USBDN */
   { 'G', 15 }, /* SHAFT1 */    {  0,   0 }, /* Gnd */
   {  0,   0 }, /* Gnd */       {  0,   0 }, /* USBHN */
   { 'F',  4 }, /* GIOAN0 */    {  0,   0 }, /* USBHP */
   { 'F',  5 }, /* GIOAN1 */    {  0,   0 }, /* Gnd */
   { 'F',  6 }, /* GIOAN2 */    { 'H', 10 }, /* nUDEVFSPEED */
   {  0,   0 }, /* Gnd */       { 'H',  9 }, /* nUDEVPOWER */
   { 'H',  7 }, /* nLED_ON */   {  0,   0 }, /* VDD3v3 */
   {  0,   0 }, /* nRESET */    {  0,   0 }, /* VDD1v8 */
   {  0,   0 }, /* nRSTOUT */   {  0,   0 }, /* VDDRST */
   {  0,   0 }, /* PWREN */     {  0,   0 }, /* Gnd */
 };

static const struct gpio_channel connector3[] =
 {
   {  0,   0 }, /* Gnd */       { 'D', 10 }, /* KBDCOL2 */
   { 'C', 15 }, /* LCDD7 */     { 'D',  9 }, /* KBDCOL1 */
   { 'C', 14 }, /* LCDD6 */     { 'D',  8 }, /* KBDCOL0 */
   { 'C', 13 }, /* LCDD5 */     {  0,   0 }, /* Gnd */
   { 'C', 12 }, /* LCDD4 */     { 'C',  5 }, /* LCDCN5 */
   { 'C', 11 }, /* LCDD3 */     { 'C',  4 }, /* LCDCN4 */
   { 'C', 10 }, /* LCDD2 */     { 'C',  3 }, /* LCDCN3 */
   { 'C',  9 }, /* LCDD1 */     { 'C',  2 }, /* LCDCN2 */
   { 'C',  8 }, /* LCDD0 */     { 'C',  1 }, /* LCDCN1 */
   {  0,   0 }, /* Gnd */       { 'C',  0 }, /* LCDCN0 */
 };

static char acModuleName[] = "Reciva Misc Control";

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_misc_control",
  &reciva_fops
};

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_misc_control_init(void)
{
  printk("RMC:%s module\n", acModuleName);

  if (standby_pin || ethernet_ic_power)
  {
    printk("RMC: Deprecated options provided: standby_pin=%d, ethernet_ic_power=%d\n", standby_pin, ethernet_ic_power);
  }

  /* Tell GPIO module which GPIO pins we are using.  XXX This is no longer
   * possible as the module provides arbitrary control to any of the pins now.
   * Sorry. */

  /* Initial settings for pins (in deprecated mode for this module) */
  set_standby_pin(0);
  set_ethernet_power(1);

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);
  
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_misc_control_exit(void)
{
  printk("RMC:%s module: unloaded\n", acModuleName);
  misc_deregister (&reciva_miscdev);
}

/****************************************************************************
 * Get info about the GPIO channel to use, from the connector and pin numbers.
 ****************************************************************************/
static const struct gpio_channel *
get_gpio_channel(int connector, int pin)
{
  printk("%s: %d, %d\n", __FUNCTION__, connector, pin);

  /* 1 based counting still */
  if (pin < 1)
    return 0;
  
#define CONNECTOR(n) \
    case n: \
      if (pin > sizeof(connector ## n) / sizeof((connector ## n)[0])) \
        return 0; \
      else \
        return &(connector ## n)[pin - 1]; 

  switch (connector)
  {
    CONNECTOR(1);
    CONNECTOR(2);
    CONNECTOR(3);
  }

#undef CONNECTOR

  return 0;
}

/****************************************************************************
 * Write bit to the given GPIO register.
 ****************************************************************************/
static int 
gpio_write(const struct gpio_channel *pin, int bit)
{
  int acted;

  printk("%s bit %d to GP%c%d\n", __FUNCTION__, bit, pin->c, pin->n);

  if (!pin->c)
    return -EACCES;

  if (pin->c < 'A' || pin->c > 'H')
    return -EINVAL;

  if (pin->n < 0 || pin->n > 15)
    return -EINVAL;
  
  if (bit != 0 && bit != 1)
    return -EINVAL;

  /* disable pullup */
  rutl_regwrite(1 << pin->n, 0, GP(pin->c, UP));

  /* set as output */
  rutl_regwrite(1 << (2 * pin->n), 3 << (2 * pin->n), GP(pin->c, CON));

  /* send data */
  acted = rutl_regwrite(bit << pin->n, !bit << pin->n, GP(pin->c, DAT));

  return acted ? 0 : -EALREADY;
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
  struct pin_control pc;
  const struct gpio_channel *pin;
  int i;

  switch (cmd)
  {
    case IOC_RMC_STANDBY:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      return set_standby_pin(i);

    case IOC_RMC_ETHERNET_POWER:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      return set_ethernet_power(i);

    case IOC_RMC_PIN_WRITE:
      if (copy_from_user(&pc, (void *)arg, sizeof(pc)))
        return -EFAULT;

      printk("IOC_RMC_PIN_WRITE J%d pin %d, data=%d\n", pc.connector, pc.pin, pc.data);

      pin = get_gpio_channel(pc.connector, pc.pin);
      if (!pin)
        return -EINVAL;

      printk("Using gpio channel GP%c%d\n", pin->c, pin->n);
      return gpio_write(pin, pc.data);

    case IOC_RMC_PIN_READ:
      return -ENOSYS;

    default:
      return -ENODEV;
  }

  return 0;
}

/****************************************************************************
 * Controls hardware STANDBY signal
 ****************************************************************************/
static int 
set_standby_pin(int on)
{
  struct gpio_channel gpb9 = { 'B', 9 };
  struct gpio_channel gpg3 = { 'G', 3 };
  struct gpio_channel gph7 = { 'H', 7 };

  switch (standby_pin)
  {
    case RMC_STANDBY_NONE:
    default:
      /* No STANDBY pin */
      break;
    case RMC_STANDBY_GPB9:
      return gpio_write(&gpb9, on);
    case RMC_STANDBY_GPG3:
      return gpio_write(&gpg3, on);
    case RMC_STANDBY_GPH7:
      return gpio_write(&gph7, on);
  }

  return 0;
}

/****************************************************************************
 * Controls ETHERNET_POWER signal
 ****************************************************************************/
static int 
set_ethernet_power(int on)
{
  struct gpio_channel gpg9 = { 'G', 9 };

  switch (ethernet_ic_power)
  {
    case RMC_ETHERNET_IC_POWER_NONE:
      break;
    case RMC_ETHERNET_IC_POWER_nGPG9: // active low
      return gpio_write(&gpg9, !on);
  }

  return 0;
}

module_init(reciva_misc_control_init);
module_exit(reciva_misc_control_exit);

MODULE_LICENSE("GPL");

