/*
 * Reciva Touchpanel - eKT8120/ekt2101
 * Copyright (c) $Date: 2008-12-17 15:13:37 $ Reciva Ltd
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
#include <linux/poll.h>

#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

#include "reciva_touchpanel_ekt8120.h"
#include "reciva_util.h"
#include "reciva_gpio.h"
#include "reciva_keypad_generic.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM(x) module_param(x, int, S_IRUGO)
#else
#define RECIVA_MODULE_PARM(x) MODULE_PARM(x, "i")
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define INT_HANDLER_DECL(x) static irqreturn_t x (int irq, void *dev)
#else
#define INT_HANDLER_DECL(x) static void x (int irq, void *dev, struct pt_regs *regs)
#endif


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

typedef enum
{
  WAITING_FOR_HELLO,
  INITIALISATION_REQUIRED,
  NORMAL,
} InitState_t;


   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

static void setup_gpio(void);
static void request_interrupt(void);
static void release_interrupt(void);
static int exchange_bit(int level);

static int __init reciva_touchpanel_ekt8120_init(void);
static void __exit reciva_touchpanel_ekt8120_exit(void);
static int reciva_ioctl ( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int reciva_poll (struct file *file, poll_table *wait);
static int reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos);
static int reciva_open(struct inode * inode, struct file * file);
static int reciva_release(struct inode * inode, struct file * file);

static int exchange_data(unsigned int data, int count);

static int first_set_bit(unsigned int n);
static void convert_to_key_event(int data, int *event_id, int *press);
static void check_for_key_press(int key_data);

static int load_map( unsigned long arg );

INT_HANDLER_DECL(touchpanel_int_handler);
static void init_device(void);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Debug prefix */
#define PREFIX "RT_EKT8120:"

/* Debug elements. define these to enable debugging on stated aspect */
/* A basic level of debugging is always enabled. */
//#define DEBUG_PACKETS
//#define DEBUG_BUTTONS
//#define DEBUG_KEYS
//#define DEBUG_INTERRUPTS
//#define DEBUG_INITIAL_REGISTER_VALUES
//#define DEBUG_IOCTLS
//#define DEBUG_FILE

/* Max number of touchpanel keys */
#define RT_EKT8120_MAX_KEYS 18

/* Delay between successive status packets being accepted */
/* The effect should be to provide some sort of debouncing */
#define DEBOUNCE_DELAY_MS   20

/* Minimum time for data lines to settle after clock changes state */
/* This was found by trial and error as the datasheet lacked this information */
/* It may well need some further adjustment in future */
/* NB. At 10us, some clock pulses were being missed by the TP. */
#define CLOCK_DELAY_US      25

/* Packet IDs - Device to host */
#define PCKT_HELLO          0x55555555  // Hello packet sent at startup
#define PCKT_HELLO_MASK     0xFFFFFFFF
#define PCKT_HOLA           0x56555555  // Sometimes, a malformed hello packet is received
#define PCKT_HOLA_MASK      0xFFFFFFFF
#define PCKT_READ_RESP      0x52000001  // Packet ID #2
#define PCKT_READ_RESP_MASK 0xFF000001
#define PCKT_STATUS         0x56000001  // Packet ID #6
#define PCKT_STATUS_MASK    0xFF000003
#define EKT2101_PKT7_READ_ENHANCED 0x57000001 // Packet ID #7

/* Packet IDs - Host to device */
#define PCKT_READ           0x53000001  // Packet ID #3
#define PCKT_WRITE          0x54000001  // Packet ID #4
#define EKT2101_PKT8_READ_REQ_ENHANCED 0x58000001 // Packet ID #8
#define EKT2101_PKT9_WRITE_ENHANCED 0x59000001 // Packet ID #9

/* Registers - for use with packets 2,3 and 4 */
#define REG_FW_VERS         0x00000000  // Reg  0 - Firmware version
#define REG_BUTN_STAT       0x00100000  // Reg  1 - Button Status
#define REG_SENSITIVITY     0x00400000  // Reg  4 - TP Sensitivity
#define REG_POWER_STATE     0x00500000  // Reg  5 - Power State
#define REG_COLD_RESET      0x00D00000  // Reg 13 - Cold Reset (write to this register)
#define REG_REPORT_RATE     0x00E00000  // Reg 14 - Report rate
#define REG_FW_ID           0x00F00000  // Reg 15 - Firmware ID

