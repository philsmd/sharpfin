/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2006 Rob Landley
 * Copyright (C) 2006 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* Saves 2 bytes on x86! Oh my... */
int sigaction_set(int signum, const struct sigaction *act)
{
	return sigaction(signum, act, NULL);
}

int sigprocmask_allsigs(int how)
{
	sigset_t set;
	sigfillset(&set);
	return sigprocmask(how, &set, NULL);
}

void bb_signals(int sigs, void (*f)(int))
{
	int sig_no = 0;
	int bit = 1;

	while (sigs) {
		if (sigs & bit) {
			sigs &= ~bit;
			signal(sig_no, f);
		}
		sig_no++;
		bit <<= 1;
	}
}

void bb_signals_recursive(int sigs, void (*f)(int))
{
	int sig_no = 0;
	int bit = 1;
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = f;
	/*sa.sa_flags = 0;*/
	/*sigemptyset(&sa.sa_mask); - hope memset did it*/

	while (sigs) {
		if (sigs & bit) {
			sigs &= ~bit;
			sigaction_set(sig_no, &sa);
		}
		sig_no++;
		bit <<= 1;
	}
}

void sig_block(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_BLOCK, &ss, NULL);
}

void sig_unblock(int sig)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, sig);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
}

void wait_for_any_sig(void)
{
	sigset_t ss;
	sigemptyset(&ss);
	sigsuspend(&ss);
}

/* Assuming the sig is fatal */
void kill_myself_with_sig(int sig)
{
	signal(sig, SIG_DFL);
	sig_unblock(sig);
	raise(sig);
	_exit(1); /* Should not reach it */
}

void signal_SA_RESTART_empty_mask(int sig, void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	/*sigemptyset(&sa.sa_mask);*/
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = handler;
	sigaction_set(sig, &sa);
}

void signal_no_SA_RESTART_empty_mask(int sig, void (*handler)(int))
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	/*sigemptyset(&sa.sa_mask);*/
	/*sa.sa_flags = 0;*/
	sa.sa_handler = handler;
	sigaction_set(sig, &sa);
}
