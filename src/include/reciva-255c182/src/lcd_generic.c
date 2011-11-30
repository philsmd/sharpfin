/*
 * Reciva LCD driver - non-device-specific functions
 * Copyright (c) 2004-06 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
 
   /*************************************************************************/
   /***                        Include Files                              ***/
   /*************************************************************************/

#define __NO_VERSION__

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include "reciva_util.h"
#include "reciva_lcd.h"
#include "lcd_generic.h"

   /*************************************************************************/
   /***                        Local defines                              ***/
   /*************************************************************************/

#define BL_TIMER_PERIOD		4
#undef LCD_PROFILING

#define DEFAULT_GREET_LINE_1 "Reciva Radio"
#define DEFAULT_GREET_LINE_2 "ARM Powered"

   /*************************************************************************/
   /***                        Static function prototypes                 ***/
   /*************************************************************************/

static int call_draw_screen(char **acText,
                            int    iX,
                            int    iY,
                            int    iCursorType,
                            int   *ppiArrows,
                            int   *ppiLineContents);

static int lcd_ioctl(struct inode *inode, struct file *file,
                     unsigned int cmd, unsigned long arg);

static int resize(int h, int w);

static int  alloc_arrays(void);
static void free_arrays(void);

#ifdef LCD_PROFILING
static void do_profiling(void);
#endif

static void bl_timer_func (unsigned long time);
static void vDrawGreetingFrame(int iFrame);
static int iGreetingFrameThread(void *arg);

   /*************************************************************************/
   /***                        Local typedefs                             ***/
   /*************************************************************************/
   
   /*************************************************************************/
   /***                        Static data                                ***/
   /*************************************************************************/

static struct file_operations reciva_lcd_fops =
{
  owner:    THIS_MODULE,
  ioctl:    lcd_ioctl,
};

static struct miscdevice lcd_miscdev =
{
  MISC_DYNAMIC_MINOR,
  "lcd",
  &reciva_lcd_fops
};

static const struct reciva_lcd_driver *driver;

static char **ppcTexts; // Contains dynamically allocated strings
static int *piArrows;
static int *piLineContents;

static int iHeight, iWidth;

static rutl_hashtable *utf8_to_char;

#ifdef LCD_PROFILING
static int profiling = 0;

MODULE_PARM(profiling, "i");
#endif

static struct timer_list bl_timer;
static int bl_level_current;
static int bl_level_desired;

static struct semaphore start_thread_sem;
static DECLARE_COMPLETION(greeting_frame_complete);
static pid_t tGreetingFramePid = 0; 

static const char *line1frame1 = DEFAULT_GREET_LINE_1;
static const char *line2frame1 = DEFAULT_GREET_LINE_2;
static const char *line1frame2 = DEFAULT_GREET_LINE_1;
static const char *line2frame2 = DEFAULT_GREET_LINE_2;
static int banner_update_time_secs = 3;
MODULE_PARM(line1frame1, "s");
MODULE_PARM_DESC(line1frame1, "The top line of the first greeting frame");
MODULE_PARM(line2frame1, "s");
MODULE_PARM_DESC(line2frame1, "The bottom line of the first greeting frame");
MODULE_PARM(line1frame2, "s");
MODULE_PARM_DESC(line1frame2, "The top line of the second greeting frame");
MODULE_PARM(line2frame2, "s");
MODULE_PARM_DESC(line2frame2, "The bottom line of the second greeting frame");
MODULE_PARM(banner_update_time_secs, "i");

   /*************************************************************************/
   /***                        Global  functions                          ***/
   /*************************************************************************/