/* ekt2101 Registers - for use with packets 2,3 and 4 */
// Normal registers
#define EKT2101_REG0_VERSION               (0x00 << 20)
#define EKT2101_REG1_BUTTON_STATUS         (0x01 << 20)
#define EKT2101_REG2_KEY_IO                (0x02 << 20)
#define EKT2101_REG3_OPERATION_MODE        (0x03 << 20)
#define EKT2101_REG4_SENSITIVITY           (0x04 << 20)
#define EKT2101_REG5_POWER_SAVING          (0x05 << 20)
#define EKT2101_REG6_CALIBRATION           (0x06 << 20)
#define EKT2101_REG7_IDLE_TIME             (0x07 << 20)
#define EKT2101_REG10_TRIGGER_LEVEL        (0x0a << 20)
// Enhanced registers
#define EKT2101_EREG0_BOUNCE_COUNT         (0x00 << 20)
#define EKT2101_EREG4_NOISE_LEVEL          (0x04 << 20)


/* Register masks and shifts */
#define FW_VERS_MAJOR_MASK  0x000FF000
#define FW_VERS_MAJOR_BASE  12
#define FW_VERS_MINOR_MASK  0x00000FF0
#define FW_VERS_MINOR_BASE  4

#define BUTN_STAT_MASK      0x000FFFFC  // Also used to extract data from status packet (ID #6)
#define BUTN_STAT_BASE      2

#define SENSITIVITY_MASK    0x000F0000
#define SENSITIVITY_BASE    16

#define POWER_STATE_MASK    0x00080000
#define POWER_STATE_BASE    19

#define COLD_RESET_MASK     0x00080000
#define COLD_RESET_BASE     19

#define REPORT_RATE_MASK    0x00080000
#define REPORT_RATE_BASE    19

#define FW_ID_MASK          0x000FFFF0
#define FW_ID_BASE          4

/* Register value constants */
#define POWER_STATE_NORMAL  1
#define POWER_STATE_SLEEP   0
#define COLD_RESET_ON       1
#define COLD_RESET_OFF      0
#define REPORT_RATE_NORMAL  1
#define REPORT_RATE_FAST    0

/* Status packet masks and shifts */
#define STATUS_SR1_MASK     0x00C00000
#define STATUS_SR1_BASE     22
#define STATUS_SR2_MASK     0x00300000
#define STATUS_SR2_BASE     20
#define STATUS_BUTTONS_MASK 0x000FFFFC
#define STATUS_BUTTONS_BASE 2

/* Status packet values */
#define STATUS_SR_POSITIVE  0b10
#define STATUS_SR_NEGATIVE  0b01

/* Useful Macros */
/* Note for preprocessor beginners:
 * ## is the token-merging operator
 * GET_VALUE(packet,FW_VERS_MINOR)
 * expands to
 * ((packet & FW_VERS_MINOR_MASK) >> FW_VERS_MINOR_BASE)
 * This minor complexity allows for tidier code
 */
#define TEST_PACKET(pckt,type)  ((pckt & type##_MASK) == type)
#define PUT_VALUE(val,name)     ((val << name##_BASE) & name##_MASK) 
#define GET_VALUE(pckt,name)    ((pckt & name##_MASK) >> name##_BASE)


   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static const char acModuleName[] = "Reciva Touchpanel eKT8120";

/* IOCTL related */
static struct file_operations reciva_fops =
{
  owner:    THIS_MODULE,
  read:     reciva_read,
  poll:     reciva_poll,
  ioctl:    reciva_ioctl,
  open:     reciva_open,
  release:  reciva_release,
};
static struct miscdevice reciva_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "touchpanel",
  &reciva_fops
};

// File poll queue
static wait_queue_head_t wait_queue;

// Device file - data available
static int data_available = 0;

/* Current button state - i.e. which button is pressed */
/* Contains a value from 0 to RT_EKT8120_MAX_KEYS */
/* 0 = no button currently pressed */
static int button_state;

/* Set to configure the sensitivity */
static int sensitivity = 2;
RECIVA_MODULE_PARM(sensitivity);

/* Hardware configuration - see section below on pin configurations */
static int hw_config = 0;
RECIVA_MODULE_PARM(hw_config);

/* Device type */
typedef enum
{
  TP_DEVICE_TYPE_EK8120                 = 0,
  TP_DEVICE_TYPE_EKT2101                = 1,
} device_type_t;
static device_type_t device_type;
RECIVA_MODULE_PARM(device_type);

