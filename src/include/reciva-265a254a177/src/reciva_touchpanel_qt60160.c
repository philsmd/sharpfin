/*
 * Reciva Touchpanel - Quantum QT60160
 * Copyright (c) $Date: 2007-07-24 15:41:12 $ Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_keypad_generic.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define MODULE_NAME "Reciva qt60160"

/* Enable debug */
//#define DEBUG_ENABLED

/* Debug prefix */
#define PREFIX "RT_QT6:"

/* Max number of touchpanel keys */
#define RT_QT6_MAX_KEYS 24

/* Register addresses */
#define RT_QT6_REG_ADDR_REV           0
#define RT_QT6_REG_ADDR_KEYS          1
#define RT_QT6_REG_ADDR_RECALIBRATE   125
#define RT_QT6_REG_ADDR_UNLOCK        130
#define RT_QT6_REG_ADDR_SETUP         131

#define LEN_QT_SETUP            123                   /* setup table length */
#define NUM_QT_KEY_BYTES        3

#define QT_COMMAND_CODE         0x55


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva Touchpanel QT60160";

/* Converts touchpanel data to key events */
static const int cf959_lookup[RT_QT6_MAX_KEYS] =
{
  /*  0 */  RKD_POWER,
  /*  1 */  RKD_BACK,
  /*  2 */  RKD_REPLY,
  /*  3 */  RKD_SELECT,
  /*  4 */  RKD_UNUSED,
  /*  5 */  RKD_UNUSED,
  /*  6 */  RKD_UNUSED,
  /*  7 */  RKD_UNUSED,
  /*  8 */  RKD_IR_FM_MODE_SWITCH,
  /*  9 */  RKD_ALARM,
  /* 10 */  RKD_PRESET_6,
  /* 11 */  RKD_PRESET_5,
  /* 12 */  RKD_UNUSED,
  /* 13 */  RKD_UNUSED,
  /* 14 */  RKD_UNUSED,
  /* 15 */  RKD_UNUSED,
  /* 16 */  RKD_PRESET_4,
  /* 17 */  RKD_PRESET_3,
  /* 18 */  RKD_PRESET_2,
  /* 19 */  RKD_PRESET_1,
  /* 20 */  RKD_UNUSED,
  /* 21 */  RKD_UNUSED,
  /* 22 */  RKD_UNUSED,
  /* 23 */  RKD_UNUSED,
};

/* Default values for setups block - controls key sensitivity */
#define NTHR  0x93
#define PDRI  0x4f
#define NDIL  0x52
#define NRD   0x14
#define WAKE  0x3f
#define SLEEP 0xbb
#define AWAKE 0xff
#define DHT   0x00
static const char setups_block_defaults[LEN_QT_SETUP] =
{
  /* NTHR/NDRIFT */
  NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,
  NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,NTHR,

  /* PDRIFT */
  PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,
  PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,PDRI,

  /* NDIL/FDIL  */
  NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,
  NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,NDIL,

  /* NRD */
  NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,
  NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,NRD ,

  /* WAKE/BL/AKS/SSYNC */
  WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,
  WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,WAKE,

  /* SLEEP/MSYNC */
  SLEEP,

  /* AWAKE */
  AWAKE,

  /* DHT */
  DHT
};

/* I2C address */
static int i2c_address = 117;
MODULE_PARM(i2c_address, "i");

/* Set to 1 to initialise the Setups Block 
 * (which controls key sensitivity) */
static int init_setups = 0;
MODULE_PARM(init_setups, "i");

/* Set to use interrupt method to detect key presses
 * Default is to use polling method as it has been reported that
 * there are problems with the CHANGE signal */
static int use_interrupts = 0;
MODULE_PARM(use_interrupts, "i");

/* The I2C client - need for all I2C reads/writes */
static struct i2c_client *reciva_i2c_client = NULL;

/* Keeps track of how many I2C devices are found */
static int i2c_device_found = 0;

/* Thread related */
static struct semaphore start_thread_sem;
static DECLARE_COMPLETION(device_read_thread_complete);
static pid_t tDeviceReadThreadPid = 0; 
static int iDeviceReadThreadEnabled = 1;

/* Not used when we are using polling method 
 * When we're using interrupts this is used to tell the 
 * reader thread to read from the device */
static int key_status_changed = 0;


   /*************************************************************************/
   /***                   Private function prototypes                     ***/
   /*************************************************************************/

static int read_from_device (int address, char *dest, int length);


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Get level of CHANGE pin
 ****************************************************************************/
