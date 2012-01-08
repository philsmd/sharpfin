/*
 * linux/reciva/reciva_keypad_generic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004-2007 Reciva Ltd. All Rights Reserved
 * 
 * Reciva Keypad driver
 * Columns - outputs
 * Rows - inputs
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
#include <linux/version.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

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
 * KEY_CONFIG_3x3_PLUS_1   = 3 rows   : GPG8,9,10
 *                    3 columns: GPD8,9,10
 *                    standalone key J2-11 (GPG11) - high = key pressed
 * KEY_CONFIG_4x4_3 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPG7
 * KEY_CONFIG_4x3_2 = 4 rows   : GPC 11:8
 *                    3 columns: GPD 10:8
 * KEY_CONFIG_5x3   = 5 rows   : GPG8,9,10,11,GPG3
 *                    3 columns: GPD8,9,10
 * KEY_CONFIG_4x5   = 4 rows   : GPG8,9,10,11
 *                    5 columns: GPD8,9,10 GPE3 GPB0
 * KEY_CONFIG_4x4_4 = 4 rows   : GPG8,9,10,11    
 *                    4 columns: GPD8,9,10 GPF4 (J2-25)
 * KEY_CONFIG_5x4_1  = 5 rows   : GPG8,9,10,11,GPG3
 *                    4 columns: GPD8,9,10 GPB9
 * KEY_CONFIG_4x2_1  = 4 rows   : GPG8,9,10,11,GPG3
 *                    2 columns: GPD8,9
 * KEY_CONFIG_4x4_5 = 4 rows   : GPG8,9,10,11
 *                    4 columns: GPD8,9,10 GPE3 (J1-12)
 * KEY_CONFIG_4x1   = 4 rows   : GPG8,9,10,11
 *                    1 column : GPD10
 *                    
 *                    
 * If pins_shared=1, columns are GPC[12:15] in all cases.
 */
typedef enum
{
  KEY_CONFIG_4x3                        = 0,
  KEY_CONFIG_4x4_1                      = 1,
  KEY_CONFIG_4x4_2                      = 2,
  KEY_CONFIG_DUMMY                      = 3,
  KEY_CONFIG_3x3			= 4,
  KEY_CONFIG_3x3_PLUS_1			= 5,
  KEY_CONFIG_4x4_3                      = 6,
  KEY_CONFIG_4x3_2                      = 7,
  KEY_CONFIG_5x3                        = 8,
  KEY_CONFIG_4x5                        = 9,
  KEY_CONFIG_4x4_4                      = 10,
  KEY_CONFIG_5x4_1                      = 11,
  KEY_CONFIG_4x2_1                      = 12,
  KEY_CONFIG_4x4_5                      = 13,
  KEY_CONFIG_4x1                        = 14
} key_config_t;
static key_config_t keypad_config = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  module_param(keypad_config, uint, S_IRUGO);
#else
  MODULE_PARM(keypad_config, "i");
#endif

static int pins_shared;
static int polled;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  module_param(pins_shared, int, S_IRUGO);
  module_param(polled, int, S_IRUGO);
#else
  MODULE_PARM(pins_shared, "i");
  MODULE_PARM(polled, "i");
#endif

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define INT_HANDLER_DECL(x) static irqreturn_t x (int irq, void *dev)
#else
#define INT_HANDLER_DECL(x) static void x (int irq, void *dev, struct pt_regs *regs)
#endif

INT_HANDLER_DECL(reciva_gpio_int_handler);
INT_HANDLER_DECL(reciva_gpio_standalone_int_handler);

static void gp_state_machine(unsigned long data);
static void polled_state_machine(unsigned long data);
static void standalone_debounce(unsigned long data);
static int decode_key_press(int row, int column);
static int reverse_lookup_keycode(int code, int *irow, int *icolumn);
static int reciva_keypad_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static void set_columns(int c);
static void setup_gpio_direction(void);
static void setup_gpio_output(void);
static void setup_gpio_input(void);
static void gpio_setpin_buffered(unsigned int pin, unsigned int to);
static int read_input(void);
static void check_for_keypress(void);
static void enable_interrupts(void);
static void disable_interrupts(void);
static void setup_row_pins(void);
static void set_rows_to(int pin_function);

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

//FIXME Really should use the standard kernel header files for these definitions
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  #define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C24XX_VA_GPIO + (x)))
#else
  #define GPIO_REG(x) (unsigned int)((volatile unsigned int *)(S3C2410_VA_GPIO + (x)))
#endif

/* GPIO port C */
#define GPCCON GPIO_REG(0x20)
#define GPCDAT GPIO_REG(0x24)
#define GPCUP GPIO_REG(0x28)
/* GPIO port G */
#define GPGCON	GPIO_REG(0x60)
#define GPGDAT	GPIO_REG(0x64)
#define GPGUP   GPIO_REG(0x68)