/* Converts touchpanel data to key events */
/* Must be loaded via ioctl */
static int keymap[RT_EKT8120_MAX_KEYS];

static InitState_t init_state = WAITING_FOR_HELLO;

static spinlock_t spi_lock = SPIN_LOCK_UNLOCKED;

/****************************************************************************
 * Pin configurations
 * To add a new configuration it should be sufficient to add it here
 * At the moment this is over-complicated with the config constants,
 * but the 2.6 kernel HAL should make this considerably cleaner.
 ****************************************************************************/
 
// Config | CLOCK | MOSI  | MISO  | IRQ
// 0      | J2-7  | J2-5  | J2-3  | J2-13
// 1      | J2-11 | J2-15 | J2-13 | J2-17
// 2      | J3-13 | J3-2  | J3-15 | J2-1 
// 3      | J1-3  | J1-5  | J1-7  | J2-3 

#define RT_EKT8120_MAX_HW_CONFIG 3

typedef struct
{
  unsigned int clock_pin;
  unsigned int clock_pin_cfg;
  unsigned int mosi_pin;
  unsigned int mosi_pin_cfg;
  unsigned int miso_pin;
  unsigned int miso_pin_cfg;
  unsigned int irq_pin;
  unsigned int irq_pin_cfg;
  unsigned int irq;
  unsigned int irq_flt_reg;
  unsigned int irq_flt_reg_mask;
  unsigned int irq_flt_reg_val;
  unsigned int irq_reg;
  unsigned int irq_reg_mask;
  unsigned int irq_reg_val;

} pin_config_t;

static pin_config_t *pin_config;

