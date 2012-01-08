/*
 * CTX2050 SPI Driver
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/random.h>

#include <asm/hardware.h>
#include <asm/arch/regs-gpio.h>

#include "ctx2050_spi.h"
#include "ctx2050_regs.h"

#define PREFIX "CTX2050_SPI: "


static unsigned char spi_write_buffer[2048];

static struct spi_device *ctx2050_ctrl_dev;
static struct spi_device *ctx2050_data_dev;

static int ctrl_driver_added = -1;
static int data_driver_added = -1;

static void spi_loopback_test(void);
static void spi_reg_test(void);

void spi_test(int arg)
{
  if (arg == 1)
  {
      spi_loopback_test();
  }
  else
  {
    spi_reg_test();
  }
}

static void spi_loopback_test(void)
{
  printk("%s\n", __FUNCTION__);

  char tx_data[1024];
  char rx_data[1024];
  int seq = 0;
  int total_good = 0;
  int failed1 = 0;
  int failed2 = 0;
  while(1)
  {
    int i = 0;
    while (i<1024)
    {
      rx_data[i] = 0;
      tx_data[i++] = random32() & 0xff;
    }
      
#if 0
    printk("TX data:\n");
    for (i=0; i<1024; i++)
    {
      printk("%02x ", tx_data[i]);
    }
    printk("\n");
#endif

    int result = spi_write(ctx2050_ctrl_dev, tx_data, 1024);
    if (result < 0)
    {
      printk("%d: Write fail %d\n", seq, result);
    }
    result = spi_read(ctx2050_data_dev, rx_data, 1024);
    if (result < 0)
    {
      printk("%d: Read 1 fail %d\n", seq, result);
    }
#if 0
    result = spi_read(ctx2050_data_dev, rx_data+512, 512);
    if (result < 0)
    {
      printk("%d: Read 2 fail %d\n", seq, result);
    }
#endif
    else
    {
#if 0    
      printk("RX data:\n");
      for (i=0; i<1024; i++)
      {
        printk("%02x ", rx_data[i]);
        if (i==511)
          printk("\n");
      }
      printk("\n");
#endif
      int failed_at = -1;
      for (i=0; i<1024; i++)
      {
        if (tx_data[i] != rx_data[i])
        {
          printk("%d %d tx:%02x rx:%02x\n", seq, i, tx_data[i], rx_data[i]);
          if (failed_at < 0) failed_at = i;
        }
      }
      if (failed_at == -1)
      {
        total_good++;
      }
      else
      {
        if (failed_at < 512) failed1++; else failed2++;
        printk("%d done (good %d failed1 %d failed2 %d)\n", seq+1, total_good, failed1, failed2);
      }
    }
    seq++;
  }
}

static void spi_reg_test(void)
{
  printk("%s\n", __FUNCTION__);
  if ((ctx2050_ctrl_dev == NULL) || (ctx2050_data_dev == NULL))
  {
    printk(PREFIX "No device!\n");
    return;
  }

  int i;
  char data[4];

  // This should return 09 (month) 06 (year)
  spireg_read(0xd, data, 2);
  printk("Read: %d %d\n", data[0], data[1]);

  // Check bank selection
  spireg_read(0x12, data, 1);
  printk("Read 0x12: %x\n", data[0]);
  spireg_read(0x312, data, 1);
  printk("Read 0x312: %x\n", data[0]);
  data[0] = 0xaa;
  spireg_write8(0x312, data[0]);
  spireg_read(0x12, data, 1);
  printk("Read 0x12: %x\n", data[0]);
  spireg_read(0x312, data, 1);
  printk("Read 0x312: %x\n", data[0]);

  // Check four byte read/write
  int pass = 0, fail1 = 0, fail2 = 0, fail3 = 0, fail4 = 0;
  for (i=0xff; i; i--)
  {
    printk("%02x ", i);
    data[0] = i;
    data[1] = ~i;
    data[2] = i;
    data[3] = ~i;
    spireg_write32(0x312, *((unsigned int *)data));

    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    spireg_read(0x312, data, 4);

    printk(" %02x %02x", data[0], data[1]);
    if (data[0] != i)
    {
      fail1++;
      printk(" FAIL1\n");
    }
    else if (data[1] != (~i & 0xff))
    {
      fail2++;
      printk(" FAIL2\n");
    }
    else if (data[2] != i)
    {
      fail3++;
      printk(" FAIL3\n");
    }
    else if (data[3] != (~i & 0xff))
    {
      fail4++;
      printk(" FAIL4\n");
    }
    else
    {
      pass++;
      printk("\n");
    }
  }

  printk("\n\nPASS: %d FAIL: %d %d %d %d\n", pass, fail1, fail2, fail3, fail4);
}

   /*************************************************************************/
   /***                      SPI Driver - START                           ***/
   /*************************************************************************/

static int ctx2050_probe(struct spi_device *spi);
static int ctx2050_remove(struct spi_device *spi);

static struct spi_driver ctx2050_ctrl_driver = {
  .driver = {
    .name   = "ctx2050_ctrl",
    .owner    = THIS_MODULE,
  },
  .probe    = ctx2050_probe,
  .remove   = __devexit_p(ctx2050_remove),
};

static struct spi_driver ctx2050_data_driver = {
  .driver = {
    .name   = "ctx2050_data",
    .owner    = THIS_MODULE,
  },
  .probe    = ctx2050_probe,
  .remove   = __devexit_p(ctx2050_remove),
};


