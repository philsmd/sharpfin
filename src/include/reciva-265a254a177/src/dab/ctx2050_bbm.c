/*
 * CTX2050 DAB Modem Control
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>

#include <asm/uaccess.h>
#ifdef LEVEL_TRIGGER
# include <asm/io.h>
# include <asm/arch/regs-gpio.h>
# include <asm/hardware.h>
# include <asm/arch/regs-irq.h>
#endif

#include "ctx2050.h"
#include "ctx2050_spi.h"
#include "ctx2050_regs.h"
#include "ctx2050_bbm.h"

#define PREFIX "CTX2050_BBM: "

static wait_queue_head_t fic_wait_queue;
static wait_queue_head_t msc_wait_queue;

#define DATA_SIZE (128 * 1024)
#define DATA_MASK ((128 * 1024) - 1)

typedef struct
{
  unsigned char *buffer;
  unsigned int in;
  unsigned int out;
  int reader;
} cbuffer;

static cbuffer fic_data;
static cbuffer msc_data;
static int currentSubChId;

   /*************************************************************************/
   /***                        MSC Buffer - START                         ***/
   /*************************************************************************/

int bbm_dump_msc_data(void)
{
  unsigned int ptr = msc_data.out - (5*1024);
  int c;
  printk("MSC_DATA:\n");
  for (c=0; c<5*1024; c++)
  {
    unsigned int i = ptr & DATA_MASK;
    ptr++;
    printk("%02x ", msc_data.buffer[i]);
    if (c%16 == 15)
    {
      printk("\n");
      msleep(1);
    }
  }
  return 0;
}

// XXX Once everything's working happily remove the copied code for the different buffers
static int msc_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  // claim lock
  while (msc_data.in == msc_data.out)
  {
    //printk("BLOCK!\n");
    // release lock as we're about to sleep

    // Handle non-blocking access
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }

    if (wait_event_interruptible(msc_wait_queue, (msc_data.in != msc_data.out)))
    {
      return -ERESTARTSYS;
    }

    // acquire lock before looping
  }

  unsigned int size = min(count, (msc_data.in - msc_data.out) & DATA_MASK);
  unsigned int len = min(size, (DATA_SIZE - (msc_data.out & DATA_MASK)));
  if (copy_to_user((void __user *)buffer,
                   msc_data.buffer + (msc_data.out & DATA_MASK),
                   len))
  {
    printk(PREFIX "Couldn't return data!\n");
    return -EFAULT;
  }
  if (size - len)
  {
    if (copy_to_user((void __user *)(buffer + len), msc_data.buffer, size - len))
    {
      printk(PREFIX "Couldn't return data!\n");
      return -EFAULT;
    }
  }
  msc_data.out += size;
  // release lock

  // If returning 0 bytes, implies that input has caught up with output
  if (size == 0)
  {
    printk("%s count %d size %d len %d data.out %d data.in %d\n", __FUNCTION__,
     count, size, len, (msc_data.out & DATA_MASK), (msc_data.in & DATA_MASK));
  }
  return size;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int msc_open(struct inode * inode, struct file * file)
{
  printk(PREFIX "%s non-blocking %d\n", __FUNCTION__, file->f_flags & O_NONBLOCK);
  msc_data.in = 0;
  msc_data.out = 0;
  msc_data.reader = 1;
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int msc_release(struct inode * inode, struct file * file)
{
  printk(PREFIX "%s\n", __FUNCTION__);
  printk(PREFIX "in %d out %d\n", msc_data.in, msc_data.out);
  msc_data.reader = 0;
  return 0;
}


static struct file_operations msc_data_fops =
{
  owner:    THIS_MODULE,
  read:     msc_read,
  open:     msc_open,
  release:  msc_release,
};

static struct miscdevice msc_data_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_dab_data",
  &msc_data_fops
};

   /*************************************************************************/
   /***                        MSC Buffer - END                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        FIC Buffer - START                         ***/
   /*************************************************************************/

