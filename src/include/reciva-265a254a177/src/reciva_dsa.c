/*
 * DSA protocol implementation
 *
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include <linux/kernel.h>
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
#include <linux/serial.h>
#include <linux/version.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/regs-irq.h>
#include <asm/arch/gpio.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/mach/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM(x) module_param(x, int, S_IRUGO)
#else
#define RECIVA_MODULE_PARM(x) MODULE_PARM(x, "i")
#endif

// IRQ test. Can remove this at some point
// With this enabled double edge triggered interrupts are enabled on all
// input pins.
// 3 output pins are set up to replicate levels on the input pins
// output pins are
// J1-3 (follows DATA)
// J1-5 (follows STRB)
// J1-7 (follows ACK)
static int irqtest;
RECIVA_MODULE_PARM(irqtest);

// Dynamically enable/disable debug
static int debug_level;
RECIVA_MODULE_PARM(debug_level);

// Release reset line on module load
static int release_reset;
RECIVA_MODULE_PARM(release_reset);

static int reset_pin;

// Module pins (stingray)
// DATA    STRB   ACK    RESET
// J2-12   J2-7   J2-25  J1-9


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

/* Debug prefix */
#define PREFIX "RDSA: "

// Debug
//#define QDEBUG     /* debug to internal ram array */
//#define QDEBUG_RX  /* rx specific debug */
//#define QDEBUG_TX  /* tx specific debug */
#define DBG(...) if (debug_level > 0) printk(PREFIX __VA_ARGS__ );

// Test modes

// First write to device will result in a test command being sent out continuously
//#define TX_TEST_MODE_CONTINUOUS_TX



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

// Pin numbers
static int ack_pin;
static int strb_pin;
static int data_pin;

// IRQ numbers
static int strb_irq;
static int data_irq;
static int ack_irq;

// Output pins for irq test
static int test_out1_pin;
static int test_out2_pin;
static int test_out3_pin;

#define RX_FIFO_SIZE    128
#define TX_FIFO_SIZE    8

static u16 rx_fifo[RX_FIFO_SIZE];
static u16 tx_fifo[TX_FIFO_SIZE];

static int tx_fifo_write_index, tx_fifo_read_index;
static int rx_fifo_write_index, rx_fifo_read_index;

static spinlock_t dsa_lock = SPIN_LOCK_UNLOCKED;

static u16 shift_register;
static int bit_count;

// Tx statistics
static int tx_ok_count;
static int tx_retry_count;
static int tx_timeout_count;

// Rx statistics
static int rx_ok_count;
static int rx_timeout_count;


static wait_queue_head_t tx_wait_queue, rx_wait_queue;

static struct timer_list timeout, ticker;


typedef enum {
  DSA_INIT = 0,
  DSA_IDLE,
  DSA_RX_SYNC,
  DSA_RX_BIT,
  DSA_RX_BIT_ACK,
  DSA_RX_FINAL_ACK,
  DSA_TX_SYNC,
  DSA_TX_SYNC2,
  DSA_TX_BIT_ACK,
  DSA_TX_BIT_ACK2,
  DSA_TX_FINAL_ACK,
  DSA_TX_FINAL_ACK_CLEAR,
  DSA_TX_FINAL_ACK_CLEAR_RETRY,
} dsa_state_t;

typedef enum {
  DSA_NO_EVENT = 0,
  DSA_DATA_LOW,
  DSA_DATA_HIGH,
  DSA_STRB_LOW,
  DSA_STRB_HIGH,
  DSA_ACK_LOW,
  DSA_ACK_HIGH,
  DSA_TIMEOUT,
} dsa_event_t;


// Fast debug to avoid use of printk
#ifdef QDEBUG

static const char *event_name[] = { "N ", "D-", "D+", "S-", "S+", "A-", "A+", "T " };

#define QDEBUG_TEXT_LENGTH 30
typedef struct
{
  unsigned long timestamp;
  char text[QDEBUG_TEXT_LENGTH];

} qdebug_entry_t;

#define MAX_QDEBUG_ENTRIES 350
static qdebug_entry_t qdebug_log[MAX_QDEBUG_ENTRIES];
static int qdebug_index;
static int qdebug_suspended;

#define QDBG(a) qdebug_print(a)
#else
#define QDBG(a)

#endif /* QDEBUG */


static volatile dsa_state_t dsa_state;



   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

// Used in pin_levels bitmap
#define ACK_BIT         0
#define STRB_BIT        1
#define DATA_BIT        2

