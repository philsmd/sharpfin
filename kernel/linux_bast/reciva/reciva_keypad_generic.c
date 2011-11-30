/*
 * linux/reciva/reciva_keypad_generic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004-2006 Reciva Ltd. All Rights Reserved
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

//#define RKG_DEBUG
#define PFX "RKG: "
#ifdef RKG_DEBUG
#define DEBUG(...) printk(PFX __VA_ARGS__ );
#else
#define DEBUG(...)
#endif

/* This module takes a parameter to define the keypad config
 * KEY_CONFIG_4x3 =   4 rows   : GPG8,9,10,11
 *                    3 columns: GPD8,9,10
 * KEY_CONFIG_4x4_1 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPB9
 * KEY_CONFIG_4x4_2 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPG5
 * KEY_CONFIG_DUMMY = dummy keypad - no gpio lines used
 * KEY_CONFIG_3x3   = 3 rows   : GPG8,9,10
 *                    3 columns: GPD8,9,10
 *                    
 */
typedef enum
{
  KEY_CONFIG_4x3                        = 0,
  KEY_CONFIG_4x4_1                      = 1,
  KEY_CONFIG_4x4_2                      = 2,
  KEY_CONFIG_DUMMY                      = 3,
  KEY_CONFIG_3x3			= 4,
  KEY_CONFIG_3x3_PLUS_1			= 5,
} key_config_t;
static key_config_t keypad_config = 0;
MODULE_PARM(keypad_config, "i");

static int pins_shared = 0;
MODULE_PARM(pins_shared, "i");

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void reciva_gpio_int_handler(int irq, void *dev, struct pt_regs *regs);
static void reciva_gpio_standalone_int_handler(int irq, void *dev, struct pt_regs *regs);
static void gp_state_machine(unsigned long data);
static void standalone_debounce(unsigned long data);
static int decode_key_press(int row, int column);
static int reciva_keypad_ioctl(struct inode *inode, struct file *file,
			       unsigned int cmd, unsigned long arg);
static void set_columns(int c);
static void setup_gpio_direction(int register_pin_usage);
static void setup_gpio_output(int register_pin_usage);
static void setup_gpio_input(int register_pin_usage);
static int read_input(void);
static void check_for_keypress(void);
static void enable_interrupts(void);
static void disable_interrupts(void);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

//FIXME Really should use the standard kernel header files for these definitions
#define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))

/* GPIO port B */
#define GPBCON GPIO_REG(0x10)
#define GPBDAT GPIO_REG(0x14)
#define GPBUP GPIO_REG(0x18)
/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
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

#define EXTINTPEND GPIO_REG(0xA8)

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
static struct timer_list standalone_timer;  /* Debounce timer for standalone keys */
static int standalone_key_state;
static int standalone_idle = 1;
static int key;
static struct input_dev *input_dev;
static const reciva_keypad_driver_t *driver = NULL;
static int num_cols, num_rows;

static char acModuleName[] = "Reciva Generic Keypad";

/* Controls access to the GPIO pins normally used by this module 
 * (on some hardware configs the LCD and Keypad control lines are shared) */
static spinlock_t access_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t standalone_access_lock = SPIN_LOCK_UNLOCKED;

/* Indicates if interrupt has fired 
 * Used to prevent race in check_for_keypress after external module access */
static int interrupt_ran = 0;

/* Used to clear spurious interrupts during extenral module access */
static unsigned int clear_pending_mask;

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  ioctl:    reciva_keypad_ioctl,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "key",
  &reciva_fops
};

/* Column Levels 
 * This gets saved when an external module takes control and is restored
 * when external module releases control */
static int column_levels;

// Power-on key detect
typedef enum
{
  POKS_NONE,            // no key found
  POKS_ALLOWED_TO_FIND, // first check after power-on
  POKS_GOT_INTERRUPT,   // was key on at init, detecting key
  POKS_FOUND,           // detected key on
} PowerOnKeyState_t;
static PowerOnKeyState_t power_on_key_state = POKS_NONE;
static int power_on_row = 0;
static int power_on_col = 0;

/****************************************************************************
 * Restart timers
 ****************************************************************************/
