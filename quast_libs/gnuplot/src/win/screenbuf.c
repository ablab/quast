#ifndef lint
static char *RCSid() { return RCSid("$Id: screenbuf.c,v 1.2 2014/03/30 18:33:49 markisch Exp $"); }
#endif

/* GNUPLOT - screenbuf.c 

   Data structure and methods to implement a screen buffer.
   Implementation as a circular buffer.

   (NB: sb->head == sb->tail means NO element stored)
*/

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

#ifdef _Windows
# include <windows.h>
#else
typedef unsigned char BYTE;
#endif
#include <memory.h>
#include <assert.h>
#include "stdfn.h"
#include "screenbuf.h"

static uint sb_internal_length(LPSB sb);
static LPLB sb_internal_get(LPSB sb, uint index);
static void lb_free(LPLB lb);
static void lb_copy(LPLB dest, LPLB src);


/* ------------------------------------ */
/*         line buffer functions        */
/* ------------------------------------ */


/*  lb_init:
 *  initialize a line buffer, mark as free
 */
void 
lb_init(LPLB lb)
{
    assert(lb != NULL);

    lb->str = NULL;
    lb->attr = NULL;
    lb->size = 0;
    lb->len = 0;
}


/*  lb_free:
 *  free members of a line buffer, mark as free
 */
static void 
lb_free(LPLB lb)
{
    assert(lb != NULL);

    free(lb->str);
    free(lb->attr);
    lb_init(lb);
}


/*  lb_copy:
 *  copy a line buffer from <src> to <dest>
 */
static void 
lb_copy(LPLB dest, LPLB src)
{
    assert(dest!= NULL);
    assert(src != NULL);

    dest->str = src->str;
    dest->attr = src->attr;
    dest->size = src->size;
    dest->len = src->len;
}


uint 
lb_length(LPLB lb)
{
    assert(lb != NULL);
    return lb->len;
}


/*  lb_insert_char:
 *  insert a character <ch> at position <pos> into the line buffer,
 *  increase the size of the line buffer if necessary,
 *  fill gaps with spaces
 */
void 
lb_insert_char(LPLB lb, uint pos, char ch)
{
    lb_insert_str(lb, pos, &ch, 1);
}


/*  lb_insert_str:
 *  (actually this is a misnomer as it overwrites any previous text)
 *  insert a string <s> at position <pos> into the line buffer,
 *  increase the size of the line buffer if necessary,
 *  fill gaps with spaces
 */
void 
lb_insert_str(LPLB lb, uint pos, char *s, uint count)
{
    assert(lb != NULL);

    /* enlarge string buffer if necessary */
    if (lb->size <= pos + count) {
	char * newstr;
	uint newsize = ((pos + count + 8) / 8) * 8 + 32;
	newstr = (char *) realloc(lb->str, newsize);
	if (newstr) {
	    lb->str = newstr;
	    lb->size = newsize;
	} else {
	    /* memory allocation failed */
	    if (pos < lb->size)
		return;
	    else
		count = lb->size - pos - 1;
	}
    }
    
    /* fill up with spaces */
    if (pos > lb->len)
	memset(lb->str + lb->len, ' ', pos - lb->len);

    /* copy characters */
    memcpy(lb->str + pos, s, count);
    lb->len = GPMAX(pos + count, lb->len);
    lb->str[lb->len] = '\0';
}


/*  lb_substr:
 *  get a substring from the line buffer, 
 *  this string has to bee free'd afterwards!
 */
char * 
lb_substr(LPLB lb, uint offset, uint count)
{
    uint len;
    char * retval;

    len = (lb != NULL) ? lb->len : 0;

    /* allocate return string */
    retval = (char *) malloc(count + 1);
    if (retval == NULL)
	return NULL;

    if (offset >= len) {
	memset(retval, ' ', count);
    } else {
	if (len >= (count + offset)) {
	    memcpy(retval, lb->str + offset, count);    
	} else {
	    memcpy(retval, lb->str + offset, len - offset);
	    memset(retval + len - offset, ' ', count + offset - len);
	}
    }
    retval[count] = '\0';
    return retval;
}



/* ------------------------------------ */
/*       screen buffer functions        */
/* ------------------------------------ */


/*  sb_init:
 *  initialize a screen buffer with <size> line buffer elements
 */
void 
sb_init(LPSB sb, uint size)
{
    assert(sb != NULL);

    sb->head = sb->tail = 0;
    sb->wrap_at = 0;
    sb->lb = (LPLB) calloc(size + 1, sizeof(LB));
    sb->size = (sb->lb != NULL) ? size + 1 : 0;
    sb->current_line = (LPLB) malloc(sizeof(LB));
    sb->length = 0;
    sb->last_line = 0;
    sb->last_line_index = 0;
}


