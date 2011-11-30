/*
 * linux/reciva/reciva_keypad_driver.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2004 Reciva Ltd. All Rights Reserved
 *
 * Generic interface to reciva keypad drivers.
 *
 * Version 1.0 2004-22-12  John Stirling <js@reciva.com>
 *
 */

#ifndef LINUX_RECIVA_KEYPAD_GENERIC_H
#define LINUX_RECIVA_KEYPAD_GENERIC_H

#include <linux/input.h>

/* IOCTL Stuff */
#define KEY_IOCTL_BASE  'K'
#define IOC_KEY_GETSUPPORTED           _IOR(KEY_IOCTL_BASE, 0, int *)
#define IOC_KEY_GET_ALT_FN             _IOR(KEY_IOCTL_BASE, 1, int *)


/* Keypad Event IDs */
#define RKD_POWER     BTN_0
#define RKD_PRESET_1  BTN_1
#define RKD_PRESET_2  BTN_2
#define RKD_PRESET_3  BTN_3
#define RKD_PRESET_4  BTN_4
#define RKD_PRESET_5  BTN_5
#define RKD_PRESET_6  BTN_6
#define RKD_SELECT    BTN_7
#define RKD_REPLY     BTN_8
#define RKD_BACK      BTN_9
#define RKD_PRESET_7  BTN_A
#define RKD_PRESET_8  BTN_B
#define RKD_PRESET_9  BTN_C
#define RKD_POWER_ON  BTN_X
#define RKD_POWER_OFF BTN_Y
#define RKD_SHIFT     BTN_Z
#define RKD_PRESET_10 (BTN_Z + 1)
#define RKD_VOL_UP    (BTN_Z + 2)
#define RKD_VOL_DOWN  (BTN_Z + 3)
#define RKD_MUTE      (BTN_Z + 4)
#define RKD_ZOOM      (BTN_Z + 5)
#define RKD_SKIP_NEXT      (BTN_Z + 6)
#define RKD_SKIP_PREVIOUS  (BTN_Z + 7)
#define RKD_PLAY_PAUSE     (BTN_Z + 8)
#define RKD_STOP           (BTN_Z + 9)
#define RKD_PLAYBACK_MODE  (BTN_Z + 10)
#define RKD_BROWSE_QUEUE   (BTN_Z + 11)
#define RKD_ALARM          (BTN_Z + 12)
#define RKD_PRESET_11 (BTN_Z + 13)
#define RKD_PRESET_12 (BTN_Z + 14)
#define RKD_UNUSED    (BTN_Z + 15)
#define RKD_UP        (BTN_Z + 16)
#define RKD_DOWN      (BTN_Z + 17)
#define RKD_SLEEP_TIMER (BTN_Z + 18)


#define RKD_NUM_ROWS 4
#define RKD_NUM_COLS 4

#define RKD_MAX_KEYS 32


typedef struct 
{
  const char *name;
  int   (*decode_key_press)(int, int, int);
  int * (*keys_present)(void);
  int * (*alt_key_functions)(void);
} reciva_keypad_driver_t;

extern void rkg_cancel_shift(void);
extern void rkg_register(const reciva_keypad_driver_t *driver);




#endif