// Bitmask of pins that we should not ignore interrupts on
static int interrupt_enabled_pins;


/****************************************************************************
 * Init debug log
 ****************************************************************************/
static void qdebug_init (void)
{
#ifdef QDEBUG
  qdebug_index = 0;
#endif
}

/****************************************************************************
 * Dump debug log
 ****************************************************************************/
static void qdebug_dump (void)
{
#ifdef QDEBUG
  qdebug_entry_t *e;
  int i;
  for (i=0; i<qdebug_index; i++)
  {
    e = &qdebug_log[i];

    if (e->timestamp == 0)
      break;

    printk("%d %s\n", i, e->text);
  }
#endif
}

/****************************************************************************
 * Quick debug print
 ****************************************************************************/
#ifdef QDEBUG
static void qdebug_print (char *text)
{
  int i;

  if (qdebug_index >= MAX_QDEBUG_ENTRIES || qdebug_suspended)
    return;

  qdebug_entry_t *e = &qdebug_log[qdebug_index];
  e->timestamp = jiffies;

  for (i=0; i<QDEBUG_TEXT_LENGTH; i++)
  {
    e->text[i] = text[i];

    if (e->text[i] == 0)
      break;
  }
  e->text[QDEBUG_TEXT_LENGTH-1] = 0;

  qdebug_index++;
}
#endif

/****************************************************************************
 * Reset qdebug log trace
 ****************************************************************************/
#ifdef QDEBUG
static void qdebug_reset (void)
{
  if (qdebug_suspended)
    return;

  qdebug_init();
  QDBG("RESET");
}
#endif

/****************************************************************************
 * Supend or resume qdebug logging
 ****************************************************************************/
#ifdef QDEBUG
static void qdebug_suspend (int suspend)
{
  if (suspend)
    QDBG("suspend");

  qdebug_suspended = suspend;

  if (suspend == 0)
    QDBG("resume");
}
#endif

/****************************************************************************
 * Indicates if we are in an rx state
 ****************************************************************************/
#ifdef QDEBUG
static int in_a_rx_state (void)
{
  int rx = 0;

  switch (dsa_state)
  {
    case DSA_RX_SYNC:
    case DSA_RX_BIT:
    case DSA_RX_BIT_ACK:
    case DSA_RX_FINAL_ACK:
      rx = 1;
      break;
    case DSA_INIT:
    case DSA_IDLE:
    case DSA_TX_SYNC:
    case DSA_TX_SYNC2:
    case DSA_TX_BIT_ACK:
    case DSA_TX_BIT_ACK2:
    case DSA_TX_FINAL_ACK:
    case DSA_TX_FINAL_ACK_CLEAR:
    case DSA_TX_FINAL_ACK_CLEAR_RETRY:
      break;
  }

  return rx;
}
#endif

/****************************************************************************
 * Indicates if we are in a tx state
 ****************************************************************************/
#ifdef QDEBUG
static int in_a_tx_state (void)
{
  int tx = 0;

  switch (dsa_state)
  {
    case DSA_TX_SYNC:
    case DSA_TX_SYNC2:
    case DSA_TX_BIT_ACK:
    case DSA_TX_BIT_ACK2:
    case DSA_TX_FINAL_ACK:
    case DSA_TX_FINAL_ACK_CLEAR:
    case DSA_TX_FINAL_ACK_CLEAR_RETRY:
      tx = 1;
      break;
    case DSA_INIT:
    case DSA_IDLE:
    case DSA_RX_SYNC:
    case DSA_RX_BIT:
    case DSA_RX_BIT_ACK:
    case DSA_RX_FINAL_ACK:
      break;
  }

  return tx;
}
#endif

/****************************************************************************
 * Get current level of DATA pin
 ****************************************************************************/
static inline int get_level_data_pin (void)
{
  // S3C2412/13 insists we change pin config to input before reading them
  s3c2410_gpio_cfgpin(data_pin, S3C2410_GPIO_INPUT);

  int level = 0;
  unsigned long raw = readl(S3C2410_GPFDAT);
  if (raw & (1 << 2))
    level = 1;

  s3c2410_gpio_cfgpin(data_pin, S3C2410_GPF2_EINT2);

  return level;
}

/****************************************************************************
 * Get current level of STB pin
 ****************************************************************************/
static inline int get_level_strb_pin (void)
{
  int level;

  // S3C2412/13 insists we change pin config to input before reading them
  s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPIO_INPUT);

  level = s3c2410_gpio_getpin (strb_pin) ? 1 : 0;

  if (machine_is_rirm3 ()) 
    s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG2_EINT10);
  else
    s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG7_EINT15);

  return level;
}

