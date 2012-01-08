/* Reciva Font */

#ifndef __RECIVA_FONT_H
#define __RECIVA_FONT_H


#include "reciva_lcd.h"

typedef struct
{
  const char *name;                  /* Font name */
  lcd_font_id_t font_id;             /* The font ID */
  void * (*get_font_buffer)(void);   /* Return pointer to font buffer */
  int (*get_font_buffer_size)(void); /* Return the size of the font buffer */
  int (*is_char_present)(int iChar); /* Is specified character present in the font */
  int (*is_16x16)(int iChar);        /* Is specified character double width */
  const unsigned char * (*lookup_16x16)(int iChar);  /* Raw character data */  
} rfont_driver;

/* Driver registration */
extern int rfont_register_driver(rfont_driver *driver);



/* Return pointer to font buffer */
extern void *rfont_get_font_buffer(lcd_font_id_t id);

/* Return the size of the font buffer */
extern int rfont_get_font_buffer_size(lcd_font_id_t id);

/* Is specified character double width */
extern int rfont_is_16x16(int iChar);

/* Raw character data
 * For 16x16 characters this is 16 short array */
extern const unsigned char * rfont_lookup_16x16(int iChar);

/* Notification of current langauge setting in application */
extern void rfont_set_language(lcd_language_id_t tLanguage);


#endif // __RECIVA_FONT_H

