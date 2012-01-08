/*
 * CTX2050 SPI Driver
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef LINUX_CTX2050_SPI_H
#define LINUX_CTX2050_SPI_H

extern void spi_init(void);
extern void spi_exit(void);

extern void spi_test(int arg);

extern int spidata_read(unsigned char * buffer, int len);
extern int spireg_write32(u16 addr, u32 data);
extern int spireg_write16(u16 addr, u16 data);
extern int spireg_write8(u16 addr, u8 data);
extern int spireg_read32(u16 addr, u32 *data);
extern int spireg_read16(u16 addr, u16 *data);
extern int spireg_read8(u16 addr, u8 *data);
extern int spireg_read(int reg_addr, unsigned char * buffer, int len);

#endif // LINUX_CTX2050_SPI_H
