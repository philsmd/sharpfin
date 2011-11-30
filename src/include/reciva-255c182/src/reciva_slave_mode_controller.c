/*
 * Slave Mode Controller
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
 * GPIO pins claimed by this module:
 * GPC8 : nMRDY. Module Ready (active low)
 * GPC9 : nINTERRUPT. Attention Request (active low)
 * 
 * This module receives Slave Mode messages from the slave mode driver
 * and passes them on to the application. It is only capable of storing
 * one message at a time. 
 * 
 * The application sends messages and these get passed on to the 
 * slave mode driver.
 *
 * The slave mode master must wait for a command to be acknowledged before 
 * attempting to send another comnmand.
 *
 * The following message sequence charts show how it works.
 * MASTER = master controller 
 * SMCD = slave mode driver
 * SMC = slave mode controller (this module)
 * APP = application
 *
 * Initialisation
 * --------------
 *
 * MASTER                     SMCD                                             SMC                                         APP
 *   |                          |                                               |                                           | 
 *   |  nMRDY=1 nINTERRUPT=1    |                                               |                                           | 
 *   |<-------------------------------------------------------------------------|                                           | 
 *   |                          |             smc_register_driver()             |                                           |
 *   |                          |---------------------------------------------->|                                           |
 *   |                          |                                               |                                           |
 *   |                          |             get_name()                        |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |                          |                                               |                                           |
 *   |                          |             get_driver_id()                   |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |                          |                                               |                                           |
 *   |                          |                                               |            IOC_GetDriverID()              | 
 *   |                          |                                               |<------------------------------------------|
 *   |                          |                                               |                                           |
 *   |                          |                                               |            IOC_Reset()                    |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |             reset()                           |                                           | 
 *   |                          |<----------------------------------------------|                                           | 
 *   |  nMRDY=0 nINTERRUPT=1    |                                               |                                           | 
 *   |<-------------------------------------------------------------------------|                                           | 
 *   |                          |                                               |                                           | 
 *   |                          |                                               |                                           | 
 *
 *
 * Master to Slave Write Commands
 * ------------------------------
 *
 * MASTER                     SMCD                                             SMC                                         APP
 *   |                          |                                               |                                           | 
 *   | write(eg via I2C)        |                                               |           select()                        | 
 *   |------------------------->|                                               |<------------------------------------------| 
 *   |                          |             smc_command()                     |                                           |
 *   |                          |---------------------------------------------->|                                           |
 *   |                          |                                               |           read()                          |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |                                               |                                           |
 *   |                          |                                               |           write()                         |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |             write()                           |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |  command_response(ack)   |                                               |                                           |
 *   |<-------------------------|                                               |                                           | 
 *   |                          |                                               |                                           | 
 *
 *
 *
 * Display Update using nINTERRUPT (eg for I2C driver)
 * --------------------------------------------
 *
 * MASTER                     SMCD                                             SMC                                         APP
 *   |                          |                                               |                                           | 
 *   |                          |                                               |           IOC_AttentionReq()              | 
 *   |                          |                                               |<------------------------------------------| 
 *   |                          |         nINTERRUPT=0                          |                                           | 
 *   |<-------------------------------------------------------------------------|                                           | 
 *   |                          |                                               |           select()                        | 
 *   | write(eg via I2C)        |                                               |<------------------------------------------| 
 *   |------------------------->|                                               |                                           | 
 *   |                          |             smc_command()                     |                                           |
 *   |                          |---------------------------------------------->|                                           |
 *   |                          |                                               |           read()                          |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |                                               |                                           | 
 *   |                          |                                               |           write()                         |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |             write()                           |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |  command_response        |                                               |                                           |
 *   |<-------------------------|                                               |                                           | 
 *   |       etc.               |         //Master sends multiple read          |                                           | 
 *   |------------------------->|         //commands to read whole screen       |                                           | 
 *   |                          |---------------------------------------------->|                                           | 
 *   |                          |<----------------------------------------------|                                           | 
 *   |<-------------------------|                                               |                                           | 
 *   |                          |                                               |                                           | 
 *   |    read_completed()      |                                               |                                           | 
 *   |------------------------->|                                               |                                           | 
 *   |                          |---------------------------------------------->|              ReadCompleted()              | 
 *   |                          |                                               |------------------------------------------>| 
 *   |                          |                                               |                                           | 
 *   |                          |                                               |           IOC_AttentionAck()              | 
 *   |                          |                                               |<------------------------------------------| 
 *   |                          |         nINTERRUPT=1                          |                                           | 
 *   |<-------------------------------------------------------------------------|                                           | 
 *   |                          |                                               |                                           | 
 *
 *
 * Display Update not using nINTERRUPT (eg for Serial driver)
 * ----------------------------------------------------------
 * APP uses IOC_GET_DRIVER_ID to determine that it doesn't need to be asked for display updates.
 * It just sends display related info straight out to the MASTER.
 *
 * MASTER                     SMCD                                             SMC                                         APP
 *   |                          |                                               |                                           | 
 *   |                          |                                               |           write()                         |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |             write()                           |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |  tx_data                 |                                               |                                           |
 *   |<-------------------------|                                               |                                           | 
 *   |                          |                                               |           write()                         |
 *   |                          |                                               |<------------------------------------------|
 *   |                          |             write()                           |                                           |
 *   |                          |<----------------------------------------------|                                           |
 *   |  tx_data                 |                                               |                                           |
 *   |<-------------------------|                                               |                                           | 
 *   |                          |                                               |                                           | 
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

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/io.h>

#include "reciva_slave_mode_controller.h"
#include "reciva_util.h"
#include "reciva_gpio.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef enum
{
  PIN_LEVEL_LOW,
  PIN_LEVEL_HIGH,
} pin_level_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void set_level_nmrdy(pin_level_t level);
static void set_level_ninterrupt(pin_level_t level);
static void reset_module(void);
static void attention_req(void);
static void attention_ack(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RSMC:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = "Reciva Slave Mode Controller";
static wait_queue_head_t wait_queue;

/* Slave Mode driver (I2C, serial etc) */
static const reciva_smc_driver_t *driver = NULL;

