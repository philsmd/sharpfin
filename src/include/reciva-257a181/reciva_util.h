/* Reciva Utilities */

#ifndef __RECIVA_UTIL_H
#define __RECIVA_UTIL_H

struct rutl_unicode_charmap
{
  const char *pcUTF8Rep;
  char cLocalRep;
};

typedef struct rutl_hashtable_element
{
  const char *key;
  unsigned char value;
  struct rutl_hashtable_element *next;
} rutl_hashtable_element;

typedef struct rutl_hashtable
{
  int size;
  rutl_hashtable_element **elements;
} rutl_hashtable;

typedef struct rutl_utf8_seq_info
{
  const char *pcSeq;     /* utf8 sequence to decode */
  int iUnicode;          /* unicode char value decoded from sequence */
  int iHash;             /* hash key for charmap lookup */
  int iLength;           /* length of utf8 sequence in chars */
  const char *pcNextSeq; /* next utf8 sequence in string */
} rutl_utf8_seq_info;

extern int rutl_regwrite(unsigned long bits_to_set, 
                         unsigned long bits_to_clear,
                         int address);

extern char *rutl_find_next_utf8(const char *s);
extern int rutl_count_utf8_chars(const char *s);
extern int rutl_utf8_to_unicode(rutl_utf8_seq_info * const psCodeInfo);

/* Very simplistic hashtable implementation, but does the job */
extern int rutl_utf8_hash(const char *string);
extern rutl_hashtable *rutl_new_hashtable(int size);
extern void rutl_hashtable_put(rutl_hashtable *t, const char *key, char value);
extern int rutl_hashtable_get(const rutl_hashtable *t, const char *key);
extern int rutl_hashtable_search(const rutl_hashtable *t, const rutl_utf8_seq_info *seqInfo);

extern void rutl_dump_hashtable_stats(const rutl_hashtable *t);

extern char *rutl_strdup(const char *s, int flags);

#endif // __RECIVA_UTIL_H