/*  sb_free:
 *  free all line buffers of a screen buffer
 */
void 
sb_free(LPSB sb)
{
    uint idx, len;

    assert(sb != NULL);
    assert(sb->lb != NULL);

    /* free all line buffers */
    len = sb_internal_length(sb);
    for(idx = 0; idx < len; idx++)
	lb_free(&(sb->lb[idx]));

    free(sb->lb);
    sb->lb = NULL;
    sb->head = sb->tail = 0;
    sb->size = 0;
}


/*  sb_internal_get:
 *  retrieve line buffer according to index
 */
LPLB 
sb_internal_get(LPSB sb, uint index) 
{
    LPLB line = NULL;

    assert(sb != NULL);
    assert(index < sb->size);
    assert(sb->lb != NULL);

    if (index < sb_internal_length(sb))
	line = &(sb->lb[(sb->head + index) % sb->size]);
    return line;
}


/*  sb_get:
 *  retrieve (wrapped) line buffer
 */
LPLB 
sb_get(LPSB sb, uint index)
{
    LPLB line = NULL;

    assert(sb != NULL); assert((index < sb->size) || (sb->wrap_at > 0));
    assert(sb->lb != NULL);

    if (sb->wrap_at == 0) {
	if (index < sb_internal_length(sb))
	    line = &(sb->lb[(sb->head + index) % sb->size]);
    } else {
	/* count number of wrapped lines */
	uint line_count;
	uint idx;
	uint internal_length = sb_internal_length(sb);

	if (sb->last_line <= index) {
	    /* use cached values */
	    line_count = sb->last_line;
	    idx = sb->last_line_index;
	} else {
	    line_count = 0;
	    idx = 0;
	}
	for ( ; (idx < internal_length); idx++) {
	    line_count += sb_lines(sb, sb_internal_get(sb, idx));
	    if (line_count > index) break;
	}

	if (idx < internal_length) {
	    uint wrapped_lines;
	    uint len;
	    LPLB lb;
	    uint start, count;

	    /* get last line buffer */
	    lb = sb_internal_get(sb, idx);
	    len = lb_length(lb);
	    wrapped_lines = sb_lines(sb, lb);
	    line_count -= wrapped_lines;

	    /* cache current index */
	    sb->last_line_index = idx;
	    sb->last_line = line_count;

	    /* index into current line buffer */
	    start = (index - line_count) * sb->wrap_at;
	    count = GPMIN(len - start, sb->wrap_at);

	    /* copy substring from current line buffer */
	    lb_init(sb->current_line);
	    if (lb->str) {
		sb->current_line->len = count;
		sb->current_line->str = lb->str + start;
		//lb_insert_str(sb->current_line, 0, lb->str + start, count);
	    }

	    /* return temporary buffer */
	    line = sb->current_line;
	}
    }
    return line;
}


/*  sb_get_last:
 *  retrieve last line buffer
 */
LPLB 
sb_get_last(LPSB sb)
{
    uint last;

    assert(sb != NULL);

    last = sb_internal_length(sb) - 1;
    return sb_internal_get(sb, last);
}


/*  sb_append:
 *  append a line buffer at the end of the screen buffer,
 *  if the screen buffer is full discard the first line;
 *  the line is _copied_ to the screen buffer
 */
int 
sb_append(LPSB sb, LPLB lb)
{
    uint idx;
    int y_correction = 0;

    assert(sb != NULL);
    assert(lb != NULL);

    idx = sb->tail;
    sb->tail = (sb->tail + 1) % sb->size;
    if (sb->tail == sb->head) {
	y_correction = sb_lines(sb, &(sb->lb[sb->head]));
	lb_free(&(sb->lb[sb->head]));
	sb->head = (sb->head + 1) % sb->size;
    }
    lb_copy(&(sb->lb[idx]), lb);

    sb->length += sb_lines(sb, lb) - y_correction;
    return y_correction;
}


/*  sb_internal_length:
 *  return number of entries (line buffers) of the screen buffer
 */
uint 
sb_internal_length(LPSB sb)
{
    uint lines;
    assert(sb != NULL);

    if (sb->head <= sb->tail)
	lines = sb->tail - sb->head;
    else
	lines = sb->size - 1;

    return lines;
}



/*  sb_length:
 *  get the current number of lines in the screen buffer
 */
uint 
sb_length(LPSB sb)
{
    return sb->length;
}


