/*
 * linux/reciva/reciva_keypad_generic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 * 
 * Reciva Keypad driver
 *
 * Version 1.0 2003-12-12  John Stirling <js@reciva.com>
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>
#include <linux/string.h>
#include <linux/delay.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>


#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_keypad_generic.h"
#include "reciva_keypad_driver.h"

/* This module takes a parameter to define the keypad config
 * KEY_CONFIG_4x3 =   4 rows   : GPG8,9,10,11
 *                    3 columns: GPD8,9,10
 * KEY_CONFIG_4x4_1 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPB9
 * KEY_CONFIG_4x4_2 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPG5
 * KEY_CONFIG_DUMMY = dummy keypad - no gpio lines used
 *                    
 */
typedef enum
{
  KEY_CONFIG_4x3                        = 0,
  KEY_CONFIG_4x4_1                      = 1,
  KEY_CONFIG_4x4_2                      = 2,
  KEY_CONFIG_DUMMY                      = 3,

} key_config_t;
static key_config_t keypad_config = 0;
MODULE_PARM(keypad_config, "i");


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void reciva_gpio_int_handler(int irq, void *dev, struct pt_regs *regs);
static void gp_state_machine(unsigned long data);
static int decode_key_press(int row, int column);
static void update_shift_key_status (int key_pressed);
static int reciva_ioctl ( struct inode *inode, struct file *file,
                          unsigned int cmd, unsigned long arg);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port D */
#define GPDCON	GPIO_REG(0x30)
#define GPDDAT  GPIO_REG(0x34)
#define GPDUP   GPIO_REG(0x38)
/* GPIO port E */
#define GPECON	GPIO_REG(0x40)
#define GPEDAT	GPIO_REG(0x44)
/* GPIO port F */
#define GPFCON	GPIO_REG(0x50)
#define GPFDAT	GPIO_REG(0x54)
/* GPIO port G */
#define GPGCON	GPIO_REG(0x60)
#define GPGDAT	GPIO_REG(0x64)
#define GPGUP   GPIO_REG(0x68)

#define EXTINT1 GPIO_REG(0x8c)
#define EXTINT2 GPIO_REG(0x90)

/* External interrupt filter */
#define EINTFLT2 GPIO_REG(0x9c)


/* States */
#define IDLE                0
#define IDENTIFY_BUTTON     1
#define RELEASE_PENDING     2
#define RELEASE_DETECTED    3

/* Timeouts */
#define DEBOUNCE_TIMEOUT        ((HZ*20)/1000) /* 20 ms */

   

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static int state;                /* State */
static int gpio;                 /* GPIO number */
static int column;               /* Column currently being driven to determine
                                  * which button was pressed */
static struct timer_list timer;  /* Timer used for scheduling events */
static int key;
static int shift_key_status;
static struct input_dev *input_dev;
static const reciva_keypad_driver_t *driver = NULL;

static char acModuleName[] = "Reciva Generic Keypad";

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_ioctl,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "key",
  &reciva_fops
};



   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Cancels current shift key status. Rotary encoder movement should cancel
 * shift status
 ****************************************************************************/
void rkg_cancel_shift(void)
{
  shift_key_status = 0;
}  
   
/****************************************************************************
 * Keypad drivers should call this function to register themselves with this 
 * module
 ****************************************************************************/