static void restart_timer(void)
{
  mod_timer (&timer, jiffies + DEBOUNCE_TIMEOUT);
}
static void restart_standalone_timer(void)
{
  mod_timer (&standalone_timer, jiffies + DEBOUNCE_TIMEOUT);
}

/****************************************************************************
 * Initialise timers
 ****************************************************************************/
static void init_keypad_timers(void)
{
  init_timer(&timer);
  timer.function = gp_state_machine;

  init_timer(&standalone_timer);
  standalone_timer.function = standalone_debounce;
}

/****************************************************************************
 * Delete timers
 ****************************************************************************/
static void delete_keypad_timers(void)
{
  del_timer(&timer);
  del_timer(&standalone_timer);
}
   
/****************************************************************************
 * Keypad drivers should call this function to register themselves with this 
 * module
 ****************************************************************************/
void rkg_register(const reciva_keypad_driver_t *d)
{
  driver = d;
  printk(PFX "register driver: %s\n", driver->name);
}

/****************************************************************************
 * Unregister a previously-registered driver
 ****************************************************************************/
void
rkg_unregister(const reciva_keypad_driver_t *d)
{
  if (driver == d)
    driver = NULL;
}

/****************************************************************************
 * Request access to the GPIO pins normally used by this module
 * (the Keypad and LCD are shared in some hardware configs)
 * This function will only return when access has been granted
 ****************************************************************************/
void rkg_access_request(void)
{
  DEBUG("%s\n", __FUNCTION__);

  if (pins_shared)
  {
    /* Wait for the keypad module to finish with the GPIO pins */
    unsigned long flags;
    spin_lock_irqsave(&access_lock, flags);
    switch (state)
    {
      case IDLE:
        disable_interrupts();
        break;
      case IDENTIFY_BUTTON:
      case RELEASE_PENDING:
      case RELEASE_DETECTED:
        del_timer(&timer);
        break;
    }
    spin_unlock_irqrestore(&access_lock, flags);

    spin_lock_irqsave(&standalone_access_lock, flags);
    if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
    {
      if (standalone_idle)
      {
        disable_irq(IRQ_EINT19);
      }
      else
      {
        del_timer(&standalone_timer);
      }
    }
    spin_unlock_irqrestore(&standalone_access_lock, flags);
  }
}

/****************************************************************************
 * Relinquish access to the GPIO pins normally used by this module
 * (the Keypad and LCD are shared in some hardware configs)
 * This should be used in conjunction with rkg_access_request
 ****************************************************************************/
void rkg_access_release(void)
{
  DEBUG("%s\n", __FUNCTION__);

  if (pins_shared)
  {

    /* Restore column levels */
    set_columns(column_levels);

    /* Set up GPIO direction */
    setup_gpio_output(0);

    /* delay before reading back GPIO level to allow for board capacitance */
    // TODO This could be too conservative - need to recalculate/measure
    udelay(1000);

    // Clear pending flags to ignore spurious interrupts
    __raw_writel(clear_pending_mask, EXTINTPEND);

    // Resumme matrix keypress state machine
    switch (state)
    {
      case IDLE:
        interrupt_ran = 0;
        enable_interrupts();
        check_for_keypress();
        break;
      case IDENTIFY_BUTTON:
      case RELEASE_PENDING:
      case RELEASE_DETECTED:
        restart_timer();
        break;
    }

    // Resume standalone keypress checking
    if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
    {
      if (standalone_idle)
      {
        standalone_debounce(0);
      }
      else
      {
        restart_standalone_timer();
      }
    }
  }
}

static void enable_interrupts()
{
  enable_irq(IRQ_EINT16);
  enable_irq(IRQ_EINT17);
  enable_irq(IRQ_EINT18);
  if (num_rows > 3)
  {
    enable_irq(IRQ_EINT19);
  }
}

static void disable_interrupts()
{
  disable_irq(IRQ_EINT16);
  disable_irq(IRQ_EINT17);
  disable_irq(IRQ_EINT18);
  if (num_rows > 3)
  {
    disable_irq(IRQ_EINT19);
  }
}

/****************************************************************************
 * Check for evidence of a keypress while the LCD is in control
 ****************************************************************************/