int __init
reciva_lcd_init(const struct reciva_lcd_driver *d, int height, int width)
{
  int i, iErr;
  int charmap_length = 50; //XXX this is a guess but shouldn't be too bad
    
  driver  = d;
  iHeight = height;
  iWidth  = width;

  init_timer (&bl_timer);
  bl_timer.function = bl_timer_func;

  /* Initialise LCD */
  driver->init_hardware();

  /* Register the device */
  misc_register (&lcd_miscdev);

  iErr = alloc_arrays();
  if (iErr < 0)
    return iErr;

  /* Set up charmap hashtable */
  utf8_to_char = rutl_new_hashtable(charmap_length);

  if (!utf8_to_char)
  {
    free_arrays();
    return -ENOMEM;
  }
  
  i = 0;
  while (d->charmap[i].pcUTF8Rep) {
    rutl_hashtable_put(utf8_to_char, d->charmap[i].pcUTF8Rep,
                       d->charmap[i].cLocalRep);
    ++i;
  }

  vDrawGreetingFrame(1);

#ifdef LCD_PROFILING
  if (profiling)
    do_profiling();
#endif

  printk("RLCD:%s module: loaded\n", driver->name);

  if (driver->set_backlight)
    reciva_lcd_set_backlight (LCDG_MAX_BACKLIGHT);

#ifdef LCD_DEBUG
  if (driver->test_pattern)
  {
    driver->test_pattern();
  }
#endif

  /* handle changing text during module startup */
  if (strcmp(line1frame1, line1frame2) || strcmp(line2frame1, line2frame2))
  {
    init_MUTEX(&start_thread_sem);
    i = kernel_thread(iGreetingFrameThread, (void *)0, 0);
    if (i >= 0) 
    {
      tGreetingFramePid = i;
      /* Wait for thread to finish init */
      down(&start_thread_sem);
    }
  }
  
  return 0;
}

#ifdef LCD_PROFILING
static char oldstyle_utf8_lookup(const char *character)
{
  int iCount, iCharIndex;
  iCount = 0;

  while (driver->charmap[iCount].pcUTF8Rep)
  {
    // The following five lines seem useless but I'm leaving them in as I'm
    // supposed to be showing how much better my new hashtable-based technique
    // is than what was here before :)
    iCharIndex = 0;
    while (driver->charmap[iCount].pcUTF8Rep[iCharIndex])
    {
      iCharIndex++;
    }
    if (strncmp(character, driver->charmap[iCount].pcUTF8Rep,
          strlen(driver->charmap[iCount].pcUTF8Rep)) == 0)
    {

      return driver->charmap[iCount].cLocalRep;
    }
    iCount++;
  }

  if (character[0] & 1<<7)
    return ' ';
  else
    return character[0];
}

/****************************************************************************
 * Return the number of jiffies taken to translate the given string the given
 * number of times, using the given function.
 ****************************************************************************/
static void time_translation(const char *characters,
                             char (*fn)(const char *),
                             int iterations)
{
  long unsigned int start_jiffies;
  int i, j;
  int len = rutl_count_utf8_chars(characters);
  const char *pc, *pc_next, *pct;
  char c;
  int first_run = 1;

  start_jiffies = jiffies;
  for (j = 0; j < iterations / len; ++j)
  {
    pc = characters;

    for (i = 0; i < len; ++i)
    {
      pc_next = rutl_find_next_utf8(pc);
      c = fn(pc);
      if (first_run) {
        printk(KERN_ERR "Translated 0x");
        for (pct=pc; pct < pc_next; pct++)
          printk("%x", *pct);
        printk(" to 0x%x\n", c);
      }
      pc = pc_next;
    }
    first_run = 0;
  }

  printk(KERN_ERR "%d iterations took %lu jiffies\n", 
         iterations, jiffies - start_jiffies);
}

/****************************************************************************
 * Run some profiling tests on the lcd system and print the results to the
 * kernel log
 ****************************************************************************/