/****************************************************************************
 * Get current level of ACK pin
 ****************************************************************************/
static inline int get_level_ack_pin (void)
{
  int level;

  // S3C2412/13 insists we change pin config to input before reading them
  s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPIO_INPUT);

  level = s3c2410_gpio_getpin (ack_pin) ? 1 : 0;

  if (machine_is_rirm3 ())
    s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF5_EINT5);
  else
    s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF4_EINT4);

  return level;
}

/****************************************************************************
 * Set pin function to interrupt
 ****************************************************************************/
static void enable_pin_interrupt (int pin)
{
  if (pin == DATA_BIT)
  {
    s3c2410_gpio_cfgpin(data_pin, S3C2410_GPF2_EINT2);
  }
  else if (pin == STRB_BIT)
  {
    if (machine_is_rirm3 ()) 
      s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG2_EINT10);
    else
      s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG7_EINT15);
  }
  else if (pin == ACK_BIT)
  {
    if (machine_is_rirm3 ()) 
      s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF5_EINT5);
    else
      s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF4_EINT4);
  }
}

/****************************************************************************
 * Set up bitmask defining which interrupt pin we currently care about
 ****************************************************************************/
static void enable_pin_event(int pin, int enable)
{
  if (enable)
    interrupt_enabled_pins |= (1 << pin);
  else
    interrupt_enabled_pins &= ~(1 << pin);

  if (enable)
    enable_pin_interrupt(pin);
}

/****************************************************************************
 * Indicates whether pin events are enabled on specified pin
 ****************************************************************************/
static int pin_event_enabled(int pin)
{
  int enabled = 0;

  if (interrupt_enabled_pins & (1 << pin))
    enabled = 1;

  return enabled;
}

/****************************************************************************
 * Drive DATA low
 ****************************************************************************/
