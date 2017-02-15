#ifndef lint
static char *RCSid() { return RCSid("$Id: time.c,v 1.28.2.3 2015/10/22 16:16:33 sfeam Exp $"); }
#endif

/* GNUPLOT - time.c */

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


/* This module either adds a routine gstrptime() to read a formatted time,
 * augmenting the standard suite of time routines provided by ansi,
 * or it completely replaces the whole lot with a new set of routines,
 * which count time relative to the year 2000. Default is to use the
 * new routines. Define USE_SYSTEM_TIME to use the system routines, at your
 * own risk. One problem in particular is that not all systems allow
 * the time with integer value 0 to be represented symbolically, which
 * prevents use of relative times.  Also, the system routines do not allow
 * you to read in fractional seconds.
 */


#include "gp_time.h"

#include "util.h"
#include "variable.h"

static char *read_int __PROTO((char *s, int nr, int *d));

static char *
read_int(char *s, int nr, int *d)
{
    int result = 0;

    while (--nr >= 0 && *s >= '0' && *s <= '9')
	result = result * 10 + (*s++ - '0');

    *d = result;
    return (s);
}



#ifndef USE_SYSTEM_TIME

/* a new set of routines to completely replace the ansi ones
 * Use at your own risk
 */

static int gdysize __PROTO((int yr));