static void do_profiling(void)
{
  int i;
  int lookup_iterations = 10000;
  int draw_iterations = 100;
  const char *c1 = "abcdefghijklmnopqrstuvwxyz"
                   "ABCDEFGHIJKLMNOPQRSTUVWX";
  const char *c2 = LCD_END_SYMBOL_PART1
                   LCD_END_SYMBOL_PART2
                   LCD_END_SYMBOL_PART3
                   LCD_VOLUME_1_BAR
                   LCD_VOLUME_2_BAR
                   LCD_LEFT_ARROW_SOLID
                   LCD_RIGHT_ARROW_SOLID
                   LCD_PAUSE_ICON
                   LCD_EJECT_ICON
                   LCD_FAST_FORWARD_ICON
                   ;

  long unsigned int start_jiffies;

  printk(KERN_ERR "Single-byte, old-style conversion\n");
  time_translation(c1, oldstyle_utf8_lookup, lookup_iterations);

  printk(KERN_ERR "Three-byte, old-style conversion\n");
  time_translation(c2, oldstyle_utf8_lookup, lookup_iterations);

  printk(KERN_ERR "Single-byte characters, new-style conversion:\n");
  time_translation(c1, reciva_lcd_utf8_lookup, lookup_iterations);

  printk(KERN_ERR "Three-byte characters, new-style conversion:\n");
  time_translation(c2, reciva_lcd_utf8_lookup, lookup_iterations);


  printk(KERN_ERR "Drawing %d screenfuls of text (including conversion): ", 
         draw_iterations);
  start_jiffies = jiffies;
  for (i = 0; i < draw_iterations; ++i)
    driver->draw_screen(ppcTexts, 0, 0, 0, piArrows, piLineContents);
  printk("%lu jiffies\n", jiffies - start_jiffies);

  rutl_dump_hashtable_stats(utf8_to_char);
}
#endif

/****************************************************************************
 * Tidy up on exit
 ****************************************************************************/
void __exit
reciva_lcd_exit(void)
{
  if (tGreetingFramePid)
    wait_for_completion(&greeting_frame_complete);
  
  driver->power_off();
  misc_deregister(&lcd_miscdev);
  free_arrays();
  
  printk("RLCD:%s module: unloaded\n", driver->name);
}

/****************************************************************************
 * Function (run in a thread) which changes the screen contents during a fixed
 * time period following module initialisation, then disappears.
 * Ideally we'd provide a way to kill this thread if the module gets unloaded
 * before it finishes its work...
 ****************************************************************************/
static int iGreetingFrameThread(void *arg)
{
  up(&start_thread_sem);
  set_current_state(TASK_INTERRUPTIBLE);
  schedule_timeout(banner_update_time_secs * HZ); /* sleep */
  vDrawGreetingFrame(2);
  complete_and_exit(&greeting_frame_complete, 0);
}

/* Display a friendly message */
static void vDrawGreetingFrame(int iFrame)
{
  int i, n, iErr;
  const char *acLine[] = {DEFAULT_GREET_LINE_1, DEFAULT_GREET_LINE_2};

  switch (iFrame)
  {
    case 1:
      acLine[0] = line1frame1;
      acLine[1] = line2frame1;
      break;

    case 2:
      acLine[0] = line1frame2;
      acLine[1] = line2frame2;
      break;

    default:
      printk(KERN_ERR "Unexpected frame number %d\n", iFrame);
      break;
  }

  if (iHeight < 2)
    n = iHeight;
  else
    n = 2;

  for (i = 0; i < n; ++i)
    ppcTexts[i] = rutl_strdup(acLine[i], GFP_KERNEL);

  iErr = driver->draw_screen(ppcTexts, 0, 0, 0, piArrows, piLineContents);
  if (iErr < 0)
  {
    printk("RLCD: Failed to print initial message (frame %d) (err: %d)\n", iFrame, iErr);
  }
}

/****************************************************************************
 * Get LCD height in chars
 ****************************************************************************/
int reciva_lcd_get_height()
{
  return iHeight;
}

/****************************************************************************
 * Get LCD width in chars
 ****************************************************************************/