static void check_for_keypress(void)
{
  int input_mask = (1<<8)|(1<<9)|(1<<10);
  if (keypad_config != KEY_CONFIG_3x3)
  {
    input_mask |= (1<<11);
  }
  // Switch is active low, but invert input to look for a high
  int input_levels = (~read_input() & input_mask) >> 8;
  if (input_levels)
  {
    // Only interested in first read, as that will put the
    // state machine into a non-IDLE state, meaning the other interrupts
    // will be ignored (unless they take more than 20ms to process!)
    int interrupt_number=8;
    while (input_levels)
    {
      if (input_levels & 1)
      {
        break;
      }
      interrupt_number++;
      input_levels >>= 1;
    }
    disable_interrupts();
    // Only call handler if interrupt hasn't run since they were re-enabled
    if (!interrupt_ran)
    {
      DEBUG("found lost keypress %d\n", interrupt_number);
      reciva_gpio_int_handler(0, (void *)interrupt_number, NULL);
    }
    // Enable to balance disable above
    // (Interrupts should still be disabled as state machine will have
    // disabled them)
    enable_interrupts();
  }
}

/****************************************************************************
 * Sets up the direction and pullup status of all GPIO pins
 * register_pin_usage - set to register the pins we are using with 
 * external module
 ****************************************************************************/
static void setup_gpio_direction(int register_pin_usage)
{
  setup_gpio_output(register_pin_usage);
  setup_gpio_input(register_pin_usage);
}

static void setup_gpio_output(int register_pin_usage)
{
  /* Set up the columns (outputs) */
  /********************************/

  if (pins_shared)
  {
    /* GPC12 */
    rutl_regwrite((1 << 12), (0 << 12), GPCUP) ; // Disable pullup
    rutl_regwrite((0 << 12), (1 << 12), GPCDAT); // Set data low
    rutl_regwrite((1 << 24), (3 << 24), GPCCON); // Set as output
    /* GPC13 */
    rutl_regwrite((1 << 13), (0 << 13), GPCUP) ; // Disable pullup
    rutl_regwrite((0 << 13), (1 << 13), GPCDAT); // Set data low
    rutl_regwrite((1 << 26), (3 << 26), GPCCON); // Set as output
    /* GPC14 */
    rutl_regwrite((1 << 14), (0 << 14), GPCUP) ; // Disable pullup
    rutl_regwrite((0 << 14), (1 << 14), GPCDAT); // Set data low
    rutl_regwrite((1 << 28), (3 << 28), GPCCON); // Set as output

    if (register_pin_usage)  
      rgpio_register("GPC12,13,14 (output)", acModuleName);
  }
  else
  {
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
    rutl_regwrite((0 << 10), (1 << 10), GPDDAT);   // Set data low
    rutl_regwrite((1 << 20), (3 << 20), GPDCON); // Set as output

    if (register_pin_usage)  
      rgpio_register("GPD8,9,10 (output)", acModuleName);
  }

  switch(keypad_config)
  {
    /* 4 rows(GPG8,9,10,11), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
      num_cols = 3;
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPB9) */
    case KEY_CONFIG_4x4_1: 
      if (pins_shared)
      {
        if (register_pin_usage)  
          rgpio_register("GPC15 (output)", acModuleName);

        rutl_regwrite((1 << 15), (0 << 15), GPCUP) ; // Disable pullup
        rutl_regwrite((0 << 15), (1 << 15), GPCDAT); // Set data low
        rutl_regwrite((1 << 30), (3 << 30), GPCCON); // Set as output
      }
      else
      {
        if (register_pin_usage)  
          rgpio_register("GPB9 (output)", acModuleName);

        rutl_regwrite((1 << 9), (0 << 9), GPBUP) ;   // Disable pullup
        rutl_regwrite((0 << 9), (1 << 9), GPBDAT);   // Set data low
        rutl_regwrite((1 << 18), (3 << 18), GPBCON); // Set as output
      }
      num_cols = 4;
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG5) */
    case KEY_CONFIG_4x4_2: 
      if (pins_shared)
      {
        if (register_pin_usage)  
          rgpio_register("GPC15 (output)", acModuleName);

        rutl_regwrite((1 << 15), (0 << 15), GPCUP) ; // Disable pullup
        rutl_regwrite((0 << 15), (1 << 15), GPCDAT); // Set data low
        rutl_regwrite((1 << 30), (3 << 30), GPCCON); // Set as output
      }
      else
      {
        if (register_pin_usage)  
          rgpio_register("GPG5 (output)", acModuleName);

        rutl_regwrite((1 << 5), (0 << 5), GPGUP) ;   // Disable pullup
        rutl_regwrite((0 << 5), (1 << 5), GPGDAT);   // Set data low
        rutl_regwrite((1 << 10), (3 << 10), GPGCON); // Set as output
      }
      num_cols = 4;
      break;
  }
}