/* Last message received from driver */
static char *message = NULL;  
static int message_length = 0;



   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Driver registration
 * d : driver
 ****************************************************************************/
int reciva_smc_register_driver(const reciva_smc_driver_t *d)
{
  printk(PREFIX "register_driver\n");
  driver = d;
  return 0;
}

/****************************************************************************
 * Commands from driver arrive here
 * data : null terminated command
 * length : data length
 * Return : 0 on success
 ****************************************************************************/
void reciva_smc_command(const char *data, int length)
{
  if (message)  
    kfree(message);
  message = NULL;

  /* Make a copy of the message */
  if (length > 0)
  {
    message = kmalloc(sizeof(data[0]) * length, GFP_KERNEL);
    message_length = length;
    memcpy(message, data, length);
  }

  /* This makes poll work */
  wake_up_interruptible(&wait_queue);
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/


   /*************************************************************************/
   /***                        File Operations - START                    ***/
   /*************************************************************************/

/****************************************************************************
 * Returns status of device. Indicates if there is data available to read
 ****************************************************************************/
static unsigned int 
smc_poll (struct file *file, poll_table *wait)
{
  poll_wait(file, &wait_queue, wait);
  if (message)
  {
    /* Message available to read */
    return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
  }
  else 
  {
    /* Device is writable */
    return POLLOUT | POLLWRNORM;
  }
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
smc_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  int ret;
  int length = count;

  if (message == NULL)
    return -EFAULT;

  /* Copy data to user application
   * Ignore the offset */
  if (message_length < count)
    length = message_length;

  if (copy_to_user (buffer, message, length))
    ret = -EFAULT;
  else
    ret = length;

  if (message)
    kfree(message);    
  message = NULL;

  return ret;
}

/****************************************************************************
 * Write data to device
 ****************************************************************************/
static int 
smc_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  char temp_buf[count+1];
  copy_from_user(temp_buf, buf, count);
  temp_buf[count] = 0;

  /* Send it on to driver */
  if (driver && driver->smc_write)
    driver->smc_write(temp_buf, count);

  return count;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
smc_open(struct inode * inode, struct file * file)
{
  printk(PREFIX "open\n");
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
smc_release(struct inode * inode, struct file * file)
{
  printk(PREFIX "release\n");
  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
smc_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  smc_driver_id_t driver_id = SMC_DRIVER_UNKNOWN;
  int tx_empty = 0;

  switch(cmd)
  {
    case IOC_SMC_RESET:
      reset_module();
      break;

    case IOC_SMC_ATTENTION_REQ:
      attention_req();
      break;

    case IOC_SMC_ATTENTION_ACK:
      attention_ack();
      break;

    case IOC_SMC_GET_DRIVER_ID:
      if (driver && driver->smc_get_driver_id)
        driver_id = driver->smc_get_driver_id();
      
      if (put_user (driver_id, (int *)arg))
        return -EFAULT;
      break;

    case IOC_SMC_GET_TX_BUFFER_EMPTY:
      if (driver && driver->smc_get_tx_buffer_empty)
        tx_empty = driver->smc_get_tx_buffer_empty();
      
      if (put_user (tx_empty, (int *)arg))
        return -EFAULT;
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

static struct file_operations smc_fops =
{
  owner:    THIS_MODULE,
  ioctl:    smc_ioctl,
  read:     smc_read,
  write:    smc_write,
  poll:     smc_poll,  
  open:     smc_open,
  release:  smc_release,
};

static struct miscdevice smc_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_smc",
  &smc_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/


/****************************************************************************
 * Sets level of nMRDY pin (GPC8)
 ****************************************************************************/
static void set_level_nmrdy(pin_level_t level)
{
  if (level == PIN_LEVEL_LOW)
    rutl_regwrite((0 << 8), (1 << 8), GPCDAT);    // Clear
  else
    rutl_regwrite((1 << 8), (0 << 8), GPCDAT);    // Set
}

/****************************************************************************
 * Sets level of nINTERRUPT pin (GPC9)
 ****************************************************************************/
static void set_level_ninterrupt(pin_level_t level)
{
  if (level == PIN_LEVEL_LOW)
    rutl_regwrite((0 << 9), (1 << 9), GPCDAT);    // Clear
  else
    rutl_regwrite((1 << 9), (0 << 9), GPCDAT);    // Set
}

/****************************************************************************
 * Reset the module
 ****************************************************************************/
static void reset_module(void)
{
  if (driver)
    driver->smc_reset();

  set_level_ninterrupt(PIN_LEVEL_HIGH);
  set_level_nmrdy(PIN_LEVEL_LOW);  /* Module is now ready to accept commands */

  if (message)
    kfree(message);    
  message = NULL;
}

/****************************************************************************
 * Request attention from the master
 ****************************************************************************/
static void attention_req(void)
{
  set_level_ninterrupt(PIN_LEVEL_LOW);
}

/****************************************************************************
 * Cancel attention request
 ****************************************************************************/
static void attention_ack(void)
{
  set_level_ninterrupt(PIN_LEVEL_HIGH);
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);

  init_waitqueue_head (&wait_queue);

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPC8 (nMRDY)", acModuleName);
  rgpio_register("GPC9 (nINTERRUPT)", acModuleName);

  /* nMRDY (GPC8). Active low. */
  rutl_regwrite((1 << 8), (0 << 8), GPCDAT);    // Set high
  rutl_regwrite((1 << 8), (0 << 8), GPCUP);     // Disable pullup
  rutl_regwrite((1 << 16), (3 << 16), GPCCON);  // Set as ouput

  /* nINTERRUPT (GPC9). Active low. */
  rutl_regwrite((1 << 9), (0 << 9), GPCDAT);   // Set high
  rutl_regwrite((1 << 9), (0 << 9), GPCUP);    // Disable pullup
  rutl_regwrite((1 << 18), (3 << 18), GPCCON); // Set as ouput

  /* Register the device */
  misc_register (&smc_miscdev);

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  /* Unregister the device */
  misc_deregister(&smc_miscdev);
}


EXPORT_SYMBOL(reciva_smc_register_driver);
EXPORT_SYMBOL(reciva_smc_command);

module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