int reciva_lcd_get_width()
{
  return iWidth;
}

/****************************************************************************
 * Request backlight level change
 ****************************************************************************/
int reciva_lcd_set_backlight (int level)
{
  printk("reciva_lcd_set_backlight %d\n", level);

  if (level < 0 || level > LCDG_MAX_BACKLIGHT)
    return -EINVAL;

  bl_level_desired = level;
  mod_timer (&bl_timer, jiffies + BL_TIMER_PERIOD);

  return 0;
}

/****************************************************************************
 * Do the lookup of UTF-8 to local representation of a character
 ****************************************************************************/
char reciva_lcd_utf8_lookup(const char *utf8rep)
{
  int iChar;

#define RETURN(r) return r
//#define RETURN(r) do {printk(KERN_ERR "Looked up \"%s\" returning %x\n", utf8rep, r); return r;} while (0)
  iChar = rutl_hashtable_get(utf8_to_char, utf8rep);

  if (iChar == INT_MAX)
  {
    if (utf8rep[0] & 1<<7)
      RETURN(' ');
    else
      RETURN(utf8rep[0]);
  }

  RETURN((char)iChar);
}

/****************************************************************************
 * Do the lookup of unicode to local representation of a character
 ****************************************************************************/
char reciva_lcd_unicode_lookup(const rutl_utf8_seq_info *unicode)
{
  int iChar;

  iChar = rutl_hashtable_search(utf8_to_char, unicode);

  if (iChar == INT_MAX)
  {
    if (unicode->pcSeq[0] & 1<<7)
      return ' ';
    else
      return unicode->pcSeq[0];
  }

  return (char)iChar;
}

/****************************************************************************
 * Free all dynamic memory that depended on the size, and reallocate it to the
 * new size.
 ****************************************************************************/
static int resize(int h, int w)
{
  printk("LCD:resize h=%d, w=%d\n", h,w);
  
  free_arrays();

  iHeight = h;
  iWidth  = w;

  return alloc_arrays();
}

/****************************************************************************
 * Allocate memory for the global variables, using the current values of
 * iHeight and iWidth.
 ****************************************************************************/
static int alloc_arrays()
{
  int iRetVal = 0;
  int i;

  ppcTexts = 0;
  piArrows = 0;
  piLineContents = 0;

  /* ppcTexts */
  if (!(ppcTexts = kmalloc(sizeof(char *) * iHeight, GFP_KERNEL)))
  {
    printk(KERN_ERR "Couldn't kmalloc room for string pointer array\n");
    iRetVal = -ENOMEM;
  }
  else
  {
    memset(ppcTexts, '\0', sizeof(char *) * iHeight);
  }

  /* piArrows */
  if ((iRetVal == 0) &&
      !(piArrows = kmalloc(sizeof(int) * iHeight, GFP_KERNEL)))
  {
    printk(KERN_ERR "Couldn't kmalloc room for Arrows array\n");
    iRetVal = -ENOMEM;
  }

  /* piLineContents */
  if ((iRetVal == 0) &&
      !(piLineContents = kmalloc(sizeof(int) * iHeight, GFP_KERNEL)))
  {
    printk(KERN_ERR "Couldn't kmalloc room for Line Contents array\n");
    iRetVal = -ENOMEM;
  }

  /* Inside each array */
  for (i = 0; i < iHeight; ++i)
  {
    ppcTexts[i] = rutl_strdup(" ", GFP_KERNEL);
    if (!ppcTexts[i])
    {
      iRetVal = -ENOMEM;
      break;
    }
    piArrows[i] = LCD_ARROW_NONE;
    piLineContents[i] = LCD_LINE_CONTENTS_TEXT;
  }

  if (iRetVal != 0)
  {
    free_arrays();
  }

  return iRetVal;
}

/****************************************************************************
 * Free dynamically allocated memory in global variables
 ****************************************************************************/