static void setup_gpio_input(int register_pin_usage)
{
  /* Set up the rows (inputs) */
  /****************************/

  if (register_pin_usage)  
    rgpio_register("GPG8,9,10 (input)", acModuleName);

  /* GPG8 */
  rutl_regwrite((0 << 8), (1 << 8), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 16), (3 << 16), GPGCON); // EINTn
  /* GPG9 */
  rutl_regwrite((0 << 9), (1 << 9), GPGUP) ;   // Enable pullup
  rutl_regwrite((2 << 18), (3 << 18), GPGCON); // EINTn
  /* GPG10 */
  rutl_regwrite((0 << 10), (1 << 10), GPGUP) ; // Enable pullup
  rutl_regwrite((2 << 20), (3 << 20), GPGCON); // EINTn

  if (keypad_config != KEY_CONFIG_3x3)
  {
    if (register_pin_usage)  
      rgpio_register("GPG11 (input)", acModuleName);

    /* GPG11 */
    rutl_regwrite((0 << 11), (1 << 11), GPGUP) ; // Enable pullup
    rutl_regwrite((2 << 22), (3 << 22), GPGCON); // EINTn
  }
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_keypad_init(void)
{
  printk(PFX "%s module: loaded (keypad_config=%d)\n", acModuleName, 
                                                       keypad_config);

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);

  if (keypad_config == KEY_CONFIG_DUMMY)
    return 0;

  /* Intialise static data */
  state = IDLE;
  
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
  set_bit (BTN_Z+19, input_dev->keybit);
  set_bit (BTN_Z+20, input_dev->keybit);
  set_bit (BTN_Z+21, input_dev->keybit);
  set_bit (BTN_Z+22, input_dev->keybit);
  set_bit (BTN_Z+23, input_dev->keybit);
  set_bit (BTN_Z+24, input_dev->keybit);
  set_bit (BTN_Z+25, input_dev->keybit);
  set_bit (BTN_Z+26, input_dev->keybit);
  set_bit (BTN_Z+27, input_dev->keybit);
  input_dev->name = "Reciva Generic Keypad";

  input_register_device (input_dev);

  /* Initialise timers */
  init_keypad_timers();

  /* Initialise GPIOs */
  set_columns(0);
  setup_gpio_direction(1);

  /* Set up number of columns */
  switch(keypad_config)
  {
    /* 4 rows(GPG8,9,10,11), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
      num_cols = 3;
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPB9) */
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
      num_cols = 4;
      break;
  }
  /* Set up number of rows */
  switch(keypad_config)
  {
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
      num_rows = 3;
      break;

    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
    case KEY_CONFIG_DUMMY:
      num_rows = 4;
      break;
  }

  /* Set the length of filter for external interrupt
   * Filter clock = PCLK
   * Filter width = 0x7f (max) */
  rutl_regwrite(0x7f7f7f7f, 0xffffffff, EINTFLT2);

  /* Set EINT16 - EINT19 as falling edge level triggerred interrupts - filter
   * on */
  if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
    // Set EINT19 for interrupts on both edges
    rutl_regwrite(0x0000eaaa, 0x0000ffff, EXTINT2);
  else
    rutl_regwrite(0x0000aaaa, 0x0000ffff, EXTINT2);


  /* Request interrupts */
  /* Set power_on_key_state so that any depressed keys are detected */
  power_on_key_state = POKS_ALLOWED_TO_FIND;
  interrupt_ran = 0;
  clear_pending_mask = (1 << 16) | (1 << 17) | (1 << 18);
  request_irq(IRQ_EINT16, reciva_gpio_int_handler, 0, "EINT16 (k0)", (void *)8);
  request_irq(IRQ_EINT17, reciva_gpio_int_handler, 0, "EINT17 (k1)", (void *)9);
  request_irq(IRQ_EINT18, reciva_gpio_int_handler, 0, "EINT18 (k2)", (void *)10);
  if (num_rows > 3)
  {
    clear_pending_mask |= (1 << 19);
    request_irq(IRQ_EINT19, reciva_gpio_int_handler, 0, "EINT19 (k3)", (void *)11);
  }
  else if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
  {
    clear_pending_mask |= (1 << 19);
    request_irq(IRQ_EINT19, reciva_gpio_standalone_int_handler, 0, "EINT19 (k3)", NULL);
  }

  // Check for an initial key press
  check_for_keypress();
  // If no inital key was found, stop looking for power-on key.
  // There is a race condition here if the interrupt fires whilest we're
  // setting power_on_key_state, but it doesn't matter as we'll be ignoring
  // a key that was pressed after the module powered on.
  if (power_on_key_state == POKS_ALLOWED_TO_FIND)
  {
    power_on_key_state = POKS_NONE;
  }

  return 0;
}