static int ctx2050_probe(struct spi_device *spi)
{
  printk(PREFIX "%s %s\n", __FUNCTION__, spi->modalias);
  if (strcmp(spi->modalias, ctx2050_ctrl_driver.driver.name) == 0)
  {
    ctx2050_ctrl_dev = spi;
  }
  if (strcmp(spi->modalias, ctx2050_data_driver.driver.name) == 0)
  {
    ctx2050_data_dev = spi;
  }

  return 0;
}

static int ctx2050_remove(struct spi_device *spi)
{
  printk(PREFIX "%s %s\n", __FUNCTION__, spi->modalias);
  if (spi == ctx2050_ctrl_dev)
  {
    ctx2050_ctrl_dev = NULL;
  }
  if (spi == ctx2050_data_dev)
  {
    ctx2050_data_dev = NULL;
  }

  return 0;
}

void spi_init(void)
{
  /* Claim GPIO pins for SPI function */
  s3c2410_gpio_cfgpin(S3C2410_GPE11, S3C2410_GPE11_SPIMISO0);
  s3c2410_gpio_cfgpin(S3C2410_GPE12, S3C2410_GPE12_SPIMOSI0);
  s3c2410_gpio_cfgpin(S3C2410_GPE13, S3C2410_GPE13_SPICLK0);
  s3c2410_gpio_cfgpin(S3C2410_GPG5, S3C2410_GPG5_SPIMISO1);
  s3c2410_gpio_cfgpin(S3C2410_GPG6, S3C2410_GPG6_SPIMOSI1);
  s3c2410_gpio_cfgpin(S3C2410_GPG7, S3C2410_GPG7_SPICLK1);

  // First byte of write buffer indicates a write operation
  spi_write_buffer[0] = 2;

  /* Attach SPI device */
  ctrl_driver_added = spi_register_driver(&ctx2050_ctrl_driver);
  data_driver_added = spi_register_driver(&ctx2050_data_driver);
}

void spi_exit(void)
{
  /* Remove SPI drivers */
  if (ctrl_driver_added == 0)
    spi_unregister_driver(&ctx2050_ctrl_driver);
  if (data_driver_added == 0)
    spi_unregister_driver(&ctx2050_data_driver);

}

int spidata_read(unsigned char * buffer, int len)
{
  int result = spi_read(ctx2050_data_dev, buffer, len);
  if (result < 0)
  {
    printk(PREFIX "data read fail %d\n", result);
  }
  return result;
}

// Set the correct bank for the given register address
// Bank is top 8 bits of the address.
// Addresses 0x-00 to 0x-0f are common to all banks (i.e. 0x000==0x100, etc)
static char get_register_address(int reg_addr)
{
  static u8 bank_cmd[] = { 2, 0, 0 };

  //printk(PREFIX "%s %03x\n", __FUNCTION__, address);
  int bank = (reg_addr & 0xff00) >> 8;
  if ((reg_addr & 0x00f0) && (bank != bank_cmd[2]))
  {
    // set bank
    bank_cmd[2] = bank;
    //printk(PREFIX "Select bank %02x\n", bank_cmd[2]);
    spi_write(ctx2050_ctrl_dev, bank_cmd, 3);
  }

  return reg_addr & 0xff;
}

int spireg_read(int reg_addr, unsigned char * buffer, int len)
{
  static unsigned char spi_read_data[2] = { 3, 0 };
  spi_read_data[1] = get_register_address(reg_addr);
  while (len > 30)
  {
    int result = spi_write_then_read(ctx2050_ctrl_dev, spi_read_data, 2, buffer, 30);
    if (result < 0)
    {
      printk("Read fail %d\n", result);
      return result;
    }
    len -= 30;
    buffer += 30;
    spi_read_data[1] += 30;
  }
  int result = spi_write_then_read(ctx2050_ctrl_dev, spi_read_data, 2, buffer, len);
  if (result < 0)
  {
    printk("Read fail %d\n", result);
  }
  return result;
}

static int spireg_write(int reg_addr, unsigned char * buffer, int len)
{
  spi_write_buffer[1] = get_register_address(reg_addr);
  int i;
  for (i=0; i<len; i++)
  {
    spi_write_buffer[2+i] = buffer[i];
  }

  int result = spi_write(ctx2050_ctrl_dev, spi_write_buffer, len+2);
  if (result < 0)
  {
    printk("Write fail %d\n", result);
  }
  return result;
}

int spireg_read8(u16 addr, u8 *data)
{
  return spireg_read(addr, data, 1);
}

int spireg_read16(u16 addr, u16 *data)
{
  return spireg_read(addr, (u8 *)data, 2);
}

int spireg_read32(u16 addr, u32 *data)
{
  return spireg_read(addr, (u8 *)data, 4);
}

int spireg_write8(u16 addr, u8 data)
{
//  printk("B %04x %02x\n", addr, data);
  return spireg_write(addr, &data, 1);
}

int spireg_write16(u16 addr, u16 data)
{
//  printk("W %04x %04x\n", addr, data);
  return spireg_write(addr, (u8 *)&data, 2);
}

int spireg_write32(u16 addr, u32 data)
{
//  printk("L %04x %08x\n", addr, data);
  return spireg_write(addr, (u8 *)&data, 4);
}