#define EXTINT1 GPIO_REG(0x8C)
#define EXTINT1_RIRM3 GPIO_REG(0x9C)
#define EXTINT2 GPIO_REG(0x90)
#define EXTINT2_RIRM3 GPIO_REG(0xA0)

#define EXTINTPEND GPIO_REG(0xA8)
#define EXTINTPEND_RIRM3 GPIO_REG(0xB8)

/* External interrupt filter */
#define EINTFLT2 GPIO_REG(0x9c)
#define EINTFLT2_RIRM3 GPIO_REG(0xac)

/* States */
#define IDLE                 0
#define IDENTIFY_BUTTON      1
#define RELEASE_PENDING      2
#define CHECK_FOR_KEY_CHANGE 3
#define RELEASE_DETECTED     4

/* Timeouts */
#define DEBOUNCE_TIMEOUT        ((HZ*20)/1000) /* 20 ms */
#define POLL_TIMEOUT            ((HZ*20)/1000) /* 20 ms */   

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static int state;                /* State */
static int gpio;                 /* GPIO number */
static int column;               /* Column currently being driven to determine
                                  * which button was pressed */
static struct timer_list timer;  /* Timer used for scheduling events */
static struct timer_list standalone_timer;  /* Debounce timer for standalone keys */
static struct timer_list poll_timer;
static int standalone_key_state;
static int standalone_idle = 1;
static int key;
static struct input_dev *input_dev;
static const reciva_keypad_driver_t *driver = NULL;
static int num_cols, num_rows;
static unsigned char old_state[RKD_NUM_ROWS][RKD_NUM_COLS];

static int col_pins[RKD_NUM_COLS];
static int row_pins[RKD_NUM_ROWS];

static char acModuleName[] = "Reciva Generic Keypad";

/* Controls access to the GPIO pins normally used by this module 
 * (on some hardware configs the LCD and Keypad control lines are shared) */
static spinlock_t access_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t standalone_access_lock = SPIN_LOCK_UNLOCKED;

/* Indicates if interrupt has fired 
 * Used to prevent race in check_for_keypress after external module access */
static int interrupt_ran;

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
  POKS_GOT_INTERRUPT,   // was key on at init, detecting key
  POKS_FOUND,           // detected key on
} PowerOnKeyState_t;
static PowerOnKeyState_t power_on_key_state = POKS_NONE;
static int power_on_row;
static int power_on_col;
static long power_on_time;

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

        if (polled) {
                init_timer (&poll_timer);
                poll_timer.function = polled_state_machine;
        }
}

/****************************************************************************
 * Delete timers
 ****************************************************************************/