static void
data_low (void)
{
  // s3c2410 - need to set data up before setting as output
  // s3c2412 - need to set data up after setting as output
  s3c2410_gpio_setpin (data_pin, 0);
  s3c2410_gpio_cfgpin (data_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (data_pin, 0);
}

/****************************************************************************
 * Release DATA. It will be pulled high
 ****************************************************************************/
static void
data_high (void)
{
  s3c2410_gpio_cfgpin (data_pin, S3C2410_GPIO_INPUT);
}

/****************************************************************************
 * Drive STRB low
 ****************************************************************************/
static void
strb_low (void)
{
  // s3c2410 - need to set data up before setting as output
  // s3c2412 - need to set data up after setting as output
  s3c2410_gpio_setpin (strb_pin, 0);
  s3c2410_gpio_cfgpin (strb_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (strb_pin, 0);
}

/****************************************************************************
 * Release STRB. It will be pulled high
 ****************************************************************************/
static void
strb_high (void)
{
  s3c2410_gpio_cfgpin (strb_pin, S3C2410_GPIO_INPUT);
}

/****************************************************************************
 * Drive ACK low and 'disable' interrupt
 ****************************************************************************/
static void
ack_low (void)
{
  // s3c2410 - need to set data up before setting as output
  // s3c2412 - need to set data up after setting as output
  s3c2410_gpio_setpin (ack_pin, 0);
  s3c2410_gpio_cfgpin (ack_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (ack_pin, 0);
}

/****************************************************************************
 * Release ACK and 'disable' interrupt. It will be pulled high
 ****************************************************************************/
static void
ack_high (void)
{
  s3c2410_gpio_cfgpin (ack_pin, S3C2410_GPIO_INPUT);
}

/****************************************************************************
 * Drive ACK high
 ****************************************************************************/
static void drive_ack_high (void)
{
  s3c2410_gpio_setpin (ack_pin, 1);
  s3c2410_gpio_cfgpin (ack_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (ack_pin, 1);
}

/****************************************************************************
 * Send one bit of data to other side
 ****************************************************************************/
static void
send_bit (void)
{
  int bit = (shift_register & 0x8000) ? 1 : 0;

  shift_register <<= 1;
  bit_count ++;

  if (bit) {
    data_high ();
  } else {
    data_low ();
  }

  strb_low ();
}

/****************************************************************************
 * Change state and set up interrupt mask
 ****************************************************************************/
static void change_state (dsa_state_t new_state)
{
  int data_event_enabled = 0;
  int ack_event_enabled = 0;
  int strb_event_enabled = 0;

  switch (new_state)
  {
    case DSA_INIT:
      QDBG("->INIT"); 
      break;
    case DSA_IDLE:
      QDBG("->IDLE"); 
      data_event_enabled = 1;
      break;
    case DSA_RX_SYNC:
      QDBG("->RX_SYNC"); 
      data_event_enabled = 1;
      break;
    case DSA_RX_BIT:
      QDBG("->RX_BIT"); 
      ack_event_enabled = 1;
      strb_event_enabled = 1;
      break;
    case DSA_RX_BIT_ACK:
      QDBG("->RX_BIT_ACK"); 
      strb_event_enabled = 1;
      break;
    case DSA_RX_FINAL_ACK:
      QDBG("->RX_FINAL_ACK"); 
      ack_event_enabled = 1;
      break;
    case DSA_TX_SYNC:
      QDBG("->TX_SYNC"); 
      ack_event_enabled = 1;
      break;
    case DSA_TX_SYNC2:
      QDBG("->TX_SYNC2");
      ack_event_enabled = 1;
      break;
    case DSA_TX_BIT_ACK:
      QDBG("->TX_BIT_ACK");
      ack_event_enabled = 1;
      break;
    case DSA_TX_BIT_ACK2:
      QDBG("->TX_BIT_ACK2");
      ack_event_enabled = 1;
      break;
    case DSA_TX_FINAL_ACK:
      QDBG("->TX_FINAL_ACK");
      strb_event_enabled = 1;
      break;
    case DSA_TX_FINAL_ACK_CLEAR:
      QDBG("->TX_FINAL_ACK_CLEAR");
      strb_event_enabled = 1;
      break;
    case DSA_TX_FINAL_ACK_CLEAR_RETRY:
      QDBG("->TX_FINAL_ACK_CLEAR_RETRY");
      strb_event_enabled = 1;
      break;
  }

  enable_pin_event(DATA_BIT, data_event_enabled);
  enable_pin_event(ACK_BIT, ack_event_enabled);
  enable_pin_event(STRB_BIT, strb_event_enabled);

  dsa_state = new_state;
}

/****************************************************************************
 * Restart the transfer timeout timer
 ****************************************************************************/
static inline void restart_timeout_timer (void)
{
  mod_timer (&timeout, jiffies + (HZ / 4));
}

/****************************************************************************
 * DSA state machine
 ****************************************************************************/
static void
dsa_state_machine (int event)
{
  int do_loop;

  // Check for timeout
  if (event == DSA_TIMEOUT) 
  {
    QDBG("TIMEOUT");
    DBG ("timeout in state %d bc=%d\n", dsa_state, bit_count);

    // Catch first timeout
#ifdef QDEBUG_RX
    if (in_a_rx_state())
    {
      rx_timeout_count++;
      qdebug_suspend (1);
      DBG ("  (rx state)\n");
    }
#endif

#ifdef QDEBUG_TX
    if (in_a_tx_state())
    { 
      tx_timeout_count++;
      qdebug_suspend (1);
      DBG ("  (tx state) %d\n", tx_timeout_count);
    }
#endif

    dsa_state = DSA_INIT;
  }

 loop:
  do_loop = 0;

  switch (dsa_state) 
  {
  case DSA_INIT:
    del_timer (&timeout);

    change_state(DSA_IDLE);
    strb_high ();
    ack_high ();

    // For another loop in case there is any tx data pending
    do_loop = 1;
    event = DSA_NO_EVENT;
    break;

  // Bus is in IDLE state. All signals are high.
  case DSA_IDLE:
    if (timer_pending (&timeout)) 
    {
      DBG ("timer pending in idle state?!\n");
      del_timer (&timeout);
    }

    if (event == DSA_DATA_LOW) 
    {
      // Seen low level on DATA for start of sync.  Drive ACK low and wait
      // for transmitter to raise DATA
      change_state(DSA_RX_SYNC);
      ack_low ();
      restart_timeout_timer ();
    }
    else if (tx_fifo_write_index != tx_fifo_read_index)
    {
      // We drive data low and wait for other side to respond with ACK low
      change_state(DSA_TX_SYNC);
      restart_timeout_timer ();
      strb_high ();
      data_low ();
    }
    break;

  // Other side has set DATA low and we have responded by setting ACK low.
  // We are expecting the other side to set DATA high next
  case DSA_RX_SYNC:
    if (event == DSA_DATA_HIGH) 
    {
      // Seen high level on DATA for end of sync.  Release ACK and wait for
      // transmitter to start sending bits

      bit_count = 0;
      shift_register = 0;
      restart_timeout_timer ();
      change_state(DSA_RX_BIT);
      ack_high ();
    }
    break;

  // The rx sync sequence has completed. Now ready to receive data
  case DSA_RX_BIT:
    if (event == DSA_STRB_LOW) 
    {
      restart_timeout_timer ();
      int data_bit = get_level_data_pin();
      shift_register = (shift_register << 1) | data_bit;
      bit_count++;
      change_state(DSA_RX_BIT_ACK);
      ack_low ();
    } 
    else if (event == DSA_ACK_LOW) 
    {
      change_state(DSA_RX_FINAL_ACK);
      restart_timeout_timer ();

      if (bit_count != 16)
        data_low ();

      udelay (1);
      strb_low ();
    }
    break;

  case DSA_RX_BIT_ACK:
    if (event == DSA_STRB_HIGH) 
    {
      // XX
      // Driving ACK and delaying seems to be only way of making RX transfers 
      // reliable. 
      drive_ack_high();
      udelay(200);

      change_state(DSA_RX_BIT);
      restart_timeout_timer ();
    }
    break;

  case DSA_RX_FINAL_ACK:
    if (event == DSA_ACK_HIGH) 
    {
      // Reset log buffer at end of each successful rx transfer
#ifdef QDEBUG_RX
      qdebug_reset();
#endif

      change_state(DSA_INIT);
      do_loop = 1;
      event = DSA_NO_EVENT;

      strb_high ();
      ack_high ();
      udelay (1);
      DBG ("RX %x\n", shift_register);
      rx_ok_count++;

      // Write to rx buffer
      rx_fifo[rx_fifo_write_index] = shift_register;
      rx_fifo_write_index = (rx_fifo_write_index + 1) % RX_FIFO_SIZE;
    }
    break;

  case DSA_TX_SYNC:
    if (event == DSA_ACK_LOW) 
    {
      // Now set data high and wait for ACK from other side
      restart_timeout_timer ();
      change_state (DSA_TX_SYNC2);
      data_high ();
      strb_high ();
    }
    break;

  case DSA_TX_SYNC2:
    if (event == DSA_ACK_HIGH) 
    {
      // Set up tx data
      bit_count = 0;
      shift_register = tx_fifo[tx_fifo_read_index];

      // .. and start transitting it
      change_state (DSA_TX_BIT_ACK);
      restart_timeout_timer ();
      send_bit ();
    }
    break;

  case DSA_TX_BIT_ACK:
    if (event == DSA_ACK_LOW) 
    {
      change_state (DSA_TX_BIT_ACK2);
      restart_timeout_timer ();
      strb_high();
    }
    break;

  case DSA_TX_BIT_ACK2:
    if (event == DSA_ACK_HIGH) 
    {
      if (bit_count < 16) 
      {
        change_state (DSA_TX_BIT_ACK);
        restart_timeout_timer ();
        send_bit ();
      } 
      else 
      {
        change_state (DSA_TX_FINAL_ACK);
        restart_timeout_timer ();
        ack_low ();
        data_high ();
      }
    }
    break;

  case DSA_TX_FINAL_ACK:
    if (event == DSA_STRB_LOW) 
    {
      restart_timeout_timer ();
      int ack_val = get_level_data_pin ();
      if (ack_val)
        change_state (DSA_TX_FINAL_ACK_CLEAR);
      else
        change_state (DSA_TX_FINAL_ACK_CLEAR_RETRY);

      ack_high ();
    }
    break;

  case DSA_TX_FINAL_ACK_CLEAR:
  case DSA_TX_FINAL_ACK_CLEAR_RETRY:
    if (event == DSA_STRB_HIGH) 
    {
      if (dsa_state == DSA_TX_FINAL_ACK_CLEAR)
      {
        tx_ok_count++;
        DBG("  TX ok %d\n", tx_ok_count);
#ifdef TX_TEST_MODE_CONTINUOUS_TX
#else
        tx_fifo_read_index = (tx_fifo_read_index + 1) % TX_FIFO_SIZE;
#endif
      }
      else
      {
        tx_retry_count++;
        DBG("  TX retry %d\n", tx_retry_count);
      }

      change_state(DSA_INIT);
      do_loop = 1;
      event = DSA_NO_EVENT;

#ifdef QDEBUG_TX
      qdebug_reset();
#endif
    }
    break;
  }

  if (do_loop) 
  {
    event = DSA_NO_EVENT;
    goto loop;
  }
}

/****************************************************************************
 * Timeout expired
 ****************************************************************************/
static void timeout_func (unsigned long time)
{
  unsigned long flags;
  spin_lock_irqsave (&dsa_lock, flags);

  dsa_state_machine(DSA_TIMEOUT);

  spin_unlock_irqrestore (&dsa_lock, flags);
}

/****************************************************************************
 * Handle interrupt on any DSA pin
 ****************************************************************************/
static irqreturn_t
irq_handler (int irq, void *dev)
{
#ifdef QDEBUG
  char text[QDEBUG_TEXT_LENGTH];
#endif
  int data;
  int strb;
  int ack;
  int pin_levels = 0;;

  unsigned long flags;
  spin_lock_irqsave (&dsa_lock, flags);
  // .. interrupts disabled now

  // Read all pins
  data = get_level_data_pin();
  strb = get_level_strb_pin();
  ack = get_level_ack_pin();
  if (data)
    pin_levels |= (1 << DATA_BIT);
  if (strb)
    pin_levels |= (1 << STRB_BIT);
  if (ack)
    pin_levels |= (1 << ACK_BIT);

  // Work out event
  dsa_event_t event = DSA_NO_EVENT;
  if (irq == data_irq && pin_event_enabled(DATA_BIT))
  {
    if (data)
      event = DSA_DATA_HIGH;
    else
      event = DSA_DATA_LOW;
  }
  else if (irq == strb_irq && pin_event_enabled(STRB_BIT))
  {
    if (strb)
      event = DSA_STRB_HIGH;
    else
      event = DSA_STRB_LOW;
  }
  else if (irq == ack_irq && pin_event_enabled(ACK_BIT))
  {
    if (ack)
      event = DSA_ACK_HIGH;
    else
      event = DSA_ACK_LOW;
  }

#ifdef QDEBUG
  snprintf(text, QDEBUG_TEXT_LENGTH, "IRQ(%d) %d%d%d %d%d%d %d %s %d", 
           irq,
           data,
           strb,
           ack,
           pin_event_enabled(DATA_BIT),
           pin_event_enabled(STRB_BIT),
           pin_event_enabled(ACK_BIT),
           dsa_state,
           event_name[event],
           bit_count
          );
  QDBG(text);
#endif

  if (irqtest)
  {
    // Set levels on output pins to match inputs
    if (irq == data_irq)
      s3c2410_gpio_setpin (test_out1_pin, data);
    else if (irq == strb_irq)
      s3c2410_gpio_setpin (test_out2_pin, strb);
    else if (irq == ack_irq)
      s3c2410_gpio_setpin (test_out3_pin, ack);
  }
  else
  {
    dsa_state_machine (event);
  }

  spin_unlock_irqrestore (&dsa_lock, flags);
  // .. interrupt enabled again

  return IRQ_HANDLED;
}

   /*************************************************************************/
   /***                        File Operations                            ***/
   /*************************************************************************/

/****************************************************************************
 * Returns status of device. Indicates if there is data available to read
 ****************************************************************************/
static unsigned int 
dsa_poll (struct file *file, poll_table *wait)
{
  /* Message always available to read */
  return POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;
}

/****************************************************************************
 * Read data from device
 ****************************************************************************/
static int 
dsa_read (struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  int ret = 0;

  // Only accept two byte reads
  if ((count == 2) && (rx_fifo_write_index != rx_fifo_read_index))
  {
    // Extract data from rx buffer
    u16 data = rx_fifo[rx_fifo_read_index];
    rx_fifo_read_index = (rx_fifo_read_index + 1) % RX_FIFO_SIZE;

    // Copy data to user space
    u8 temp_buf[count];
    temp_buf[0] = data;
    temp_buf[1] = data >> 8;
    if (copy_to_user (buffer, temp_buf, count))
      ret = -EFAULT;
    else
      ret = count;
  }

  return ret;
}

/****************************************************************************
 * Write data to device
****************************************************************************/
static int 
dsa_write (struct file *filp, const char *buf, size_t count, loff_t *f_os)
{
  // We're expecting all servo commands to be 2 bytes
  // Anything with length != 2 might be a text command
  if (count != 2) 
  {
    char temp_buf[count+1];
    copy_from_user(temp_buf, buf, count);
    temp_buf[count] = 0;
    DBG ("WRITE %s\n", temp_buf);

    if (memcmp(".RESET", temp_buf, 6) == 0)
    {
      DBG ("  RESET\n");
      tx_fifo_write_index = 0;
      tx_fifo_read_index = 0;
      rx_fifo_write_index = 0;
      rx_fifo_read_index = 0;;

      s3c2410_gpio_setpin (reset_pin, 0);
      mdelay(1);
      s3c2410_gpio_setpin (reset_pin, 1);
    }
    else if (memcmp(".SLEEP", temp_buf, 6) == 0)
    {
      DBG ("  SLEEP\n");
      s3c2410_gpio_setpin (reset_pin, 0);
    }

    return count;
  }

  unsigned int fifo_space = TX_FIFO_SIZE - ((tx_fifo_write_index - tx_fifo_read_index) % TX_FIFO_SIZE) - 1;
  int words = min (fifo_space, count / 2);
  DBG ("WRITE %d %d\n", words, fifo_space);
  int i;

  for (i = 0; i < words; i++) 
  {
#ifdef TX_TEST_MODE_CONTINUOUS_TX
    u16 w = 0x0e03; // door open
    DBG ("  0x%04x\n", w);
#else
    u16 w;
    if (get_user (w, (u16 *)buf))
      return -EFAULT;

    DBG ("  0x%04x\n", w);
#endif

    tx_fifo[tx_fifo_write_index] = w;
    tx_fifo_write_index = (tx_fifo_write_index + 1) % TX_FIFO_SIZE;
    buf += 2;
  }

  unsigned long flags;
  spin_lock_irqsave (&dsa_lock, flags);
  dsa_state_machine (DSA_NO_EVENT);
  spin_unlock_irqrestore (&dsa_lock, flags);

  return words * 2;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int 
dsa_open (struct inode * inode, struct file * file)
{
  printk(PREFIX "open\n");
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int 
dsa_release (struct inode * inode, struct file * file)
{
  printk(PREFIX "release\n");
  return 0;
}

static struct file_operations dsa_fops =
{
  owner:    THIS_MODULE,
  read:     dsa_read,
  write:    dsa_write,
  poll:     dsa_poll,
  open:     dsa_open,
  release:  dsa_release,
};

static struct miscdevice dsa_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_dsa",
  &dsa_fops
};

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
dsa_init (void)
{
  printk (PREFIX "module loaded\n");
  printk (PREFIX "  irqtest=%d\n", irqtest);
  printk (PREFIX "  debug_level=%d\n", debug_level);
  printk (PREFIX "  release_reset=%d\n", release_reset);

  tx_ok_count = 0;
  tx_retry_count = 0;
  tx_timeout_count = 0;
  rx_ok_count = 0;
  rx_timeout_count = 0;

  qdebug_init();
  QDBG("init");

  init_waitqueue_head (&rx_wait_queue);
  init_waitqueue_head (&tx_wait_queue);

  reset_pin = S3C2410_GPE7;

  s3c2410_gpio_cfgpin (reset_pin, S3C2410_GPIO_OUTPUT);
  s3c2410_gpio_setpin (reset_pin, 0);

  // Set up any test output pins
  if (irqtest)
  {
    test_out1_pin = S3C2410_GPE10; // J1-3
    test_out2_pin = S3C2410_GPE9;  // J1-5
    test_out3_pin = S3C2410_GPE8;  // J1-7

    // Enable pullups on Barracuda, disable pulldowns on Stingray
    int pull = machine_is_rirm2() ? 0 : 1;
    s3c2410_gpio_pullup (test_out1_pin, pull);
    s3c2410_gpio_pullup (test_out2_pin, pull);
    s3c2410_gpio_pullup (test_out3_pin, pull);

    // Set high
    s3c2410_gpio_cfgpin (test_out1_pin, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_setpin (test_out1_pin, 1);
    s3c2410_gpio_cfgpin (test_out2_pin, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_setpin (test_out2_pin, 1);
    s3c2410_gpio_cfgpin (test_out3_pin, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_setpin (test_out3_pin, 1);
  }

  data_pin = S3C2410_GPF2;

  if (machine_is_rirm3 ()) 
  {
    printk (PREFIX "  STB=GPG2\n");
    printk (PREFIX "  ACK=GPF5\n");
    strb_pin = S3C2410_GPG2;
    ack_pin = S3C2410_GPF5;
  } 
  else 
  {
    printk (PREFIX "  STB=GPG7\n");
    printk (PREFIX "  ACK=GPF4\n");
    strb_pin = S3C2410_GPG7;
    ack_pin = S3C2410_GPF4;
  }

  data_irq = gpio_to_irq (data_pin);
  strb_irq = gpio_to_irq (strb_pin);
  ack_irq = gpio_to_irq (ack_pin);

  // Set up IRQ trigger types
  set_irq_type(data_irq, IRQT_BOTHEDGE);
  set_irq_type(strb_irq, IRQT_BOTHEDGE);
  set_irq_type(ack_irq, IRQT_BOTHEDGE);

  // Enable filters.
  // Interrupts seem to get missed on s3c2412 if they are disabled
  unsigned long extint0 = (unsigned long)S3C2410_EXTINT0;
  unsigned long extint1 = (unsigned long)S3C2410_EXTINT1;
  unsigned long extint2 = (unsigned long)S3C2410_EXTINT2;
  if (machine_is_rirm3 ()) 
  {
    // Addresses changed on s3c2412
    extint0 += 0x10;
    extint1 += 0x10;
    extint2 += 0x10;
  }
  unsigned long data0 = __raw_readl(extint0);
  unsigned long data1 = __raw_readl(extint1);
  unsigned long data2 = __raw_readl(extint2);

  if (machine_is_rirm3 ()) 
  {
    data0 |= (0x8 << 8);  // J2-12 = GPF2 = EINT2
    data0 |= (0x8 << 20); // J2-25 = GPF5 = EINT5
    data1 |= (0x8 << 8);  // J2-7  = GPG2 = EINT10
  }
  else
  {
    // J2-12 = GPF2 = EINT2 (no filter available)
    // J2-25 = GPF4 = EINT4 (no filter available)
    // J2-25 = GPG7 = EINT15 (no filter available)
  }

  __raw_writel(data0, extint0);
  __raw_writel(data1, extint1);
  __raw_writel(data2, extint2);
  // .. filters enabled now

  // Enable pullups on Barracuda, disable pulldowns on Stingray
  int pull = machine_is_rirm2() ? 0 : 1;
  s3c2410_gpio_pullup (data_pin, pull);
  s3c2410_gpio_pullup (strb_pin, pull);
  s3c2410_gpio_pullup (ack_pin, pull);

  // Configure all gpio as IRQ
  if (machine_is_rirm3 ()) 
  {
    s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG2_EINT10);
    s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF5_EINT5);
  }
  else
  {
    s3c2410_gpio_cfgpin(strb_pin, S3C2410_GPG7_EINT15);
    s3c2410_gpio_cfgpin(ack_pin, S3C2410_GPF4_EINT4);
  }
  s3c2410_gpio_cfgpin(data_pin, S3C2410_GPF2_EINT2);

  misc_register (&dsa_miscdev);

  init_timer (&timeout);
  timeout.function = timeout_func;

  // Initialise state machine
  unsigned long flags;
  spin_lock_irqsave (&dsa_lock, flags);
  dsa_state_machine (DSA_NO_EVENT);
  spin_unlock_irqrestore (&dsa_lock, flags);

  // Maybe hold cd mech in reset until we get a RESET command
  if (release_reset)
    s3c2410_gpio_setpin (reset_pin, 1);

  printk (PREFIX "  Requesting IRQs\n");
  if (request_irq (data_irq, irq_handler, 0, "reciva_dsa DATA", NULL)) {
    printk ("couldn't request data interrupt line\n");
  }
  if (request_irq (strb_irq, irq_handler, 0, "reciva_dsa STRB", NULL)) {
    printk ("couldn't request strb interrupt line\n");
  }
  if (request_irq (ack_irq, irq_handler, 0, "reciva_dsa ACK", NULL)) {
    printk ("couldn't request ack interrupt line\n");
  }

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
dsa_exit (void)
{
  printk (PREFIX "module unloaded\n");
  del_timer (&ticker);
  del_timer (&timeout);

  free_irq (data_irq, NULL);
  free_irq (strb_irq, NULL);
  free_irq (ack_irq, NULL);

  /* Unregister the device */
  misc_deregister (&dsa_miscdev);

  printk (PREFIX "  tx_ok_count = %d\n", tx_ok_count);
  printk (PREFIX "  tx_retry_count = %d\n", tx_retry_count);
  printk (PREFIX "  tx_timeout_count = %d\n", tx_timeout_count);
  printk (PREFIX "  rx_ok_count = %d\n", rx_ok_count);
  printk (PREFIX "  rx_timeout_count = %d\n", rx_timeout_count);

  // Dump quick debug log
  qdebug_dump();
}

module_init (dsa_init);
module_exit (dsa_exit);

MODULE_LICENSE("GPL");
