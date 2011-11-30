/*
 * linux/reciva/reciva_font_chinese.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Reciva Ltd. All Rights Reserved
 * 
 * Wrapper for Chinese font
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "reciva_font_chinese.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/



   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

#ifdef RECIVA_DUMMY_FONT /* no font data here for this module */

static char acModuleName[] = "Reciva Font (Dummy)";

#else                   /* using Chinese font data */

static char acModuleName[] = "Reciva Font (Chinese)";

/* Data for 16x16 Chinese characters */
static const unsigned short auChineseFont16x16[] =
{
  #include "fontdata_chinese_imt.c"
};

#endif /* RECIVA_DUMMY_FONT */

/* Blank character */
static const unsigned short auBlankChar[] =
{
  0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 
};


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rfont_init(void)
{
  printk("RFONT:%s module: loaded\n", acModuleName);
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rfont_exit(void)
{
  printk("RFONT:%s module: unloaded\n", acModuleName);
}

/****************************************************************************
 * Check character is contained within font
 * iChar - unicode number of character
 * Return true if charcater exists in font
 ****************************************************************************/
int rfont_ischinese(const int iChar)
{
#ifndef RECIVA_DUMMY_FONT
  return ((iChar >= CHINESE_FONT_START) && (iChar <= CHINESE_FONT_END));
#else
  return 0;
#endif
}

/****************************************************************************
 * Lookup font data for a given character
 * iChar - unicode number of character
 * Return: pointer to data (array of 16 16-bit values)
 *         or pointer to a blank character if not in font's range
 ****************************************************************************/
const unsigned short * rfont_lookup_chinese(const int iChar)
{
#ifndef RECIVA_DUMMY_FONT
  if ((iChar >= CHINESE_FONT_START) && (iChar <= CHINESE_FONT_END))
  {
    return &auChineseFont16x16[(iChar - CHINESE_FONT_START) * 16];
  }
  else
  {
    /* Should not be presented with a character outside of the Chinese font
     * but return a blank character if we are */
    return auBlankChar;
  }
#else
  /* For dummy font this function should never be called!
   * but return a blank character just in case */
  return auBlankChar;
#endif
}

EXPORT_SYMBOL(rfont_ischinese);
EXPORT_SYMBOL(rfont_lookup_chinese);

module_init(rfont_init);
module_exit(rfont_exit);

MODULE_AUTHOR("Jonathan Miles <jm@reciva.com>");
MODULE_DESCRIPTION("Reciva Font (Chinese)");
MODULE_LICENSE("GPL");