// Pin configs for s3c2410
static pin_config_t pin_configs_s3c2410[] =
{
  {
    .clock_pin = S3C2410_GPG7,
    .clock_pin_cfg = S3C2410_GPG7_OUTP,
    
    .mosi_pin = S3C2410_GPD10,
    .mosi_pin_cfg = S3C2410_GPD10_OUTP,
    
    .miso_pin = S3C2410_GPG5,
    .miso_pin_cfg = S3C2410_GPG5_INP,
    
    .irq_pin = S3C2410_GPG10,
    .irq_pin_cfg = S3C2410_GPG10_EINT18,
    
    .irq = IRQ_EINT18,
    
    .irq_flt_reg = (unsigned int)S3C2410_EINFLT2,
    .irq_flt_reg_mask = (1<<23) | (127<<16),
    .irq_flt_reg_val = (0<<23) | (127<<16),
    
    .irq_reg = (unsigned int)S3C2410_EXTINT2,
    .irq_reg_mask = (1<<11) | (7<<8),
    .irq_reg_val = (1<<11) | (2<<8),
  },
  {
    .clock_pin = S3C2410_GPG11,
    .clock_pin_cfg = S3C2410_GPG11_OUTP,
    
    .mosi_pin = S3C2410_GPG9,
    .mosi_pin_cfg = S3C2410_GPG9_OUTP,
    
    .miso_pin = S3C2410_GPG10,
    .miso_pin_cfg = S3C2410_GPG10_INP,
    
    .irq_pin = S3C2410_GPG8,
    .irq_pin_cfg = S3C2410_GPG8_EINT16,
    
    .irq = IRQ_EINT16,
    
    .irq_flt_reg = (unsigned int)S3C2410_EINFLT2,
    .irq_flt_reg_mask = (1<<7) | (127<<0),
    .irq_flt_reg_val = (0<<7) | (127<<0),
    
    .irq_reg = (unsigned int)S3C2410_EXTINT2,
    .irq_reg_mask = (1<<3) | (7<<0),
    .irq_reg_val = (1<<3) | (2<<0),
  },

  {
    .clock_pin = S3C2410_GPC10,
    .clock_pin_cfg = S3C2410_GPC10_OUTP,
    
    .mosi_pin = S3C2410_GPD10,
    .mosi_pin_cfg = S3C2410_GPD10_OUTP,
    
    .miso_pin = S3C2410_GPC9,
    .miso_pin_cfg = S3C2410_GPC9_INP,
    
    .irq_pin = S3C2410_GPG3,
    .irq_pin_cfg = S3C2410_GPG3_EINT11,
    
    .irq = IRQ_EINT11,
    
    .irq_flt_reg = (unsigned int)S3C2410_EINFLT2,
    .irq_flt_reg_mask = (1<<7) | (127<<0),
    .irq_flt_reg_val = (0<<7) | (127<<0),
    
    .irq_reg = (unsigned int)S3C2410_EXTINT1,
    .irq_reg_mask = (1<<15) | (7<<12),
    .irq_reg_val = (1<<15) | (2<<12),
  },

  // 3
  {
    .clock_pin = S3C2410_GPE10,
    .clock_pin_cfg = S3C2410_GPE10_OUTP,

    .mosi_pin = S3C2410_GPE9,
    .mosi_pin_cfg = S3C2410_GPE9_OUTP,

    .miso_pin = S3C2410_GPE8,
    .miso_pin_cfg = S3C2410_GPE8_INP,

    .irq_pin = S3C2410_GPG5,
    .irq_pin_cfg = S3C2410_GPG5_EINT13,

    .irq = IRQ_EINT13,

    // No filter reg for EINT13
    .irq_flt_reg = 0,
    .irq_flt_reg_mask = 0,
    .irq_flt_reg_val = 0,

    .irq_reg = (unsigned int)S3C2410_EXTINT1,
    .irq_reg_mask = (1<<23) | (7<<20),
    .irq_reg_val = (1<<23) | (2<<20),
  },
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

// Pin configs for s3c2412
static pin_config_t pin_configs_s3c2412[] =
{
  // 0
  {
    .clock_pin = S3C2410_GPG2,
    .clock_pin_cfg = S3C2410_GPG2_OUTP,

    .mosi_pin = S3C2410_GPD10,
    .mosi_pin_cfg = S3C2410_GPD10_OUTP,

    .miso_pin = S3C2410_GPG0,
    .miso_pin_cfg = S3C2410_GPG0_INP,

    .irq_pin = S3C2410_GPG10,
    .irq_pin_cfg = S3C2410_GPG10_EINT18,

    .irq = IRQ_EINT18,

    .irq_flt_reg = ((unsigned int)S3C2410_EINFLT2) + 0x10,
    .irq_flt_reg_mask = (1<<23) | (127<<16),
    .irq_flt_reg_val = (0<<23) | (127<<16),

    .irq_reg = (unsigned int)S3C2412_EXTINT2,
    .irq_reg_mask = (1<<11) | (7<<8),
    .irq_reg_val = (1<<11) | (2<<8),
  },

  // 1
  {
    .clock_pin = S3C2410_GPG11,
    .clock_pin_cfg = S3C2410_GPG11_OUTP,

    .mosi_pin = S3C2410_GPG9,
    .mosi_pin_cfg = S3C2410_GPG9_OUTP,

    .miso_pin = S3C2410_GPG10,
    .miso_pin_cfg = S3C2410_GPG10_INP,

    .irq_pin = S3C2410_GPG8,
    .irq_pin_cfg = S3C2410_GPG8_EINT16,

    .irq = IRQ_EINT16,

    .irq_flt_reg = ((unsigned int)S3C2410_EINFLT2) + 0x10,
    .irq_flt_reg_mask = (1<<7) | (127<<0),
    .irq_flt_reg_val = (0<<7) | (127<<0),

    .irq_reg = (unsigned int)S3C2412_EXTINT2,
    .irq_reg_mask = (1<<3) | (7<<0),
    .irq_reg_val = (1<<3) | (2<<0),
  },

  // 2
  {
    .clock_pin = S3C2410_GPC10,
    .clock_pin_cfg = S3C2410_GPC10_OUTP,

    .mosi_pin = S3C2410_GPD10,
    .mosi_pin_cfg = S3C2410_GPD10_OUTP,

    .miso_pin = S3C2410_GPC9,
    .miso_pin_cfg = S3C2410_GPC9_INP,

    .irq_pin = S3C2410_GPG3,
    .irq_pin_cfg = S3C2410_GPG3_EINT11,

    .irq = IRQ_EINT11,

    .irq_flt_reg = ((unsigned int)S3C2410_EINFLT2) + 0x10,
    .irq_flt_reg_mask = (1<<7) | (127<<0),
    .irq_flt_reg_val = (0<<7) | (127<<0),

    .irq_reg = (unsigned int)S3C2412_EXTINT1,
    .irq_reg_mask = (1<<15) | (7<<12),
    .irq_reg_val = (1<<15) | (2<<12),
  },

  // 3
  {
    .clock_pin = S3C2410_GPE10,
    .clock_pin_cfg = S3C2410_GPE10_OUTP,

    .mosi_pin = S3C2410_GPE9,
    .mosi_pin_cfg = S3C2410_GPE9_OUTP,

    .miso_pin = S3C2410_GPE8,
    .miso_pin_cfg = S3C2410_GPE8_INP,

    .irq_pin = S3C2410_GPG0,
    .irq_pin_cfg = S3C2410_GPG0_EINT8,

    .irq = IRQ_EINT8,

    .irq_flt_reg = 0,
    .irq_flt_reg_mask = 0,
    .irq_flt_reg_val = 0,

    .irq_reg = (unsigned int)S3C2412_EXTINT1,
    .irq_reg_mask = (1<<3) | (7<<0),
    .irq_reg_val = (1 << 3) | (2<<0),
  },
};

#endif




   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

   /*************************************************************************/
   /*** Hardware layer - all pin-specific actions should be in this block ***/
   /*************************************************************************/
 
/****************************************************************************
 * Setup gpio for writing to the device
 ****************************************************************************/
static void setup_gpio(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  if (machine_is_rirm3())
    pin_config = &pin_configs_s3c2412[ hw_config ];
  else
#endif
    pin_config = &pin_configs_s3c2410[ hw_config ];
  
  /* CLOCK */
  /* Output, no pullup, normally high */
  s3c2410_gpio_setpin( pin_config->clock_pin, 1 );
  s3c2410_gpio_pullup( pin_config->clock_pin, 1 );
  s3c2410_gpio_cfgpin( pin_config->clock_pin, pin_config->clock_pin_cfg );

  /* MOSI */
  /* Output, no pullup, level doesn't matter */
  s3c2410_gpio_pullup( pin_config->mosi_pin, 1 );
  s3c2410_gpio_cfgpin( pin_config->mosi_pin, pin_config->mosi_pin_cfg );

  /* MISO */
  /* Input, no pullup */
  s3c2410_gpio_pullup( pin_config->miso_pin, 1 );
  s3c2410_gpio_cfgpin( pin_config->miso_pin, pin_config->miso_pin_cfg );

  /* INT */
  /* Interrupt, no pullup */
  s3c2410_gpio_pullup( pin_config->irq_pin, 1 );
  s3c2410_gpio_cfgpin( pin_config->irq_pin, pin_config->irq_pin_cfg );
  
  /* Interrupt config */
  /* Enable filter: clock = PCLK, width = max; falling edge triggered */
  if (pin_config->irq_flt_reg)
    rutl_regwrite(pin_config->irq_flt_reg_val, pin_config->irq_flt_reg_mask, pin_config->irq_flt_reg);

  rutl_regwrite(pin_config->irq_reg_val, pin_config->irq_reg_mask, pin_config->irq_reg);
}

/****************************************************************************
 * Request and release interrupt
 ****************************************************************************/
static void request_interrupt(void)
{
  request_irq(pin_config->irq, touchpanel_int_handler, 0, "eKT8120 IRQ", NULL);
}

static void release_interrupt(void)
{
  free_irq(pin_config->irq, NULL);
}

/****************************************************************************
 * Send one bit of data to the module, and read the bit being sent back
 ****************************************************************************/
static int exchange_bit(int level)
{
  int ret = 0;
  
  s3c2410_gpio_setpin( pin_config->mosi_pin, level );   /* Set up output bit */
  udelay(CLOCK_DELAY_US);
  s3c2410_gpio_setpin( pin_config->clock_pin, 0 );      /* Drive CLOCK low - triggers sample on device */
  ret = (s3c2410_gpio_getpin( pin_config->miso_pin ) ? 1 : 0); /* Get the input bit */
  udelay(CLOCK_DELAY_US);
  s3c2410_gpio_setpin( pin_config->clock_pin, 1 );      /* Drive CLOCK high - triggers new set-up on device */
  
  return ret;
}

   /*************************************************************************/
   /*** End of hardware layer                                             ***/
   /*************************************************************************/

/****************************************************************************
 * Exchange data with the module. Data is clocked out msb first
 * data - up to 32 bits worth of data
 * count - number of bits to exchange (1 to 32)
 ****************************************************************************/
static int exchange_data(unsigned int data, int count)
{
  unsigned long irq_flags;
  int retbit = 0;
  int retval = 0;
  
  spin_lock_irqsave( spi_lock, irq_flags );
  
  #ifdef DEBUG_PACKETS
  printk(PREFIX "exchange_data( %08x, %d ). ", data, count );
  #endif
  
  int i = 0;
  while (count)
  {
    retbit = exchange_bit( (data >> (count-1)) & 0x01 );
    retval |= ( retbit << (count-1) );
    count--;

    // Need a > 50us delay between bytes for ekt2101
    if (device_type == TP_DEVICE_TYPE_EKT2101)
    {
      if (++i % 8 == 0)
        udelay(100);
    }
  }
  
  #ifdef DEBUG_PACKETS
  printk("Received %08x.\n", retval);
  #endif
  
  spin_unlock_irqrestore( spi_lock, irq_flags );
  
  return retval;
}

/****************************************************************************
 * Returns the position of the first bit that is set in a 32 bit integer
 * (least significant bit = 0, most significant = 31)
 ****************************************************************************/
static int first_set_bit(unsigned int n)
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
 * Converts a touchpanel event into a key event
 * data - data from touchpanel (bit x = 0:key not pressed, 1:key pressed)
 ****************************************************************************/
static void convert_to_key_event(int data, int *event_id, int *press)
{
  *event_id = RKD_UNUSED;
  *press = 0;

  // eKT8120 reports it's keys in reverse order relative to bit numbers.
  int key = 17 - first_set_bit(data);
  if (key >= 0 && key < RT_EKT8120_MAX_KEYS)
  {
    button_state = key + 1;
    #ifdef DEBUG_BUTTONS
    printk(PREFIX "Button %d detected.\n", button_state);
    #endif
    *press = 1;
    *event_id = keymap[key];
  }
  else
  {
    button_state = 0;
    #ifdef DEBUG_BUTTONS
    printk(PREFIX "No buttons detected.\n");
    #endif
  }
  
  // New data available for device file readers
  data_available = 1;
  wake_up_interruptible(&wait_queue);
}
/****************************************************************************
 * Read key status registers to check if a key has been pressed
 ****************************************************************************/
static void check_for_key_press(int key_data)
{
  static int last_key = RKD_UNUSED;
  int key;
  int press;
  
  /* Convert key data into a key event */
  convert_to_key_event(key_data, &key, &press);

  if (key != RKD_UNUSED)
  {

    if (key != last_key)
    {
      /* Report release of last key */
      if (last_key != RKD_UNUSED)
      {
        #ifdef DEBUG_KEYS
        printk(PREFIX "KEY release %d\n", last_key);
        #endif
        rkg_report_key(last_key, 0);
      }

      /* Report key press */
      #ifdef DEBUG_KEYS
      printk(PREFIX "KEY press %d\n", key);
      #endif
      rkg_report_key(key, 1);
      last_key = key;
    }
  }
  else
  {
    /* Report key release */
    if (press == 0 && last_key != RKD_UNUSED)
    {
      #ifdef DEBUG_KEYS
      printk(PREFIX "KEY release %d\n", last_key);
      #endif
      rkg_report_key(last_key, 0);
      last_key = RKD_UNUSED;
    }
  }
}

/****************************************************************************
 * Handles an interrupt on INT line
 * Parameters - Standard interrupt handler params. Not used.
 ****************************************************************************/
INT_HANDLER_DECL(touchpanel_int_handler)
{
  static unsigned long last_status_received_time;
  unsigned int packet;
  
  #ifdef DEBUG_INTERRUPTS
  printk(PREFIX "Got Interrupt\n");
  #endif
  
  // Get packet
  packet = exchange_data(0,32);
  
  // Operation depends on init state
  switch (init_state)
  {
    case WAITING_FOR_HELLO:
      // Should be getting a hello packet
      if ( TEST_PACKET(packet,PCKT_HELLO) || TEST_PACKET(packet,PCKT_HOLA) )
      {
        // Hello packet received - device is in it's default state
        printk(PREFIX "Hello packet received.\n");
        init_state = INITIALISATION_REQUIRED;
      }
      else
      {
        // If not, the device is in an unexpected state
        // and there's not a lot we can do about that.
        printk(PREFIX "Hello packet not received when expected!!\n");
        break;
      }
      // deliberate drop-through.
      
    case INITIALISATION_REQUIRED:
      init_device();
      break;
      
    case NORMAL:
      // Only expect 'status' packet at present
      if ( TEST_PACKET(packet,PCKT_STATUS) )
      {
        // Perform a certain level of debouncing here
        if ( (jiffies - last_status_received_time )
            > ( DEBOUNCE_DELAY_MS * HZ / 1000 ) )
        {
          // Check for keypress and send keypad event if required
          check_for_key_press( GET_VALUE(packet,STATUS_BUTTONS) );
          
          last_status_received_time = jiffies;
        }
        else
        {
          // If we just drop packets within the debounce period, half the time
          // we'll end up dropping the final button release.
          // because it doesn't actually matter when we send the release event
          // in a debounce situation, it should be safe to send it now.
          // This is done by pretending that all keys have been released.
          check_for_key_press( 0 );
          #ifdef DEBUG_INTERRUPTS
          printk(PREFIX "Filtering status packet for debounce purposes\n");
          #endif
        }
      }
      break;
    }
    
    #ifdef DEBUG_INTERRUPTS
    printk(PREFIX "Leaving interrupt handler\n");
    #endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  return IRQ_HANDLED;
#endif
}

/****************************************************************************
 * Initialise device
 ****************************************************************************/
static void init_device(void)
{
  unsigned int packet;
  
  if ( init_state > WAITING_FOR_HELLO )
  {
    printk(PREFIX "Initialising device\n");
    
    // Set the sensitivity
    printk(PREFIX "Setting sensitivity = %d\n", sensitivity);
    packet = PCKT_WRITE | REG_SENSITIVITY | PUT_VALUE(sensitivity,SENSITIVITY);
    exchange_data(packet,32);
    
    // Device is now ready for action
    init_state = NORMAL;
  }
  // If still in WAITING_FOR_HELLO state, device will get initialised
  // from the interrupt handler when the hello packet is received.
}


   /*************************************************************************/
   /***                        File operations                            ***/
   /*************************************************************************/

/****************************************************************************
 * Load touchpanel keymap data from user space
 ****************************************************************************/
static int load_map ( unsigned long arg )
{
  struct reciva_touchpanel_map k;
  int i;

  if (copy_from_user (&k, (void *)arg, sizeof (k)))
    return -EFAULT;

  printk( PREFIX "load_map n=%d\n", k.nbuttons);

  if (k.nbuttons > RT_EKT8120_MAX_KEYS)
    return -EINVAL;

  int *buf = kmalloc((k.nbuttons * sizeof (int)), GFP_KERNEL);
  if (!buf)
    return -ENOMEM;

  if (copy_from_user (buf, (void *)k.codes, (k.nbuttons * sizeof (int))))
  {
    kfree (buf);
    return -EFAULT;
  }

  for (i = 0; i < k.nbuttons; i++)
  {
    #ifdef DEBUG_KEYS
    printk( PREFIX "  button %d = %d\n", i+1, buf[i] );
    #endif
    keymap[i] = buf[i];
  }

  kfree (buf);  
  
  return 0;
}

/****************************************************************************
 * Ioctl handler
 ****************************************************************************/
static int
reciva_ioctl ( struct inode *inode, struct file *file,
               unsigned int cmd, unsigned long arg)
{
  int i;
  
  switch(cmd)
  {
    case IOC_TOUCHPANEL_SENSITIVITY:
      #ifdef DEBUG_IOCTLS
      printk(PREFIX "Ioctl received: IOC_TOUCHPANEL_SENSITIVITY\n");
      #endif
      
      if ( copy_from_user(&i, (void *)arg, sizeof(i)) )
        return -EFAULT;
      
      // Set sensitivity setting within valid limits
      if ( i < 0 )
        sensitivity = 0;
      else if ( i > 6 )
        sensitivity = 6;
      else
        sensitivity = i;
      
      // Reinitialise the device
      init_device();
      break;
    
    case IOC_TOUCHPANEL_BUTTONSTATE:
      #ifdef DEBUG_IOCTLS
      printk(PREFIX "Ioctl received: IOC_TOUCHPANEL_BUTTONSTATE\n");
      #endif
      return put_user( button_state, (int *)arg );
      break;
    
    case IOC_TOUCHPANEL_LOAD_MAP:
      #ifdef DEBUG_IOCTLS
      printk(PREFIX "Ioctl received: IOC_TOUCHPANEL_LOAD_MAP\n");
      #endif
      return load_map( arg );
      break;
    
    default:
      #ifdef DEBUG_IOCTLS
      printk(PREFIX "Unknown ioctl received!!\n");
      #endif
      return -ENODEV;
  }

  return 0;
}

/****************************************************************************
 * Returns status of device. Indicates if there is data available to read
 ****************************************************************************/
static unsigned int 
reciva_poll (struct file *file, poll_table *wait)
{
  #ifdef DEBUG_FILE
  printk(PREFIX "poll\n");
  #endif
  
  poll_wait(file, &wait_queue, wait);
  
  if (data_available)
    return POLLIN | POLLRDNORM;
  else
    return 0;
}

/****************************************************************************
 * Read handler
 * Note: no buffering so some state changes can be lost if repeatedly
 * reading the file (e.g. with "tail -f")
 ****************************************************************************/
static int 
reciva_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  int ret = 0;
  char tempbuf;
  
  #ifdef DEBUG_FILE
  printk(PREFIX "read\n");
  #endif
  
  if (data_available)
  {
    tempbuf = button_state ? ('A' + button_state - 1) : '0';
    ret = 1;
    
    if ( copy_to_user(buffer, &tempbuf, 1) )
      ret = -EFAULT;
    
    data_available = 0;
  }

  return ret;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
reciva_open(struct inode * inode, struct file * file)
{
  #ifdef DEBUG_FILE
  printk(PREFIX "open\n");
  #endif
  
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
reciva_release(struct inode * inode, struct file * file)
{
  #ifdef DEBUG_FILE
  printk(PREFIX "release\n");
  #endif
  
  return 0;
}

/****************************************************************************
 * Read register
 ****************************************************************************/
#ifdef DEBUG_INITIAL_REGISTER_VALUES
static int read_register(unsigned int reg)
{
  unsigned int packet;
  unsigned int rxdata;

  packet = PCKT_READ | (reg << 20);
  exchange_data(packet,32);      // Write
  rxdata = exchange_data(0,32);  // Read
  printk(PREFIX "REG[%d] = %08x\n", reg, rxdata);

  return rxdata;
}
#endif

/****************************************************************************
 * Print registers
 ****************************************************************************/
static void ekt2101_dump_registers(void)
{
#ifdef DEBUG_INITIAL_REGISTER_VALUES
  unsigned int packet;
  unsigned int rxdata;
  printk(PREFIX "Dump Registers\n");

  read_register(0);
  read_register(0);
  read_register(0);
  read_register(0);
  read_register(0);

  if (0)
  {
    read_register(1);
    read_register(2);
    read_register(3);
    read_register(4);
    read_register(5);
    read_register(6);
    read_register(7);
    read_register(10);
    read_register(0);
  }

#endif
}

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
reciva_touchpanel_ekt8120_init(void)
{
  unsigned int packet;
  int i;
  
  printk(PREFIX "%s module: loaded\n", acModuleName);
  
  /* Crash out if an invalid configuration has been passed */
  if ( hw_config < 0 || hw_config > RT_EKT8120_MAX_HW_CONFIG )
    return -EINVAL;
  
  /* Initialise the keymap */
  for ( i = 0; i < RT_EKT8120_MAX_KEYS; i++ )
    keymap[i] = RKD_UNUSED;
  
  /* Register the character device */
  init_waitqueue_head (&wait_queue);
  misc_register (&reciva_miscdev);
  
  // Clamp sensitivity setting within valid limits
  if ( sensitivity < 0 )
    sensitivity = 0;
  else if ( sensitivity > 6 )
    sensitivity = 6;
  
  /* Set up IO lines and interrupts */
  setup_gpio();
  
  // XXX temp - debugging ekt2101 - just dump registers for now
  if (device_type == TP_DEVICE_TYPE_EKT2101)
  {
    ekt2101_dump_registers();
    return 0;
  }
 
  // Get first packet
  packet = exchange_data(0,32);
  
  // Configure the interrupt
  request_interrupt();
  
  // Check first packet for device state
  if ( TEST_PACKET(packet,PCKT_HELLO) )
  {
    // Hello packet received - device is in it's default state
    printk(PREFIX "Hello packet received.\n");
    init_state = INITIALISATION_REQUIRED;
    init_device();
  }
  else
  {
    // Hello packet not received - need to reset device
    printk(PREFIX "Hello packet not received! Resetting device.\n");
    packet = PCKT_WRITE | REG_COLD_RESET | PUT_VALUE(COLD_RESET_ON,COLD_RESET);
    exchange_data(packet,32);
  }
  
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
reciva_touchpanel_ekt8120_exit(void)
{
  /* Unregister the character device */
	misc_deregister (&reciva_miscdev);
  
  release_interrupt();
  
  printk(PREFIX "%s module: unloaded\n", acModuleName);
}


module_init(reciva_touchpanel_ekt8120_init);
module_exit(reciva_touchpanel_ekt8120_exit);

MODULE_LICENSE("GPL");