static void delete_keypad_timers(void)
{
        if (polled)
                del_timer (&poll_timer);
        del_timer (&timer);
        del_timer (&standalone_timer);
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
    /* Set up GPIO direction */
    setup_gpio_output();

    /* Restore column levels */
    set_columns(column_levels);

    /* delay before reading back GPIO level to allow for board capacitance */
    // TODO This could be too conservative - need to recalculate/measure
    udelay(1000);

    // Clear pending flags to ignore spurious interrupts
    __raw_writel(clear_pending_mask,
                 machine_is_rirm2() ? EXTINTPEND: EXTINTPEND_RIRM3);

    // Resumme matrix keypress state machine
    switch (state)
    {
      case IDLE:
        interrupt_ran = 0;
        enable_interrupts();
        if (!polled)
                check_for_keypress();
        break;
      case IDENTIFY_BUTTON:
      case RELEASE_PENDING:
      case CHECK_FOR_KEY_CHANGE:
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

/****************************************************************************
 * Report key press to user space
 ****************************************************************************/
void rkg_report_key(int key, int state)
{
  input_report_key (input_dev, key, state);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  input_sync (input_dev);
#endif
}

static void enable_interrupts()
{
  if (polled)
  {
    mod_timer (&poll_timer, jiffies + POLL_TIMEOUT);
    return;
  }

  enable_irq(IRQ_EINT16);
  enable_irq(IRQ_EINT17);
  enable_irq(IRQ_EINT18);
  if (num_rows > 3)
  {
    enable_irq(IRQ_EINT19);
  }
  if (num_rows > 4)
    enable_irq (IRQ_EINT11);
}

static void disable_interrupts()
{
  if (polled)
  {
    del_timer (&poll_timer);
    return;
  }

  disable_irq(IRQ_EINT16);
  disable_irq(IRQ_EINT17);
  disable_irq(IRQ_EINT18);
  if (num_rows > 3)
  {
    disable_irq(IRQ_EINT19);
  }
  if (num_rows > 4)
    disable_irq (IRQ_EINT11);
}

/****************************************************************************
 * Check for evidence of a keypress while the LCD is in control
 ****************************************************************************/
static void check_for_keypress(void)
{
  DEBUG("%s\n", __FUNCTION__);

  int input_mask = (1<<8)|(1<<9)|(1<<10);

  // XXX fixme, this will lose if 3x3+1 is used with shared gpio
  if (keypad_config != KEY_CONFIG_3x3 && keypad_config != KEY_CONFIG_3x3_PLUS_1)
  {
    input_mask |= (1<<11);
  }

  // Ensure all row pins are set up correctly before attempting to read from them
  // Stingray won't let you read the pin levels if still set to EINT
  set_rows_to(S3C2410_GPIO_INPUT);

  // Switch is active low, but invert input to look for a high
  int input_levels = (~read_input() & input_mask) >> 8;
  DEBUG("  input_levels=0x%08x\n", input_levels);

  // Set rows back to EINT
  set_rows_to(S3C2410_GPIO_IRQ);

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
      reciva_gpio_int_handler(0, (void *)interrupt_number);
#else
      reciva_gpio_int_handler(0, (void *)interrupt_number, NULL);
#endif
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
static void setup_gpio_direction(void)
{
  setup_gpio_output();
  setup_gpio_input();
}

/****************************************************************************
 * Set up the columns (outputs)
 ****************************************************************************/
static void setup_gpio_output(void)
{
  // Stingray has pulldowns instead of pullups, so need to set the data
  // lines high to cause an edge (low to high)
  int d = machine_is_rirm2() ? 0 : 1;

  if (pins_shared)
  {
    col_pins[0] = S3C2410_GPC12;
    col_pins[1] = S3C2410_GPC13;
    col_pins[2] = S3C2410_GPC14;
  }
  else
  {
    col_pins[0] = S3C2410_GPD8;
    col_pins[1] = S3C2410_GPD9;
    col_pins[2] = S3C2410_GPD10;
  }

  num_cols = 3;

  switch(keypad_config)
  {
    /* 4 rows(GPG8,9,10,11), 2 columns(GPD8,9) */
    case KEY_CONFIG_4x2_1:
      num_cols = 2;
      break;

    /* 4 rows(GPG8,9,10,11), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
    case KEY_CONFIG_5x3:
    case KEY_CONFIG_4x3_2:
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPB9) */
    case KEY_CONFIG_4x4_1: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
        col_pins[num_cols++] = S3C2410_GPB9;
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG5) */
    case KEY_CONFIG_4x4_2: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
      {
        if (machine_is_rirm3())
          col_pins[num_cols++] = S3C2410_GPG0;
        else
          col_pins[num_cols++] = S3C2410_GPG5;
      }
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG7) */
    case KEY_CONFIG_4x4_3: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
      {
        if (machine_is_rirm3())
          col_pins[num_cols++] = S3C2410_GPG2;
        else
          col_pins[num_cols++] = S3C2410_GPG7;
      }
      break;

    /* 4 rows (GPG8,9,10,11), 5 colums (GPD8,9,10 GPE3 GPB0) */
    case KEY_CONFIG_4x5:
      if (pins_shared) {
        col_pins[num_cols++] = S3C2410_GPC15;
        col_pins[num_cols++] = S3C2410_GPC11;
      } else {
        col_pins[num_cols++] = S3C2410_GPE3;
        col_pins[num_cols++] = S3C2410_GPB0;
      }
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPC11) */
    case KEY_CONFIG_4x4_4: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
        col_pins[num_cols++] = S3C2410_GPF4;
      break;

    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPE3) */
    case KEY_CONFIG_4x4_5: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
        col_pins[num_cols++] = S3C2410_GPE3;
      break;

    /* 5 rows   : GPG8,9,10,11,GPG3
     * 4 columns: GPD8,9,10 GPB9 */
    case KEY_CONFIG_5x4_1: 
      if (pins_shared)
        col_pins[num_cols++] = S3C2410_GPC15;
      else
        col_pins[num_cols++] = S3C2410_GPB9;
      break;

    case KEY_CONFIG_4x1: 
      col_pins[0] = S3C2410_GPD10;
      num_cols = 1;
      break;
  }

  int i;
  for (i = 0; i < num_cols; i++) {
    s3c2410_gpio_pullup (col_pins[i], 1);
    gpio_setpin_buffered (col_pins[i], d);
    s3c2410_gpio_cfgpin (col_pins[i], S3C2410_GPIO_OUTPUT);
  }
}

/****************************************************************************
 * Setup row_pins array and num_rows
 ****************************************************************************/
