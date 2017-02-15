#ifndef lint
static char *RCSid() { return RCSid("$Id: strftime.c,v 1.7 2004/07/01 17:10:08 broeker Exp $"); }
#endif

/* GNUPLOT - strftime.c */

/*[
 * Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
 *
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
 * Implementation of strftime for systems missing this (e.g. vaxctrl)
 *
 * This code was written based on the NeXT strftime man-page, sample output of
 * the function and an ANSI-C quickreference chart. This code does not use
 * parts of any existing strftime implementation.
 *
 * Apparently not all format chars are implemented, but this was all I had in
 * my documentation.
 *
 * (written by Alexander Lehmann)
 */

#define NOTIMEZONE

#include "syscfg.h"		/* for MAX_LINE_LEN */
#include "stdfn.h"		/* for safe_strncpy */

#ifdef TEST_STRFTIME		/* test case; link with stdfn */
#define strftime _strftime


#include "national.h"		/* language info for the following, */
			/* extracted from set.c */

char full_month_names[12][32] =
{FMON01, FMON02, FMON03, FMON04, FMON05,
 FMON06, FMON07, FMON08, FMON09, FMON10, FMON11, FMON12};
char abbrev_month_names[12][8] =
{AMON01, AMON02, AMON03, AMON04, AMON05,
 AMON06, AMON07, AMON08, AMON09, AMON10, AMON11, AMON12};

char full_day_names[7][32] =
{FDAY0, FDAY1, FDAY2, FDAY3, FDAY4, FDAY5, FDAY6};
char abbrev_day_names[7][8] =
{ADAY0, ADAY1, ADAY2, ADAY3, ADAY4, ADAY5, ADAY6};

#endif /* TEST_STRFTIME */

static void fill __PROTO((char *from, char **pto, size_t *pmaxsize));
static void number __PROTO((int num, int pad, char **pto, size_t *pmaxsize));

static void
fill(char *from, char **pto, size_t *pmaxsize)
{
    safe_strncpy(*pto, from, *pmaxsize);
    if (*pmaxsize < strlen(from)) {
	(*pto) += *pmaxsize;
	*pmaxsize = 0;
    } else {
	(*pto) += strlen(from);
	(*pmaxsize) -= strlen(from);
    }
}

static void
number(int num, int pad, char **pto, size_t *pmaxsize)
{
    char str[100];

    sprintf(str, "%0*d", pad, num);
    fill(str, pto, pmaxsize);
}

size_t
strftime(char *s, size_t max, const char *format, const struct tm *tp)
{
    char *start = s;
    size_t maxsize = max;

    if (max > 0) {
	while (*format && max > 0) {
	    if (*format != '%') {
		*s++ = *format++;
		max--;
	    } else {
		format++;
		switch (*format++) {
		case 'a':	/* abbreviated weekday name */
		    if (tp->tm_wday >= 0 && tp->tm_wday <= 6)
			fill(abbrev_day_names[tp->tm_wday], &s, &max);
		    break;
		case 'A':	/* full name of the weekday */
		    if (tp->tm_wday >= 0 && tp->tm_wday <= 6)
			fill(full_day_names[tp->tm_wday], &s, &max);
		    break;
		case 'b':	/* abbreviated month name */
		    if (tp->tm_mon >= 0 && tp->tm_mon <= 11)
			fill(abbrev_month_names[tp->tm_mon], &s, &max);
		    break;
		case 'B':	/* full name of month */
		    if (tp->tm_mon >= 0 && tp->tm_mon <= 11)
			fill(full_month_names[tp->tm_mon], &s, &max);
		    break;
		case 'c':	/* locale's date and time reprensentation */
		    strftime(s, max, "%a %b %X %Y", tp);
		    max -= strlen(s);
		    s += strlen(s);
		    break;
		case 'd':	/* day of the month (01-31) */
		    number(tp->tm_mday, 2, &s, &max);
		    break;
		case 'H':	/* hour of the day (00-23) */
		    number(tp->tm_hour, 2, &s, &max);
		    break;
		case 'I':	/* hour of the day (01-12) */
		    number((tp->tm_hour + 11) % 12 + 1, 2, &s, &max);
		    break;
		case 'j':	/* day of the year (001-366) */
		    number(tp->tm_yday + 1, 3, &s, &max);
		    break;
		case 'm':	/* month of the year (01-12) */
		    number(tp->tm_mon + 1, 2, &s, &max);
		    break;
		case 'M':	/* minute (00-59) */
		    number(tp->tm_min, 2, &s, &max);
		    break;
		case 'p':	/* locale's version of AM or PM */
		    fill(tp->tm_hour >= 6 ? "PM" : "AM", &s, &max);
		    break;
		case 'S':	/* seconds (00-59) */
		    number(tp->tm_sec, 2, &s, &max);
		    break;
		case 'U':	/* week number of the year (00-53) with Sunday as the first day of the week */
		    number((tp->tm_yday - (tp->tm_yday - tp->tm_wday + 7) % 7 + 7) / 7, 1, &s, &max);
		    break;
		case 'w':	/* weekday (Sunday = 0 to Saturday = 6) */
		    number(tp->tm_wday, 1, &s, &max);
		    break;
		case 'W':	/* week number of the year (00-53) with Monday as the first day of the week */
		    number((tp->tm_yday - (tp->tm_yday - tp->tm_wday + 8) % 7 + 7) / 7, 2, &s, &max);
		    break;
		case 'x':	/* locale's date representation */
		    strftime(s, max, "%a %b %d %Y", tp);
		    max -= strlen(s);
		    s += strlen(s);
		    break;
		case 'X':	/* locale's time representation */
#ifndef NOTIMEZONE
		    strftime(s, max, "%H:%M:%S %Z", tp);
#else
		    strftime(s, max, "%H:%M:%S", tp);
#endif
		    max -= strlen(s);
		    s += strlen(s);
		    break;
		case 'y':	/* two-digit year representation (00-99) */
		    number(tp->tm_year % 100, 2, &s, &max);
		    break;
		case 'Y':	/* four-digit year representation */
		    number(tp->tm_year + 1900, 2, &s, &max);
		    break;
#ifndef NOTIMEZONE
		case 'Z':	/* time zone name */
		    fill(tp->tm_zone, &s, &max);
		    break;
#endif
		case '%':	/* percent sign */
		default:
		    *s++ = *(format - 1);
		    max--;
		    break;
		}
	    }
	}
	if (s - start < maxsize) {
	    *s++ = '\0';
	} else {
	    *(s - 1) = '\0';
	}
    }
    return s - start;
}

#ifdef TEST_STRFTIME

#undef strftime
#define test(s)				\
	printf("%s -> ",s );		\
	_strftime(str, 100, s, ts);	\
	printf("%s - ", str);		\
	strftime(str, 100, s, ts);	\
	printf("%s\n", str)

int
main()
{
    char str[100];
    struct tm *ts;
    time_t t;
    int i;

    t = time(NULL);

    ts = localtime(&t);

    test("%c");
    test("test%%test");
    test("%a %b %d %X %Y");
    test("%x %X");
    test("%A %B %U");
    test("%I:%M %p %j %w");

    t -= 245 * 24 * 60 * 60;

    for (i = 0; i < 366; i++) {
	ts = localtime(&t);
	printf("%03d: ", i);
	test("%a %d %m %W");
	t += 24 * 60 * 60;
    }

    return 0;
}

#endif
