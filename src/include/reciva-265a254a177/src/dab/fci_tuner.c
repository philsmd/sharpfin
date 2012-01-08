/*
 * CTX2050 FCI Tuner Control
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/uaccess.h>

#include "fci_tuner.h"
#include "ctx2050.h"
#include "ctx2050_spi.h"
#include "ctx2050_bbm.h"
#include "ctx2050_regs.h"

#define PREFIX "CTX2050_FCI: "

static int lband;
static u16 tune_intMask;
static int tuning_to = -1;

typedef struct i2c_command_s
{
  u8 reg_addr;
  u8 data;
} i2c_command;

static void WaitForXfer(void)
{
# define I2CSTAT_TIP 0x02    /* Tip bit */
  u8 status = I2CSTAT_TIP;
  while ((status & I2CSTAT_TIP)) {
    spireg_read8(I2C_SR_REG, &status);
  }
}

static void ctx_i2c_write(u8 reg_addr, u8 data)
{
  u8 reg;

  spireg_read8(SYNC_CTRL0_REG, &reg);
  spireg_write8(SYNC_CTRL0_REG, 0x08);

  // Send START and I2C address (0x60 << 1)
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, 0x60 << 1);
  spireg_write8(I2C_CR_REG, 0x90);

  // Send register address
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, reg_addr);
  spireg_write8(I2C_CR_REG, 0x10);

  // Send data
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, data);
  spireg_write8(I2C_CR_REG, 0x10);

  // Send STOP
  WaitForXfer();
  spireg_write8(I2C_CR_REG, 0x40);

  spireg_write8(SYNC_CTRL0_REG, reg);
}

static void ctx_i2c_read(u8 reg_addr, u8 *data)
{
  u8 reg;

  spireg_read8(SYNC_CTRL0_REG, &reg);
  spireg_write8(SYNC_CTRL0_REG, 0x08);

  // Send START and I2C address (0x60 << 1)
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, 0x60 << 1);
  spireg_write8(I2C_CR_REG, 0x90);

  // Send register address
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, reg_addr);
  spireg_write8(I2C_CR_REG, 0x10);

  // Resend START for read
  WaitForXfer();
  spireg_write8(I2C_TXR_REG, (0x60 << 1) | 1);
  spireg_write8(I2C_CR_REG, 0x90);

  // Read data
  WaitForXfer();
  spireg_write8(I2C_CR_REG, 0x28);
  WaitForXfer();
  spireg_read8(I2C_RXR_REG, data);

  // Send STOP
  WaitForXfer();
  spireg_write8(I2C_CR_REG, 0x40);

  spireg_write8(SYNC_CTRL0_REG, reg);
}

static void ctx_i2c_sequence(const i2c_command *commands, int len)
{
  int i;
  for (i=0; i<len; i++)
  {
    ctx_i2c_write(commands[i].reg_addr, commands[i].data);
  }
}