static void setup_row_pins(void)
{
  num_rows = 0;

  /* GPG 8/9/10 */
  switch(keypad_config)
  {
    /* 5 rows(GPG8,9,10,11,3) */
    case KEY_CONFIG_5x3:
    case KEY_CONFIG_5x4_1:
      row_pins[num_rows++] = S3C2410_GPG3;
      // fall through
    /* 4 rows (GPG8,9,10,11) */
    case KEY_CONFIG_4x2_1:
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_DUMMY:
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
    case KEY_CONFIG_4x4_3: 
    case KEY_CONFIG_4x4_4: 
    case KEY_CONFIG_4x4_5: 
    case KEY_CONFIG_4x5:
    case KEY_CONFIG_4x1: 
      row_pins[num_rows++] = S3C2410_GPG11;
      // fall through
    /* 3 rows (GPG8,9,10) */
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
      row_pins[num_rows++] = S3C2410_GPG8;
      row_pins[num_rows++] = S3C2410_GPG9;
      row_pins[num_rows++] = S3C2410_GPG10;
      break;
    case KEY_CONFIG_4x3_2: 
      row_pins[num_rows++] = S3C2410_GPC8;
      row_pins[num_rows++] = S3C2410_GPC9;
      row_pins[num_rows++] = S3C2410_GPC10;
      row_pins[num_rows++] = S3C2410_GPC11;
      break;
  }
  printk("num_rows = %d\n", num_rows);
}

/****************************************************************************
 * Set gpio function of all row pins to pin_function
 ****************************************************************************/
static void set_rows_to(int pin_function)
{
  unsigned long flags;
  local_irq_save(flags);

  int i;
  for (i=0; i<num_rows; i++)
    s3c2410_gpio_cfgpin (row_pins[i], pin_function);

  local_irq_restore(flags);
}

/****************************************************************************
 * Set up the rows (inputs)
 ****************************************************************************/
static void setup_gpio_input(void)
{
  /* GPG 8/9/10 */
  switch(keypad_config)
  {
    /* 5 rows(GPG8,9,10,11,3) */
    case KEY_CONFIG_5x3:
    case KEY_CONFIG_5x4_1:
      /* GPG3 */
      rutl_regwrite((0 << 3), (1 << 3), GPGUP) ; // Enable pullup
      rutl_regwrite((2 << 6), (3 << 6), GPGCON); // EINTn
      // fall through
    /* 4 rows (GPG8,9,10,11) */
    case KEY_CONFIG_4x2_1:
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
    case KEY_CONFIG_4x4_3: 
    case KEY_CONFIG_4x4_4: 
    case KEY_CONFIG_4x4_5: 
    case KEY_CONFIG_4x5:
    case KEY_CONFIG_4x1: 
      /* GPG8 */
      rutl_regwrite((0 << 8), (1 << 8), GPGUP) ;   // Enable pullup
      rutl_regwrite((2 << 16), (3 << 16), GPGCON); // EINTn
      /* GPG9 */
      rutl_regwrite((0 << 9), (1 << 9), GPGUP) ;   // Enable pullup
      rutl_regwrite((2 << 18), (3 << 18), GPGCON); // EINTn
      /* GPG10 */
      rutl_regwrite((0 << 10), (1 << 10), GPGUP) ; // Enable pullup
      rutl_regwrite((2 << 20), (3 << 20), GPGCON); // EINTn
      break;
    case KEY_CONFIG_4x3_2: 
      /* GPC8 */
      rutl_regwrite((0 << 8), (1 << 8), GPCUP) ;   // Enable pullup
      rutl_regwrite((2 << 16), (3 << 16), GPCCON); // EINTn
      /* GPC9 */
      rutl_regwrite((0 << 9), (1 << 9), GPCUP) ;   // Enable pullup
      rutl_regwrite((2 << 18), (3 << 18), GPCCON); // EINTn
      /* GPC10 */
      rutl_regwrite((0 << 10), (1 << 10), GPCUP) ; // Enable pullup
      rutl_regwrite((2 << 20), (3 << 20), GPCCON); // EINTn
      break;
  }

  /* GPG 11 */
  switch(keypad_config)
  {
    case KEY_CONFIG_3x3:
      break;

    /* 4 rows(GPG8,9,10,11), 2 columns(GPD8,9) */
    case KEY_CONFIG_4x2_1:
    /* 4 rows(GPG8,9,10,11), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPB9) */
    case KEY_CONFIG_4x4_1: 
    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG5) */
    case KEY_CONFIG_4x4_2: 
    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPG7) */
    case KEY_CONFIG_4x4_3: 
    /* 4 rows(GPG8,9,10,11), 4 columns(GPD8,9,10 GPF4) */
    case KEY_CONFIG_4x4_4: 
    case KEY_CONFIG_4x4_5: 
    /* 5 rows(GPG8,9,10,11,3), 3 columns(GPD8,9,10) */
    case KEY_CONFIG_5x3:
    case KEY_CONFIG_4x5:
    case KEY_CONFIG_5x4_1:
    case KEY_CONFIG_4x1: 
      /* GPG11 */
      rutl_regwrite((0 << 11), (1 << 11), GPGUP) ; // Enable pullup
      rutl_regwrite((2 << 22), (3 << 22), GPGCON); // EINTn
      break;
    case KEY_CONFIG_4x3_2: 
      /* GPC11 */
      rutl_regwrite((0 << 11), (1 << 11), GPCUP) ; // Enable pullup
      rutl_regwrite((2 << 22), (3 << 22), GPCCON); // EINTn
      break;
  }
}