void rkg_register(const reciva_keypad_driver_t *d)
{
  driver = d;
  printk("RKG:register driver: %s\n", driver->name);
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_keypad_init(void)
{
  printk("RKG:%s module: loaded (keypad_config=%d)\n", acModuleName, 
                                                       keypad_config);

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);

  switch(keypad_config)
  {
    case KEY_CONFIG_DUMMY:
      return 0;
      break;

    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
      break;
  }

  /* Intialise static data */
  state = IDLE;
  shift_key_status = 0;
  
  input_dev = kmalloc (sizeof (*input_dev), GFP_KERNEL);

  memset (input_dev, 0, sizeof (*input_dev));

  /* Set up input system */
#ifdef KERNEL_26
  init_input_dev (input_dev);
#endif
  input_dev->evbit[0] = BIT(EV_KEY);
  set_bit (BTN_0, input_dev->keybit);
  set_bit (BTN_1, input_dev->keybit);
  set_bit (BTN_2, input_dev->keybit);
  set_bit (BTN_3, input_dev->keybit);
  set_bit (BTN_4, input_dev->keybit);
  set_bit (BTN_5, input_dev->keybit);
  set_bit (BTN_6, input_dev->keybit);
  set_bit (BTN_7, input_dev->keybit);
  set_bit (BTN_8, input_dev->keybit);
  set_bit (BTN_9, input_dev->keybit);
  set_bit (BTN_A, input_dev->keybit);
  set_bit (BTN_B, input_dev->keybit);
  set_bit (BTN_C, input_dev->keybit);
  set_bit (BTN_X, input_dev->keybit);
  set_bit (BTN_Y, input_dev->keybit);
  set_bit (BTN_Z, input_dev->keybit);
  set_bit (BTN_Z+1, input_dev->keybit);
  set_bit (BTN_Z+2, input_dev->keybit);
  set_bit (BTN_Z+3, input_dev->keybit);
  set_bit (BTN_Z+4, input_dev->keybit);
  set_bit (BTN_Z+5, input_dev->keybit);
  set_bit (BTN_Z+6, input_dev->keybit);
  set_bit (BTN_Z+7, input_dev->keybit);
  set_bit (BTN_Z+8, input_dev->keybit);
  set_bit (BTN_Z+9, input_dev->keybit);
  set_bit (BTN_Z+10, input_dev->keybit);
  set_bit (BTN_Z+11, input_dev->keybit);
  set_bit (BTN_Z+12, input_dev->keybit);
  set_bit (BTN_Z+13, input_dev->keybit);
  set_bit (BTN_Z+14, input_dev->keybit);
  set_bit (BTN_Z+15, input_dev->keybit);
  set_bit (BTN_Z+16, input_dev->keybit);
  set_bit (BTN_Z+17, input_dev->keybit);
  set_bit (BTN_Z+18, input_dev->keybit);
  input_dev->name = "Reciva Generic Keypad";

  input_register_device (input_dev);

  /* Initialise and set debounce timer */
  init_timer(&timer);
  timer.function = gp_state_machine;

  /* Initialise GPIOs */
  /* GPD8 */
  rutl_regwrite((1 << 8), (0 << 8), GPDUP) ;   // Disable pullup
  rutl_regwrite((0 << 8), (1 << 8), GPDDAT);   // Set data low
  rutl_regwrite((1 << 16), (3 << 16), GPDCON); // Set as output
  /* GPD9 */
  rutl_regwrite((1 << 9), (0 << 9), GPDUP) ;   // Disable pullup
  rutl_regwrite((0 << 9), (1 << 9), GPDDAT);   // Set data low
  rutl_regwrite((1 << 18), (3 << 18), GPDCON); // Set as output
  /* GPD10 */
  rutl_regwrite((1 << 10), (0 << 10), GPDUP) ; // Disable pullup
  rutl_regwrite((0 << 10), (1 << 10), GPDDAT); // Set data low
  rutl_regwrite((1 << 20), (3 << 20), GPDCON); // Set as output

  /* Tell GPIO module which GPIO pins we are using */
  rgpio_register("GPD8,9,10 (output)", acModuleName);
  rgpio_register("GPG8,9,10,11 (input)", acModuleName);
  switch(keypad_config)
  {
    /* 4 rows(GPG8,9,10,11), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_DUMMY:
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPB9) */
    case KEY_CONFIG_4x4_1: 
      rgpio_register("GPB9 (output)", acModuleName);
      rutl_regwrite((1 << 9), (0 << 9), GPBUP) ;   // Disable pullup
      rutl_regwrite((0 << 9), (1 << 9), GPBDAT);   // Set data low
      rutl_regwrite((1 << 18), (3 << 18), GPBCON); // Set as output
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG5) */
    case KEY_CONFIG_4x4_2: 
      rgpio_register("GPG5 (output)", acModuleName);
      rutl_regwrite((1 << 5), (0 << 5), GPGUP) ;   // Disable pullup
      rutl_regwrite((0 << 5), (1 << 5), GPGDAT);   // Set data low
      rutl_regwrite((1 << 10), (3 << 10), GPGCON); // Set as output
      break;
  }

  /* Set the length of filter for external interrupt
   * Filter clock = PCLK
   * Filter width = 0x7f (max) */
  rutl_regwrite(0x7f7f7f7f, 0xffffffff, EINTFLT2); // Set as output

  /* Set EINT16 - EINT19 as falling edge level triggerred interrupts - filter
   * on */
  rutl_regwrite(0x0000aaaa, 0x0000ffff, EXTINT2); // Set as output

  /* Set GPG8-11 as EINTn, pullups on */
  /* GPG8 */
  rutl_regwrite((0 << 8), (1 << 8), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 16), (3 << 16), GPGCON); // EINTn
  /* GPG9 */
  rutl_regwrite((0 << 9), (1 << 9), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 18), (3 << 18), GPGCON); // EINTn
  /* GPG10 */
  rutl_regwrite((0 << 10), (1 << 10), GPGUP) ; // Enable pullup
  rutl_regwrite((2 << 20), (3 << 20), GPGCON); // EINTn
  /* GPG11 */
  rutl_regwrite((0 << 11), (1 << 11), GPGUP) ; // Enable pullup
  rutl_regwrite((2 << 22), (3 << 22), GPGCON); // EINTn

  /* Request interrupts */
  request_irq(IRQ_EINT16, reciva_gpio_int_handler, 0, "EINT16", (void *)8);
  request_irq(IRQ_EINT17, reciva_gpio_int_handler, 0, "EINT17", (void *)9);
  request_irq(IRQ_EINT18, reciva_gpio_int_handler, 0, "EINT18", (void *)10);
  request_irq(IRQ_EINT19, reciva_gpio_int_handler, 0, "EINT19", (void *)11);

  return 0;
}

