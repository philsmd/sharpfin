/*
 * linux/reciva/reciva_util.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005-2006 Reciva Ltd. All Rights Reserved
 * 
 * Utility functions
 *
 * Version 1.0 2005-04-14  John Stirling <js@reciva.com>
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <asm/delay.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#ifdef CONFIG_ARCH_S3C2410
#include <asm/arch/regs-gpio.h>
#endif

#include "reciva_util.h"


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

static char acModuleName[] = "Reciva Util";
static spinlock_t lock;

#ifdef CONFIG_ARCH_S3C2410
#define DAB_EN        S3C2410_GPA15
#define FM_EN         S3C2410_GPA16
static DABFMFilterState_t dab_fm_filter;
#endif

   /*************************************************************************/
   /***                        Public functions                           ***/
   /*************************************************************************/


/****************************************************************************
 * Module initialisation
 ****************************************************************************/
static int __init 
rutl_init(void)
{
  printk("RUTL:%s module: loaded\n", acModuleName);
  spin_lock_init(&lock);

#ifdef CONFIG_ARCH_S3C2410
  if (machine_is_rirm3())
  {
    /* Set-up FM/DAB BandIII filter control */
    dab_fm_filter = DAB_FM_FILTER_NONE;
    s3c2410_gpio_setpin(DAB_EN, 0);
    s3c2410_gpio_cfgpin(DAB_EN, S3C2410_GPIO_OUTPUT);
    s3c2410_gpio_setpin(FM_EN, 0);
    s3c2410_gpio_cfgpin(FM_EN, S3C2410_GPIO_OUTPUT);
  }
#endif

  return 0;
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit 
rutl_exit(void)
{
  printk("RUTL:%s module: unloaded\n", acModuleName);
}


/****************************************************************************
 * Set/Clear all specified bits leaving the other bits untouched. Disable
 * interrupts around the read/modify/write access. If the same bits are 
 * defined in the set and clear bitmasks then the set will take priority.
 * bits_to_set - bit mask defining which bits should be set
 * bits_to_clear - bit mask defining which bits should be cleared
 * address - register address
 * returns 1 if anything was done, otherwise 0
 ****************************************************************************/
int rutl_regwrite(unsigned long bits_to_set, 
                  unsigned long bits_to_clear,
                  int address)
{
  unsigned long flags;
  unsigned long mask;
  unsigned long before;
  unsigned long temp;

  spin_lock_irqsave(&lock, flags);

  mask = bits_to_set | bits_to_clear;
  temp = __raw_readl(address);
  before = temp;
  temp &= ~mask;
  temp |= bits_to_set;
  __raw_writel (temp, address);

  spin_unlock_irqrestore(&lock, flags);

  return before != temp;
}  

/* Find the start of the next utf8 character in string s */
char *rutl_find_next_utf8(const char *s)
{
  for (s++; ; s++)
  {
    if ((*s) == '\0')
      return (char *)s;

    if ((*s & 3<<6) != 2<<6)
      return (char *)s;
  }
}

/* Count the number of utf8 chars in string s */
int rutl_count_utf8_chars(const char *s)
{
  int i = 0;

  while (*s != '\0')
  {
    s = rutl_find_next_utf8(s);
    ++i;
  }

  return i;
}

/* This hash function only considers the first utf8 character in the string */
int rutl_utf8_hash(const char *string)
{
  const char *p = string;
  int h = *p++;

  /* the following loop terminates either when we reach the end of the string
   * or when we reach a byte that begins a new UTF-8 character */
  while ((*p & 3<<6) == 2<<6) 
  {
    h = (h << 5) - h + *(p++);
  } 

  return h;
}

struct rutl_hashtable *rutl_new_hashtable(int size)
{
  struct rutl_hashtable *ret;
  
  ret = kmalloc(sizeof(struct rutl_hashtable), GFP_KERNEL);

  if (!ret)
    return 0;

  ret->size = size;

  ret->elements = kmalloc(size * sizeof(struct rutl_hashtable_element *),
                          GFP_KERNEL);

  if (!ret->elements) 
  {
    kfree(ret);
    return 0;
  }

  memset(ret->elements, 0, size * sizeof(struct rutl_hashtable_element *));

  return ret;
}

/* Use this carefully!  You must only give this function a single UTF-8
 * character, otherwise the behaviour will not be what you expect!
 * Also it doesn't check to see if the element is already present. */
void rutl_hashtable_put(rutl_hashtable *t, const char *key, unsigned int value)
{
  int bucket;
  rutl_hashtable_element *new_element;

  bucket = rutl_utf8_hash(key) % t->size;
  new_element = kmalloc(sizeof(rutl_hashtable_element), GFP_KERNEL);

  if (!new_element) 
  {
    printk(KERN_ERR "Out of memory in rutl_hashtable_put!\n");
    return;
  }

  new_element->next = t->elements[bucket];
  new_element->key = key;
  new_element->value = value;

  t->elements[bucket] = new_element;
}

/* Returns the char the user placed in the table, or INT_MAX if it wasn't
 * found */
int rutl_hashtable_get(const rutl_hashtable *t, const char *key)
{
  rutl_utf8_seq_info seqInfo;

  seqInfo.pcSeq = key;
  seqInfo.iLength = rutl_find_next_utf8(key) - key;

  if (seqInfo.iLength < 0) {
    return INT_MAX;
  }

  seqInfo.iHash = rutl_utf8_hash(key);
  
  return rutl_hashtable_search(t, &seqInfo);
}

int rutl_hashtable_search(const rutl_hashtable *t,
                          const rutl_utf8_seq_info *seqInfo)
{
  int bucket = seqInfo->iHash % t->size;
  rutl_hashtable_element *element = t->elements[bucket];

  while (element) 
  {
    if (strncmp(seqInfo->pcSeq, element->key, seqInfo->iLength) == 0) {
      return element->value;
    }
    element = element->next;
  }

#ifdef HASH_DEBUG_VERBOSITY
  int tmp;
  if (*(seqInfo->pcSeq) & 1<<7) {
    printk(KERN_ERR "rutl_hashtable_get unexpectedly missed.  key was:\n");
    printk(KERN_ERR);
    for (tmp = 0; tmp < seqInfo->iLength; tmp++)
    {
      printk("%x", seqInfo->pcSeq[tmp]);
    }
    printk("\n");
  }
#endif
  
  return INT_MAX;
}

void rutl_dump_hashtable_stats(const rutl_hashtable *t)
{
  int i, c;
  rutl_hashtable_element *element;
  int *stat_array = kmalloc(t->size * sizeof(int), GFP_KERNEL);

  printk(KERN_ERR "Dump hashtable stats:\n");
  
  if (!stat_array)
  {
    printk(KERN_ERR "Unable to allocate stat_array of size %d!", t->size);
    printk(KERN_ERR "t = %x\n", (int)t);
    return;
  }

  memset(stat_array, 0, t->size * sizeof(int));

  for (i = 0; i < t->size; ++i)
  {
    c = 0;
    element = t->elements[i];
    while (element)
    {
      c++;
      element = element->next;
    }
    stat_array[c]++;
  }

  printk(KERN_ERR "  Size = %d\n", t->size);
  for (i = 0; i < t->size; ++i)
  {
    if (stat_array[i])
      printk(KERN_ERR "  %d buckets with %d elements\n", stat_array[i], i);
  }

  kfree(stat_array);
}

/* 
 * Simple strdup implementation.
 * flags are passed straight to kmalloc, so all usual kmalloc caveats apply.
 */
char *rutl_strdup(const char *in, int flags)
{
  int len;
  char *out;

  len = strlen(in);
  out = kmalloc(len + 1, flags);
  if (!out)
    return 0;

  strncpy(out, in, len + 1);

  return out;
}

/****************************************************************************
 * Calculate a unicode value from a UTF8 sequence
 * psCodeInfo - structure with pcSeq set to sequence to decode. On return the
 *              rest of the structure will be filled in if there was no error
 * Returns 0 on sucess.
 ****************************************************************************/
int rutl_utf8_to_unicode(rutl_utf8_seq_info * const psCodeInfo)
{
  int iResult;
  int iZeroSearchMask;
  int iIndex;
  int iHash;
  const char *pcSeq;

  if (psCodeInfo == NULL)
  {
    return -EFAULT;
  }

  pcSeq = psCodeInfo->pcSeq;
  iHash = pcSeq[0];

  /* Deal with the common case where we have a 1-byte character */
  if ((*pcSeq & (1 << 7)) == 0)
  {
    psCodeInfo->iUnicode = pcSeq[0];
    psCodeInfo->iLength = 1;
    psCodeInfo->iHash = iHash;
    psCodeInfo->pcNextSeq = pcSeq+1;
    return 0;
  }

  /* Check for invalid first UTF-8 character (0x10xxxxxx or 0x11111xxx) */
  if (((*pcSeq & (3 << 6)) == (2 << 6)) || ((*pcSeq & 0xf1) == 0xf1))
  {
    printk("RUTL:Unexpected UTF-8 follow-on character %x\n", *pcSeq);
    return -EILSEQ;
  }

  /* UTF8 uses the first char to indicate over how many bytes the character
   * is encoded:
   *            0x0xxxxxxx - 1 byte
   *            0x110xxxxx - 2 bytes
   *            0x1110xxxx - 3 bytes
   *            0x11110xxx - 4 bytes
   *
   * The following bytes have the format: 0x10xxxxxx
   *
   * We search the first char for the most significant zero bit - this
   * should indicate how many following bytes there are.
   * For each following byte found add it to the result
   */
  iZeroSearchMask = (1 << 6); /* start at 0x1 *1* 0xxxxx */
  iResult = 0;
  iIndex = 1;
  while ((iZeroSearchMask & pcSeq[0]) == iZeroSearchMask)
  {
    /* Check that the following byte is in the right format */
    if (((pcSeq[iIndex] & (3 << 6)) != (2 << 6)))
    {
      printk("RUTL:UTF-8 follow-on character incorrect format %x\n", pcSeq[iIndex]);
      return -EILSEQ;
    }

    /* Add following byte to the result */
    iResult = (iResult << 6) | (pcSeq[iIndex] & ~(3 << 6));
    iHash = (iHash << 5) - iHash + pcSeq[iIndex];

    iIndex++;
    iZeroSearchMask >>= 1;
  }

  /* Add the remaining bits of the first character (i.e. those
   * below the resultant zero-search mask */
  iZeroSearchMask = (iZeroSearchMask << 1) - 1;
  iResult |= (pcSeq[0] & iZeroSearchMask) << ((iIndex - 1) * 6);

  /* Fill in results */
  psCodeInfo->iUnicode = iResult;
  psCodeInfo->iLength = iIndex;
  psCodeInfo->iHash = iHash;
  psCodeInfo->pcNextSeq = pcSeq + iIndex;
  return 0;
}

#ifdef CONFIG_ARCH_S3C2410

/****************************************************************************
 * Set the state of the DAB/FM filter
 * Returns 0 on sucess.
 ****************************************************************************/
int rutl_set_dab_fm_filter(int new_state)
{
  int result = 0;
  if (machine_is_rirm3())
  {
    printk("%s %d from %d\n", __FUNCTION__, new_state, dab_fm_filter);

    if (new_state != DAB_FM_FILTER_NONE)
    {
      if (  (dab_fm_filter != DAB_FM_FILTER_NONE)
          && (dab_fm_filter != new_state))
      {
        printk("Trying to turn use both DAB & FM at once!\n");
        result = 1;
      }
    }

    switch(new_state)
    {
      case DAB_FM_FILTER_NONE:
        s3c2410_gpio_setpin(FM_EN, 0);
        s3c2410_gpio_setpin(DAB_EN, 0);
        break;
      case DAB_FM_FILTER_FM:
        s3c2410_gpio_setpin(FM_EN, 1);
        s3c2410_gpio_setpin(DAB_EN, 0);
        break;
      case DAB_FM_FILTER_DAB:
        s3c2410_gpio_setpin(FM_EN, 0);
        s3c2410_gpio_setpin(DAB_EN, 1);
        break;
      default:
        printk("Bad parameter %d\n", new_state);
        return 1;
    }

    dab_fm_filter = new_state;
  }
  return result;
}

EXPORT_SYMBOL(rutl_set_dab_fm_filter);

#endif

   /*************************************************************************/
   /***                        Private functions                          ***/
   /*************************************************************************/

EXPORT_SYMBOL(rutl_regwrite);
EXPORT_SYMBOL(rutl_find_next_utf8);
EXPORT_SYMBOL(rutl_hashtable_search);
EXPORT_SYMBOL(rutl_count_utf8_chars);
EXPORT_SYMBOL(rutl_strdup);
EXPORT_SYMBOL(rutl_utf8_hash);
EXPORT_SYMBOL(rutl_new_hashtable);
EXPORT_SYMBOL(rutl_hashtable_put);
EXPORT_SYMBOL(rutl_hashtable_get);
EXPORT_SYMBOL(rutl_dump_hashtable_stats);
EXPORT_SYMBOL(rutl_utf8_to_unicode);

module_init(rutl_init);
module_exit(rutl_exit);

MODULE_AUTHOR("John Stirling <js@reciva.com>");
MODULE_DESCRIPTION("Reciva Utility Functions");
MODULE_LICENSE("GPL");


