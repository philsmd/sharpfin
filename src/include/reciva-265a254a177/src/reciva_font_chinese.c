/*
 * linux/reciva/reciva_font_chinese.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Reciva Ltd. All Rights Reserved
 * 
 * Font driver - Chinese
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>

#include "reciva_lcd.h"
#include "reciva_font.h"
#include "reciva_font_chinese.h"


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

#define NAME "RecivaFontChinese"
#define PREFIX "RFONTC:"

#define NUM_CHARS      20903    
#define BYTES_PER_CHAR 32
#define BUF_SIZE       (NUM_CHARS*BYTES_PER_CHAR)

#define CHINESE_FONT_START 0x4e00
#define CHINESE_FONT_END   0x9fa5

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Data for 16x16 Chinese characters 
 * This gets sent down from application  */
static unsigned char *pucFontData;

/* Note that any elements not explicitly initialised here are guaranteed to 
 * be zero initialised */
static rfont_driver driver = 
{
  name:                 NAME,
  font_id:              LCD_FONT_ID_CHINESE,
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
  if ((iChar >= CHINESE_FONT_START) && (iChar <= CHINESE_FONT_END))
    return 1;
  else 
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
    return &pucFontData[(iChar - CHINESE_FONT_START) * BYTES_PER_CHAR];

  return NULL;  
}


   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rfontc_init(void)
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
rfontc_exit(void)
{
  printk(PREFIX "%s module: unloaded\n", NAME);
}

module_init(rfontc_init);
module_exit(rfontc_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION(NAME);
MODULE_LICENSE("GPL");