/****************************************************************************
 * Module exit
 ****************************************************************************/
static void __exit 
reciva_keypad_exit(void)
{
  printk("RKG:Reciva Generic Keypad module: unloaded\n");

  misc_deregister (&reciva_miscdev);

  switch(keypad_config)
  {
    case KEY_CONFIG_DUMMY:
      return;
      break;

    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
      break;
  }

  free_irq(IRQ_EINT16, (void *)8);
  free_irq(IRQ_EINT17, (void *)9);
  free_irq(IRQ_EINT18, (void *)10);
  free_irq(IRQ_EINT19, (void *)11);

  del_timer(&timer);

  input_unregister_device (input_dev);
  kfree (input_dev);
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Command handler
 * Parameters : Standard ioctl params
 * Returns : 0 == success, otherwise error code
 ****************************************************************************/
static int
reciva_ioctl ( struct inode *inode, struct file *file,
               unsigned int cmd, unsigned long arg)
{
  int *piTemp;
  switch(cmd)
  {
    /* A list of keys */
    case IOC_KEY_GETSUPPORTED:
      if (copy_from_user(&piTemp, (void *)arg, sizeof(piTemp)))
        return -EFAULT;

      if (driver && driver->keys_present)
      { 
        if (copy_to_user(piTemp, driver->keys_present(), sizeof(int)*RKD_MAX_KEYS))
          return -EFAULT;
      } 
      else
      {
        return -EFAULT;
      }
        
      break;

    /* A list of alternate functions for the keys */
    case IOC_KEY_GET_ALT_FN:
      if (copy_from_user(&piTemp, (void *)arg, sizeof(piTemp)))
        return -EFAULT;

      if (driver && driver->alt_key_functions)
      { 
        if (copy_to_user(piTemp, driver->alt_key_functions(), sizeof(int)*RKD_MAX_KEYS))
          return -EFAULT;
      }
      else
      {
        return -EFAULT;
      }
        
      break;

    default:
      return -ENODEV;
  }

  return 0;
}

/****************************************************************************
 * Convert a given row and column into an ascii key press value.
 * row - row (range 2 to 5)
 * column - column (range 35 to 37)
 ****************************************************************************/
static int decode_key_press(int row, int column)
{
  int key_pressed = RKD_UNUSED;

  row = row - 8;        /* Convert to range 0 to 3 */

  if (driver && driver->decode_key_press)
    key_pressed = driver->decode_key_press(row, column, shift_key_status);

  update_shift_key_status (key_pressed);

  return key_pressed;
}

/****************************************************************************
 * Updates the shift key status
 * key_pressed - key that has been pressed
 ****************************************************************************/
static void update_shift_key_status (int key_pressed)
{
  /* Any button press will cause shift key status to be reset */
  if (shift_key_status == 0)
  {
    if (key_pressed == RKD_SHIFT)
      shift_key_status = 1;
  }
  else
  {
    if (key_pressed != RKD_UNUSED)
      shift_key_status = 0;
  }
}  

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 **Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_gpio_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  printk("GPIOint\n");
  if (state == IDLE)
  {
    gpio = (int)dev;
    gp_state_machine(0);
  }
}

/****************************************************************************
 ** Read the status of the keypad rows
 ***************************************************************************/
static int read_input(void)
{
  unsigned long temp;
  temp = __raw_readl(GPGDAT);
  return temp;
}

/****************************************************************************
 ** Sets the column levels
 ***************************************************************************/
static void set_columns(int c)
{
  unsigned int con = 0;
  if ((c & 1) == 0)
    con |= 1 << 16;
  if ((c & 2) == 0)
    con |= 1 << 18;
  if ((c & 4) == 0)
    con |= 1 << 20;
  rutl_regwrite(con, (3 << 20 | 3 << 18 | 3 << 16), GPDCON);

  switch(keypad_config)
  {
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_DUMMY:
      break;

    case KEY_CONFIG_4x4_1: 
      if ((c & 8) == 0)
        rutl_regwrite((1 << 18), (3 << 18), GPBCON); // Set as output
      else
        rutl_regwrite((0 << 18), (3 << 18), GPBCON); // Set as input
      break;

    case KEY_CONFIG_4x4_2: 
      if ((c & 8) == 0)
        rutl_regwrite((1 << 10), (3 << 10), GPGCON); // Set as output
      else
        rutl_regwrite((0 << 10), (3 << 10), GPGCON); // Set as input
      break;
  }
}

