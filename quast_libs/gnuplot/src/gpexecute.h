/* GNUPLOT - gpexecute.h */

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

#ifndef GPEXECUTE_H
#define GPEXECUTE_H

#include "syscfg.h"
#include "mousecmn.h"

#ifdef OS2_IPC

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>

void gp_execute(char *command);
extern char mouseShareMemName[];

extern PVOID input_from_PM_Terminal;
extern HEV semInputReady;
extern int pausing;
extern ULONG ppidGnu;

/* forward declarations */
void gp_post_shared_mem __PROTO((void));
void gp_execute __PROTO((char *s));

#endif /* OS2_IPC */

#ifdef PIPE_IPC

extern int pipe_died;
RETSIGTYPE pipe_died_handler __PROTO((int signum));

#endif /* PIPE_IPC */

#if defined(PIPE_IPC) || defined(WIN_IPC)

typedef struct gpe_fifo_t {
    struct gpe_fifo_t* prev;
    struct gp_event_t ge;
    struct gpe_fifo_t* next;
} gpe_fifo_t;
extern int buffered_output_pending;

#endif /* PIPE_IPC || WIN_IPC */


void gp_exec_event __PROTO((char type, int mx, int my, int par1, int par2, int winid));

#endif /* GPEXECUTE_H */