void fci_init(void)
{
  printk(PREFIX "%s lband=%d\n", __FUNCTION__, lband);

  tuning_to = -1;

  // Set-up I2C interface
    spireg_write16(I2C_PRER_REG, 0x000B);   // 400khz
    spireg_write8(I2C_CTR_REG, 0xC0);

  // JM This bit same for Band3 and LBand
    spireg_write8(0x64, 0x14);
    spireg_write8(0xa4, 0x41);
    spireg_write8(0x65, 0x6A);
    spireg_write8(0x67, 0x66);
    spireg_write8(0x68, 0x84);
    spireg_write8(0x9A, 0x4F);
    spireg_write8(0x9B, 0x11);
    spireg_write8(0x9D, 0x11);
    spireg_write8(0x9F, 0x40);
    spireg_write8(0xA0, 0x84);
    spireg_write8(0xA1, 0x01);
    spireg_write8(0xAE, 0x02);

  static const i2c_command band3_init_commands[] = {
    {0x00, 0x00},
    //BGR CONTROL
    {0x05, 0x02},  //VCXO mode
    {0x3B, 0x9A},  //CLK input buffer enable, TCXO BUF2 select

    {0x06, 0x40},
    {0x07, 0x4E},  //Hold_agc  pull up (o)
    //BIAS CONTROL
    {0x0B, 0x25},
    {0x0C, 0xA5},
    {0x0D, 0xED},
    {0x20, 0x56},
    {0x21, 0x80},
    {0x22, 0x80},
    {0x23, 0x00},
    {0x35, 0x4A},
    {0x36, 0xE1},
    {0x38, 0xF0},
    {0x45, 0xFF},
    {0x47, 0x78},
    //AGC
    {0x53, 0xE9},
    {0x60, 0x0A},
    {0x61, 0x05},
    {0x62, 0x00},
    {0x63, 0x0E},
    {0x64, 0x06},
    {0x5C, 0x4D},
    {0x5D, 0x2D},
    {0x5E, 0x3B},
    {0x68, 0xCC},
    {0x69, 0x3F},
    {0x6E, 0x08},
    //Wait clock
    {0x55, 0xFF},
    {0x56, 0xFF},
    {0x57, 0xFF},
    {0x58, 0xFF},
    {0x59, 0xFF},
    {0x5A, 0xFF},
    {0x16, 0x99},
    {0x5B, 0xFF},
    {0x6D, 0xFF},

    {0x02, 0x6e},    // XTAL mode, 24.576MHz
  };

  static const i2c_command lband_init_commands[] = {
    {0x00, 0x00},

    //BGR CONTROL
    {0x05, 0x02},      //VCXO mode
    {0x3B, 0x9A}, //CLK input buffer enable, TCXO BUF2 select
    {0x06, 0x40},
    {0x07, 0x4E}, //Hold_agc  pull up (o)
    {0x08, 0x54},
    {0x0A, 0x3C},
    {0x34, 0x75},
    {0x0B, 0x25},
    {0x0C, 0xA5},
    {0x0D, 0x2D},
    {0x16, 0x99},
    {0x20, 0x56},
    {0x21, 0x80},
    {0x22, 0x80},
    {0x23, 0x00},
    {0x35, 0x4A},
    {0x36, 0xE1},
    {0x38, 0xF0},
    {0x45, 0xFF},
    {0x47, 0x78},

    //AGC
    {0x53, 0x29},    //AGC speed fast
    {0x60, 0x0F},
    {0x61, 0x05},
    {0x62, 0x00},
    {0x63, 0x0E},
    {0x64, 0x06},
    {0x5C, 0x50},
    {0x5D, 0x4D},
    {0x5E, 0x3B},
    {0x68, 0xFF},
    {0x69, 0x3F},
    {0x6E, 0x08},
    //Wait Clock 
    {0x55, 0x32},
    {0x56, 0x32},
    {0x57, 0x14},
    {0x58, 0x14},
    {0x59, 0xFF},
    {0x5A, 0xFF},
    {0x5B, 0xFF},
    {0x6D, 0xFF},

    {0x02, 0x8e},    // VCXO mode, 24.576MHz
    // FCI say it shouldn't matter whether VCXO or XTAL mode is used
    // Previous versions have used VCXO mode, so stick with that
    //{0x02, 0xAe},   // XTAL mode, 24.576MHz
  };

  if (lband)
  {
    ctx_i2c_sequence(lband_init_commands,
                     sizeof(lband_init_commands)/sizeof(i2c_command));
  }
  else
  {
    ctx_i2c_sequence(band3_init_commands,
                     sizeof(band3_init_commands)/sizeof(i2c_command));
  }
}

static void fci_set_tune_data(u32 N_total)
{
  u32  N_frac;
  u32  N_frac_norm;
  u8   N_int;

# define IF_FREQ 24576
  N_int = N_total / IF_FREQ;
  N_frac = N_total % IF_FREQ;
  // K_const = 1048576;  // 2^20
  // K_const / IF_FREQ = 2^7 / 3
  N_frac_norm = (N_frac * 128) / 3;

  ctx_i2c_write(0x43, (N_frac_norm & 0x000ff));
  ctx_i2c_write(0x42, (N_frac_norm & 0x0ff00) >> 8);
  ctx_i2c_write(0x41, (N_frac_norm & 0xf0000) >> 16);
  ctx_i2c_write(0x44, (N_int & 0xff));

  printk(PREFIX "%s %02x %05x\n", __FUNCTION__, N_int, N_frac_norm);
}

