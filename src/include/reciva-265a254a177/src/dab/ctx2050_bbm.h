/*
 * CTX2050 DAB Modem Control
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef LINUX_CTX2050_BBM_H
#define LINUX_CTX2050_BBM_H

#include <linux/workqueue.h>

extern void bbm_reset(void);
extern int bbm_init(void);
extern void bbm_interrupt_handler(struct work_struct *work);
extern int bbm_init_buffers(void);
extern void bbm_release_buffers(void);
extern int bbm_set_subchannel(int arg);
extern void bbm_get_viterbi_error(unsigned int *viterbi_period,
                                  unsigned int *viterbi_errors);
extern void bbm_reset_fic(void);
extern int bbm_dump_msc_data(void);
extern void bbm_didp_clean(u16 base_address);

#endif // LINUX_CTX2050_BBM_H