/****************************************************************************
 * Common debounce state machine for GP2,3,4,5.
 *
 *     State Machine Operation
 *     1. IDLE : Waiting for keypress to be detected
 *     2. IDENTIFY_BUTTON : Attempting to identify which button has been pressed
 *        Columns driven low one at a time and rows read to determine button.
 *        The time interval between between setting column low and reading back
 *        row is to allow for board capacitance.
 *     3. RELEASE_PENDING : Waiting for the button to be released.
 *     4. RELEASE_DETECTED : Button has been released, waiitng another debounce
 *        period before re-enabling interrupts.
 ****************************************************************************/
static void gp_state_machine(unsigned long ignore)
{
  int i;
  static int count;                   /* Release Pending timeout count */
  static int release_detect_count;    /* Used to ensure we don't wrongly
                                       * detect a key release */
  int keypress_detected = 0;
                                                                
  switch (state)
  {
    case IDLE :
      /* Disable interrupt */
      disable_irq(IRQ_EINT16);
      disable_irq(IRQ_EINT17);
      disable_irq(IRQ_EINT18);
      disable_irq(IRQ_EINT19);

      /* Drive first column low */
      set_columns(7);

      /* Set timer */
      mod_timer(&timer, jiffies + DEBOUNCE_TIMEOUT);

      /* Set up state machine data and change state */
      column = 3;
      state = IDENTIFY_BUTTON;
      break;

    case IDENTIFY_BUTTON :
      /* Scan to identify key press */
      for (i=0;i<4;i++)
      {

        /* Check for detection of key press */
        /* Read GPIO level 3 times to handle and noise on the line */
        if ((read_input() & (1 << gpio)) == 0)
        {
          udelay(10);
          if ((read_input() & (1 << gpio)) == 0)
          {
            udelay(10);
            if ((read_input() & (1 << gpio)) == 0)
            {
              /* Key press detected - decode it and send it to application */
              keypress_detected = 1;
              key = decode_key_press(gpio, column);
              input_report_key (input_dev, key, 1);
#ifdef KERNEL_26
              input_sync (input_dev);
#endif

              /* Drive columns low */
              /* If no switch is pressed then rows should read back 1 */
              set_columns(0);
              break;
            }
          }
        }

        if (column >= 1)
        {
          /* Drive all columns high */
          set_columns (0x0f);

          /* Set column low */
          column--;
          set_columns (~(1 << column));

          /* .. and delay before reading back GPIO level
           * to allow for board capacitance */
          udelay(1000);
        }
        else
        {
          /* Drive columns low */
          /* If no switch is pressed then rows should read back 1 */
          set_columns(0);
        }
      }
    
      /* Set timer */
      mod_timer(&timer, jiffies + DEBOUNCE_TIMEOUT);

      /* Don't bother with RELEASE_PENDING if we didn't detect a key press */
      count = 0;
      release_detect_count = 0;
     if (keypress_detected)
        state = RELEASE_PENDING;
      else
        state = RELEASE_DETECTED;
      break;

    case RELEASE_PENDING :
      /* Ensure we don't wrongly detect a key release */
      if (read_input() & (1 << gpio))
        release_detect_count++;
      else
        release_detect_count = 0;
                    
      /* Check for key release.
       * Time out if key release has not been detected after 5 seconds */
      if (release_detect_count > 2)
      {
        /* Key release detected */
        state = RELEASE_DETECTED;
      }  

      /* Set timer */
      mod_timer(&timer, jiffies + DEBOUNCE_TIMEOUT);
      break;

    case RELEASE_DETECTED :
      /* Back to IDLE */
      state = IDLE;

      /* Re-enable interrupts */
      enable_irq(IRQ_EINT16);
      enable_irq(IRQ_EINT17);
      enable_irq(IRQ_EINT18);
      enable_irq(IRQ_EINT19);
      input_report_key (input_dev, key, 0);
#ifdef KERNEL_26
      input_sync (input_dev);
#endif
      break;

    default :
      printk("RKG:gp_state_machine : error unknown state %d\n", state);
      state = IDLE;
      break;
  }
}

EXPORT_SYMBOL(rkg_cancel_shift);
EXPORT_SYMBOL(rkg_register);

module_init(reciva_keypad_init);
module_exit(reciva_keypad_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Generic Keypad");
MODULE_LICENSE("GPL");