static int fic_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
  // claim lock
  while (fic_data.in == fic_data.out)
  {
    // release lock as we're about to sleep
    // printk(PREFIX "No data\n");

    // Handle non-blocking access
    if (filp->f_flags & O_NONBLOCK)
    {
      return -EAGAIN;
    }

    if (wait_event_interruptible(fic_wait_queue, (fic_data.in != fic_data.out)))
    {
      return -ERESTARTSYS;
    }

    // acquire lock before looping
  }

  unsigned int size = min(count, (fic_data.in - fic_data.out) & DATA_MASK);
  unsigned int len = min(size, (DATA_SIZE - (fic_data.out & DATA_MASK)));
  if (copy_to_user((void __user *)buffer,
                   fic_data.buffer + (fic_data.out & DATA_MASK),
                   len))
  {
    printk(PREFIX "Couldn't return data!\n");
    return -EFAULT;
  }
  if (size - len)
  {
    if (copy_to_user((void __user *)(buffer + len), fic_data.buffer, size - len))
    {
      printk(PREFIX "Couldn't return data!\n");
      return -EFAULT;
    }
  }
  fic_data.out += size;
  // release lock

  //printk("%s count %d size %d len %d data.out %d data.in %d\n", __FUNCTION__, count, size, len, (data.out & DATA_MASK), (data.in & DATA_MASK));
  return size;
}

/****************************************************************************
 * Open device. This is the first operation performed on the file structure.
 ****************************************************************************/
static int fic_open(struct inode * inode, struct file * file)
{
  printk(PREFIX "%s\n", __FUNCTION__);
  return 0;
}

/****************************************************************************
 * Release file structure. Invoked when the file structure is being released
 ****************************************************************************/
static int fic_release(struct inode * inode, struct file * file)
{
  printk(PREFIX "%s\n", __FUNCTION__);
  return 0;
}

static unsigned int fic_poll(struct file *file, poll_table *wait)
{
  poll_wait(file, &fic_wait_queue, wait);
  if (fic_data.in != fic_data.out)
  {
    return POLLIN | POLLRDNORM;
  }

  return 0;
}


static void read_fic(int *reg_addr, unsigned char *buffer, int len)
{
  int toread = len;
  if (*reg_addr < 0x500)
  {
    toread = min(len, 0x500 - *reg_addr);
    spireg_read(*reg_addr, buffer, toread);
    buffer += toread;
    *reg_addr += toread;
    if (*reg_addr > 0x4ff)
      *reg_addr = 0x510;
  }
  if (len - toread)
  {
    spireg_read(*reg_addr, buffer, len - toread);
  }
}

static void read_msc(int reg_addr, int len)
{
  int toread = min((unsigned int)len, (DATA_SIZE - (msc_data.in & DATA_MASK)));
  if (toread)
  {
    spireg_read(reg_addr, msc_data.buffer + (msc_data.in & DATA_MASK), toread);
    reg_addr += toread;
  }
  if (len - toread)
  {
    spireg_read(reg_addr, msc_data.buffer, len - toread);
  }
  msc_data.in += len;
}

void bbm_reset_fic(void)
{
  fic_data.in = 0;
  fic_data.out = 0;
}

static struct file_operations fic_data_fops =
{
  owner:    THIS_MODULE,
  read:     fic_read,
  open:     fic_open,
  release:  fic_release,
  poll:     fic_poll,
};

static struct miscdevice fic_data_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "reciva_dab_fic",
  &fic_data_fops
};

   /*************************************************************************/
   /***                        FIC Buffer - END                           ***/
   /*************************************************************************/

int bbm_init_buffers(void)
{
  init_waitqueue_head (&fic_wait_queue);
  init_waitqueue_head (&msc_wait_queue);

  fic_data.buffer = kmalloc(DATA_SIZE, GFP_KERNEL);
  if (fic_data.buffer == NULL)
  {
    printk("Couldn't alloc FIC data buffer\n");
    return -ENOMEM;
  }
  fic_data.reader = 0;

  msc_data.buffer = kzalloc(DATA_SIZE, GFP_KERNEL);
  if (msc_data.buffer == NULL)
  {
    printk("Couldn't alloc MSC data buffer\n");
    return -ENOMEM;
  }
  msc_data.reader = 0;

  misc_register(&fic_data_miscdev);
  misc_register(&msc_data_miscdev);

  return 0;
}