static inline int get_CHANGE(void)
{
  int level = 0;
  unsigned long temp;

  temp = __raw_readl(S3C2410_GPGDAT);
  if (temp & (0x01<<11))
    level = 1;

  return level;
}

/****************************************************************************
 * Returns the position of the first bit that is set in a 32 bit integer
 * (least significant bit = 0, most significant = 31)
 ****************************************************************************/
static int first_set_bit(int n)
{
  int first_set_bit = -1;

  int i;
  for (i=0; i<32; i++)
  { 
    if (n & 0x01)    
    {
      first_set_bit = i;
      break;
    }

    n >>= 1;
  }

  return first_set_bit;
}

/****************************************************************************
 * Returns count of number of bits set to 1 in a 32 bit integer
 ****************************************************************************/
static int bitcount (int n)
{
  static const int bits_in_nibble[16] =
  {
    0, /* 0  0000 */
    1, /* 1  0001 */
    1, /* 2  0010 */
    2, /* 3  0011 */
    1, /* 4  0100 */
    2, /* 5  0101 */
    2, /* 6  0110 */
    3, /* 7  0111 */
    1, /* 8  1000 */
    2, /* 9  1001 */
    2, /* 10 1010 */
    3, /* 11 1011 */
    2, /* 12 1100 */
    3, /* 13 1101 */
    3, /* 14 1110 */
    4, /* 15 1111 */
  };

  int count = 0;
  
  count += bits_in_nibble [(n >> 0 )   & 0x0000000f];
  count += bits_in_nibble [(n >> 4 )   & 0x0000000f];
  count += bits_in_nibble [(n >> 8 )   & 0x0000000f];
  count += bits_in_nibble [(n >> 12 )  & 0x0000000f];
  count += bits_in_nibble [(n >> 16 )  & 0x0000000f];
  count += bits_in_nibble [(n >> 20 )  & 0x0000000f];
  count += bits_in_nibble [(n >> 24 )  & 0x0000000f];
  count += bits_in_nibble [(n >> 28 )  & 0x0000000f];

  return count;
}

/****************************************************************************
 * Converts a touchpanel event into a key event
 * data - data from touchpanel (bit 5 = key state, 4:0 = key id)
 ****************************************************************************/
static void convert_to_key_event(int data, int *event_id, int *press)
{
  int count = bitcount(data);

  *event_id = RKD_UNUSED;
  *press = 0;

  /* Ignore multi key presses */
  if (count == 1)
  {
    int key = first_set_bit(data);
    if (key >= 0 && key < RT_QT6_MAX_KEYS)
    {
      *press = 1;
      *event_id = cf959_lookup[key];
    } 
  }
}

/****************************************************************************
 * Read from device
 * data - read data gets dumped here
 * count - number of bytes to read
 ****************************************************************************/
static int
reciva_chip_read (struct i2c_client *clnt, unsigned char *data, int count)
{
  int r;

  r = i2c_master_recv (clnt, (unsigned char *)data, count);
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
  int r;

  r = i2c_master_send (clnt, (unsigned char *)data, count);
  if (r != count) 
  {
    printk(PREFIX "write failed, status %d\n", r);
    return r;
  }

  return 0;
}

/****************************************************************************
 * Read key status information from device
 * key_status - key status info returned here
 * Returns - 0 on success, non zero on error
 ****************************************************************************/
static int read_key_status(int *key_status)
{
  int r = 0;

  if (i2c_device_found == 0)
    return -1;

  char kdata[4];
  read_from_device (RT_QT6_REG_ADDR_KEYS, kdata, 3);
  *key_status = kdata[0] | (kdata[1] << 8) | (kdata[2] << 16);

#ifdef DEBUG_ENABLED2
  static int count = 0;
  count++;
  printk(PREFIX "c=%d KEY status %02x %02x %02x (%08x)\n", 
                        count, kdata[0], kdata[1], kdata[2], *key_status);
#endif

#ifdef DEBUG_ENABLED2
  char data[125];

  /* Set up address of register we're going to read from */
  data[0] = 1;
  r = reciva_chip_write (reciva_i2c_client, data, 1);
  if (r != 0)
    return r;

  /* Read key status registers (addresses 1,2,3) */  
  r = reciva_chip_read (reciva_i2c_client, data, 125);
  if (r != 0)
    return r;

  int i;
  int j = 3;
  for (i=0; i<8; i++)
  {
    printk(PREFIX "  [%d] = %02x %02x %02x %02x %02x\n", 
                   i, data[j], data[j+1], data[j+2], data[j+3], data[j+4]);
    j+=5;
  }
  for (i=8; i<16; i++)
  {
    printk(PREFIX "    [%d] = %02x %02x %02x %02x %02x\n", 
                   i, data[j], data[j+1], data[j+2], data[j+3], data[j+4]);
    j+=5;
  }
  for (i=16; i<24; i++)
  {
    printk(PREFIX "      [%d] = %02x %02x %02x %02x %02x\n", 
                   i, data[j], data[j+1], data[j+2], data[j+3], data[j+4]);
    j+=5;
  }
#endif

  return r;
} 