/****************************************************************************
 * Module exit
 ****************************************************************************/
static void __exit 
reciva_keypad_exit(void)
{
  printk(PFX "Reciva Generic Keypad module: unloaded\n");

  misc_deregister (&reciva_miscdev);

  if (keypad_config == KEY_CONFIG_DUMMY)
    return;

  free_irq(IRQ_EINT16, (void *)8);
  free_irq(IRQ_EINT17, (void *)9);
  free_irq(IRQ_EINT18, (void *)10);
  if (num_rows > 3)
    free_irq(IRQ_EINT19, (void *)11);
  else if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
    free_irq(IRQ_EINT19, NULL);
    
  delete_keypad_timers();

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
reciva_keypad_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
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

        return 0;
      } 
      return -ENXIO;

    /* A list of alternate functions for the keys */
    case IOC_KEY_GET_ALT_FN:
      if (copy_from_user(&piTemp, (void *)arg, sizeof(piTemp)))
        return -EFAULT;

      if (driver && driver->alt_key_functions)
      { 
        if (copy_to_user(piTemp, driver->alt_key_functions(), sizeof(int)*RKD_MAX_KEYS))
          return -EFAULT;

        return 0;
      }
      return -ENXIO;

    /* Check if a key was held down on power-on and is still down */
    case IOC_KEY_GET_POWER_ON_KEY:
      if (copy_from_user(&piTemp, (void *)arg, sizeof(piTemp)))
        return -EFAULT;

      if (power_on_key_state == POKS_FOUND)
      { 
        int power_on_key = decode_key_press(power_on_row, power_on_col);
        if (copy_to_user(piTemp, &power_on_key, sizeof(int)))
          return -EFAULT;

        // Reset so that key is only read once
        power_on_key_state = POKS_NONE;
        return 0;
      }
      return -EPERM;

    default:
      break;
  }

  if (driver && driver->ioctl_extension_hook)
    return driver->ioctl_extension_hook (inode, file, cmd, arg);

  return -ENODEV;
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

  DEBUG("%s row %d column %d \n", __FUNCTION__, row, column);

  if (driver && driver->decode_key_press)
    key_pressed = driver->decode_key_press(row, column);

  return key_pressed;
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
 * Debouncing timeout for standalone GPIOs
 ****************************************************************************/
