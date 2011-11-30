/*
 * Reciva LCD driver code for "A-Team" character mapped LCD module
 * Copyright (c) 2004 Nexus Electronics Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */



/* Non generic driver functions. Generic ATeam module expects these
 * functions to be provided by separate driver modules */
void reciva_lcd_init_mode(void);
void reciva_lcd_cycle (int iA0, int iData, int iDelay);


