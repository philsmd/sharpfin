/*
 * CTX2050 FCI Tuner Control
 * Copyright (c) 2008 Reciva Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef LINUX_FCI_TUNER_H
#define LINUX_FCI_TUNER_H

extern void fci_init(void);
extern int fci_tune(int freq);
extern int fci_tune_lock(void);
extern int fci_tune_stop(void);
extern int fci_rf_power(void);

#endif // LINUX_FCI_TUNER_H