static void standalone_debounce(unsigned long ignore)
{
  int state;
  unsigned long flags;

  spin_lock_irqsave(&standalone_access_lock, flags);

  enable_irq(IRQ_EINT19);
  standalone_idle = 1;

  state = (read_input () & (1 << 11)) ? 1 : 0;
  if (state != standalone_key_state)
  {
    standalone_key_state = state;
    key = decode_key_press (8, 3);
    input_report_key (input_dev, key, state);
  }

  spin_unlock_irqrestore(&standalone_access_lock, flags);
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 ****************************************************************************/
static void reciva_gpio_standalone_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  int state = (read_input () & (1 << 11)) ? 1 : 0;
  
  if (state != standalone_key_state)
  {
    disable_irq(IRQ_EINT19);
    standalone_idle = 0;
    standalone_key_state = state;
    restart_standalone_timer ();

    key = decode_key_press (8, 3);
    input_report_key (input_dev, key, state);
  }
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 **Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
static void reciva_gpio_int_handler(int irq, void *dev, struct pt_regs *regs)
{
  DEBUG("GPIOint %d\n", (int)dev);
  interrupt_ran = 1;
  if (state == IDLE)
  {
    if (power_on_key_state == POKS_ALLOWED_TO_FIND)
    {
      power_on_key_state = POKS_GOT_INTERRUPT;
    }
    gpio = (int)dev;
    gp_state_machine(0);

  }
}

/****************************************************************************
 ** Sets the column levels
 ***************************************************************************/
static void set_columns(int c)
{
  unsigned int con = 0;
  int base = pins_shared ? 24 : 16;
  if ((c & 1) == 0)
    con |= 1 << base;
  if ((c & 2) == 0)
    con |= 1 << (base + 2);
  if ((c & 4) == 0)
    con |= 1 << (base + 4);
  if (pins_shared)
  {
    rutl_regwrite(con, (3 << 28 | 3 << 26 | 3 << 24), GPCCON);
  }
  else
  {
    rutl_regwrite(con, (3 << 20 | 3 << 18 | 3 << 16), GPDCON);
  }

  con = ((c & 8) == 0) ? 1 : 0;
  switch(keypad_config)
  {
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_DUMMY:
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
      break;

    case KEY_CONFIG_4x4_1: 
      if (pins_shared)
      {
        rutl_regwrite((con << 30), (3 << 30), GPCCON); // Set as output
      }
      else
      {
        rutl_regwrite((con << 18), (3 << 18), GPBCON); // Set as output
      }
      break;

    case KEY_CONFIG_4x4_2: 
      if (pins_shared)
      {
        rutl_regwrite((con << 30), (3 <<30), GPCCON); // Set as output
      }
      else
      {
        rutl_regwrite((con << 18), (3 << 18), GPBCON); // Set as output
      }
      break;
  }

  /* Store column levels so that we can restore state following 
   * an interruption from external module (eg LCD) */
  column_levels = c;
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
  /* Stop external modules claiming the GPIO lines */
  spin_lock(&access_lock);

  static int print_state = 1;
#ifdef RKG_DEBUG
  if (print_state)
  {
    DEBUG("%s %d\n", __FUNCTION__, state);
    print_state = 0;
  }
#endif

  int i;
  static int count;                   /* Release Pending timeout count */
  static int release_detect_count;    /* Used to ensure we don't wrongly
                                       * detect a key release */
  int keypress_detected = 0;
                                                                
  switch (state)
  {
    case IDLE :
      /* Disable interrupt */
      disable_interrupts();

      /* Set timer */
      restart_timer();

      /* Set up state machine data and change state */
      column = num_cols - 1;
      print_state = 1;
      state = IDENTIFY_BUTTON;

      /* Drive first column low */
      set_columns (~(1 << column));
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
              // Store power-on key if we're looking
              if (power_on_key_state == POKS_GOT_INTERRUPT)
              {
                power_on_row = gpio;
                power_on_col = column;
                power_on_key_state = POKS_FOUND;
              }

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
      restart_timer();

      /* Don't bother with RELEASE_PENDING if we didn't detect a key press */
      count = 0;
      release_detect_count = 0;
      print_state = 1;
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
        print_state = 1;
        state = RELEASE_DETECTED;
      }  

      /* Set timer */
      restart_timer();
      break;

    case RELEASE_DETECTED :
      /* Back to IDLE */
      print_state = 1;
      state = IDLE;
      power_on_key_state = POKS_NONE;

      /* Re-enable interrupts */
      enable_interrupts();
      input_report_key (input_dev, key, 0);
#ifdef KERNEL_26
      input_sync (input_dev);
#endif
      break;

    default :
      printk(PFX "gp_state_machine : error unknown state %d\n", state);
      print_state = 1;
      state = IDLE;
      break;
  }

  /* Allow external modules to claim the GPIO lines */
  spin_unlock(&access_lock);
}

EXPORT_SYMBOL(rkg_register);
EXPORT_SYMBOL(rkg_unregister);
EXPORT_SYMBOL(rkg_access_request);
EXPORT_SYMBOL(rkg_access_release);

module_init(reciva_keypad_init);
module_exit(reciva_keypad_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Generic Keypad");
MODULE_LICENSE("GPL");

