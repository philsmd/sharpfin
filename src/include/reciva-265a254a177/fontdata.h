/*
 * Generic charmap for lcd modules using the fonts in fontdata.c
 * Copyright (c) 2005 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

static const struct rutl_unicode_charmap asCharMap[] = {
  /* 'Homegrown' Unicode characters */
  { LCD_VOLUME_1_BAR,      0xb4 },
  { LCD_VOLUME_2_BAR,      0xb5 },
  { LCD_END_SYMBOL_PART1,  0xb6 },
  { LCD_END_SYMBOL_PART2,  0xb7 },
  { LCD_END_SYMBOL_PART3,  0xb8 },
  { LCD_LEFT_ARROW_SOLID,  0xb9 },
  { LCD_RIGHT_ARROW_SOLID, 0xba },
  { LCD_PAUSE_ICON,        0xbb },
  { LCD_EJECT_ICON,        0xbc },
  { LCD_FAST_FORWARD_ICON, 0xbd },
  { LCD_REWIND_ICON,       0xbe },
  { LCD_PADLOCK_ICON,      0xbf },
  { LCD_ENTER_ICON,        0xc2 },
  { LCD_STOP_ICON,         0xfe },
  { LCD_SPACE_BRIDGE_ICON, 0xc3 },
{ "\xc2\xa1", 0xad }, /* 0xa1  INVERTED EXCLAMATION MARK */
{ "\xc2\xa2", 0x9b }, /* 0xa2  CENT SIGN */
{ "\xc2\xa3", 0x9c }, /* 0xa3  POUND SIGN */
{ "\xc2\xa5", 0x9d }, /* 0xa5  YEN SIGN */
{ "\xc2\xaa", 0xa6 }, /* 0xaa  FEMININE ORDINAL INDICATOR */
{ "\xc2\xab", 0xae }, /* 0xab  LEFT POINTING GUILLEMET */
{ "\xc2\xba", 0xa7 }, /* 0xba  MASCULINE ORDINAL INDICATOR */
{ "\xc2\xbb", 0xaf }, /* 0xbb  RIGHT POINTING GUILLEMET */
{ "\xc2\xbf", 0xa8 }, /* 0xbf  INVERTED QUESTION MARK */
{ "\xc3\x84", 0x8e }, /* 0xc4  CAPITAL LETTER A WITH DIAERESIS */
{ "\xc3\x85", 0x8f }, /* 0xc5  CAPITAL LETTER A WITH RING ABOVE */
{ "\xc3\x86", 0x92 }, /* 0xc6  CAPITAL LIGATURE AE */
{ "\xc3\x87", 0x80 }, /* 0xc7  CAPITAL LETTER C WITH CEDILLA */
{ "\xc3\x89", 0x90 }, /* 0xc9  CAPITAL LETTER E WITH ACUTE */
{ "\xc3\x8b", 0xc4 }, /* 0xcb  CAPITAL LETTER E WITH DIAERESIS */
{ "\xc3\x8c", 0x8d }, /*       CAPITAL LETTER I WITH GRAVE */
{ "\xc3\x8d", 0xa1 }, /*       CAPITAL LETTER I WITH ACUTE */
{ "\xc3\x8f", 0xc5 }, /* 0xcf  CAPITAL LETTER I WITH DIAERESIS */
{ "\xc3\x91", 0xa5 }, /* 0xd1  CAPITAL LETTER N WITH TILDE */
{ "\xc3\x96", 0x99 }, /* 0xd6  CAPITAL LETTER O WITH DIAERESIS */
{ "\xc3\x98", 0xec }, /* 0xd8  CAPITAL LETTER O WITH STROKE */
{ "\xc3\x9c", 0x9a }, /* 0xdc  CAPITAL LETTER U WITH DIAERESIS */
{ "\xc3\x9f", 0xe1 }, /* 0xdf  SMALL LETTER SHARP S */
{ "\xc3\xa0", 0x85 }, /* 0xe0  SMALL LETTER A WITH GRAVE */
{ "\xc3\xa1", 0xa0 }, /* 0xe1  SMALL LETTER A WITH ACUTE */
{ "\xc3\xa2", 0x83 }, /* 0xe2  SMALL LETTER A WITH CIRCUMFLEX */
{ "\xc3\xa3", 0xc0 }, /* 0xe3  SMALL LETTER A WITH TILDE */
{ "\xc3\xa4", 0x84 }, /* 0xe4  SMALL LETTER A WITH DIAERESIS */
{ "\xc3\xa5", 0x86 }, /* 0xe5  SMALL LETTER A WITH RING ABOVE */
{ "\xc3\xa6", 0x91 }, /* 0xe6  SMALL LIGATURE AE */
{ "\xc3\xa7", 0x87 }, /* 0xe7  SMALL LETTER C WITH CEDILLA */
{ "\xc3\xa8", 0x8a }, /* 0xe8  SMALL LETTER E WITH GRAVE */
{ "\xc3\xa9", 0x82 }, /* 0xe9  SMALL LETTER E WITH ACUTE */
{ "\xc3\xaa", 0x88 }, /* 0xea  SMALL LETTER E WITH CIRCUMFLEX */
{ "\xc3\xab", 0x89 }, /* 0xeb  SMALL LETTER E WITH DIAERESIS */
{ "\xc3\xac", 0x8d }, /* 0xec  SMALL LETTER I WITH GRAVE */
{ "\xc3\xad", 0xa1 }, /* 0xed  SMALL LETTER I WITH ACUTE */
{ "\xc3\xae", 0x8c }, /* 0xee  SMALL LETTER I WITH CIRCUMFLEX */
{ "\xc3\xaf", 0x8b }, /* 0xef  SMALL LETTER I WITH DIAERESIS */
{ "\xc3\xb1", 0xa4 }, /* 0xf1  SMALL LETTER N WITH TILDE */
{ "\xc3\xb2", 0x95 }, /* 0xf2  SMALL LETTER O WITH GRAVE */
{ "\xc3\xb3", 0xa2 }, /* 0xf3  SMALL LETTER O WITH ACUTE */
{ "\xc3\xb4", 0x93 }, /* 0xf4  SMALL LETTER O WITH CIRCUMFLEX */
{ "\xc3\xb5", 0xc1 }, /* 0xf5  SMALL LETTER O WITH TILDE */
{ "\xc3\xb6", 0x94 }, /* 0xf6  SMALL LETTER O WITH DIAERESIS */
{ "\xc3\xb8", 0xed }, /* 0xf8  SMALL LETTER O WITH STROKE */
{ "\xc3\xb9", 0x97 }, /* 0xf9  SMALL LETTER U WITH GRAVE */
{ "\xc3\xba", 0xa3 }, /* 0xfa  SMALL LETTER U WITH ACUTE */
{ "\xc3\xbb", 0x96 }, /* 0xfc  SMALL LETTER U WITH CIRCUMFLEX */
{ "\xc3\xbc", 0x81 }, /* 0xfb  SMALL LETTER U WITH DIAERESIS */
{ "\xc3\xbf", 0x98 }, /* 0xff  SMALL LETTER Y WITH DIAERESIS */

