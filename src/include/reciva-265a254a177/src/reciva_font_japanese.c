/*
 * linux/reciva/reciva_font_japanese.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Reciva Ltd. All Rights Reserved
 * 
 * Font driver - Japanese
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

#include "reciva_lcd.h"
#include "reciva_font.h"
#include "reciva_font_japanese.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

/* Driver functions */
static void *get_font_buffer(void);   
static int get_font_buffer_size(void);
static int is_char_present(int iChar);
static int is_16x16(int iChar);
static const unsigned char *lookup_16x16(int iChar);


   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define NAME "RecivaFontJapanese"
#define PREFIX "RFONTJ:"

#define NUM_CHARS      6879
#define BYTES_PER_CHAR 32
#define BUF_SIZE       (NUM_CHARS*BYTES_PER_CHAR)

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Raw font data
 * This gets initialised via an lcd ioctl */
static unsigned char *pucFontData;

/* This converts a unicode value in range 0x0000 to 0xffff 
 * into an index used to look up the raw character data
 * If character is not present then this returns 0xffff  */
static unsigned short au16Lookup[] =
{
  #include "reciva_fontlookup_japanese.c"
};

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static rfont_driver driver = 
{
  name:                 NAME,
  font_id:              LCD_FONT_ID_JAPANESE,
  get_font_buffer:      get_font_buffer,
  get_font_buffer_size: get_font_buffer_size,
  is_char_present:      is_char_present,
  is_16x16:             is_16x16, 
  lookup_16x16:         lookup_16x16,
};


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Range checking for unicode iChar
 ****************************************************************************/
static inline int in_range(int iChar)
{
  if (iChar >=0 && iChar < sizeof(au16Lookup)/sizeof(au16Lookup[0]))
    return 1;
  else
    return 0;
}

/****************************************************************************
 * Return pointer to font buffer
 ****************************************************************************/
static void *get_font_buffer(void)   
{
  return pucFontData;
}

/****************************************************************************
 * Return font buffer size in bytes
 ****************************************************************************/
static int get_font_buffer_size(void)
{
  return BUF_SIZE;
}

/****************************************************************************
 * Is specified character present in the font
 ****************************************************************************/
static int is_char_present(int iChar)
{
  if (in_range(iChar))
  {
    int index = au16Lookup[iChar];
    if (index > 0 && index < NUM_CHARS)
      return 1;
  }

  return 0;
}

/****************************************************************************
 *  Is specified character double width
 ****************************************************************************/
static int is_16x16(int iChar)
{
  return is_char_present(iChar);
}

/****************************************************************************
 * Return pointer to raw character data or NULL if not found
 ****************************************************************************/
static const unsigned char *lookup_16x16(int iChar)
{
  if (is_char_present(iChar))
  {
    int index = au16Lookup[iChar];
    if (index > 0 && index < NUM_CHARS)
      return &pucFontData[index*BYTES_PER_CHAR];
  }

  return NULL;  
}


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rfontj_init(void)
{
  printk(PREFIX "%s module: loaded\n", NAME);
  rfont_register_driver(&driver);
  pucFontData = vmalloc(BUF_SIZE);
  printk(PREFIX "  buf=%p\n", pucFontData);
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rfontj_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", NAME);
}

module_init(rfontj_init);
module_exit(rfontj_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION(NAME);
MODULE_LICENSE("GPL");