static void fci_lband_revision(int freq)
{
  u32  N_total;
  short CERR, LOOP_cal;
  int  LOOP;
  u8   CERR_mid, LOOP_mid;

  u8 tmode;
  spireg_read8(0x80, &tmode);
  tmode = tmode & 0x7;
  u8 tmode_factor = 1;
  switch (tmode)
  {
    case 1: tmode_factor = 1;
    case 2: tmode_factor = 4;
    case 3: tmode_factor = 8;
    case 4: tmode_factor = 2;
  }

  spireg_read8(0x89, &CERR_mid);
  CERR = (CERR_mid << 8);
  spireg_read8(0x88, &CERR_mid);
  CERR |= CERR_mid;
  CERR *= tmode_factor;

  spireg_read8(0xa3, &LOOP_mid);
  LOOP_cal = (LOOP_mid & 0x08) ? (0xF0 | LOOP_mid) : LOOP_mid;
  spireg_read8(0xa2, &LOOP_mid);
  LOOP_cal = (LOOP_cal << 8) | LOOP_mid;
  LOOP = (LOOP_cal * tmode_factor * 1000) / 2048;
  LOOP += (freq + CERR) * 1000;
  //printk(PREFIX "LOOP %d CERR %d\n", CERR, LOOP);
  //printk(PREFIX "after revision FCI TUNER SET %dkhz\n", LOOP);

  //B3_DIV = 2;  // lband division factor
  N_total = (LOOP - 2048000) * 2;
  N_total /= 1000;

  fci_set_tune_data(N_total);

  bbm_reset();
}

int fci_tune(int freq)
{
  printk(PREFIX "%s %d (%d)\n", __FUNCTION__, freq, tuning_to);

  int is_lband = freq > 1000000;
  if (is_lband != lband)
  {
    // Band change, reset tuner
    printk(PREFIX "Changing to %s\n", is_lband ? "L-Band" : "BandIII");
    lband = is_lband;
    ctx2050_power_on();
  }

  u8 status;
  spireg_read8(0x65, &status);
  spireg_write8(0x65, status & 0xFE); // AGC ON
  spireg_read8(0xB2, &status);
  spireg_write8(0xB2, status & 0xFE); //Digital AGC ON
  spireg_write8(0x9E, 0x20);  //sample sync ON
  spireg_write8(0x0C, 0x0); //ADC power ON

  // Disable interrupts (only if there's no current tune going on)
  if (tuning_to == -1)
  {
    spireg_read16(BBM_INT_MASK_REG, &tune_intMask);
    spireg_write16(BBM_INT_MASK_REG, 0);
  }
  tuning_to = freq;

  // Reset FIC buffer
  bbm_reset_fic();

  // Set tuner
  u32 N_total;
  u8 div_factor = lband ? 2 : 16;
  N_total = (freq - 2048) * div_factor;
  fci_set_tune_data(N_total);

  ctx_i2c_write(0x37, 0x61);
  ctx_i2c_write(0x37, 0xE1);

  bbm_reset();
  bbm_reset();

  return 0;
}

int fci_tune_lock(void)
{
  if (tuning_to == -1)
  {
    // Not currently trying to tune to anything
    return -EPERM;
  }

  u8 syncStatus;
  spireg_read8(0x98, &syncStatus);

  if(syncStatus == 0x1f)
  {
    if (lband)
    {
      fci_lband_revision(tuning_to);
    }
    u16 CF_err;
    spireg_read16(0x88, &CF_err);
    printk(PREFIX "Sync lock CF_ERROR %04x\n", CF_err);
    fci_tune_stop();
    bbm_didp_clean(TDLV_CIF_CTRL_REG0);
    bbm_didp_clean(TDLV_CIF_CTRL_REG1);
    spireg_write8(TDLV_CIF_REG, 0x0);
    return CF_err & 0xffff;
  }

  // Haven't got lock yet, try again later
  return -EAGAIN;
}

int fci_tune_stop(void)
{
  if (tuning_to == -1)
  {
    // Not currently trying to tune to anything
    return -EPERM;
  }
  printk(PREFIX "%s (tune_intMask %04x)\n", __FUNCTION__, tune_intMask);

  // Re-enable interrupts
  spireg_write16(BBM_INT_MASK_REG, tune_intMask);
  tuning_to = -1;
  
  return 0;
}

int fci_rf_power(void)
{
  unsigned char lna_gain;
  unsigned char agc_gain;
  unsigned char atten;
  unsigned char filter;

  ctx_i2c_read(0x6A, &atten);
  lna_gain = atten & 0x3;
  atten = (atten >> 3) & 0xf;
  ctx_i2c_read(0x6B, &filter);
  filter = (filter >> 1) & 0x7;
  spireg_read8(0x6c, &agc_gain);

  // multiply by 1000 to avoid floating point
  int rf_power = (lna_gain * 13) + (atten * 2) + (filter * 6) - 28;
  rf_power = (rf_power * 1000) - (agc_gain * 84600 / 63);
  return rf_power;
}