{ "\xce\xb1", 0xe0 }, /* 0x3b1  GREEK SMALL LETTER ALPHA */
{ "\xce\x93", 0xe2 }, /* 0x393  GREEK CAPITAL LETTER GAMMA */
{ "\xcf\x80", 0xe3 }, /* 0x3c0  GREEK SMALL LETTER PI  maybe capital */
{ "\xce\xa3", 0xe4 }, /* 0x3a3  GREEK CAPITAL LETTER SIGMA */
{ "\xce\xb4", 0xeb }, /* 0x3b4  GREEK SMALL LETTER DELTA */
{ "\xcf\x83", 0xe5 }, /* 0x3c3  GREEK SMALL LETTER SIGMA */
{ "\xce\xbc", 0xe6 }, /* 0x3bc  GREEK SMALL LETTER MU */
{ "\xcf\x84", 0xe7 }, /* 0x3c4  GREEK SMALL LETTER TAU  is it upsilon? */
{ "\xce\xa6", 0xe8 }, /* 0x3a6  GREEK CAPITAL LETTER PHI */
{ "\xce\x98", 0xe9 }, /* 0x398  GREEK CAPITAL LETTER THETA */
{ "\xce\xa9", 0xea }, /* 0x3a9  GREEK CAPITAL LETTER OMEGA */
{ "\xcf\xb5", 0xee }, /* 0x3f5  GREEK LUNATE EPSILON SYMBOL */

#if 0
  { "", 0x9e }, /* peseta sign? */
  { "", 0x9f }, /* franc symbol? */
  0xa9 REVERSED NOT SIGN 2310
  0xaa NOT SIGN
  0xab VULGAR FRACTION ONE HALF
  0xac VULGAR FRACTION ONE QUARTER
#endif

  /* graphical characters including 'hatching' 0xb0 through 0xb3 omitted */

  /* box drawing characters 0xbf through 0xdf omitted */

  /* mathematical symbols 0xf0 to 0xff omitted */

  { NULL, 0 }
};