void bbm_release_buffers(void)
{
  kfree(fic_data.buffer);
  kfree(msc_data.buffer);

  misc_deregister(&msc_data_miscdev);
  misc_deregister(&fic_data_miscdev);
}

void bbm_interrupt_handler(struct work_struct *work)
{
  u16 intStatus;
#ifdef LEVEL_TRIGGER
  while (!s3c2410_gpio_getpin(INTBB))
#else
  while (1)
#endif
  {
    spireg_read16(BBM_INT_STATUS_REG, &intStatus);
    if (!intStatus)
    {
#ifdef LEVEL_TRIGGER
      spireg_write16(BBM_INT_CLEAR_REG, 0);
      continue;
#else
      break;
#endif
    }
    spireg_write16(BBM_INT_CLEAR_REG, intStatus);
    spireg_write16(BBM_INT_CLEAR_REG, 0); // because FCI say so

    if (intStatus & BBM_SPI_INT)
    {
#if 0
      u8 sch_status;
      spireg_read8(0xfa, &sch_status);
      printk("SCH_STATUS %02x\n", sch_status);
#endif
      u16 bufferCnt;
      spireg_read16(0xfc, &bufferCnt);

      // CTX2050 appears to get upset if we don't read the data when it's
      // available, so do the transfer regardless of whether we currently
      // have a read for the data.
      if(bufferCnt)
      {
        //printk(PREFIX "MSC %d ", bufferCnt);
        //unsigned long read_start = jiffies;
        // get lock on data
        int len = min((unsigned int)bufferCnt, (DATA_SIZE - (msc_data.in & DATA_MASK)));
        //printk("%s size %d data.in %d len %d\n", __FUNCTION__, bufferCnt, (data.in & DATA_MASK), len);
        if (len)
        {
          spidata_read(msc_data.buffer + (msc_data.in & DATA_MASK), len);
        }
        if (bufferCnt - len)
        {
          spidata_read(msc_data.buffer, bufferCnt - len);
        }
        //unsigned long read_end = jiffies;
        msc_data.in += bufferCnt;
        // release lock on data
        if (msc_data.reader)
        {
          wake_up_interruptible(&msc_wait_queue);
        }
        //printk("%lu %lu %lu\n", get_last_int_time(), read_start - get_int_time(), read_end - read_start);
      }
    }

    if (intStatus & BBM_FIC_INT)
    {
      u16 bufferCnt;
      spireg_read16(FIC_DDEPTH_REG, &bufferCnt);
      //printk(PREFIX "FIC data %d\n", bufferCnt);

      if(bufferCnt)
      {
        int fic_addr = 0x410;
        // get lock on data
        int len = min((unsigned int)bufferCnt, (DATA_SIZE - (fic_data.in & DATA_MASK)));
        //printk("%s size %d data.in %d len %d\n", __FUNCTION__, bufferCnt, (data.in & DATA_MASK), len);
        if (len)
        {
          read_fic(&fic_addr, fic_data.buffer + (fic_data.in & DATA_MASK), len);
        }
        if (bufferCnt - len)
        {
          read_fic(&fic_addr, fic_data.buffer, bufferCnt - len);
        }
        fic_data.in += bufferCnt;
        // release lock on data
        wake_up_interruptible(&fic_wait_queue);
      }
    }

    if (intStatus & BBM_MSC_INT)
    {
      u8 subChId;
      spireg_read8(MSC_SCHID_REG, &subChId);
      spireg_write8(MSC_HW_BANK_SEL, (subChId & 0x40) >> 6);
      subChId &= 0x3F;

      if (subChId == currentSubChId)
      {
        u16 bufferCnt;
        spireg_read16(MSC_DDEPTH_REG, &bufferCnt);

        if(bufferCnt)
        {
          int addr = 0x710;
          while (bufferCnt >= 0xF0)
          {
            read_msc(addr, 0xF0);
            addr += 0x100;
            bufferCnt -= 0xF0;
          }
          if (bufferCnt)
          {
            read_msc(addr, bufferCnt);
          }

          if (msc_data.reader)
          {
            wake_up_interruptible(&msc_wait_queue);
          }
        }
      }
    }

  }

#ifdef LEVEL_TRIGGER
  // Re-enable interrupt on low
  printk("%08x\n", __raw_readl(S3C2412_EINTPEND));
  s3c2410_gpio_cfgpin(INTBB, S3C2410_GPF3_EINT3);
#endif
}

