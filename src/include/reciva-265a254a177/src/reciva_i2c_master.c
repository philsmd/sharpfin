/*
 * Reciva I2C master for s3c2410
 *
 * Copyright (c) 2006 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * Description:
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

#include "reciva_i2c_master.h"
#include "reciva_util.h"
#include "reciva_gpio.h"

/* Used to select slave address */
static int slave_index = 15;
MODULE_PARM(slave_index, "i");


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void reset_module(void);
static void setup_slave(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define PREFIX "RI2CM:"

/* GPIO registers */
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
/* GPIO port E */
#define GPECON GPIO_REG(0x40)
#define GPEDAT GPIO_REG(0x44)
#define GPEUP GPIO_REG(0x48)
/* GPIO port G */
#define GPGCON GPIO_REG(0x60)
#define GPGDAT GPIO_REG(0x64)
#define GPGUP GPIO_REG(0x68)

/* IIC registers */ 
#define IICCON   (S3C2410_VA_IIC + 0x00)
#define IICSTAT  (S3C2410_VA_IIC + 0x04) 
#define IICADD   (S3C2410_VA_IIC + 0x08) 
#define IICDS    (S3C2410_VA_IIC + 0x0c) 

/* Interrupt device ID */
#define DEV_ID 1

#define MODULE_NAME "Reciva I2C Master"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static char acModuleName[] = MODULE_NAME;
static wait_queue_head_t wait_queue;

/* Slave address lookup */
#define SLAVE_ADDR_COUNT 16
static int slave_address_lookup[SLAVE_ADDR_COUNT] =
{
  0x00,  // 0x00
  0x01,  // 0x01
  0x02,  // 0x02
  0x03,  // 0x03
  0x10,  // 0x04
  0x11,  // 0x05
  0x12,  // 0x06
  0x13,  // 0x07
  0x20,  // 0x08
  0x21,  // 0x09
  0x22,  // 0x0a
  0x23,  // 0x0b
  0x30,  // 0x0c
  0x31,  // 0x0d
  0x32,  // 0x0e
  0x33,  // 0x0f
};

/* Keeps track of how many I2C devices are found */
static int i2c_device_found = 0;

/* The I2C client - need for all I2C reads/writes */
static struct i2c_client *reciva_i2c_client = NULL;

/* Rx buffer */
#define BUF_SIZE 1024
static char rx_buffer[BUF_SIZE];
static int rx_data_valid = 0;
static int rx_length = 0;

static int driver_added = 0;


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

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
reciva_poll (struct file *file, poll_table *wait)
{
  poll_wait(file, &wait_queue, wait);
  if (rx_data_valid)
  {
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
reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  int ret = 0;

  if (rx_data_valid)
  {
    if (count > rx_length)
      count = rx_length;

    if (copy_to_user (buffer, rx_buffer, count))
      ret = -EFAULT;
    else
      ret = count;

    rx_data_valid = 0;
  }

  return ret;
}

/****************************************************************************
 * Write data to device
 * Then read back reply(s) from slave
 * Sequence is:
 * 1. Master Write
 * 2. Master Read (slave will respond with length on following message)
 * 3. Master Read (length bytes)
 ****************************************************************************/
static int 
reciva_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  char temp_buf[count+1];
  copy_from_user(temp_buf, buf, count);
  temp_buf[count] = 0;

  /* Abort if the slave device has not been found */
  if (reciva_i2c_client == NULL)
  {
    printk(PREFIX "Error : reciva_i2c_client == NULL\n");
    return 0;
  }

  /* Master Write to slave */
  int r;
  r = i2c_master_send(reciva_i2c_client, temp_buf, count);
  if (r != count) 
  {
    printk(PREFIX "write failed, status %d\n", r);
    return 0;
  }

  /* First Master Read - to determine length of following read */
  r = i2c_master_recv(reciva_i2c_client, rx_buffer, 2);
  if (r != 2) 
  {
    printk(PREFIX "1st read failed, status %d\n", r);
    return count;
  }

  /* 2nd Master Read - to get the real data */    
  rx_length = (rx_buffer[0] << 8) | rx_buffer[1];

  r = i2c_master_recv(reciva_i2c_client, rx_buffer, rx_length);
  if (r != rx_length) 
  {
    printk(PREFIX "2nd read failed, status %d\n", r);
    return count;
  }
  else
  {
    /* Inform app that new data has arrived */
    rx_data_valid = 1;
    wake_up_interruptible(&wait_queue);
  }

  return count;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
reciva_open(struct inode * inode, struct file * file)
{
  printk(PREFIX "open\n");
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
reciva_release(struct inode * inode, struct file * file)
{
  printk(PREFIX "release\n");
  return 0;
}

/****************************************************************************
 * Handle command from application
 ****************************************************************************/
static int
reciva_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  int temp;

  switch(cmd)
  {
    case IOC_RI2CM_RESET:
      reset_module();
      break;

    case IOC_RI2CM_INIT:
      printk(PREFIX "IOC_RI2CM_INIT\n");
      setup_slave(); 
      break;

    case IOC_RI2CM_GET_STATUS:
      temp = 0; 
      if (reciva_i2c_client)
        temp |= RI2CM_STATUS_SLAVE_DETECTED;

      if (put_user(temp, (int *)arg))
        return -EFAULT;
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
  read:     reciva_read,
  write:    reciva_write,
  poll:     reciva_poll,  
  open:     reciva_open,
  release:  reciva_release,
};

static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_i2c_master",
  &reciva_fops
};

   /*************************************************************************/
   /***                        File Operations - END                      ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                      I2C Driver - START                           ***/
   /*************************************************************************/

static struct i2c_driver reciva_i2c_driver;

static struct i2c_client client_template = {
  name: "(unset)",
  flags:  I2C_CLIENT_ALLOW_USE,
  driver: &reciva_i2c_driver
};

/****************************************************************************
 * This will get called for every I2C device that is found on the bus in the
 * address range specified in normal_i2c_range[]
 ****************************************************************************/
static int reciva_i2c_attach(struct i2c_adapter *adap, int addr, unsigned short flags, int kind)   
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
  clnt->data = NULL;

  reciva_i2c_client = clnt;
  i2c_attach_client(clnt);

  return 0;
}

static int reciva_i2c_detach_client(struct i2c_client *clnt)
{
  printk(PREFIX "reciva_i2c_detach_client\n");
  i2c_detach_client(clnt);

  if (clnt->data)
    kfree(clnt->data);
  kfree(clnt);

  return 0;
}

/* Addresses to scan */
#define RECIVA_I2C_ADDR_NORMAL      0x33
#define RECIVA_I2C_ADDR_RANGE_END   0x33  

static unsigned short normal_i2c[] = { RECIVA_I2C_ADDR_NORMAL, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = {RECIVA_I2C_ADDR_NORMAL, RECIVA_I2C_ADDR_RANGE_END, I2C_CLIENT_END};   
static unsigned short probe[] =       { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short probe_range[] = {I2C_CLIENT_END,  I2C_CLIENT_END};   
static unsigned short ignore[]       = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short ignore_range[] = { I2C_CLIENT_END, I2C_CLIENT_END };
static unsigned short force[] =        { RECIVA_I2C_ADDR_NORMAL, I2C_CLIENT_END, I2C_CLIENT_END, I2C_CLIENT_END };


static struct i2c_client_address_data addr_data = {
  normal_i2c, normal_i2c_range, 
  probe, probe_range, 
  ignore, ignore_range, 
  force
};

static int reciva_i2c_attach_adapter(struct i2c_adapter *adap)
{
  printk(PREFIX "reciva_i2c_attach_adapter\n");
  return i2c_probe(adap, &addr_data, reciva_i2c_attach);
}

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

static struct i2c_driver reciva_i2c_driver = {
  name: MODULE_NAME,
  id:   I2C_DRIVERID_WM8721,
  flags:    I2C_DF_NOTIFY,
  attach_adapter: reciva_i2c_attach_adapter,
  detach_client:  reciva_i2c_detach_client,
  inc_use:  reciva_i2c_inc_use,
  dec_use:  reciva_i2c_dec_use
};


   /*************************************************************************/
   /***                      I2C Driver - END                             ***/
   /*************************************************************************/

/****************************************************************************
 * Work out the slave address and set up the I2C driver
 ****************************************************************************/
static void setup_slave(void)
{
  if (reciva_i2c_client)
  {
    /* Don't need to do anything - slave is already detected */
  }
  else if (slave_index < SLAVE_ADDR_COUNT)
  {
    /* Set up slave address */
    int slave_address;
    slave_address = slave_address_lookup[slave_index];
    printk(PREFIX "slave_index=%d slave_address=0x%02x\n", slave_index,
                                                           slave_address);
    force[0] = slave_address;
    i2c_add_driver(&reciva_i2c_driver);
    driver_added = 1;
  }
}

/****************************************************************************
 * Reset the module
 ****************************************************************************/
static void reset_module(void)
{
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_module_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  int ret = 0;

  init_waitqueue_head (&wait_queue);

  /* Register the device */
  misc_register (&reciva_miscdev);

  i2c_device_found = 0;
  driver_added = 0;
  return ret;                         
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_module_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  /* Unregister the device */
  misc_deregister(&reciva_miscdev);

  /* Remove I2C driver */
  if (driver_added)
    i2c_del_driver(&reciva_i2c_driver);
}


module_init(reciva_module_init);
module_exit(reciva_module_exit);

MODULE_LICENSE("GPL");