/*  sb_length:
 *  get the current number of lines in the screen buffer
 */
uint 
sb_calc_length(LPSB sb)
{
    int lines;
    assert(sb != NULL);

    if (sb->wrap_at == 0) {
	lines = sb_internal_length(sb);
    } else {
	uint idx;

	/* count number of wrapped lines */
        for(idx=0, lines=0; idx < sb_internal_length(sb); idx++)
    	    lines += sb_lines(sb, sb_internal_get(sb, idx));
    }
    return lines;
}


/*  sb_resize:
 *  change the maximum number of lines in the screen buffer to <size>
 *  discard lines at the top if necessary
 */
void 
sb_resize(LPSB sb, uint size)
{
    LPLB lb;
    uint sidx, idx, count;
    uint len;

    /* allocate new buffer */
    lb = (LPLB) calloc(size + 1, sizeof(LB));
    if (lb == NULL) /* memory allocation failed */
	return;

    len = sb_internal_length(sb);
    sidx = (size > len) ? 0 : (len - size);
    count = (size > len) ? len : size;

    /* free elements if necessary */
    for(idx = 0; idx < sidx; idx++)
	lb_free(sb_internal_get(sb, idx));

    /* copy elements to new buffer */
    for(idx = 0; idx < count; idx++, sidx++)
	lb_copy(&(lb[idx]), sb_internal_get(sb, sidx));

    /* replace old buffer by new one */
    free(sb->lb);
    sb->lb = lb;
    sb->size = size + 1;
    sb->head = 0;
    sb->tail = count;
}


/*  sb_lines:
 *  return the number of (wrapped) text lines
 */
uint 
sb_lines(LPSB sb, LPLB lb)
{
    if (sb->wrap_at != 0)
	return (lb_length(lb) + sb->wrap_at) / sb->wrap_at;
    else
	return 1;
}


/*  sb_max_line_length:
 *  determine maximum length of a single text line
 */
uint 
sb_max_line_length(LPSB sb)
{
    uint idx;
    uint len;
    uint count;
    
    len = 0;
    count = sb_internal_length(sb);
    for(idx = 0; idx < count; idx++)
	len = GPMAX(lb_length(sb_internal_get(sb, idx)), len);

    if ((sb->wrap_at != 0) && (len > sb->wrap_at))
	len = sb->wrap_at;

    return len;
}


/*  sb_find_new_pos:
 *  determine new x,y position after a change of the wrapping position
 */
void 
sb_find_new_pos(LPSB sb, uint x, uint y, uint new_wrap_at, uint * new_x, uint * new_y)
{
    uint internal_length;
    uint line_count;
    uint old_wrap_at;
    uint idx, xofs;
    uint i;

    /* determine index of corresponding internal line */
    internal_length = sb_internal_length(sb);
    for (idx = line_count = 0; idx < internal_length; idx++) {
	uint lines = sb_lines(sb, sb_internal_get(sb, idx));
	if (line_count + lines > y) break;
	line_count += lines;
    }

    if (line_count == 0) {
	*new_x = *new_y = 0;
	return;
    }

    /* calculate x offset within this line */
    xofs = x + (y - line_count) * sb->wrap_at;

    if (new_wrap_at) {
	/* temporarily switch wrapping */
	old_wrap_at = sb->wrap_at;
	sb->wrap_at = new_wrap_at;
     
	/* count lines with new wrapping */
	for (i = line_count = 0; i < idx; i++)
	    line_count += sb_lines(sb, sb_internal_get(sb, i));

	/* determine new position */
	*new_x = xofs % new_wrap_at;
	*new_y = line_count + (xofs / new_wrap_at);

	/* switch wrapping back */
        sb->wrap_at = old_wrap_at;
    } else {
	*new_x = xofs;
	*new_y = idx;
    }
}


void
sb_wrap(LPSB sb, uint wrap_at)
{
    sb->wrap_at = wrap_at;

    /* invalidate line cache */
    sb->last_line = 0;
    sb->last_line_index = 0;

    /* update length cache */
    sb->length = sb_calc_length(sb);
}


/*  sb_last_insert_str:
 *  call lb_insert str for the last line, 
 *  adjust total number of lines accordingly
 */
void
sb_last_insert_str(LPSB sb, uint pos, char *s, uint count)
{
    LPLB lb;
    uint len;

    lb = sb_get_last(sb);
    len = sb_lines(sb, lb);
    lb_insert_str(lb, pos, s, count);
    /* check if total length of sb has changed */
    sb->length += sb_lines(sb, lb) - len;
}