void bbm_reset(void)
{
  spireg_write8(BBM_SW_RESET_REG, 0xFE);
  msleep(1);
  spireg_write8(BBM_SW_RESET_REG, 0xF7);
}

static int bbm_probe(void)
{
  u16  timeout, data;
  for(timeout=10; timeout; timeout--)
  {
    spireg_write16(BBM_INT_MASK_REG, timeout);
    msleep(10);
    spireg_read16(BBM_INT_MASK_REG, &data);
    if(data == timeout)
    {
      printk(PREFIX "CTX2050 detected\n");
      return 0;
    }
  }
  printk(PREFIX "CTX2050 probe failed\n");
  return 1;
}

int bbm_init(void)
{
  fic_data.in = 0;
  fic_data.out = 0;
  currentSubChId = -1;

  if(bbm_probe())
  {
    return -ENODEV;
  }

  spireg_write8(0x17, 0x00);      // 24.576Mhz CLK out
  spireg_write8(0xa1, 0xc2);      // Coarse Freq
  // Sync Block
  spireg_write8(SYNC_CTRL0_REG, 0xf8);      // SYNC INTERRUPT & Auto reset
  spireg_write8(SYNC_CTRL1_REG, 0x80);

  // 1sec SYNC for integrant tuner
  spireg_write8(0x9F, 0x5A);
  spireg_write8(0x9E, 0x20);
  spireg_write8(0x65, 0x68);
  spireg_write8(0x67, 0x88);
  spireg_write8(0x68, 0x60);
  spireg_write8(0x6A, 0x00);
  spireg_write8(0x6B, 0x3F);
  spireg_write8(0x9A, 0x68);
  spireg_write8(0xB2, 0x0C);

  spireg_write8(DSCR_MEM_ENABLE, 0x03);   // Dscr FIC Buffer Enable
  spireg_write8(VIT_BERMEM_REG, 0x01);    // Viterbi Buffer Enable

  // FIC Buffer Size
  spireg_write8(FIC_INT_MODE_REG, 0x01); // every 96ms

  // Viterbi BER
  spireg_write32(VIT_BPER_REG, 0xF000);   // 384*8*20

  // ADC Enable
  spireg_write8(0x0C, 0x00);    // MP2 master mode mp1

  // SYNC Interrupt Clear
  u8 syncIntStatus;
  spireg_read8(0x99, &syncIntStatus);
  spireg_write8(0x99, syncIntStatus);
  spireg_write8(0x99,0);

  spireg_write16(BBM_INT_CLEAR_REG, 0xFF);  // Interrupt Clear
  spireg_write16(BBM_INT_CLEAR_REG, 0x0);   // Interrupt Clear

  // Register for MSC and FIC data interrupts
  spireg_write16(BBM_INT_MASK_REG, BBM_MSC_INT | BBM_FIC_INT);

  return 0;
}

void bbm_didp_clean(u16 base_address)
{
  int i;
  for(i=0; i<192; i++)
  {
    spireg_write8(base_address++, 0);
  }
}

