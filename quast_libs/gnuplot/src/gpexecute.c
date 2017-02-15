#ifndef lint
static char *RCSid() { return RCSid("$Id: gpexecute.c,v 1.18 2011/03/13 19:55:29 markisch Exp $"); }
#endif

/* GNUPLOT - gpexecute.c */

/*[
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
]*/

/*
 * AUTHORS
 *
 *   Original Software (October 1999 - January 2000):
 *     Pieter-Tjerk de Boer <ptdeboer@cs.utwente.nl>
 *     Petr Mikulik <mikulik@physics.muni.cz>
 *     Johannes Zellner <johannes@zellner.org>
 */

#include "gpexecute.h"

#include "stdfn.h"

#ifdef OS2_IPC
# include <stdio.h>
#endif

#ifdef PIPE_IPC
# include <unistd.h>	/* open(), write() */
# include <stdlib.h>
# include <assert.h>
# include <errno.h>
int pipe_died = 0;
#endif /* PIPE_IPC */

#ifdef WIN_IPC
# include <stdlib.h>
# include <assert.h>
# include "mouse.h"	/* do_event() */
#endif

#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
static gpe_fifo_t *gpe_init __PROTO((void));
static void gpe_push __PROTO((gpe_fifo_t ** base, struct gp_event_t * ge));
static struct gp_event_t *gpe_front __PROTO((gpe_fifo_t ** base));
static int gpe_pop __PROTO((gpe_fifo_t ** base));
#endif /* PIPE_IPC || WIN_IPC */

/*
 * gp_execute functions
 */

#ifdef OS2_IPC
char mouseShareMemName[40];
PVOID input_from_PM_Terminal;
  /* pointer to shared memory for storing the command to be executed */
HEV semInputReady = 0;
  /* handle to event semaphore (post an event to gnuplot that the shared
     memory contains a command to be executed) */
int pausing = 0;
  /* avoid passing data back to gnuplot in `pause' mode */
  /* gplt_x11.c */
ULONG ppidGnu = 0;


/*
 * Let the command in the shared memory be executed.
 */
void
gp_post_shared_mem()
{
    APIRET rc;
    if (semInputReady == 0) {	/* but it must be open for the first time */
	char semInputReadyName[40];
	sprintf(semInputReadyName, "\\SEM32\\GP%i_Input_Ready", (int) ppidGnu);
	DosOpenEventSem(semInputReadyName, &semInputReady);
    }
    rc = DosPostEventSem(semInputReady);
    DosSleep(10);
    /* dirty trick: wait a little bit; otherwise problems to
     * distinguish mouse button down and up, for instance
     * (info sent to shared memory was too fast; maybe a blocking
     * semaphore would help, but no fun to implement it...)
     &*/
}

/* Copy the command (given by the input string) to the shared memory
 * and let gnuplot execute it.
 * If this routine is called during a 'pause', then the command is
 * ignored (shared memory is cleared). Needed for actions launched by a
 * hotkey.
 * Firstly, the command is copied from shared memory to clipboard
 * if this option is set on.
 * Secondly, gnuplot is informed that shared memory contains a command
 * by posting semInputReady event semaphore.
 *
 * OS/2 specific: if (!s), then the command has been already sprintf'ed to
 * the shared memory.
 */
void
gp_execute(char *s)
{
    if (input_from_PM_Terminal == NULL)
	return;
    if (s)			/* copy the command to shared memory */
	strcpy(input_from_PM_Terminal, s);
    if (((char *) input_from_PM_Terminal)[0] == 0)
	return;
    if (pausing) {		/* no communication during pause */
	/* DosBeep(440,111); */
	((char *) input_from_PM_Terminal)[0] = 0;
	return;
    }
    gp_post_shared_mem();
}

#endif /* OS2_IPC */

#if defined(PIPE_IPC) /* || defined(WIN_IPC) */

int buffered_output_pending = 0;

