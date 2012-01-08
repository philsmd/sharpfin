/*
 * Reciva DAB Driver (CTX2050)
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef LINUX_CTX2050_H
#define LINUX_CTX2050_H

#include <linux/ioctl.h>

extern unsigned long get_int_time(void);
extern unsigned long get_last_int_time(void);
extern void ctx2050_power_on(void);

struct SignalStrength_s
{
  unsigned int viterbi_period;
  unsigned int viterbi_errors;
  int rf_power; // dBm * 1000
};

struct ChannelInfo_s
{
  unsigned int uiStartAddress;
  unsigned int uiSubChId;
  unsigned int uiSubChSize;
  unsigned int uiFormType;
  unsigned int l[4];
  unsigned int p[4];
  unsigned int uiPad;
  unsigned int uiReconfig;
};

#define INTBB         S3C2410_GPF3

#define CTX2050_IOCTL_BASE  'J'
#define IOC_DAB_SET_SUBCH       _IOW(CTX2050_IOCTL_BASE, 0, struct ChannelInfo_s)
#define IOC_DAB_TUNE            _IOW(CTX2050_IOCTL_BASE, 1, int)
#define IOC_DAB_TUNE_LOCK       _IO(CTX2050_IOCTL_BASE, 2)
#define IOC_DAB_TUNE_STOP       _IO(CTX2050_IOCTL_BASE, 3)
#define IOC_CTX2050_SPI_TEST    _IO(CTX2050_IOCTL_BASE, 5)
#define IOC_DAB_POWER_ON        _IO(CTX2050_IOCTL_BASE, 6)
#define IOC_DAB_POWER_OFF       _IO(CTX2050_IOCTL_BASE, 7)
#define IOC_DAB_GET_SIGSTRENGTH _IOR(CTX2050_IOCTL_BASE, 8, struct SignalStrength_s *)
#define IOC_DAB_DUMP_MSC_DATA   _IOR(CTX2050_IOCTL_BASE, 9, char *)

#endif