static void bbm_didp_set(struct ChannelInfo_s *sInfo)
{
  u8  didpBuffer[12];

  didpBuffer[0] = (sInfo->uiStartAddress & 0xFF);
  didpBuffer[1] = (sInfo->uiSubChId & 0x3F) << 2  | (sInfo->uiStartAddress & 0x3FF) >> 8;
  didpBuffer[2] = (sInfo->uiSubChSize & 0xFF);
  didpBuffer[3] = (~sInfo->uiFormType & 0x1) << 7 | (sInfo->uiSubChSize & 0x3FF) >> 8;
  didpBuffer[4] = (sInfo->l[0] & 0x7) << 5 | (sInfo->p[0] & 0x1F);
  didpBuffer[5] = (sInfo->l[0] & 0x7FF ) >> 3;
  didpBuffer[6] = (sInfo->l[1] & 0x7) << 5 | (sInfo->p[1] & 0x1F);
  didpBuffer[7] = (sInfo->l[1] & 0x3FF) >> 3;
  didpBuffer[8] = (sInfo->l[2] & 0x7) << 5 | (sInfo->p[2] & 0xF);
  didpBuffer[9] = (sInfo->l[2] & 0xFF) >> 3;
  didpBuffer[10] = (sInfo->l[3] & 0x3) << 5 | (sInfo->p[3] & 0x1F);
  didpBuffer[11] = ((sInfo->uiPad >> 2) & 0x3) << 6;

  // Currently only use data channel 0
  u16 address = 0x312;
  int i;
  for(i=0; i<12; i++, address++)
  {
    //printk("%03x: didp[%d] = %02x\n", address, i, didpBuffer[i]);
    spireg_write8(address, didpBuffer[i]);
  }

  u8 data;
  spireg_read8(TDLV_CIF_REG, &data);
  data |= 1;
  spireg_write8(TDLV_CIF_REG, data);

  spireg_read8(0x210, &data);
  if (sInfo->uiReconfig & 1)
  {
    data |= 1;
  }
  else
  {
    data &= ~1;
  }
  spireg_write8(0x210, data);

  spireg_write8(DESCR_CFG0_REG, 0x40 | sInfo->uiSubChId);
  currentSubChId = sInfo->uiSubChId;

  printk(PREFIX "Audio channel selected (reconfig %02x)\n", sInfo->uiReconfig);

  spireg_write8(TDLV_CFG_EN_REG, (sInfo->uiReconfig & 2) ? 0x02 : 0x01);
  spireg_write8(TDLV_CFG_EN_REG, 0x00);
}

int bbm_set_subchannel(int arg)
{
  struct ChannelInfo_s sChannel;
  if (copy_from_user((void *)&sChannel, (void __user *)arg, sizeof(struct ChannelInfo_s)))
  {
    return -EFAULT;
  }

#if 0
  printk("Channel Info\n\tStart Address %03x SubChId %02x SubChSize %03x Form %01x\n\tL1 %03x L2 %03x L3 %02x L4 %01x\n\tP1 %02x P2 %02x P3 %02x P4 %02x\n\tPad %01x Reconfig %01x\n",
      sChannel.uiStartAddress, sChannel.uiSubChId, sChannel.uiSubChSize,
      sChannel.uiFormType,
      sChannel.l[0],sChannel.l[1],sChannel.l[2],sChannel.l[3],
      sChannel.p[0],sChannel.p[1],sChannel.p[2],sChannel.p[3],
      sChannel.uiPad, sChannel.uiReconfig);
#endif
  
  u16 intMask;
  spireg_read16(BBM_INT_MASK_REG, &intMask);
  spireg_write16(BBM_INT_MASK_REG, 0);

  bbm_didp_set(&sChannel);

  spireg_write16(BBM_INT_MASK_REG, intMask);

  return 0;
}

void bbm_get_viterbi_error(unsigned int *viterbi_period,
                           unsigned int *viterbi_errors)
{
  spireg_read32(VIT_BPER_REG, viterbi_period);
  spireg_read32(VIT_TBE_REG, viterbi_errors);
}

