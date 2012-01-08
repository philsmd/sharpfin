/*
 * linux/reciva/reciva_font.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2006 Reciva Ltd. All Rights Reserved
 * 
 * Wrapper for font access
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#include "reciva_font.h"


   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Static functions                           ***/
   /*************************************************************************/

   /*************************************************************************/
   /***                        Local Constants                            ***/
   /*************************************************************************/

#define MODULE_NAME "reciva_font"

   /*************************************************************************/
   /***                        Static Data                                ***/
   /*************************************************************************/

/* Blank character */
static const unsigned char aucBlankChar[32];

/* Font drivers */
static rfont_driver *drivers[LCD_FONT_ID_END];

static lcd_language_id_t tCurrentLanguage;


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

/****************************************************************************
 * Range checking
 ****************************************************************************/
static inline int in_range(int font_id)
{
  if (font_id >=0 && font_id < LCD_FONT_ID_END)
    return 1;
  else
    return 0;
}

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/

/****************************************************************************
 * Driver registration
 * Returns 0 on success, -1 on error
 ****************************************************************************/
int rfont_register_driver(rfont_driver *driver)
{
  printk("RFONT register_driver name=%s font_id=%d\n", driver->name, driver->font_id);
  if (in_range(driver->font_id))
  {
    printk("  OK\n");
    drivers[driver->font_id] = driver;
    return 0;
  }

  return -1;
}

/****************************************************************************
 * Return pointer to font buffer
 ****************************************************************************/
void *rfont_get_font_buffer(lcd_font_id_t id)
{
  if (in_range(id) && drivers[id] && drivers[id]->get_font_buffer)
    return drivers[id]->get_font_buffer();
  else
    return NULL;
}

/****************************************************************************
 Return the size of the font buffer
 ****************************************************************************/
int rfont_get_font_buffer_size(lcd_font_id_t id)
{
  if (in_range(id) && drivers[id] && drivers[id]->get_font_buffer_size)
    return drivers[id]->get_font_buffer_size();
  else
    return 0;
}

/****************************************************************************
 * Is specified character double width
 ****************************************************************************/
int rfont_is_16x16(int iChar)
{
  int font_id;
  for (font_id=0; font_id<LCD_FONT_ID_END; font_id++)
  {
    if (drivers[font_id] && drivers[font_id]->is_16x16)
      return drivers[font_id]->is_16x16(iChar);
  }

  return 0;
}

/****************************************************************************
 * Lookup font data for specified character using specified font
 * Returns font data or NULL if not found
 ****************************************************************************/
static const unsigned char *lookup_16x16(int font_id, int iChar)
{
  if (drivers[font_id] && drivers[font_id]->lookup_16x16)
  {
    const unsigned char *data = drivers[font_id]->lookup_16x16(iChar);
    if (data)
      return data;
  }

  return NULL;
}

/****************************************************************************
 * Raw character data
 * For 16x16 characters this is 16 short array
 * If specified character doesn't exist returns a blank char
 ****************************************************************************/
const unsigned char * rfont_lookup_16x16(int iChar)
{
  const unsigned char *data;
  int font_id = LCD_FONT_ID_END;

  switch (tCurrentLanguage)
  {
    case LCD_LANGUAGE_ID_DEFAULT:
      break;
    case LCD_LANGUAGE_ID_JAPANESE:
      font_id = LCD_FONT_ID_JAPANESE;
      break;
    case LCD_LANGUAGE_ID_CHINESE:
      font_id = LCD_FONT_ID_CHINESE;
      break;
  }

  /* Give preference to current language font */
  if (font_id != LCD_FONT_ID_END)
  {
    data = lookup_16x16(font_id, iChar);
    if (data)
      return data;
  }
  
  /* Otherwise find the first match in all fonts */
  for (font_id=0; font_id<LCD_FONT_ID_END; font_id++)
  {
    data = lookup_16x16(font_id, iChar);
    if (data)
      return data;
  }

  return aucBlankChar;
}

/****************************************************************************
 * Notification of current langauge setting in application
 ****************************************************************************/
void rfont_set_language(lcd_language_id_t tLanguage)
{
  tCurrentLanguage = tLanguage;
}

/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rfont_init(void)
{
  printk("RFONT:%s module: loaded\n", MODULE_NAME);
  tCurrentLanguage = LCD_LANGUAGE_ID_DEFAULT;
  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rfont_exit(void)
{
  printk("RFONT:%s module: unloaded\n", MODULE_NAME);
}

EXPORT_SYMBOL(rfont_register_driver);
EXPORT_SYMBOL(rfont_get_font_buffer);
EXPORT_SYMBOL(rfont_get_font_buffer_size);
EXPORT_SYMBOL(rfont_is_16x16);
EXPORT_SYMBOL(rfont_lookup_16x16);
EXPORT_SYMBOL(rfont_set_language);

module_init(rfont_init);
module_exit(rfont_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Font");
MODULE_LICENSE("GPL");