static int mndday[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static size_t xstrftime __PROTO((char *buf, size_t bufsz, const char *fmt, struct tm * tm, double usec, double fulltime));

/* days in year */
static int
gdysize(int yr)
{

    if (!(yr % 4)) {
	if ((!(yr % 100)) && yr % 400)
	    return (365);
	return (366);
    }
    return (365);
}


/* new strptime() and gmtime() to allow time to be read as 24 hour,
 * and spaces in the format string. time is converted to seconds from
 * year 2000.... */

char *
gstrptime(char *s, char *fmt, struct tm *tm, double *usec)
{
    int yday = 0;
    TBOOLEAN sanity_check_date = FALSE;

    tm->tm_mday = 1;
    tm->tm_mon = tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
    /* make relative times work (user-defined tic step) */
    tm->tm_year = ZERO_YEAR;

    /* Fractional seconds will be returned separately, since
     * there is no slot for the fraction in struct tm.
     */
    *usec = 0.0;

    /* we do not yet calculate wday or yday, so make them illegal
     * [but yday will be read by %j]
     */
    tm->tm_yday = tm->tm_wday = -1;

    /* If the format requests explicit day, month, or year, then we will
     * do sanity checking to make sure the input makes sense.
     * For backward compatibility with gnuplot versions through 4.6.6
     * hour, minute, seconds default to zero with no error return
     * if the corresponding field cannot be found or interpreted.
     */ 
    if (strstr(fmt,"%d")) {
	tm->tm_mday = -1;
	sanity_check_date = TRUE;
    }
    if (strstr(fmt,"%y") || strstr(fmt,"%Y")) {
	tm->tm_year = -1;
	sanity_check_date = TRUE;
    }
    if (strstr(fmt,"%m") || strstr(fmt,"%B") || strstr(fmt,"%b")) {
	tm->tm_mon = -1;
	sanity_check_date = TRUE;
    }


    while (*fmt) {
	if (*fmt != '%') {
	    if (*fmt == ' ') {
		/* space in format means zero or more spaces in input */
		while (*s == ' ')
		    ++s;
		++fmt;
		continue;
	    } else if (*fmt == *s) {
		++s;
		++fmt;
		continue;
	    } else
		break;		/* literal match has failed */
	}
	/* we are processing a percent escape */

	switch (*++fmt) {
	case 'b':		/* abbreviated month name */
	    {
		int m;

		for (m = 0; m < 12; ++m)
		    if (strncasecmp(s, abbrev_month_names[m],
				    strlen(abbrev_month_names[m])) == 0) {
			s += strlen(abbrev_month_names[m]);
			goto found_abbrev_mon;
		    }
		/* get here => not found */
		int_warn(DATAFILE, "Bad abbreviated month name");
		m = 0;
	      found_abbrev_mon:
		tm->tm_mon = m;
		break;
	    }

	case 'B':		/* full month name */
	    {
		int m;

		for (m = 0; m < 12; ++m)
		    if (strncasecmp(s, full_month_names[m],
				    strlen(full_month_names[m])) == 0) {
			s += strlen(full_month_names[m]);
			goto found_full_mon;
		    }
		/* get here => not found */
		int_warn(DATAFILE, "Bad full month name");
		m = 0;
	      found_full_mon:
		tm->tm_mon = m;
		break;
	    }

	case 'd':		/* read a day of month */
	    s = read_int(s, 2, &tm->tm_mday);
	    break;

	case 'm':		/* month number */
	    s = read_int(s, 2, &tm->tm_mon);
	    --tm->tm_mon;
	    break;

	case 'y':		/* year number */
	    s = read_int(s, 2, &tm->tm_year);
	    /* In line with the current UNIX98 specification by
	     * The Open Group and major Unix vendors,
	     * two-digit years 69-99 refer to the 20th century, and
	     * values in the range 00-68 refer to the 21st century.
	     */
	    if (tm->tm_year <= 68)
		tm->tm_year += 100;
	    tm->tm_year += 1900;
	    break;

	case 'Y':
	    s = read_int(s, 4, &tm->tm_year);
	    break;

	case 'j':
	    s = read_int(s, 3, &tm->tm_yday);
	    tm->tm_yday--;
	    sanity_check_date = TRUE;
	    yday++;
	    break;

	case 'H':
	    s = read_int(s, 2, &tm->tm_hour);
	    break;

	case 'M':
	    s = read_int(s, 2, &tm->tm_min);
	    break;

	case 'S':
	    s = read_int(s, 2, &tm->tm_sec);
	    if (*s == '.' || (decimalsign && *s == *decimalsign))
		*usec = atof(s);
	    break;


	case 's':
	    /* read EPOCH data
	     * EPOCH is the std. unix timeformat seconds since 01.01.1970 UTC
	     */
	    {
		char  *fraction = strchr(s, decimalsign ? *decimalsign : '.');
		double ufraction = 0;
		double when = strtod (s, &s) - SEC_OFFS_SYS;
		ggmtime(tm, when);
		if (fraction && fraction < s)
		    ufraction = atof(fraction);
		if (ufraction < 1.)		/* Filter out e.g. 123.456e7 */
		    *usec = ufraction;
		break;
	    }

	default:
	    int_warn(DATAFILE, "Bad time format in string");
	}
	fmt++;
    }

    FPRINTF((stderr, "read date-time : %02d/%02d/%d:%02d:%02d:%02d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec));

    /* now sanity check the date/time entered, normalising if necessary
     * read_int cannot read a -ve number, but can read %m=0 then decrement
     * it to -1
     */

#define S (tm->tm_sec)
#define M (tm->tm_min)
#define H (tm->tm_hour)

    if (S >= 60) {
	M += S / 60;
	S %= 60;
    }
    if (M >= 60) {
	H += M / 60;
	M %= 60;
    }
    if (H >= 24) {
	if (yday)
	    tm->tm_yday += H / 24;
	tm->tm_mday += H / 24;
	H %= 24;
    }
#undef S
#undef M
#undef H

    FPRINTF((stderr, "normalised time : %02d/%02d/%d:%02d:%02d:%02d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec));

    if (sanity_check_date) {
	if (yday) {

	    if (tm->tm_yday < 0) {
		// int_error(DATAFILE, "Illegal day of year");
		return (NULL);
	    }

	    /* we just set month to jan, day to yday, and let the
	     * normalising code do the work.
	     */

	    tm->tm_mon = 0;
	    /* yday is 0->365, day is 1->31 */
	    tm->tm_mday = tm->tm_yday + 1;
	}
	if (tm->tm_mon < 0) {
	    // int_error(DATAFILE, "illegal month");
	    return (NULL);
	}
	if (tm->tm_mday < 1) {
	    // int_error(DATAFILE, "illegal day of month");
	    return (NULL);
	}
	if (tm->tm_mon > 11) {
	    tm->tm_year += tm->tm_mon / 12;
	    tm->tm_mon %= 12;
	} {
	    int days_in_month;
	    while (tm->tm_mday > (days_in_month = (mndday[tm->tm_mon] + (tm->tm_mon == 1 && (gdysize(tm->tm_year) > 365))))) {
		if (++tm->tm_mon == 12) {
		    ++tm->tm_year;
		    tm->tm_mon = 0;
		}
		tm->tm_mday -= days_in_month;
	    }
	}
    }
    return (s);
}

size_t
gstrftime(char *s, size_t bsz, const char *fmt, double l_clock)
{
    struct tm tm;
    double usec;

    ggmtime(&tm, l_clock);

    usec = l_clock - (double)floor(l_clock);

    return xstrftime(s, bsz, fmt, &tm, usec, l_clock);
}


static size_t
xstrftime(
    char *str,			/* output buffer */
    size_t bsz,			/* space available */
    const char *fmt,
    struct tm *tm,
    double usec,
    double fulltime)
{
    size_t l = 0;			/* chars written so far */
    int incr = 0;			/* chars just written */
    char *s = str;
    TBOOLEAN sign_printed = FALSE;

    memset(str, '\0', bsz);

    while (*fmt != '\0') {
	if (*fmt != '%') {
	    if (l >= bsz-1)
		return 0;
	    *s++ = *fmt++;
	    l++;
	} else {
	    /* set up format modifiers */
	    int w = 0;
	    int z = 0;
	    int p = 0;

	    if (*++fmt == '0') {
		z = 1;
		++fmt;
	    }
	    while (*fmt >= '0' && *fmt <= '9') {
		w = w * 10 + (*fmt - '0');
		++fmt;
	    }
	    if (*fmt == '.') {
		++fmt;
		while (*fmt >= '0' && *fmt <= '9') {
		    p = p * 10 + (*fmt - '0');
		    ++fmt;
		}
		if (p > 6) p = 6;
	    }

	    switch (*fmt++) {

		/* some shorthands : check that there is space in the
		 * output string. */
#define CHECK_SPACE(n) do {				\
		    if ((l+(n)) > bsz) return 0;	\
		} while (0)

		/* copy a fixed string, checking that there's room */
#define COPY_STRING(z) do {			\
		    CHECK_SPACE(strlen(z)) ;	\
		    strcpy(s, z);		\
		} while (0)

		/* format a string, using default spec if none given w
		 * and z are width and zero-flag dw and dz are the
		 * defaults for these In fact, CHECK_SPACE(w) is not a
		 * sufficient test, since sprintf("%2d", 365) outputs
		 * three characters
		 */
#define FORMAT_STRING(dz, dw, x) do {				\
		    if (w==0) {					\
			w=(dw);					\
			if (!z)					\
			    z=(dz);				\
		    }						\
		    incr = snprintf(s, bsz-l-1, z ? "%0*d" : "%*d", w, (x));	\
		    CHECK_SPACE(incr);				\
		} while(0)

	    case '%':
		CHECK_SPACE(1);
		*s = '%';
		break;

	    case 'a':
		COPY_STRING(abbrev_day_names[tm->tm_wday]);
		break;

	    case 'A':
		COPY_STRING(full_day_names[tm->tm_wday]);
		break;

	    case 'b':
	    case 'h':
		COPY_STRING(abbrev_month_names[tm->tm_mon]);
		break;

	    case 'B':
		COPY_STRING(full_month_names[tm->tm_mon]);
		break;

	    case 'd':
		FORMAT_STRING(1, 2, tm->tm_mday);	/* %02d */
		break;

	    case 'D':
		if (!xstrftime(s, bsz - l, "%m/%d/%y", tm, 0., fulltime))
		    return 0;
		break;

	    case 'F':
		if (!xstrftime(s, bsz - l, "%Y-%m-%d", tm, 0., fulltime))
		    return 0;
		break;

	    case 'H':
		FORMAT_STRING(1, 2, tm->tm_hour);	/* %02d */
		break;

	    case 'I':
		FORMAT_STRING(1, 2, (tm->tm_hour + 11) % 12 + 1); /* %02d */
		break;

	    case 'j':
		FORMAT_STRING(1, 3, tm->tm_yday + 1);	/* %03d */
		break;

		/* not in linux strftime man page. Not really needed now */
	    case 'k':
		FORMAT_STRING(0, 2, tm->tm_hour);	/* %2d */
		break;

	    case 'l':
		FORMAT_STRING(0, 2, (tm->tm_hour + 11) % 12 + 1); /* %2d */
		break;

	    case 'm':
		FORMAT_STRING(1, 2, tm->tm_mon + 1);	/* %02d */
		break;

	    case 'M':
		FORMAT_STRING(1, 2, tm->tm_min);	/* %02d */
		break;

	    case 'p':
		CHECK_SPACE(2);
		strcpy(s, (tm->tm_hour < 12) ? "am" : "pm");
		break;

	    case 'r':
		if (!xstrftime(s, bsz - l, "%I:%M:%S %p", tm, 0., fulltime))
		    return 0;
		break;

	    case 'R':
		if (!xstrftime(s, bsz - l, "%H:%M", tm, 0., fulltime))
		    return 0;
		break;

	    case 's':
		CHECK_SPACE(12); /* large enough for year 9999 */
		sprintf(s, "%.0f", gtimegm(tm));
		break;

	    case 'S':
		FORMAT_STRING(1, 2, tm->tm_sec);	/* %02d */

		/* EAM FIXME - need to implement an actual format specifier */
		if (p > 0) {
		    double base = pow(10., (double)p);
		    int msec = floor(0.5 + base * usec);
		    char *f = &s[strlen(s)];
		    CHECK_SPACE(p+1);
		    sprintf(f, ".%0*d", p, msec<(int)base?msec:(int)base-1);
		}
		break;

	    case 'T':
		if (!xstrftime(s, bsz - l, "%H:%M:%S", tm, 0., fulltime))
		    return 0;
		break;

	    case 't':		/* Time (as opposed to Date) formats */
		{
		int tminute, tsecond;

		    switch (*fmt++) {
		    case 'H':
			/* +/- fractional hours (not wrapped at 24h) */
			if (p > 0) {
			    incr = snprintf(s, bsz-l-1, "%*.*f", w, p, fulltime/3600.);
			    CHECK_SPACE(incr);
			    break;
			}
			/* Set flag in case minutes come next */
			if (fulltime < 0) {
			    CHECK_SPACE(1);	/* the minus sign */
			    sign_printed = TRUE;
			    *s++ = '-';
			    l++;
			}
			/* +/- integral hour truncated toward zero */
			sprintf(s, "%0*d", w, (int)floor(fabs(fulltime/3600.)));
			break;
		    case 'M':
			/* +/- fractional minutes (not wrapped at 60m) */
			if (p > 0) {
			    incr = snprintf(s, bsz-l-1, "%*.*f", w, p,
				    sign_printed ? fabs(fulltime)/60. : fulltime/60.);
			    CHECK_SPACE(incr);
			    break;
			}
			/* +/- integral minute truncated toward zero */
			tminute = floor(60. * (fabs(fulltime/3600.) - floor(fabs(fulltime/3600.))));
			if (fulltime < 0) {
			    if (!sign_printed) {
				sign_printed = TRUE;
				*s++ = '-';
				l++;
			    }
			}
			FORMAT_STRING(1, 2, tminute);	/* %02d */
			break;
		    case 'S':
			/* +/- fractional seconds */
			tsecond = floor(60. * (fabs(fulltime/60.) - floor(fabs(fulltime/60.))));
			if (fulltime < 0) {
			    if (usec > 0)
				usec = 1.0 - usec;
			    if (!sign_printed) {
				*s++ = '-';
				l++;
			    }
			}
			FORMAT_STRING(1, 2, tsecond);	/* %02d */
			if (p > 0) {
			    double base = pow(10., (double)p);
			    int msec = floor(0.5 + base * usec);
			    char *f = &s[strlen(s)];
			    CHECK_SPACE(p+1);
			    sprintf(f, ".%0*d", p, msec<(int)base?msec:(int)base-1);
			}
			break;
		    default:
			break;
		    }
		    break;
		}

	    case 'W':		/* mon 1 day of week */
		{
		    int week;
		    if (tm->tm_yday <= tm->tm_wday) {
			week = 1;

			if ((tm->tm_mday - tm->tm_yday) > 4) {
			    week = 52;
			}
			if (tm->tm_yday == tm->tm_wday && tm->tm_wday == 0)
			    week = 52;

		    } else {

			/* sun prev week */
			int bw = tm->tm_yday - tm->tm_wday;

			if (tm->tm_wday > 0)
			    bw += 7;	/* sun end of week */

			week = (int) bw / 7;

			if ((bw % 7) > 2)	/* jan 1 is before friday */
			    week++;
		    }
		    FORMAT_STRING(1, 2, week);	/* %02d */
		    break;
		}

	    case 'U':		/* sun 1 day of week */
		{
		    int week, bw;

		    if (tm->tm_yday <= tm->tm_wday) {
			week = 1;
			if ((tm->tm_mday - tm->tm_yday) > 4) {
			    week = 52;
			}
		    } else {
			/* sat prev week */
			bw = tm->tm_yday - tm->tm_wday - 1;
			if (tm->tm_wday >= 0)
			    bw += 7;	/* sat end of week */
			week = (int) bw / 7;
			if ((bw % 7) > 1) {	/* jan 1 is before friday */
			    week++;
			}
		    }
		    FORMAT_STRING(1, 2, week);	/* %02d */
		    break;
		}

	    case 'w':		/* day of week, sun=0 */
		FORMAT_STRING(1, 2, tm->tm_wday);	/* %02d */
		break;

	    case 'y':
		FORMAT_STRING(1, 2, tm->tm_year % 100);		/* %02d */
		break;

	    case 'Y':
		FORMAT_STRING(1, 4, tm->tm_year);	/* %04d */
		break;

	    }			/* switch */

	    while (*s != '\0') {
		s++;
		l++;
	    }
#undef CHECK_SPACE
#undef COPY_STRING
#undef FORMAT_STRING
	} /* switch(fmt letter) */
    } /* if(fmt letter not '%') */
    return (l);
}



/* time_t  */
double
gtimegm(struct tm *tm)
{
    int i;
    /* returns sec from year ZERO_YEAR, defined in gp_time.h */
    double dsec = 0.;

    if (tm->tm_year < ZERO_YEAR) {
	for (i = tm->tm_year; i < ZERO_YEAR; i++) {
	    dsec -= (double) gdysize(i);
	}
    } else {
	for (i = ZERO_YEAR; i < tm->tm_year; i++) {
	    dsec += (double) gdysize(i);
	}
    }
    if (tm->tm_mday > 0) {
	for (i = 0; i < tm->tm_mon; i++) {
	    dsec += (double) mndday[i] + (i == 1 && (gdysize(tm->tm_year) > 365));
	}
	dsec += (double) tm->tm_mday - 1;
    } else {
	dsec += (double) tm->tm_yday;
    }
    dsec *= (double) 24;

    dsec += tm->tm_hour;
    dsec *= 60.0;
    dsec += tm->tm_min;
    dsec *= 60.0;
    dsec += tm->tm_sec;

    FPRINTF((stderr, "broken-down time : %02d/%02d/%d:%02d:%02d:%02d = %g seconds\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year, tm->tm_hour,
	     tm->tm_min, tm->tm_sec, dsec));

    return (dsec);
}

int
ggmtime(struct tm *tm, double l_clock)
{
    /* l_clock is relative to ZERO_YEAR, jan 1, 00:00:00,defined in plot.h */
    int i, days;

    /* dodgy way of doing wday - i hope it works ! */
    int wday = JAN_FIRST_WDAY;	/* eg 6 for 2000 */

    FPRINTF((stderr, "%g seconds = ", l_clock));
    if (fabs(l_clock) > 1.e12) {  /* Some time in the year 33688 */
	int_warn(NO_CARET, "time value out of range");
	return(-1);
    }

    tm->tm_year = ZERO_YEAR;
    tm->tm_mday = tm->tm_yday = tm->tm_mon = tm->tm_hour = tm->tm_min = tm->tm_sec = 0;
    if (l_clock < 0) {
	while (l_clock < 0) {
	    int days_in_year = gdysize(--tm->tm_year);
	    l_clock += days_in_year * DAY_SEC;	/* 24*3600 */
	    /* adding 371 is noop in modulo 7 arithmetic, but keeps wday +ve */
	    wday += 371 - days_in_year;
	}
    } else {
	for (;;) {
	    int days_in_year = gdysize(tm->tm_year);
	    if (l_clock < days_in_year * DAY_SEC)
		break;
	    l_clock -= days_in_year * DAY_SEC;
	    tm->tm_year++;
	    /* only interested in result modulo 7, but %7 is expensive */
	    wday += (days_in_year - 364);
	}
    }
    tm->tm_yday = (int) (l_clock / DAY_SEC);
    l_clock -= tm->tm_yday * DAY_SEC;
    tm->tm_hour = (int) l_clock / 3600;
    l_clock -= tm->tm_hour * 3600;
    tm->tm_min = (int) l_clock / 60;
    l_clock -= tm->tm_min * 60;
    tm->tm_sec = (int) l_clock;

    days = tm->tm_yday;

    /* wday%7 should be day of week of first day of year */
    tm->tm_wday = (wday + days) % 7;

    while (days >= (i = mndday[tm->tm_mon] + (tm->tm_mon == 1 && (gdysize(tm->tm_year) > 365)))) {
	days -= i;
	tm->tm_mon++;
    }
    tm->tm_mday = days + 1;

    FPRINTF((stderr, "broken-down time : %02d/%02d/%d:%02d:%02d:%02d\n", tm->tm_mday, tm->tm_mon + 1, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec));

    return (0);
}




#else /* USE_SYSTEM_TIME */

/* define gnu time routines in terms of system time routines */

size_t
gstrftime(char *buf, size_t bufsz, const char *fmt, double l_clock)
{
    time_t t = (time_t) l_clock;
    return strftime(buf, bufsz, fmt, gmtime(&t));
}

double
gtimegm(struct tm *tm)
{
    return (double) mktime(tm);
}

int
ggmtime(struct tm *tm, double l_clock)
{
    time_t t = (time_t) l_clock;
    struct tm *m = gmtime(&t);
    *tm = *m;			/* can any non-ansi compilers not do this ? */
    return 0;
}

/* supplemental routine gstrptime() to read a formatted time */

char *
gstrptime(char *s, char *fmt, struct tm *tm)
{
    FPRINTF((stderr, "gstrptime(\"%s\", \"%s\")\n", s, fmt));

    /* linux does not appear to like years before 1902
     * NT complains if its before 1970
     * initialise fields to midnight, 1st Jan, 1970 (for relative times)
     */
    tm->tm_sec = tm->tm_min = tm->tm_hour = 0;
    tm->tm_mday = 1;
    tm->tm_mon = 0;
    tm->tm_year = 70;
    /* oops - it goes wrong without this */
    tm->tm_isdst = 0;

    for (; *fmt && *s; ++fmt) {
	if (*fmt != '%') {
	    if (*s != *fmt)
		return s;
	    ++s;
	    continue;
	}
	assert(*fmt == '%');

	switch (*++fmt) {
	case 0:
	    /* uh oh - % is last character in format */
	    return s;
	case '%':
	    /* literal % */
	    if (*s++ != '%')
		return s - 1;
	    continue;

#define NOTHING	/* nothing */
#define LETTER(L, width, field, extra)		\
	case L:					\
	    s=read_int(s,width,&tm->field);	\
	    extra;				\
	    continue;

	    LETTER('d', 2, tm_mday, NOTHING);
	    LETTER('m', 2, tm_mon, NOTHING);
	    LETTER('y', 2, tm_year, NOTHING);
	    LETTER('Y', 4, tm_year, tm->tm_year -= 1900);
	    LETTER('H', 2, tm_hour, NOTHING);
	    LETTER('M', 2, tm_min, NOTHING);
	    LETTER('S', 2, tm_sec, NOTHING);
#undef NOTHING
#undef LETTER

	default:
	    int_error(DATAFILE, "incorrect time format character");
	}
    }

    FPRINTF((stderr, "Before mktime : %d/%d/%d:%d:%d:%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec));
    /* mktime range-checks the time */

    if (mktime(tm) == -1) {
	FPRINTF((stderr, "mktime() was not happy\n"));
	int_error(DATAFILE, "Invalid date/time [mktime() did not like it]");
    }
    FPRINTF((stderr, "After mktime : %d/%d/%d:%d:%d:%d\n", tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec));

    return s;
}


#endif /* USE_SYSTEM_TIME */
