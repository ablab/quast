/*
 * $Id: screenbuf.h,v 1.2 2011/09/04 12:01:37 markisch Exp $
 */

/* GNUPLOT - screenbuf.h */

/*
Copyright (c) 2011 Bastian Maerkisch. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED BY Bastian Maerkisch ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SCREENBUF_H
#define SCREENBUF_H

typedef unsigned int uint;

typedef struct typLB
{
    uint size;	/* actual size of the memory buffer */
    uint len;	/* length of the string */
    char *str;
    BYTE *attr;
} LB;
typedef LB * LPLB;


typedef struct typSB
{
    uint size;
    uint head;
    uint tail;
    uint wrap_at;  /* wrap lines at this position */
    LPLB lb;
    LPLB current_line;
    uint last_line;
    uint last_line_index;
    uint length;
} SB;
typedef SB * LPSB;


/* ------------------------------------ */


void sb_init(LPSB sb, uint size);
void sb_resize(LPSB sb, uint size);
void sb_free(LPSB sb);
LPLB sb_get(LPSB sb, uint index);
LPLB sb_get_last(LPSB sb);
int  sb_append(LPSB sb, LPLB lb);
uint sb_length(LPSB sb);
uint sb_calc_length(LPSB sb);
uint sb_lines(LPSB sb, LPLB lb);
uint sb_max_line_length(LPSB sb);
void sb_find_new_pos(LPSB sb, uint x, uint y, uint new_wrap_at, uint * new_x, uint * new_y);
void sb_wrap(LPSB sb, uint wrap_at);
void sb_last_insert_str(LPSB sb, uint pos, char *s, uint count);


/* ------------------------------------ */


void lb_init(LPLB lb);
uint lb_length(LPLB lb);
void lb_insert_char(LPLB lb, uint pos, char ch);
void lb_insert_str(LPLB lb, uint pos, char *s, uint count);
char * lb_substr(LPLB lb, uint offset, uint count);


#endif
