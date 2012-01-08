#include <linux/module.h>
#include <linux/init.h>

#include <linux/autoconf.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/console.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>
#include <asm/mach-types.h>

#include "lirc.h"
#include "kcompat.h"
#include "lirc_dev.h"

static int irq;
static int minor;

static struct lirc_buffer rbuf;
static struct timeval lasttv = {0, 0};

static unsigned int *extint_reg = S3C2410_EXTINT1;
static int ext_shift = 24;
static unsigned int input_pin = S3C2410_GPG6;
static unsigned int int_pin_function = S3C2410_GPG6_EINT14;

#define RBUF_LEN 256

static void inline rbwrite(lirc_t l)
{
  if(lirc_buffer_full(&rbuf))    /* no new signals will be accepted */
  {
    printk("Buffer overrun: %08x\n", l);
    return;
  }
  //printk("Write: %08x\n", l);
  _lirc_buffer_write_1(&rbuf, (void *)&l);
  wake_up_interruptible(&rbuf.wait_poll);
}

static int last_d;

static irqreturn_t lirc_irqfunc (int irq, void *dev_id)
{
  int d, deltv;
  lirc_t data;
  struct timeval tv;

  /* get current time */
  do_gettimeofday(&tv);

  if (machine_is_rirm3 ()) {
          unsigned long v;
          // S3C2412/13 insists we change pin config to input before reading them
          s3c2410_gpio_cfgpin (input_pin, S3C2410_GPIO_INPUT);
          d = s3c2410_gpio_getpin (input_pin) ? 1 : 0;

          if (d == last_d) {
                  s3c2410_gpio_cfgpin (input_pin, int_pin_function);
                  return IRQ_HANDLED;
          }

          last_d = d;
          
          v = __raw_readl (extint_reg);
          v &= ~(7 << ext_shift);
          v |= (d ? 0 : 1) << ext_shift;                // Flip levels
          __raw_writel (v, extint_reg);

          s3c2410_gpio_cfgpin (input_pin, int_pin_function);
  } else {
          d = s3c2410_gpio_getpin (input_pin) ? 1 : 0;
  }

  deltv = tv.tv_sec - lasttv.tv_sec;
  if (deltv > 15)
    data = PULSE_MASK;
  else
    data=(lirc_t) (deltv*1000000+
		   tv.tv_usec - lasttv.tv_usec);
  
  lasttv=tv;

  if (d)
    data |= PULSE_BIT;

  rbwrite (data);

  return IRQ_HANDLED;
}

static int set_use_inc(void* data)
{
  int r;

  MOD_INC_USE_COUNT;

  r = request_irq (irq, lirc_irqfunc, SA_INTERRUPT, "lirc", NULL);
  if (r < 0) {
    printk("Couldn't request irq %d\n", r);
    return r;
  }

  /* initialize timestamp */
  do_gettimeofday(&lasttv);

  return 0;
}

static void set_use_dec(void* data)
{
  free_irq (irq, NULL);

  MOD_DEC_USE_COUNT;
}

#define LIRC_DRIVER_NAME "lirc_barracuda"

static struct lirc_plugin plugin = {
	name:		LIRC_DRIVER_NAME,
	minor:		-1,
	code_length:	1,
	sample_rate:	0,
	rbuf:		&rbuf,
	.set_use_inc	= set_use_inc,
	.set_use_dec	= set_use_dec,
	.features = LIRC_CAN_REC_MODE2,
  owner:  THIS_MODULE,
};

static int
lirc_barracuda_init (void)
{
  int r;
  unsigned long v;

  if (!machine_is_rirm2 () && !machine_is_rirm3 ())
    return -ENODEV;

  /* redundant at present but maybe needed in the future */
  if (machine_is_rirm3 ())
  {
    extint_reg = S3C2410_EXTINT1 + 0x10;
    ext_shift = 4;
    input_pin = S3C2410_GPG1;
    int_pin_function = S3C2410_GPG1_EINT9;
  }

  s3c2410_gpio_cfgpin (input_pin, int_pin_function);
  s3c2410_gpio_pullup (input_pin, machine_is_rirm2() ? 0 : 1 );

  v = __raw_readl (extint_reg);
  v &= ~(7 << ext_shift);
  if (machine_is_rirm3 ())
  {
    v |= 0 << ext_shift;                // Low level
  }
  else
  {
    v |= 6 << ext_shift;
  }
  __raw_writel (v, extint_reg);

  last_d = 1;

  irq = s3c2410_gpio_getirq (input_pin);
  if (irq < 0)
  {
    printk("Couldn't register interrupt %d\n", irq);
    return -ENODEV;
  }

  /* Init read buffer. */
  if (lirc_buffer_init(&rbuf, sizeof(lirc_t), RBUF_LEN) < 0)
    return -ENOMEM;

  r = lirc_register_plugin(&plugin);
  if (r < 0) {
    printk("Couldn't register plugin %d\n", r);
    lirc_buffer_free(&rbuf);
    return r;
  }

  minor = r;

  return 0;
}

static void
lirc_barracuda_exit (void)
{
  lirc_buffer_free(&rbuf);
  lirc_unregister_plugin(minor);
}

module_init (lirc_barracuda_init);
module_exit (lirc_barracuda_exit);

MODULE_LICENSE("GPL");
