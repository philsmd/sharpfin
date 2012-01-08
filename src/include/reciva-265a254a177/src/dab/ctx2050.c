/*
 * Reciva DAB Driver (CTX2050)
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
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
#include <linux/spi/spi.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-irq.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#include "../reciva_util.h"
#include "ctx2050.h"
#include "ctx2050_spi.h"
#include "ctx2050_bbm.h"
#include "fci_tuner.h"

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define MODULE_NAME "CTX2050 driver"
#define PREFIX "CTX2050: "

//#define JUST_READ

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = MODULE_NAME;

static struct workqueue_struct *ctx2050_workqueue;
DECLARE_WORK(ctx2050_work, bbm_interrupt_handler);

static int powered;
static int detect;

#define CTX2050_PDOWN S3C2410_GPA14
#define CTX2050_RESET S3C2410_GPB4
#define RF_PWR_EN     S3C2410_GPB10
#define RST_RF        S3C2410_GPB5

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

static unsigned long int_time;
static unsigned long time_between;
static irqreturn_t ctx2050_int_handler(int irq, void *dev)
{
  time_between = jiffies - int_time;
  int_time = jiffies;
  //printk("%s %d\n", __FUNCTION__);
#ifdef LEVEL_TRIGGER
  s3c2410_gpio_cfgpin(INTBB, S3C2410_GPIO_INPUT);
#endif
  queue_work(ctx2050_workqueue, &ctx2050_work);
  return IRQ_HANDLED;
}

unsigned long get_int_time(void)
{
  return int_time;
}

unsigned long get_last_int_time(void)
{
  return time_between;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static void ctx2050_power_off(void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  if (powered)
  {
    // Put PDOWN and RESET into in-active state
    s3c2410_gpio_setpin(RST_RF, 0);
    s3c2410_gpio_setpin(CTX2050_PDOWN, 1);
    s3c2410_gpio_setpin(CTX2050_RESET, 0);
    s3c2410_gpio_setpin(RF_PWR_EN, 1);

    free_irq(IRQ_EINT3, NULL);

    bbm_reset_fic();

    powered = 0;
  }
  rutl_set_dab_fm_filter(DAB_FM_FILTER_NONE);
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
void ctx2050_power_on(void)
{
  printk(PREFIX "%s\n", __FUNCTION__);

  if (powered)
  {
    printk(PREFIX "Already on - powering off first\n");
    ctx2050_power_off();
    msleep(100);
  }

  rutl_set_dab_fm_filter(DAB_FM_FILTER_DAB);

  // Put PDOWN and RESET into active state
  s3c2410_gpio_setpin(RF_PWR_EN, 0);
  s3c2410_gpio_setpin(CTX2050_PDOWN, 0);
  msleep(1);
  s3c2410_gpio_setpin(CTX2050_RESET, 1);
  msleep(1);
  s3c2410_gpio_setpin(RST_RF, 1);
  msleep(1);

  // Clear any spurious pending interrupts before requesting irq
  __raw_writel ((1 << 3), S3C2412_EINTPEND);
  request_irq(IRQ_EINT3, ctx2050_int_handler, 0, "ctx2050", NULL);

  bbm_init(); // initliase ctx2050
  fci_init(); // initialise tuner

  powered = 1;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
ctx2050_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  //printk(PREFIX "%s %08x\n", __FUNCTION__, cmd);
  switch(cmd)
  {
    case IOC_DAB_POWER_ON:
      ctx2050_power_on();
      break;

    case IOC_DAB_POWER_OFF:
      ctx2050_power_off();
      break;

    case IOC_CTX2050_SPI_TEST:
      spi_test(arg);
      break;

    case IOC_DAB_TUNE:
      return fci_tune(arg);
      break;

    case IOC_DAB_TUNE_LOCK:
      return fci_tune_lock();
      break;

    case IOC_DAB_TUNE_STOP:
      return fci_tune_stop();
      break;

  case IOC_DAB_SET_SUBCH:
      return bbm_set_subchannel(arg);
      break;

  case IOC_DAB_GET_SIGSTRENGTH:
      {
        struct SignalStrength_s sig_strength;
        bbm_get_viterbi_error(&sig_strength.viterbi_period,
                              &sig_strength.viterbi_errors);
        sig_strength.rf_power = fci_rf_power();
        if (copy_to_user((void __user *)arg, (void *)&sig_strength, sizeof(struct SignalStrength_s)))
        {
          return -EFAULT;
        }
      }
      break;
  case IOC_DAB_DUMP_MSC_DATA:
    return bbm_dump_msc_data();
    break;

    default:
      return -ENODEV;
      break;
  }

  return 0;
}

static struct file_operations ctx2050_fops =
{
  owner:    THIS_MODULE,
  ioctl:    ctx2050_ioctl,
};

static struct miscdevice ctx2050_ctrl_dev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_dab_ctrl",
  &ctx2050_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
ctx2050_module_init(void)
{
  if (machine_is_rirm2())
  {
    printk("%s not supported on Barracuda\n", acModuleName);
    return 1;
  }

  printk(PREFIX "%s module: loaded\n", acModuleName);

  /* Set-up RF_PWR_EN, PDOWN, CTX/RF RESET lines */
  s3c2410_gpio_setpin(RST_RF, 0);
  s3c2410_gpio_cfgpin(RST_RF, S3C2410_GPIO_OUTPUT);

  s3c2410_gpio_setpin(RF_PWR_EN, 1);
  s3c2410_gpio_cfgpin(RF_PWR_EN, S3C2410_GPIO_OUTPUT);

  s3c2410_gpio_setpin(CTX2050_PDOWN, 1);
  s3c2410_gpio_cfgpin(CTX2050_PDOWN, S3C2410_GPIO_OUTPUT);

  s3c2410_gpio_setpin(CTX2050_RESET, 0);
  s3c2410_gpio_cfgpin(CTX2050_RESET, S3C2410_GPIO_OUTPUT);

  // Quick device detection
  // INT_BB has a 4k7 pull-up resistor, so if the device (and thus resistor)
  // is not fitted, should be able to pull this down to 0 with the internal
  // GPIO pull down
  s3c2410_gpio_setpin(RF_PWR_EN, 0);
  s3c2410_gpio_pullup(INTBB, 0); // enable pull-down
  s3c2410_gpio_cfgpin(INTBB, S3C2410_GPIO_INPUT);
  msleep(1);
  detect = s3c2410_gpio_getpin(INTBB);
  s3c2410_gpio_setpin(RF_PWR_EN, 1);
  if (!detect)
  {
    printk("%s not detected\n", acModuleName);
    return 1;
  }

  // Interrupt line (active low)
  s3c2410_gpio_pullup(INTBB, 1); // disable pull-down
  s3c2410_gpio_cfgpin(INTBB, S3C2410_GPF3_EINT3);
  unsigned long temp = __raw_readl(S3C2410_EXTINT0 + 0x10);
  temp &= ~(0xf << 12);
