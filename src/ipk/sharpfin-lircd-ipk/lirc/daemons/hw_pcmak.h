/*      $Id: hw_pcmak.h,v 5.2 2007/07/29 18:20:08 lirc Exp $      */

/****************************************************************************
 ** hw_pcmak.h **********************************************************
 ****************************************************************************
 *
 * routines for Logitech receiver 
 * 
 * Copyright (C) 1999 Christoph Bartelmus <lirc@bartelmus.de>
 *	modified for logitech receiver by Isaac Lauer <inl101@alumni.psu.edu>
 *      modified for pcmak receiver P_awe_L <pablozrudnika@wp.pl>
*/

#ifndef HW_PCMAK_H
#define HW_PCMAK_H

#include "drivers/lirc.h"

int pcmak_decode(struct ir_remote *remote, ir_code * prep, ir_code * codep, ir_code * postp, int *repeat_flagp,
		 lirc_t * min_remaining_gapp, lirc_t * max_remaining_gapp);
int pcmak_init(void);
int pcmak_deinit(void);
char *pcmak_rec(struct ir_remote *remotes);

#endif