/****************************************************************************
 * Read key status registers to check if a key has been pressed
 ****************************************************************************/
static void check_for_key_press(void)
{
  static int last_key = RKD_UNUSED;

  /* Read the raw key status data */
  int key_data;
  read_key_status(&key_data);

  /* Convert that into a key event */
  int key;
  int press;
  convert_to_key_event(key_data, &key, &press);

  if (key != RKD_UNUSED)
  {

    if (key != last_key)
    {
      /* Report release of last key */
      if (last_key != RKD_UNUSED)
      {
        printk(PREFIX "KEY release %d\n", last_key);
        rkg_report_key(last_key, 0);
      }

      /* Report key press */
      printk(PREFIX "KEY press %d\n", key);
      rkg_report_key(key, 1);
      last_key = key;
    }
  }
  else
  {
    /* Report key release */
    if (key_data == 0 && last_key != RKD_UNUSED)
    {
      printk(PREFIX "KEY release %d\n", last_key);
      last_key = RKD_UNUSED;
      rkg_report_key(last_key, 0);
    }
  }
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 * Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void touchpanel_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  key_status_changed = 1;
}

/****************************************************************************
 * Setup gpio for reading from the device
 ****************************************************************************/
static void setup_gpio(void)
{
  /* GPG11 (CHANGE) */
  rutl_regwrite((1 << 11), (0 << 11), S3C2410_GPGUP) ; // Disable pullup
  rutl_regwrite((2 << 22), (3 << 22), S3C2410_GPGCON); // EINT19

  /* Set the length of filter for external interrupt
   * Filter clock = PCLK
   * Filter width = 0x7f (max) */
  rutl_regwrite(0x7f000000, 0xff000000, S3C2410_EINFLT2);

  /* EINT19 - rising edge triggerred interrupts - filter on */
  rutl_regwrite(0x0000c000, 0x0000f000, S3C2410_EXTINT2);
}

/****************************************************************************
 * Initialise device
 ****************************************************************************/
static void init_device(void)
{
  /* Set up to receive key events from device */
  setup_gpio();

  if (use_interrupts)
    request_irq(IRQ_EINT19, touchpanel_int_handler, 0, "EINT19", (void *)11);
}

/****************************************************************************
 * Reads the setup block into dest
 * address - device start address
 * dest - destintation
 * length - number of bytes to read 
 * Returns - 0 on success, non zero on error
 ****************************************************************************/
static int read_from_device (int address, char *dest, int length)
{
  int r;
  char kdata[1];

  /* Set up address of register we're going to read from */
  kdata[0] = address;
  r = reciva_chip_write (reciva_i2c_client, kdata, 1);
  if (r != 0)
    return r;

  /* Read data */
  r = reciva_chip_read (reciva_i2c_client, dest, length);
  if (r != 0)
    return r;

  return  r;
}

/****************************************************************************
 * Initialise Setups Block
 ****************************************************************************/
static void initialise_setups_block(void)
{
  printk(PREFIX "Writing setups block\n");

  /* Need to write an unlock code prior to writing data */
  char data[LEN_QT_SETUP+2];
  data[0] = RT_QT6_REG_ADDR_UNLOCK;
  data[1] = QT_COMMAND_CODE;
  memcpy(&data[2], setups_block_defaults, LEN_QT_SETUP);

  reciva_chip_write (reciva_i2c_client, data, LEN_QT_SETUP+2);
}

/****************************************************************************
 * Print the setups block. This contains sensitivity settings etc
 ****************************************************************************/