#ifdef LEVEL_TRIGGER
  temp |= (0x8 << 12); // low level, with filter
#else
  temp |= (0xa << 12); // falling edge, with filter
#endif
  __raw_writel(temp, S3C2410_EXTINT0 + 0x10);

  /* Register the device */
  misc_register (&ctx2050_ctrl_dev);

  // Create workqueue and give it maximum real-time priority
  ctx2050_workqueue = create_singlethread_workqueue("ctx2050");
  struct sched_param wq_param;
  wq_param.sched_priority = MAX_USER_RT_PRIO - 1; // life's too short to use sys_sched_get_priority_max
  set_workqueue_scheduler(ctx2050_workqueue, SCHED_FIFO, &wq_param);

  // Initialise SPI interfaces
  spi_init();
  bbm_init_buffers();

  /* Switch on RF power */
  s3c2410_gpio_setpin(RF_PWR_EN, 0);

  return 0;                         
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
ctx2050_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);
  if (!detect)
  {
    return;
  }

  /* Switch off device (and stop interrupts) */
  ctx2050_power_off();

  /* Stop workqueue */
  destroy_workqueue(ctx2050_workqueue);

  /* Unregister the device */
  misc_deregister(&ctx2050_ctrl_dev);

  /* Remove SPI drivers */
  bbm_release_buffers();
  spi_exit();
}


module_init(ctx2050_module_init);
module_exit(ctx2050_module_exit);

MODULE_LICENSE("GPL");