static void free_arrays()
{
  int iTemp;

  if (ppcTexts != NULL)
  {
    for (iTemp = 0; iTemp < iHeight; iTemp++)
    {
      if (ppcTexts[iTemp])
        kfree(ppcTexts[iTemp]);
    }
    kfree(ppcTexts);
    ppcTexts = NULL;
  }

  kfree(piArrows);
  piArrows = NULL;

  kfree(piLineContents);
  piLineContents = NULL;
}

/****************************************************************************
 * Handle an ioctl
 ****************************************************************************/
static int
lcd_ioctl(struct inode *inode, struct file *file,
          unsigned int cmd, unsigned long arg)
{
  struct lcd_draw_screen ds;
  struct lcd_draw_clock  dc;
  struct lcd_resize      r;
  int capabilities;
  int i;

  switch(cmd)
  {
    case IOC_LCD_DRAW_SCREEN:
      if (copy_from_user(&ds, (void *)arg, sizeof(ds)))
        return -EFAULT;

      return call_draw_screen(ds.acText, ds.iX, ds.iY,
                              ds.iCursorType, ds.piArrows, ds.piLineContents);
      break;

    case IOC_LCD_CLEAR:
      driver->clear_screen();
      break;

    case IOC_LCD_GET_HEIGHT :
      if (put_user(driver->get_height(), (int *)arg))
        return -EFAULT;
      break;

    case IOC_LCD_GET_WIDTH :
      if (put_user(driver->get_width(), (int *)arg))
        return -EFAULT;
      break;

    case IOC_LCD_ZOOM:
      if (!driver->set_zoom)
        return -ENOSYS;

      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;

      driver->set_zoom(i, &r);
      driver->clear_screen();

      return resize(r.iHeight, r.iWidth);

      /* Draw a Grid on the LCD display (test pattern) */
    case IOC_LCD_GRID:
      if (driver->draw_grid)
        driver->draw_grid(0);
      else
        return -ENOSYS;
      break;

      /* Draw an inverse Grid on the LCD display (test pattern) */
    case IOC_LCD_INVERSE_GRID:
      if (driver->draw_grid)
        driver->draw_grid(1);
      else
        return -ENOSYS;
      break;

      /* Draw vertical lines of varying width on the LCD (test pattern) */
    case IOC_LCD_VERT_LINES:
      if (driver->draw_vertical_lines)
        driver->draw_vertical_lines(0);
      else
        return -ENOSYS;
      break;

      /* Draw vertical lines of varying width on the LCD (test pattern) */
    case IOC_LCD_INVERSE_VERT_LINES:
      if (driver->draw_vertical_lines)
        driver->draw_vertical_lines(1);
      else
        return -ENOSYS;
      break;

      /* Backlight control */
    case IOC_LCD_BACKLIGHT:
      if (!driver->set_backlight)
	return -ENOSYS;

      if (copy_from_user(&i, (void *)arg, sizeof(i)))
	return -EFAULT;

      return reciva_lcd_set_backlight (i);

      /* LED control */
    case IOC_LCD_LED:
      if (driver->set_led)
      {
        if (copy_from_user(&i, (void *)arg, sizeof(i)))
          return -EFAULT;
        driver->set_led(i);
      }
      else
      {
        return -ENOSYS;
      }
      break;

      /* LCD power control */
    case IOC_LCD_POWER_OFF:
      if (driver->power_off)
        driver->power_off();
      else
        return -ENOSYS;
      break;

      /* Clock */
    case IOC_LCD_DRAW_CLOCK:
      if (driver->draw_clock)
      {
        if (copy_from_user(&dc, (void *)arg, sizeof(dc)))
          return -EFAULT;
        driver->draw_clock(&dc);
      }
      else
        return -ENOSYS;
      break;

      /* Preset text to use for AM/PM */
    case IOC_LCD_SET_AMPM_TEXT:
      if (driver->set_ampm_text)
      {
        struct lcd_ampm_text   ampm;
        if (copy_from_user(&ampm, (void *)arg, sizeof(ampm)))
          return -EFAULT;
        return driver->set_ampm_text(&ampm);
      }
      else
        return -ENOSYS;
      break;

      /* Signal strength */
    case IOC_LCD_SIGNAL_STRENGTH:
      if (driver->draw_signal_strength)
      {
        if (copy_from_user(&i, (void *)arg, sizeof(i)))
          return -EFAULT;
        driver->draw_signal_strength(i);
      }
      else
        return -ENOSYS;
      break;

      /* Icons */
    case IOC_LCD_DRAW_ICONS:
      if (driver->draw_icons)
      {
        if (copy_from_user(&i, (void *)arg, sizeof(i)))
          return -EFAULT;
        driver->draw_icons(i);
      }
      else
        return -ENOSYS;
      break;

    case IOC_LCD_LEDS_SUPPORTED:
      if (put_user (driver->leds_supported, (int *)arg))
        return -EFAULT;
      break;
      
    case IOC_LCD_GET_CAPABILITIES:
      if (driver->get_capabilities)
        capabilities = driver->get_capabilities();
      else
        capabilities = LCD_CAPABILITIES_ARROWS; // Default
                    
      if (put_user (capabilities, (int *)arg))
        return -EFAULT;
      break;
    
    case IOC_LCD_SET_DISPLAY_MODE:
      if (copy_from_user(&i, (void *)arg, sizeof(i)))
        return -EFAULT;
      
      if (driver->set_display_mode && driver->get_height && driver->get_width)
      {  
        /* Set up the display mode - could result in a resize */
        driver->set_display_mode(i);
        resize(driver->get_height(), driver->get_width());
        if (driver->clear_screen)
          driver->clear_screen();      
      }
      else
      {
        return -EFAULT;
      }  
      break;

    case IOC_LCD_DRAW_BITMAP:
      if (driver->draw_bitmap)
      {
        void *data;
        struct bitmap_data bitmap;
        int datasize;
        if (copy_from_user(&bitmap, (void *)arg, sizeof(struct bitmap_data)))
          return -EFAULT;
        printk(KERN_ERR "bitmap @ 0x%p, data @ 0x%p\n", (void *)arg, bitmap.data);
        datasize = ((bitmap.height * bitmap.width) / 8 + 1);
        data = kmalloc(datasize, GFP_KERNEL);
        if (copy_from_user(data, (void *)bitmap.data, datasize))
          return -EFAULT;
        bitmap.data = data;
        driver->draw_bitmap(bitmap);
        kfree(bitmap.data);
      }
      else
        return -ENOSYS;
      break;
      
    case IOC_LCD_GET_GRAPHICS_HEIGHT:
      if (driver->get_graphics_height)
      {
        int height = driver->get_graphics_height();
        put_user(height, (int *)arg);
      }
      else
        return -ENOSYS;
      break;
       
    case IOC_LCD_GET_GRAPHICS_WIDTH:
      if (driver->get_graphics_width)
      {
        int width = driver->get_graphics_width();
        put_user(width, (int *)arg);
      }
      else
        return -ENOSYS;
      break;

    case IOC_LCD_GRAB_SCREEN_REGION:
      if (driver->grab_screen_region)
      {
        void *pvTargetBuffer;
        struct bitmap_data sUserBump;
        void *pvSourceBuffer;
        if (copy_from_user(&sUserBump, (void *)arg, sizeof(struct bitmap_data)))
          return -EFAULT;
        pvTargetBuffer = sUserBump.data;
        sUserBump = driver->grab_screen_region(sUserBump.left, sUserBump.top, sUserBump.width, sUserBump.height);
        pvSourceBuffer = sUserBump.data;
        sUserBump.data = pvTargetBuffer;
        if (copy_to_user((void *)arg, &sUserBump, sizeof(struct bitmap_data)))
          return -EFAULT;
        if (copy_to_user(sUserBump.data, pvSourceBuffer, (sUserBump.height * sUserBump.width)/8 + 1))
          return -EFAULT;
        kfree(pvSourceBuffer);
      }
      else
        return -ENOSYS;
      break;
     
    /* Contrast control */
    case IOC_LCD_SET_CONTRAST:
      if (driver->set_contrast)
      {
        if (copy_from_user(&i, (void *)arg, sizeof(i)))
          return -EFAULT;
        driver->set_contrast(i);
      }
      else
      {
        return -ENOSYS;
      }
      break;

    /* Screen redraw */
    case IOC_LCD_REDRAW_SCREEN:
      if (driver->redraw_screen)
      {
        driver->redraw_screen();
      }
      else
      {
        return -ENOSYS;
      }
      break;

    default:
      return -ENODEV;
  }

  return 0;
}


   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/
            
