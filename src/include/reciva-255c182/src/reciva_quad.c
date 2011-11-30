/* Quadrature driver */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/input.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/fiq.h>
#include <asm/arch/map.h>
#include <asm/hardware/s3c2410/timer.h>
#include <asm/hardware/s3c2410/irq.h>

#include "reciva_quad.h"

   /*************************************************************************/
   /***                     Static function declarations                  ***/
   /*************************************************************************/

static int quad_pm_callback(struct pm_dev *dev,pm_request_t rqst,void *data);
static void start_fiq(void);
static void stop_fiq (void);
static void do_quad (unsigned long l);

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

static struct pm_dev *pm_dev;
static struct input_dev *input_dev;
static struct timer_list timer;

volatile unsigned long reciva_quad_data;

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 **
 ** NAME:              register_quad
 **
 ** PARAMETERS:        None
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Module initialisation
 **
 ****************************************************************************/

static int __init
register_quad (void)
{
  struct pt_regs regs;
  extern void quad_fiq_start, quad_fiq_end;

  input_dev = kmalloc (sizeof (*input_dev), GFP_KERNEL);
  memset (input_dev, 0, sizeof (*input_dev));

  /* Set up input system */
#ifdef KERNEL_26
  init_input_dev (input_dev);
#endif
  input_dev->evbit[0] = BIT(EV_REL);
  set_bit (REL_Y, input_dev->relbit);
  input_dev->name = "reciva_quad";

  input_register_device (input_dev);

  pm_dev = pm_register (PM_SYS_DEV, PM_SYS_UNKNOWN, quad_pm_callback);

  get_fiq_regs (&regs);

  regs.uregs[11] = 0;
  regs.uregs[10] = -1;
  regs.uregs[9] = 0;

  set_fiq_handler (&quad_fiq_start, &quad_fiq_end - &quad_fiq_start);
  set_fiq_regs (&regs);

  start_fiq ();

  /* Initialise and set debounce timer */
  init_timer (&timer);
  timer.function = do_quad;
  timer.expires = jiffies + 1;
  add_timer (&timer);

  return 0;
}

/****************************************************************************
 **
 ** NAME:              unregister_quad
 **
 ** PARAMETERS:        None
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Module cleanup
 **
 ****************************************************************************/

static void __exit
unregister_quad (void)
{
  del_timer (&timer);
  stop_fiq ();

  input_unregister_device (input_dev);
  kfree (input_dev);
}

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 **
 ** NAME:              do_quad
 **
 ** PARAMETERS:        None
 **
 ** RETURN VALUES:     None
 **
 ** DESCRIPTION:       Checks for changes in quad value. Called on a timer
 **                    interrupt. Two messages are sent to the application
 **                    per click detected. This is how it worked on lubbock
 **                    board. Maybe change this to only send 1 per click.
 **
 ** Rotary Encoder Operation :
 **
 ** A clockwise click produces a 2 step state change in the rotary encoder
 ** outputs from 00 to 11 or 11 to 00 as shown below.
 **                      ______
 **   A ________________|      |________
 **                   ______
 **   B _____________|      |___________
 **
 **
 **   An anticlockwise click produces a 2 step state change in the rotary encoder
 **   outputs from 00 to 11 or 11 to 00 as shown below.
 **                   ______
 **   A _____________|      |___________
 **                      ______
 **   B ________________|      |________
 **  
 **
 ****************************************************************************/

static void 
do_quad (unsigned long l)
{
  static int up_count = 0;
  static int down_count = 0;

  l = l; /* Argument unused - eliminate compiler warning */

  switch (reciva_quad_data)
    {
    case 0:
      break;

    case 1:
      reciva_quad_data = 0;

      /* Only send one event per click */
      if (up_count)
	{
	  up_count = 0;
	  input_report_rel (input_dev, REL_Y, -1);
	}
      else
        up_count++;
      break;
    case 2:
      reciva_quad_data = 0;

      /* Only send one event per click */
      if (down_count)
	{
	  down_count = 0;
	  input_report_rel (input_dev, REL_Y, +1);
	}
      else
        down_count++;
      break;
    }

  timer.expires = jiffies + 1;
  add_timer (&timer);
}

static void 
start_fiq (void)
{
  unsigned long flags, tcon, tcnt, imod, imsk;

  save_flags_cli (flags);

  imsk = __raw_readl(S3C2410_INTMSK);
  imsk |= (1 << 13);
  __raw_writel(imsk, S3C2410_INTMSK);

  imod = __raw_readl(S3C2410_INTMOD);
  imod &= ~(1 << 13);
  __raw_writel(imod, S3C2410_INTMOD);

  tcnt = (12*(1000*1000)) / 4000;

  tcon = __raw_readl(S3C2410_TCON);
  tcon &= ~(7<<16);
  tcon |= S3C2410_TCON_T3RELOAD;
  tcon |= S3C2410_TCON_T3MANUALUPD;
  __raw_writel(tcon, S3C2410_TCON);

  __raw_writel(tcnt, S3C2410_TCNTB(3));
  __raw_writel(tcnt, S3C2410_TCMPB(3));

  tcon |= S3C2410_TCON_T3START;
  tcon &= ~S3C2410_TCON_T3MANUALUPD;
  __raw_writel(tcon, S3C2410_TCON);

  imod = __raw_readl(S3C2410_INTMOD);
  imod |= 1 << 13;
  __raw_writel(imod, S3C2410_INTMOD);

  imsk = __raw_readl(S3C2410_INTMSK);
  imsk &= ~(1 << 13);
  __raw_writel(imsk, S3C2410_INTMSK);

  restore_flags (flags);
}

static void
stop_fiq (void)
{
  unsigned long flags, tcon, tcnt, imod, imsk;

  save_flags_cli (flags);

  imsk = __raw_readl(S3C2410_INTMSK);
  imsk |= (1 << 13);
  __raw_writel(imsk, S3C2410_INTMSK);

  imod = __raw_readl(S3C2410_INTMOD);
  imod &= ~(1 << 13);
  __raw_writel(imod, S3C2410_INTMOD);

  tcnt = (12*(1000*1000)) / 4000;

  tcon = __raw_readl(S3C2410_TCON);
  tcon &= ~(7<<16);
  __raw_writel(tcon, S3C2410_TCON);

  restore_flags (flags);
}

static int 
quad_pm_callback(struct pm_dev *dev,pm_request_t rqst,void *data)
{
  if (rqst == PM_RESUME) 
    {
      printk("quad: restarting FIQs\n");
      start_fiq ();
    }
  return 0;
}

module_init(register_quad);
module_exit(unregister_quad);
