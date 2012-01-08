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

struct reciva_keymap_load
{
  int nrows;
  int ncols;
  int *codes;
};

/* IOCTL Stuff */
#define KEY_IOCTL_BASE  'K'
#define IOC_KEY_GETSUPPORTED           _IOR(KEY_IOCTL_BASE, 0, int *)
#define IOC_KEY_GET_ALT_FN             _IOR(KEY_IOCTL_BASE, 1, int *)
#define IOC_KEY_GET_POWER_ON_KEY       _IOR(KEY_IOCTL_BASE, 2, int *)
#define IOC_KEY_LOAD_MAP	       _IOW(KEY_IOCTL_BASE, 3, struct reciva_keymap_load)
#define IOC_KEY_GET_STATE              _IOWR(KEY_IOCTL_BASE, 4, int)
#define IOC_KEY_GET_POWER_ON_ROW_COL   _IOR(KEY_IOCTL_BASE, 5, int *)

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
#define RKD_AUDIO_SOURCE (BTN_Z + 19)
#define RKD_SNOOZE (BTN_Z + 20)
#define RKD_STORE_PRESET (BTN_Z + 21)
#define RKD_RECALL_PRESET (BTN_Z + 22)
#define RKD_EXTEND_PRESET (BTN_Z + 23)
#define RKD_IR_FM_MODE_SWITCH (BTN_Z + 24)
#define RKD_SWITCH_MODE_TO_IRADIO (BTN_Z + 25)
#define RKD_SWITCH_MODE_TO_MEDIA_PLAYER (BTN_Z + 26)
#define RKD_SWITCH_MODE_TO_LINE_IN (BTN_Z + 27)
#define RKD_RC_UP (BTN_Z + 28)
#define RKD_RC_DOWN (BTN_Z + 29)
#define RKD_RECORD (BTN_Z + 30)
#define RKD_SWITCH_MODE_TO_FM (BTN_Z + 31)
#define RKD_SEEK_DOWN (BTN_Z + 32)
#define RKD_SEEK_UP (BTN_Z + 33)
#define RKD_INFO (BTN_Z + 34)
#define RKD_FORWARD (BTN_Z + 35)
#define RKD_BATTERY_1 (BTN_Z + 36)
#define RKD_BATTERY_2 (BTN_Z + 37)
#define RKD_BATTERY_3 (BTN_Z + 38)
#define RKD_BACKLIGHT (BTN_Z + 39)
#define RKD_LANGUAGE (BTN_Z + 40)
#define RKD_CLOCK (BTN_Z + 41)
#define RKD_ALARM_ONCE (BTN_Z + 42)
#define RKD_ALARM_OFF (BTN_Z + 43)
#define RKD_SHUFFLE (BTN_Z + 44)
#define RKD_REPEAT (BTN_Z + 45)
#define RKD_MENU_TOP (BTN_Z + 46)
#define RKD_SHOW_TIME                           (BTN_Z + 47),
#define RKD_EQ                                  (BTN_Z + 48),
#define RKD_FM_STEP_UP                          (BTN_Z + 49),
#define RKD_FM_STEP_DOWN                        (BTN_Z + 50),
#define RKD_IPOD_MENU                           (BTN_Z + 51),
#define RKD_IPOD_SELECT                         (BTN_Z + 52),
#define RKD_IPOD_DOWN                           (BTN_Z + 53),
#define RKD_IPOD_UP                             (BTN_Z + 54),
#define RKD_PLAY_PAUSE_SELECT                   (BTN_Z + 55),
#define RKD_STOP_BACK                           (BTN_Z + 56),
#define RKD_RUN_MACRO                           (BTN_Z + 57),
#define RKD_BASS                                (BTN_Z + 58),
#define RKD_TREBLE                              (BTN_Z + 59),
#define RKD_ENTER                               (BTN_Z + 60),
#define RKD_ALARM_1                             (BTN_Z + 61),
#define RKD_ALARM_2                             (BTN_Z + 62),
#define RKD_NAP                                 (BTN_Z + 63),
#define RKD_PRESET_MENU                         (BTN_Z + 64),
#define RKD_NEXT_PRESET                         (BTN_Z + 65),
#define RKD_PREVIOUS_PRESET                     (BTN_Z + 66),
#define RKD_DAB_SCAN                            (BTN_Z + 67),
#define RKD_CD_EJECT                            (BTN_Z + 68),
#define RKD_SEARCH_FORWARD                      (BTN_Z + 69),
#define RKD_SEARCH_BACKWARD                     (BTN_Z + 70),




#define RKD_NUM_ROWS 6
#define RKD_NUM_COLS 6

#define RKD_MAX_KEYS 32


typedef struct 
{
  const char *name;
  int   (*decode_key_press)(int, int);
  int * (*keys_present)(void);
  int * (*alt_key_functions)(void);
  int   (*ioctl_extension_hook)(struct inode *inode, struct file *file,
				unsigned int cmd, unsigned long arg);
} reciva_keypad_driver_t;

extern void rkg_register(const reciva_keypad_driver_t *driver);
extern void rkg_unregister(const reciva_keypad_driver_t *d);

extern void rkg_access_request(void);
extern void rkg_access_release(void);

extern void rkg_report_key(int key, int state);




#endif