/****************************************************************************
 * Set up the input system - for reporting events to user space
 ****************************************************************************/
static void setup_input_system(void)
{
  /* Set up input system - for reporting events to user space */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  input_dev = input_allocate_device();
#else
  input_dev = kmalloc (sizeof (*input_dev), GFP_KERNEL);
  memset (input_dev, 0, sizeof (*input_dev));
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
  set_bit (BTN_Z+28, input_dev->keybit);
  set_bit (BTN_Z+29, input_dev->keybit);
  set_bit (BTN_Z+30, input_dev->keybit);
  set_bit (BTN_Z+31, input_dev->keybit);
  set_bit (BTN_Z+32, input_dev->keybit);
  set_bit (BTN_Z+33, input_dev->keybit);
  set_bit (BTN_Z+34, input_dev->keybit);
  set_bit (BTN_Z+35, input_dev->keybit);
  set_bit (BTN_Z+36, input_dev->keybit);
  set_bit (BTN_Z+37, input_dev->keybit);
  set_bit (BTN_Z+38, input_dev->keybit);
  set_bit (BTN_Z+39, input_dev->keybit);
  set_bit (BTN_Z+40, input_dev->keybit);
  set_bit (BTN_Z+41, input_dev->keybit);
  set_bit (BTN_Z+42, input_dev->keybit);
  set_bit (BTN_Z+43, input_dev->keybit);
  set_bit (BTN_Z+44, input_dev->keybit);
  set_bit (BTN_Z+45, input_dev->keybit);
  set_bit (BTN_Z+46, input_dev->keybit);
  set_bit (BTN_Z+47, input_dev->keybit);
  set_bit (BTN_Z+48, input_dev->keybit);
  set_bit (BTN_Z+49, input_dev->keybit);
  set_bit (BTN_Z+50, input_dev->keybit);
  set_bit (BTN_Z+51, input_dev->keybit);
  set_bit (BTN_Z+52, input_dev->keybit);
  set_bit (BTN_Z+53, input_dev->keybit);
  set_bit (BTN_Z+54, input_dev->keybit);
  set_bit (BTN_Z+55, input_dev->keybit);
  set_bit (BTN_Z+56, input_dev->keybit);
  set_bit (BTN_Z+57, input_dev->keybit);
  set_bit (BTN_Z+58, input_dev->keybit);
  set_bit (BTN_Z+59, input_dev->keybit);
  set_bit (BTN_Z+60, input_dev->keybit);
  set_bit (BTN_Z+61, input_dev->keybit);
  set_bit (BTN_Z+62, input_dev->keybit);
  set_bit (BTN_Z+63, input_dev->keybit);
  set_bit (BTN_Z+64, input_dev->keybit);
  set_bit (BTN_Z+65, input_dev->keybit);
  set_bit (BTN_Z+66, input_dev->keybit);
  set_bit (BTN_Z+67, input_dev->keybit);
  set_bit (BTN_Z+68, input_dev->keybit);
  set_bit (BTN_Z+69, input_dev->keybit);
  set_bit (BTN_Z+70, input_dev->keybit);
  set_bit (BTN_Z+71, input_dev->keybit);
  set_bit (BTN_Z+72, input_dev->keybit);
  set_bit (BTN_Z+73, input_dev->keybit);
  set_bit (BTN_Z+74, input_dev->keybit);
  set_bit (BTN_Z+75, input_dev->keybit);
  set_bit (BTN_Z+76, input_dev->keybit);
  input_dev->name = "Reciva Generic Keypad";

  input_register_device (input_dev);
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_keypad_init(void)
{
  printk(PFX "%s module: loaded (keypad_config=%d)\n", acModuleName, 
                                                       keypad_config);

  power_on_time = jiffies;

  /* Register the IOCTL */
  misc_register (&reciva_miscdev);

  /* Set up input system - for reporting events to user space */
  setup_input_system();

  if (keypad_config == KEY_CONFIG_DUMMY)
    return 0;

  /* Intialise static data */
  state = IDLE;
  
  /* Initialise timers */
  init_keypad_timers();

  /* Initialise GPIOs */
  set_columns(0);
  setup_row_pins();
  setup_gpio_direction();              // sets num_cols

  /* Set power_on_key_state so that any depressed keys will be detected */
  power_on_key_state = POKS_NONE;

  if (!polled) {
          if (machine_is_rirm2()) {
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

                  // Clear any pending interrupts
                  __raw_writel((1 << 16) | (1 << 17) | (1 << 18) | (1 << 19), EXTINTPEND);

                  if (num_rows > 4) {
                        rutl_regwrite (2 << 12, 7 << 12, EXTINT1);
                        __raw_writel (1 << 11, EXTINTPEND);
                  }
          } else {
                  // As above for Stingray (which uses rising edge trigger as the input pins
                  // have pulldowns as opposed to pullups)
                  rutl_regwrite(0x7f7f7f7f, 0xffffffff, EINTFLT2_RIRM3);
                  if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
                          rutl_regwrite(0x0000eccc, 0x0000ffff, EXTINT2_RIRM3);
                  else
                          rutl_regwrite(0x0000cccc, 0x0000ffff, EXTINT2_RIRM3);
                  // Clear any pending interrupts
                  __raw_writel((1 << 16) | (1 << 17) | (1 << 18) | (1 << 19), EXTINTPEND_RIRM3);

                  if (num_rows > 4) {
                        rutl_regwrite (0x4 << 12, 0xf << 12, EXTINT1_RIRM3);
                        __raw_writel (1 << 11, EXTINTPEND_RIRM3);
                  }
          }
          /* Request interrupts (clearing pedning flags to ignore transients) */
          interrupt_ran = 0;
          clear_pending_mask = (1 << 16) | (1 << 17) | (1 << 18);
          request_irq(IRQ_EINT16, reciva_gpio_int_handler, 0, "EINT16 (k0)", (void *)8);
          request_irq(IRQ_EINT17, reciva_gpio_int_handler, 0, "EINT17 (k1)", (void *)9);
          request_irq(IRQ_EINT18, reciva_gpio_int_handler, 0, "EINT18 (k2)", (void *)10);
          if (num_rows > 3) {
                  clear_pending_mask |= (1 << 19);
                  request_irq(IRQ_EINT19, reciva_gpio_int_handler, 0, "EINT19 (k3)", (void *)11);
          } else if (keypad_config == KEY_CONFIG_3x3_PLUS_1) {
                  clear_pending_mask |= (1 << 19);
                  request_irq(IRQ_EINT19, reciva_gpio_standalone_int_handler, 0, "EINT19 (k3)", NULL);
          }
          if (num_rows > 4)
          {
                  clear_pending_mask |= (1 << 11);
                  request_irq(IRQ_EINT11, reciva_gpio_int_handler, 0, "EINT11 (k4)", (void *)3);
          }

          // Check for an initial key press
          check_for_keypress();
  } else {
          enable_interrupts ();
  }

  return 0;
}

/****************************************************************************
 * Unregister the input device
 ****************************************************************************/
static void unregister_input_dev(void)
{
  input_unregister_device (input_dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#else
  kfree (input_dev);
#endif
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
  {
    unregister_input_dev();
    return;
  }

  if (!polled) {
          free_irq(IRQ_EINT16, (void *)8);
          free_irq(IRQ_EINT17, (void *)9);
          free_irq(IRQ_EINT18, (void *)10);
          if (num_rows > 3)
                  free_irq(IRQ_EINT19, (void *)11);
          if (num_rows > 4)
                  free_irq(IRQ_EINT11, (void *)12);
  }

  if (keypad_config == KEY_CONFIG_3x3_PLUS_1)
          free_irq(IRQ_EINT19, NULL);
    
  delete_keypad_timers();

  unregister_input_dev();
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Convert raw row into range 0 to max rows - 1
 ****************************************************************************/
static int convert_row_to_correct_range(int row)
{
  row = row - 8;        /* Convert to range 0 to 3 */
  if (row == -5)        /* Correct for GPG3 == ROW4 */
    row = 4;

  if (row < 0)
    row = 0;

  return row;
}

/****************************************************************************
 * Command handler
 * Parameters : Standard ioctl params
 * Returns : 0 == success, otherwise error code
 ****************************************************************************/
static int
reciva_keypad_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int *piTemp;
	int x;

	switch(cmd) {
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
		
	case IOC_KEY_GET_POWER_ON_ROW_COL:
		DEBUG(PFX "IOC_KEY_GET_POWER_ON_ROW_COL\n");
		if (copy_from_user(&piTemp, (void *)arg, sizeof(piTemp)))
			return -EFAULT;
		
		if (power_on_key_state == POKS_FOUND)
		{ 
			int row = convert_row_to_correct_range(power_on_row);
			int row_col = (row & 0xff) << 8;
			row_col |= (power_on_col & 0xff);
			DEBUG(PFX "row_col=%08x\n", row_col);
			if (copy_to_user(piTemp, &row_col, sizeof(int)))
				return -EFAULT;
			
			// Reset so that key is only read once
			power_on_key_state = POKS_NONE;
			return 0;
		}
		return -EPERM;
		
	case IOC_KEY_GET_STATE:
		if (get_user (x, (int *)arg))
			return -EFAULT;
		int row, column;
		if (reverse_lookup_keycode (x, &row, &column))
			return -EINVAL;
		DEBUG ("get state for %d/%d: %d\n", row, column, old_state[row][column]);
		if (put_user (old_state[row][column], (int *)arg))
			return -EFAULT;
		return 0;
		
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

  row = convert_row_to_correct_range(row);

  DEBUG("%s row %d column %d \n", __FUNCTION__, row, column);

  if ((row < 0) || (column < 0))
    return RKD_UNUSED;

  if (driver && driver->decode_key_press)
    key_pressed = driver->decode_key_press(row, column);

  return key_pressed;
}

/****************************************************************************
 * Convert a given keycode into a row and column number
 * returns 0 if ok, -1 if failed
 ****************************************************************************/
static int reverse_lookup_keycode(int code, int *irow, int *icolumn)
{
        if (!driver || !driver->decode_key_press)
                return -EOPNOTSUPP;

        int row, col;
        for (row = 0; row < num_rows; row++) {
                for (col = 0; col < num_cols; col++) {
                        if (driver->decode_key_press (row, col) == code)
                        {
                                *irow = row;
                                *icolumn = col;
                                return 0;
                        }
                }
        }

        return -EINVAL;
}

/****************************************************************************
 ** Read the status of the keypad rows
 ***************************************************************************/
static int read_input(void)
{
  // Leaving the bits we're interested in 11:8 for now. 
  // Might make more sense to change this to 3:0 at some stage
  //
  // Bit 8 - Row 0
  // Bit 9 - Row 1
  // Bit 10 - Row 2
  // Bit 11 - Row 3
  // Bit 12 - Row 4

  unsigned long temp = 0;

  switch(keypad_config)
  {
    // GPG 11:8
    case KEY_CONFIG_4x2_1:
    case KEY_CONFIG_4x3: 
    case KEY_CONFIG_3x3:
    case KEY_CONFIG_3x3_PLUS_1:
    case KEY_CONFIG_DUMMY:
    case KEY_CONFIG_4x4_1: 
    case KEY_CONFIG_4x4_2: 
    case KEY_CONFIG_4x4_3: 
    case KEY_CONFIG_4x4_4: 
    case KEY_CONFIG_4x4_5: 
    case KEY_CONFIG_4x5:
    case KEY_CONFIG_5x3: 
    case KEY_CONFIG_5x4_1: 
    case KEY_CONFIG_4x1: 
      temp = __raw_readl(GPGDAT);
      break;

    // GPC 11:8
    case KEY_CONFIG_4x3_2: 
      temp = __raw_readl(GPCDAT);
      break;
  }

  // Invert input for Stingray (to account for the pulldowns instead of pullups)
  return machine_is_rirm2() ? temp : ~temp;
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

  /* Stingray insists that we change pin config to input to read it */
  rutl_regwrite(0, (3 << 22), GPGCON);
  state = (read_input () & (1 << 11)) ? 1 : 0;
  rutl_regwrite((2 << 22), (3 << 22), GPGCON);
  if (state != standalone_key_state)
  {
    standalone_key_state = state;
    key = decode_key_press (8, 3);
    rkg_report_key (key, state);
  }

  spin_unlock_irqrestore(&standalone_access_lock, flags);
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 ****************************************************************************/
INT_HANDLER_DECL(reciva_gpio_standalone_int_handler)
{
  /* Stingray insists that we change pin config to input to read it */
  rutl_regwrite(0, (3 << 22), GPGCON);
  int state = (read_input () & (1 << 11)) ? 1 : 0;
  rutl_regwrite((2 << 22), (3 << 22), GPGCON);
  
  if (state != standalone_key_state)
  {
    disable_irq(IRQ_EINT19);
    standalone_idle = 0;
    standalone_key_state = state;
    restart_standalone_timer ();

    key = decode_key_press (8, 3);
    rkg_report_key (key, state);
  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
}

/****************************************************************************
 * Handles an interrupt on GPIO2/3/4/5
 **Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
INT_HANDLER_DECL(reciva_gpio_int_handler)
{
  DEBUG("GPIOint %d\n", (int)dev);

  interrupt_ran = 1;
  if (state == IDLE)
  {
    // Allow short time for power on key detection
    if (power_on_key_state == POKS_NONE &&
        (jiffies <= (power_on_time + 10)))
    {
      DEBUG(PFX "power on key detect - got interrupt\n");
      power_on_key_state = POKS_GOT_INTERRUPT;
    }

    gpio = (int)dev;
    gp_state_machine(0);

  }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
}

/****************************************************************************
 * Sets a pin while protecting us from a race condition between the code and
 * the hardware when performing multiple read-modify-write operations on
 * registers controlling lines with a capacitance > 100pF
 *
 * Note: This is copied from s3c2410_gpio_setpin and modified.
 * Changes to asm/arch/regs-gpio.h could break this code.
 * It may be a good plan for somebody (brave) to eventually transfer this
 * fix to a more generic solution in s3c2410_gpio_setpin.
 ***************************************************************************/
static void gpio_setpin_buffered(unsigned int pin, unsigned int to)
{
  static unsigned long buffers[8];
  static unsigned long masks[8];
  unsigned long base = (unsigned long)S3C2410_GPIO_BASE(pin);
  unsigned long offs = S3C2410_GPIO_OFFSET(pin);
  unsigned long flags;
  unsigned long dat;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  unsigned int idx = (base - (unsigned long)S3C24XX_VA_GPIO) / 16;
#else
  unsigned int idx = (base - (unsigned long)S3C2410_VA_GPIO) / 16;
#endif
  
  local_irq_save(flags);
  
  dat = __raw_readl(base + 0x04);

  dat &= ~masks[idx];
  dat |= buffers[idx];

  dat &= ~(1 << offs);
  dat |= to << offs;
  
  __raw_writel(dat, base + 0x04);
  
  local_irq_restore(flags);

  masks[idx] |= 1 << offs;
  buffers[idx] = dat & masks[idx];
}

/****************************************************************************
 ** Sets the column levels
 ***************************************************************************/
static void set_columns(int c)
{
  int i;
  int d = (machine_is_rirm2 () ? 0 : 1);

  for (i = 0; i < num_cols; i++) {
    int b = (c & (1 << i)) ? 1 : 0;
    
    s3c2410_gpio_cfgpin (col_pins[i], b ? S3C2410_GPIO_INPUT : S3C2410_GPIO_OUTPUT);
    gpio_setpin_buffered (col_pins[i], d);
  }

  /* Store column levels so that we can restore state following 
   * an interruption from external module (eg LCD) */
  column_levels = c;
}

/****************************************************************************
 * State machine for polling operation
 ****************************************************************************/
static void polled_state_machine (unsigned long ignored)
{
  /* Stop external modules claiming the GPIO lines */
  spin_lock(&access_lock);

  /* Stingray insists that we change pin config to input to read it */
  rutl_regwrite(0, (3 << (gpio * 2)), GPGCON);

  /* Drive column low */
  set_columns (~(1 << column));

  /* Read the row bits */
  unsigned long row_state = (read_input () >> 8) & 0xf;

  int i;
  for (i = 0; i < num_rows; i++) {
          int state = (row_state & (1 << i)) ? 0 : 1;

          if (state != old_state[i][column]) {
                  DEBUG("key %d %d changes state to %d\n", i, column, state);
                  old_state[i][column] = state;

                  int key = decode_key_press (i + 8, column);
                  rkg_report_key (key, state);        
          }
  }

  /* Drive columns high */
  set_columns (~0);
  column = (column + 1) % num_cols;

  /* Allow external modules to claim the GPIO lines */
  spin_unlock(&access_lock);

  enable_interrupts ();
}

static void start_scan(void)
{
  /* Set up state machine data and change state */
  column = num_cols - 1;
  state = IDENTIFY_BUTTON;

  /* Drive first column low */
  set_columns (~(1 << column));
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
#if 0 // def RKG_DEBUG
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

      /* Stingray insists that we change pin config to input to read it */
      rutl_regwrite(0, (3 << (gpio * 2)), GPGCON);

      /* Set timer */
      restart_timer();

      print_state = 1;
      start_scan();
      break;

    case IDENTIFY_BUTTON :
      /* Scan to identify key press */
      for (i=0;i<num_cols;i++)
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
              rkg_report_key (key, 1);

              // Store power-on key if we're looking
              if (power_on_key_state == POKS_GOT_INTERRUPT)
              {
                power_on_row = gpio;
                power_on_col = column;
                power_on_key_state = POKS_FOUND;
                DEBUG(PFX "power on key detect - found r=%d c=%d\n", power_on_row, power_on_col);
              }
              break;
            }
          }
        }

        if (column >= 1)
        {
          /* Drive all columns high */
          set_columns (~0);

          /* Set column low */
          column--;
          set_columns (~(1 << column));

          /* .. and delay before reading back GPIO level
           * to allow for board capacitance */
          udelay(1000);
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
      {
        set_columns(0);
        state = RELEASE_DETECTED;
      }
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

        // Reset columns and check for a change in key
        set_columns(0);
        state = CHECK_FOR_KEY_CHANGE;
      }

      /* Set timer */
      restart_timer();
      break;

    case CHECK_FOR_KEY_CHANGE:
      // If there's still a row press detected, it must be a
      // different column so release the current key and restart the scan
      if ((read_input() & (1 << gpio)) == 0)
      {
        rkg_report_key (key, 0);
        print_state = 1;
        start_scan();
        restart_timer();
        break;
      }
      // Otherwise just fall through to RELEASE_DETECTED

    case RELEASE_DETECTED :

      /* Back to IDLE */
      print_state = 1;
      state = IDLE;
      power_on_key_state = POKS_NONE;

      /* Re-enable interrupts */
      rutl_regwrite((2 << (gpio * 2)), (3 << (gpio * 2)), GPGCON); // EINTn
      enable_interrupts();
      rkg_report_key (key, 0);
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
EXPORT_SYMBOL(rkg_report_key);

module_init(reciva_keypad_init);
module_exit(reciva_keypad_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Generic Keypad");
MODULE_LICENSE("GPL");