static void print_setups_block(char *data)
{
#ifdef DEBUG_ENABLED
    int i=0;
    int j=0;
    printk(PREFIX "---------------------------------------------------\n");
    printk(PREFIX "- Setups Block\n");
    printk(PREFIX "---------------------------------------------------\n");

    printk(PREFIX "NTHR/NDRIFT        ");
    for (i=0; i<RT_QT6_MAX_KEYS; i++)
    {
      printk("%02x ", data[j]);
      j++;
    }
    printk("\n");

    printk(PREFIX "PDRIFT             ");
    for (i=0; i<RT_QT6_MAX_KEYS; i++)
    {
      printk("%02x ", data[j]);
      j++;
    }
    printk("\n");

    printk(PREFIX "NDIL/FDIL          ");
    for (i=0; i<RT_QT6_MAX_KEYS; i++)
    {
      printk("%02x ", data[j]);
      j++;
    }
    printk("\n");

    printk(PREFIX "NRD                ");
    for (i=0; i<RT_QT6_MAX_KEYS; i++)
    {
      printk("%02x ", data[j]);
      j++;
    }
    printk("\n");

    printk(PREFIX "WAKE/BL/AKS/SSYNC  ");
    for (i=0; i<RT_QT6_MAX_KEYS; i++)
    {
      printk("%02x ", data[j]);
      j++;
    }
    printk("\n");

    printk(PREFIX "SLEEP/MSYNC        ");
    printk("%02x\n", data[j]);
    j++;

    printk(PREFIX "AWAKE              ");
    printk("%02x\n", data[j]);
    j++;

    printk(PREFIX "DHT                ");
    printk("%02x\n", data[j]);
    j++;
#endif
}

/****************************************************************************
 * Thread used to read key status info from the device via I2C
 ****************************************************************************/
static int iDeviceReadThread(void *arg)
{
  up(&start_thread_sem);

  while (iDeviceReadThreadEnabled)
  {
    if (use_interrupts == 0 || key_status_changed)
    {
      key_status_changed = 0;
      check_for_key_press();
    }

    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout((HZ * 100)/1000); /* sleep for 100 ms */
  }

  complete_and_exit(&device_read_thread_complete, 0);
}


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

  /* Read the setups block */
  char setups[LEN_QT_SETUP];
  read_from_device (RT_QT6_REG_ADDR_SETUP, setups, LEN_QT_SETUP);
  print_setups_block(setups);

  /* Compare actual and expected setups block data */
  if (memcmp(setups, setups_block_defaults, LEN_QT_SETUP) == 0)
  {
    printk(PREFIX "Setups Block matches - no need to rewrite\n");  
  }
  else
  {
    printk(PREFIX "Setups Block doesn't match\n");  
    if (init_setups)
      initialise_setups_block();
  }

  /* Recalibrate keys */
  char data[1];
  data[0] = RT_QT6_REG_ADDR_RECALIBRATE;
  int r = reciva_chip_write (reciva_i2c_client, data, 1);
  if (r != 0)
    return r;

  /* Need a dummy key status read to clear CHANGE line */
  int key_status;
  read_key_status(&key_status);

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
#define RECIVA_I2C_ADDR_NORMAL      117
#define RECIVA_I2C_ADDR_RANGE_END   117  

static unsigned short normal_i2c[] = { RECIVA_I2C_ADDR_NORMAL, I2C_CLIENT_END };
static unsigned short normal_i2c_range[] = {I2C_CLIENT_END, I2C_CLIENT_END};   
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

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_touchpanel_init(void)
{
  printk(PREFIX "%s module: loaded\n", acModuleName);
  printk(PREFIX "  i2c_address=%d\n", i2c_address);
  printk(PREFIX "  init_setups=%d\n", init_setups);
  printk(PREFIX "  use_interrupts=%d\n", use_interrupts);
  init_device();

  /* Set up I2C address and add driver */
  normal_i2c[0] = i2c_address;
  normal_i2c[1] = i2c_address;
  int r = i2c_add_driver(&reciva_i2c_driver);

  /* Kick off a thread to deal with all subsequent I2C accesses to device */
  init_MUTEX(&start_thread_sem);
  int i = kernel_thread(iDeviceReadThread, (void *)0, 0);
  if (i >= 0) 
  {
    tDeviceReadThreadPid = i;
    /* Wait for thread to finish init */
    down(&start_thread_sem);
  }

  return r;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_touchpanel_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", acModuleName);

  if (use_interrupts)
    free_irq(IRQ_EINT19, (void *)11);

  i2c_del_driver(&reciva_i2c_driver);

  /* Stop the I2C access thread and wait for it to finish */
  iDeviceReadThreadEnabled = 0;
  if (tDeviceReadThreadPid)
    wait_for_completion(&device_read_thread_complete);
}


module_init(reciva_touchpanel_init);
module_exit(reciva_touchpanel_exit);

MODULE_LICENSE("GPL");