static gpe_fifo_t *
gpe_init()
{
    gpe_fifo_t *base = malloc(sizeof(gpe_fifo_t));
    /* fprintf(stderr, "(gpe_init) \n"); */
    assert(base);
    base->next = (gpe_fifo_t *) 0;
    base->prev = (gpe_fifo_t *) 0;
    return base;
}

static void
gpe_push(gpe_fifo_t ** base, struct gp_event_t *ge)
{
    buffered_output_pending++;
    if ((*base)->prev) {
	gpe_fifo_t *new = malloc(sizeof(gpe_fifo_t));
	/* fprintf(stderr, "(gpe_push) \n"); */
	assert(new);
	(*base)->prev->next = new;
	new->prev = (*base)->prev;
	(*base)->prev = new;
	new->next = (gpe_fifo_t *) 0;
    } else {
	/* first element, this is the case, if the pipe isn't clogged */
	(*base)->next = (gpe_fifo_t *) 0;	/* tail */
	(*base)->prev = (*base);	/* points to itself */
    }
    (*base)->prev->ge = *ge;
}

static struct gp_event_t *
gpe_front(gpe_fifo_t ** base)
{
    return &((*base)->ge);
}

static int
gpe_pop(gpe_fifo_t ** base)
{
    buffered_output_pending--;
    if ((*base)->prev == (*base)) {
	(*base)->prev = (gpe_fifo_t *) 0;
	return 0;
    } else {
	gpe_fifo_t *save = *base;
	/* fprintf(stderr, "(gpe_pop) \n"); */
	(*base)->next->prev = (*base)->prev;
	(*base) = (*base)->next;
	free(save);
	return 1;
    }
}
#endif /* PIPE_IPC || WIN_IPC */

#ifdef PIPE_IPC
RETSIGTYPE
pipe_died_handler(int signum)
{
    (void) signum;		/* avoid -Wunused warning. */
    /* fprintf(stderr, "\n*******(pipe_died_handler)*******\n"); */
    close(1);
    pipe_died = 1;
}
#endif /* PIPE_IPC */

void
gp_exec_event(char type, int mx, int my, int par1, int par2, int winid)
{
    struct gp_event_t ge;
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
    static struct gpe_fifo_t *base = (gpe_fifo_t *) 0;
#endif

    ge.type = type;
    ge.mx = mx;
    ge.my = my;
    ge.par1 = par1;
    ge.par2 = par2;
    ge.winid = winid;
#ifdef PIPE_IPC
    if (pipe_died)
	return;
#endif
    /* HBB 20010218: commented this out for WIN_IPC. We don't actually use the stack,
     * there */
#if defined(PIPE_IPC) /* || defined(WIN_IPC) */
    if (!base) {
	base = gpe_init();
    }
    if (GE_pending != type) {
	gpe_push(&base, &ge);
    } else if (!buffered_output_pending) {
	return;
    }
#endif
#ifdef WIN_IPC
    do_event(&ge);
    return;
#endif
#ifdef PIPE_IPC
    do {
	int status = write(1, gpe_front(&base), sizeof(ge));
	if (-1 == status) {
	    switch (errno) {
	    case EAGAIN:
		/* do nothing */
		FPRINTF((stderr, "(gp_exec_event) EAGAIN\n"));
		break;
	    default:
		FPRINTF((stderr, "(gp_exec_event) errno = %d\n", errno));
		break;
	    }
	    break;
	}
    } while (gpe_pop(&base));
#endif /* PIPE_IPC */

#ifdef OS2_IPC			/* OS/2 communication via shared memory; coded according to gp_execute() */
    if (input_from_PM_Terminal == NULL)
	return;
    ((char *) input_from_PM_Terminal)[0] = '%';	/* flag that passing gp_event_t */
    memcpy(((char *) input_from_PM_Terminal) + 1, &ge, sizeof(ge));	/* copy the command to shared memory */
    if (pausing) {		/* no communication during pause */
	/* DosBeep(440,111); */
	((char *) input_from_PM_Terminal)[0] = 0;
	return;
    }
    gp_post_shared_mem();
#endif
}