/****************************************************************************
 * Update hardware backlight setting
 ****************************************************************************/
static void bl_timer_func (unsigned long time)
{
  if (bl_level_desired > bl_level_current)
    bl_level_current++;
  else if (bl_level_desired < bl_level_current)
    bl_level_current--;

  driver->set_backlight (bl_level_current);

  if (bl_level_desired != bl_level_current)
    {
      bl_timer.expires = jiffies + BL_TIMER_PERIOD;
      add_timer (&bl_timer);
    }
}     

/****************************************************************************
 * Comment here was all wrong
 * Return value is 0 on success or negative on failure
 ****************************************************************************/
static int call_draw_screen(char **ppcTextFromUser,
                          int   iX,
                          int   iY,
                          int   iCursorType,
                          int   *ppiArrows,
                          int   *ppiLineContents)
{
  int iRow, iStringLength;
  char *apcTempPointerArray[iHeight];
 
  if (ppiArrows == NULL)
  {
    printk(KERN_ERR "Got null from ppiArrows\n");
    return -EFAULT;
  }

  /* XXX This stuff needs modifying to handle variable size display */
  if (copy_from_user(piArrows, ppiArrows,
                     sizeof(int) * iHeight))
  {
    printk(KERN_ERR "Error drawing LCD Display, could not copy arrows array\n");
    return -EFAULT;
  }

  if (ppiLineContents == NULL)
  {
    printk(KERN_ERR "Got null from ppiLineContents\n");
    return -EFAULT;
  }

  /* XXX This stuff needs modifying to handle variable size display */
  if (copy_from_user(piLineContents, ppiLineContents,
                     sizeof(int) * iHeight))
  {
    printk(KERN_ERR "Error drawing LCD Display, could not copy line contents array\n");
    return -EFAULT;
  }

  if (copy_from_user(apcTempPointerArray,
                     ppcTextFromUser, 
                     sizeof(char *) * iHeight))
  {
    printk(KERN_ERR "Error drawing LCD Display, could not copy string array pointer\n");     
    return -EFAULT;
  }
 
  for (iRow = 0; iRow < iHeight; iRow++)
  {
    iStringLength = strlen_user(apcTempPointerArray[iRow]);

    if (ppcTexts[iRow])
      kfree(ppcTexts[iRow]);
     
    if (!(ppcTexts[iRow] = kmalloc(iStringLength + 1, GFP_KERNEL)))
    {
      printk(KERN_ERR "Error drawing LCD Display, could not allocate memory\n");
      return -ENOMEM;
    }
        
    if (copy_from_user(ppcTexts[iRow], apcTempPointerArray[iRow], 
                       sizeof(char) * (iStringLength+1)))
    {
      printk(KERN_ERR "Error drawing LCD Display, could not copy string\n");
      return -EFAULT;
    }
  }

  return driver->draw_screen(ppcTexts, iX, iY, iCursorType,
                             piArrows, piLineContents);
}

