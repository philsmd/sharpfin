/* 
 * Sharpfin project
 * Copyright (C) by Steve Clarke and Ico Doornekamp
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this source files. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef key_h
#define key_h
#define EVENT_FD_COUNT 4

struct key_handler {
	int fd[EVENT_FD_COUNT];
};

enum key_state {
	KEY_STATE_RELEASED = 0,
	KEY_STATE_PRESSED = 1,
};

enum key_id {
	KEY_ID_1 = 1,
	KEY_ID_STOP = 1,

	KEY_ID_2 = 2,
	KEY_ID_PLAY = 2,

	KEY_ID_3 = 3,
	KEY_ID_REV = 3,

	KEY_ID_4 = 4,
	KEY_ID_FWD = 4,

	KEY_ID_5 = 5,
	KEY_ID_MENU = 5,

	KEY_ID_SHIFT,
	KEY_ID_BACK,
	KEY_ID_SELECT,
	KEY_ID_REPLY,
	KEY_ID_POWER,

	KEY_ID_LEFT,
	KEY_ID_RIGHT,

	KEY_ID_VOLUP,
	KEY_ID_VOLDN,

	KEY_ID_BROWSE
};

struct key {
	enum key_state state;
	enum key_id id;
};

#define KEY_PRESSED(key, i)  ((key->id == i) && (key->state == KEY_STATE_PRESSED))
#define KEY_RELEASED(key, i) ((key->id == i) && (key->state == KEY_STATE_RELEASED))

struct key_handler *key_init(void);
int key_poll(struct key_handler *eh, struct key *ev);

#endif /* key_h */
