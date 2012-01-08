/* sound/arm/wm8721-hw.h
 *
 * Wolfson WM8721 codec driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

/* struct wm8721_hw
 *
 * exported by the specific hardware driver
*/

struct wm8721_hw;

struct wm8721_hw {
	struct module	*owner;
	struct device	*dev;

	int		(*claim)(struct wm8721_hw *);
	int		(*release)(struct wm8721_hw *);
	int		(*wr)(struct wm8721_hw *hw, int reg, int val);
};

#define WM8731_LIN1V    0x00
#define WM8731_RIN1V    0x01
#define WM8721_LOUT1V   0x02
#define WM8721_ROUT1V   0x03
#define WM8721_APANA    0x04
#define WM8721_APDIGI   0x05
#define WM8721_PWR      0x06
#define WM8721_IFACE    0x07
#define WM8721_SRATE    0x08
#define WM8721_ACTIVE   0x09
#define WM8721_RESET  0x0f

#define WM8721_VOL_ZEROCROSS  (1<<7)
#define WM8721_VOL_BOTH       (1<<8)

#define WM8721_APANA_DAC (1<<4)

#define WM8721_APDIGI_DE_NONE (0<<1)
#define WM8721_APDIGI_DE_32K  (1<<1)
#define WM8721_APDIGI_DE_44K1 (2<<1)
#define WM8721_APDIGI_DE_48K  (3<<1)
#define WM8721_APANA_DAC_MUTE (1<<3)

/* Note these are active low signals (i.e. 0 = power on) */
#define WM8731_PWR_LINEIN (1<<0)
#define WM8721_PWR_ON     (1<<3)
#define WM8721_PWR_DAC    (1<<4)
#define WM8721_PWR_OSCPD  (1<<5)
#define WM8721_PWR_OUTPUT (1<<7)


#define WM8721_IFACE_I2S        (2<<0)
#define WM8721_IFACE_16BITS     (0<<2)
#define WM8721_IFACE_20BITS     (1<<2)
#define WM8721_IFACE_24BITS     (2<<2)
#define WM8721_IFACE_FORMATMASK (3<<2)
#define WM8721_IFACE_MASTER     (1<<6)

#define WM8721_SRATE_BOSR (1<<1)
#define WM8721_SRATE_USB  (1<<0)
#define WM8721_SRATE_MASK (15<<2)
