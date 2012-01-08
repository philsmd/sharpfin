/* vi: set sw=4 ts=4: */
/*
 * Mini reset implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Written by Erik Andersen and Kent Robotti <robotti@metconnect.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* BTW, which "standard" package has this utility? It doesn't seem
 * to be ncurses, coreutils, console-tools... then what? */

#if ENABLE_STTY
int stty_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
#endif

int reset_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int reset_main(int argc ATTRIBUTE_UNUSED, char **argv ATTRIBUTE_UNUSED)
{
	static const char *const args[] = {
		"stty", "sane", NULL
	};

	/* no options, no getopt */

	if (isatty(0) && isatty(1)) {
		/* See 'man 4 console_codes' for details:
		 * "ESC c"			-- Reset
		 * "ESC ( K"		-- Select user mapping
		 * "ESC [ J"		-- Erase display
		 * "ESC [ 0 m"		-- Reset all display attributes
		 * "ESC [ ? 25 h"	-- Make cursor visible.
		 */
		printf("\033c\033(K\033[J\033[0m\033[?25h");
		/* http://bugs.busybox.net/view.php?id=1414:
		 * people want it to reset echo etc: */
#if ENABLE_STTY
		return stty_main(2, (char**)args);
#else
		execvp("stty", (char**)args);
#endif
	}
	return EXIT_SUCCESS;
}
