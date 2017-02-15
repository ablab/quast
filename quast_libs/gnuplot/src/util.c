#ifndef lint
static char *RCSid() { return RCSid("$Id: util.c,v 1.128.2.5 2016/08/19 16:14:08 sfeam Exp $"); }
#endif

/* GNUPLOT - util.c */

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

#include "util.h"

#include "alloc.h"
#include "command.h"
#include "datafile.h"		/* for df_showdata and df_reset_after_error */
#include "internal.h"		/* for eval_reset_after_error */
#include "misc.h"
#include "plot.h"
#include "term_api.h"		/* for term_end_plot() used by graph_error(), also to detect enhanced mode */
#include "variable.h"		/* For locale handling */
#include "setshow.h"		/* for conv_text() */
#include "tabulate.h"		/* for table_mode */

#if defined(HAVE_DIRENT_H)
# include <sys/types.h>
# include <dirent.h>
#elif defined(_Windows)
# include <windows.h>
#endif
#if defined(__MSC__) || defined (__WATCOMC__)
# include <io.h>
#endif

/* Exported (set-table) variables */

/* decimal sign */
char *decimalsign = NULL;

/* degree sign.  Defaults to UTF-8 but will be changed to match encoding */
char degree_sign[8] = "°";

/* minus sign (encoding-specific string) */
const char *minus_sign = NULL;
TBOOLEAN use_minus_sign = FALSE;

/* Holds the name of the current LC_NUMERIC as set by "set decimal locale" */
char *numeric_locale = NULL;

/* Holds the name of the current LC_TIME as set by "set locale" */
char *current_locale = NULL;

const char *current_prompt = NULL; /* to be set by read_line() */

/* internal prototypes */

static void mant_exp __PROTO((double, double, TBOOLEAN, double *, int *, const char *));
static void parse_sq __PROTO((char *));
static TBOOLEAN utf8_getmore __PROTO((unsigned long * wch, const char **str, int nbytes));
static char *utf8_strchrn __PROTO((const char *s, int N));

/*
 * equals() compares string value of token number t_num with str[], and
 *   returns TRUE if they are identical.
 */
int
equals(int t_num, const char *str)
{
    int i;

    if (t_num < 0 || t_num >= num_tokens)	/* safer to test here than to trust all callers */
	return (FALSE);
    if (!token[t_num].is_token)
	return (FALSE);		/* must be a value--can't be equal */
    for (i = 0; i < token[t_num].length; i++) {
	if (gp_input_line[token[t_num].start_index + i] != str[i])
	    return (FALSE);
    }
    /* now return TRUE if at end of str[], FALSE if not */
    return (str[i] == NUL);
}



/*
 * almost_equals() compares string value of token number t_num with str[], and
 *   returns TRUE if they are identical up to the first $ in str[].
 */
int
almost_equals(int t_num, const char *str)
{
    int i;
    int after = 0;
    int start, length;

    if (t_num < 0 || t_num >= num_tokens)	/* safer to test here than to trust all callers */
	return FALSE;
    if (!str)
	return FALSE;
    if (!token[t_num].is_token)
	return FALSE;		/* must be a value--can't be equal */

    start = token[t_num].start_index;
    length = token[t_num].length;
    for (i = 0; i < length + after; i++) {
	if (str[i] != gp_input_line[start + i]) {
	    if (str[i] != '$')
		return (FALSE);
	    else {
		after = 1;
		start--;	/* back up token ptr */
	    }
	}
    }

    /* i now beyond end of token string */

    return (after || str[i] == '$' || str[i] == NUL);
}



int
isstring(int t_num)
{

    return (token[t_num].is_token &&
	    (gp_input_line[token[t_num].start_index] == '\'' ||
	     gp_input_line[token[t_num].start_index] == '"'));
}

/* Test for the existence of a variable without triggering errors.
 * Return values:
 *  0	variable does not exist or is not defined
 * >0	type of variable: INTGR, CMPLX, STRING
 */
int
type_udv(int t_num)
{
    struct udvt_entry **udv_ptr = &first_udv;

    while (*udv_ptr) {
	if (equals(t_num, (*udv_ptr)->udv_name)) {
	    if ((*udv_ptr)->udv_undef)
		return 0;
	    else
		return (*udv_ptr)->udv_value.type;
	    }
	udv_ptr = &((*udv_ptr)->next_udv);
    }
    return 0;
}

int
isanumber(int t_num)
{
    return (!token[t_num].is_token);
}


int
isletter(int t_num)
{
    unsigned char c = gp_input_line[token[t_num].start_index];
    return (token[t_num].is_token &&
	    (isalpha(c) || (c == '_') || ALLOWED_8BITVAR(c)));
}


/*
 * is_definition() returns TRUE if the next tokens are of the form
 *   identifier =
 *              -or-
 *   identifier ( identifer {,identifier} ) =
 */
int
is_definition(int t_num)
{
    /* variable? */
    if (isletter(t_num) && equals(t_num + 1, "="))
	return 1;

    /* function? */
    /* look for dummy variables */
    if (isletter(t_num) && equals(t_num + 1, "(") && isletter(t_num + 2)) {
	t_num += 3;		/* point past first dummy */
	while (equals(t_num, ",")) {
	    if (!isletter(++t_num))
		return 0;
	    t_num += 1;
	}
	return (equals(t_num, ")") && equals(t_num + 1, "="));
    }
    /* neither */
    return 0;
}



/*
 * copy_str() copies the string in token number t_num into str, appending
 *   a null.  No more than max chars are copied (including \0).
 */
void
copy_str(char *str, int t_num, int max)
{
    int i, start, count;

    if (t_num >= num_tokens) {
	*str = NUL;
	return;
    }

    i = 0;
    start = token[t_num].start_index;
    count = token[t_num].length;

    if (count >= max) {
	count = max - 1;
	FPRINTF((stderr, "str buffer overflow in copy_str"));
    }

    do {
	str[i++] = gp_input_line[start++];
    } while (i != count);
    str[i] = NUL;

}

/* length of token string */
size_t
token_len(int t_num)
{
    return (size_t)(token[t_num].length);
}

#ifdef NEXT
/*
 * quote_str() no longer has any callers in the core code.
 * However, it is called by the next/openstep terminal.
 */

/*
 * quote_str() does the same thing as copy_str, except it ignores the
 *   quotes at both ends.  This seems redundant, but is done for
 *   efficency.
 */
void
quote_str(char *str, int t_num, int max)
{
    int i = 0;
    int start = token[t_num].start_index + 1;
    int count;

    if ((count = token[t_num].length - 2) >= max) {
	count = max - 1;
	FPRINTF((stderr, "str buffer overflow in quote_str"));
    }
    if (count > 0) {
	do {
	    str[i++] = gp_input_line[start++];
	} while (i != count);
    }
    str[i] = NUL;
    /* convert \t and \nnn (octal) to char if in double quotes */
    if (gp_input_line[token[t_num].start_index] == '"')
	parse_esc(str);
    else
	parse_sq(str);
}
#endif

/*
 * capture() copies into str[] the part of gp_input_line[] which lies between
 * the begining of token[start] and end of token[end].
 */
void
capture(char *str, int start, int end, int max)
{
    int i, e;

    e = token[end].start_index + token[end].length;
    if (e - token[start].start_index >= max) {
	e = token[start].start_index + max - 1;
	FPRINTF((stderr, "str buffer overflow in capture"));
    }
    for (i = token[start].start_index; i < e && gp_input_line[i] != NUL; i++)
	*str++ = gp_input_line[i];
    *str = NUL;
}


/*
 * m_capture() is similar to capture(), but it mallocs storage for the
 * string.
 */
void
m_capture(char **str, int start, int end)
{
    int i, e;
    char *s;

    e = token[end].start_index + token[end].length;
    *str = gp_realloc(*str, (e - token[start].start_index + 1), "string");
    s = *str;
    for (i = token[start].start_index; i < e && gp_input_line[i] != NUL; i++)
	*s++ = gp_input_line[i];
    *s = NUL;
}


/*
 * m_quote_capture() is similar to m_capture(), but it removes
 * quotes from either end of the string.
 */
void
m_quote_capture(char **str, int start, int end)
{
    int i, e;
    char *s;

    e = token[end].start_index + token[end].length - 1;
    *str = gp_realloc(*str, (e - token[start].start_index + 1), "string");
    s = *str;
    for (i = token[start].start_index + 1; i < e && gp_input_line[i] != NUL; i++)
	*s++ = gp_input_line[i];
    *s = NUL;

    if (gp_input_line[token[start].start_index] == '"')
	parse_esc(*str);
    else
	parse_sq(*str);

}

/*
 * Wrapper for isstring + m_quote_capture that can be used with
 * or without GP_STRING_VARS enabled.
 * EAM Aug 2004
 */
char *
try_to_get_string()
{
    char *newstring = NULL;
    struct value a;
    int save_token = c_token;

    if (END_OF_COMMAND)
	return NULL;
    const_string_express(&a);
    if (a.type == STRING)
	newstring = a.v.string_val;
    else
	c_token = save_token;

    return newstring;
}


/* Our own version of strdup()
 * Make copy of string into gp_alloc'd memory
 * As with all conforming str*() functions,
 * it is the caller's responsibility to pass
 * valid parameters!
 */
char *
gp_strdup(const char *s)
{
    char *d;

    if (!s)
	return NULL;

#ifndef HAVE_STRDUP
    d = gp_alloc(strlen(s) + 1, "gp_strdup");
    if (d)
	memcpy (d, s, strlen(s) + 1);
#else
    d = strdup(s);
#endif
    return d;
}

/*
 * Allocate a new string and initialize it by concatenating two
 * existing strings.
 */
char *
gp_stradd(const char *a, const char *b)
{
    char *new = gp_alloc(strlen(a)+strlen(b)+1,"gp_stradd");
    strcpy(new,a);
    strcat(new,b);
    return new;
}

/* HBB 20020405: moved these functions here from axis.c, where they no
 * longer truly belong. */
/*{{{  mant_exp - split into mantissa and/or exponent */
/* HBB 20010121: added code that attempts to fix rounding-induced
 * off-by-one errors in 10^%T and similar output formats */
static void
mant_exp(
    double log10_base,
    double x,
    TBOOLEAN scientific,	/* round to power of 3 */
    double *m,			/* results */
    int *p,
    const char *format)		/* format string for fixup */
{
    int sign = 1;
    double l10;
    int power;
    double mantissa;

    /*{{{  check 0 */
    if (x == 0) {
	if (m)
	    *m = 0;
	if (p)
	    *p = 0;
	return;
    }
    /*}}} */
    /*{{{  check -ve */
    if (x < 0) {
	sign = (-1);
	x = (-x);
    }
    /*}}} */

    l10 = log10(x) / log10_base;
    power = floor(l10);
    mantissa = pow(10.0, log10_base * (l10 - power));

    /* round power to an integer multiple of 3, to get what's
     * sometimes called 'scientific' or 'engineering' notation. Also
     * useful for handling metric unit prefixes like 'kilo' or 'micro'
     * */
    if (scientific) {
	/* Scientific mode makes no sense whatsoever if the base of
	 * the logarithmic axis is anything but 10.0 */
	assert(log10_base == 1.0);

	/* HBB FIXED 20040701: negative modulo positive may yield
	 * negative result.  But we always want an effectively
	 * positive modulus --> adjust input by one step */
	switch (power % 3) {
	case -1:
	    power -= 3;
	case 2:
	    mantissa *= 100;
	    break;
	case -2:
	    power -= 3;
	case 1:
	    mantissa *= 10;
	    break;
	case 0:
	    break;
	default:
	    int_error (NO_CARET, "Internal error in scientific number formatting");
	}
	power -= (power % 3);
    }

    /* HBB 20010121: new code for decimal mantissa fixups.  Looks at
     * format string to see how many decimals will be put there.  Iff
     * the number is so close to an exact power of 10 that it will be
     * rounded up to 10.0e??? by an sprintf() with that many digits of
     * precision, increase the power by 1 to get a mantissa in the
     * region of 1.0.  If this handling is not wanted, pass NULL as
     * the format string */
    /* HBB 20040521: extended to also work for bases other than 10.0 */
    if (format) {
	double actual_base = (scientific ? 1000 : pow(10.0, log10_base));
	int precision = 0;
	double tolerance;

	format = strchr (format, '.');
	if (format != NULL)
	    /* a decimal point was found in the format, so use that
	     * precision. */
	    precision = strtol(format + 1, NULL, 10);

	/* See if mantissa would be right on the border.  The
	 * condition to watch out for is that the mantissa is within
	 * one printing precision of the next power of the logarithm
	 * base.  So add the 0.5*10^-precision to the mantissa, and
	 * see if it's now larger than the base of the scale */
	tolerance = pow(10.0, -precision) / 2;
	if (mantissa + tolerance >= actual_base) {
	    mantissa /= actual_base;
	    power += (scientific ? 3 : 1);
	}
    }
    if (m)
	*m = sign * mantissa;
    if (p)
	*p = power;
}

/*}}} */


/*{{{  gprintf */
/* extended s(n)printf */
/* HBB 20010121: added code to maintain consistency between mantissa
 * and exponent across sprintf() calls.  The problem: format string
 * '%t*10^%T' will display 9.99 as '10.0*10^0', but 10.01 as
 * '1.0*10^1'.  This causes problems for people using the %T part,
 * only, with logscaled axes, in combination with the occasional
 * round-off error. */
/* EAM Nov 2012:
 * Unbelievably, the count parameter has been silently ignored or
 * improperly applied ever since this routine was introduced back
 * in version 3.7.  Now fixed to prevent buffer overflow.
 */
void
gprintf(
    char *outstring,
    size_t count,
    char *format,
    double log10_base,
    double x)
{
    char tempdest[MAX_LINE_LEN + 1];
    char temp[MAX_LINE_LEN + 1];
    char *t;
    TBOOLEAN seen_mantissa = FALSE; /* remember if mantissa was already output */
    double stored_power_base = 0;   /* base for the last mantissa output*/
    int stored_power = 0;	/* power matching the mantissa output earlier */
    TBOOLEAN got_hash = FALSE;

    char *dest  = &tempdest[0];
    char *limit = &tempdest[MAX_LINE_LEN];
    static double log10_of_1024; /* to avoid excess precision comparison in check of connection %b -- %B */
    
    log10_of_1024 = log10(1024);
    
#define remaining_space (size_t)(limit-dest)

    *dest = '\0';

    set_numeric_locale();

    /* Oct 2013 - default format is now expected to be "%h" */
    if (((term->flags & TERM_IS_LATEX)) && !strcmp(format, DEF_FORMAT))
	format = DEF_FORMAT_LATEX;

    for (;;) {
	/*{{{  copy to dest until % */
	while (*format != '%')
	    if (!(*dest++ = *format++) || (remaining_space == 0)) {
		goto done;
	    }
	/*}}} */

	/*{{{  check for %% */
	if (format[1] == '%') {
	    *dest++ = '%';
	    format += 2;
	    continue;
	}
	/*}}} */

	/*{{{  copy format part to temp, excluding conversion character */
	t = temp;
	*t++ = '%';
	if (format[1] == '#') {
	    *t++ = '#';
	    format++;
	    got_hash = TRUE;
	}
	/* dont put isdigit first since side effect in macro is bad */
	while (*++format == '.' || isdigit((unsigned char) *format)
	       || *format == '-' || *format == '+' || *format == ' '
	       || *format == '\'')
	    *t++ = *format;
	/*}}} */

	/*{{{  convert conversion character */
	switch (*format) {
	    /*{{{  x and o */
	case 'x':
	case 'X':
	case 'o':
	case 'O':
	    if (fabs(x) >= (double)INT_MAX) {
		t[0] = 'l';
		t[1] = 'l';
		t[2] = *format;
		t[3] = '\0';
		snprintf(dest, remaining_space, temp, (long long) x);
	    } else {
		t[0] = *format;
		t[1] = '\0';
		snprintf(dest, remaining_space, temp, (int) x);
	    }
	    break;
	    /*}}} */
	    /*{{{  e, f and g */
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	    t[0] = *format;
	    t[1] = 0;
	    snprintf(dest, remaining_space, temp, x);
	    break;
	case 'h':
	case 'H':
	    /* g/G with enhanced formating (if applicable) */
	    t[0] = (*format == 'h') ? 'g' : 'G';
	    t[1] = 0;

	    if ((term->flags & (TERM_ENHANCED_TEXT | TERM_IS_LATEX)) == 0) {
		/* Not enhanced, not latex, just print it */
		snprintf(dest, remaining_space, temp, x);

	    } else if (table_mode) {
		/* Tabular output should contain no markup */
		snprintf(dest, remaining_space, temp, x);

	    } else {
		/* in enhanced mode -- convert E/e to x10^{foo} or *10^{foo} */
#define LOCAL_BUFFER_SIZE 256
		char tmp[LOCAL_BUFFER_SIZE];
		char tmp2[LOCAL_BUFFER_SIZE];
		int i,j;
		TBOOLEAN bracket_flag = FALSE;
		snprintf(tmp, 240, temp, x); /* magic number alert: why 240? */
		for (i=j=0; tmp[i] && (i < LOCAL_BUFFER_SIZE); i++) {
		    if (tmp[i]=='E' || tmp[i]=='e') {
			if ((term-> flags & TERM_IS_LATEX)) {
			    if (*format == 'h') {
				strcpy(&tmp2[j], "\\times");
				j+= 6;
			    } else {
				strcpy(&tmp2[j], "\\cdot");
				j+= 5;
			    }
			} else switch (encoding) {
			    case S_ENC_UTF8:
				strcpy(&tmp2[j], "\xc3\x97"); /* UTF character '×' */
				j+= 2;
				break;
			    case S_ENC_CP1252:
				tmp2[j++] = (*format=='h') ? 0xd7 : 0xb7;
				break;
			    case S_ENC_ISO8859_1:
			    case S_ENC_ISO8859_2:
			    case S_ENC_ISO8859_9:
			    case S_ENC_ISO8859_15:
				tmp2[j++] = (*format=='h') ? 0xd7 : '*';
				break;
			    default:
				tmp2[j++] = (*format=='h') ? 'x' : '*';
				break;
			}

			strcpy(&tmp2[j], "10^{");
			j += 4;
			bracket_flag = TRUE;

			/* Skip + and leading 0 in exponent */
			i++; /* skip E */
			if (tmp[i] == '+')
			    i++;
			else if (tmp[i] == '-') /* copy sign */
			    tmp2[j++] = tmp[i++];
			while (tmp[i] == '0')
			    i++;
			i--; /* undo following loop increment */
		    } else {
			tmp2[j++] = tmp[i];
		    }
		}
		if (bracket_flag)
		    tmp2[j++] = '}';
		tmp2[j] = '\0';
		strncpy(dest, tmp2, remaining_space);
#undef LOCAL_BUFFER_SIZE
	    }

	    break;

	    /*}}} */
	    /*{{{  l --- mantissa to current log base */
	case 'l':
	    {
		double mantissa;

		t[0] = 'f';
		t[1] = 0;
		stored_power_base = log10_base;
		mant_exp(stored_power_base, x, FALSE, &mantissa,
				&stored_power, temp);
		seen_mantissa = TRUE;
		snprintf(dest, remaining_space, temp, mantissa);
		break;
	    }
	    /*}}} */
	    /*{{{  t --- base-10 mantissa */
	case 't':
	    {
		double mantissa;

		t[0] = 'f';
		t[1] = 0;
		stored_power_base = 1.0;
		mant_exp(stored_power_base, x, FALSE, &mantissa,
				&stored_power, temp);
		seen_mantissa = TRUE;
		snprintf(dest, remaining_space, temp, mantissa);
		break;
	    }
	    /*}}} */
	    /*{{{  s --- base-1000 / 'scientific' mantissa */
	case 's':
	    {
		double mantissa;

		t[0] = 'f';
		t[1] = 0;
		stored_power_base = 1.0;
		mant_exp(stored_power_base, x, TRUE, &mantissa,
				&stored_power, temp);
		seen_mantissa = TRUE;
		snprintf(dest, remaining_space, temp, mantissa);
		break;
	    }
	    /*}}} */
	    /*{{{  b --- base-1024 mantissa */
	case 'b':
	    {
		double mantissa;

		t[0] = 'f';
		t[1] = 0;
		stored_power_base = log10_of_1024;
		mant_exp(stored_power_base, x, FALSE, &mantissa,
				&stored_power, temp);
		seen_mantissa = TRUE;
		snprintf(dest, remaining_space, temp, mantissa);
		break;
	    }
	    /*}}} */
	    /*{{{  L --- power to current log base */
	case 'L':
	    {
		int power;

		t[0] = 'd';
		t[1] = 0;
		if (seen_mantissa) {
		    if (stored_power_base == log10_base) {
			power = stored_power;
		    } else {
			int_error(NO_CARET, "Format character mismatch: %%L is only valid with %%l");
		    }
		} else {
		    stored_power_base = log10_base;
		    mant_exp(log10_base, x, FALSE, NULL, &power, "%.0f");
		}
		snprintf(dest, remaining_space, temp, power);
		break;
	    }
	    /*}}} */
	    /*{{{  T --- power of ten */
	case 'T':
	    {
		int power;

		t[0] = 'd';
		t[1] = 0;
		if (seen_mantissa) {
		    if (stored_power_base == 1.0) {
			power = stored_power;
		    } else {
			int_error(NO_CARET, "Format character mismatch: %%T is only valid with %%t");
		    }
		} else {
		    mant_exp(1.0, x, FALSE, NULL, &power, "%.0f");
		}
		snprintf(dest, remaining_space, temp, power);
		break;
	    }
	    /*}}} */
	    /*{{{  S --- power of 1000 / 'scientific' */
	case 'S':
	    {
		int power;

		t[0] = 'd';
		t[1] = 0;
		if (seen_mantissa) {
		    if (stored_power_base == 1.0) {
			power = stored_power;
		    } else {
			int_error(NO_CARET, "Format character mismatch: %%S is only valid with %%s");
		    }
		} else {
		    mant_exp(1.0, x, TRUE, NULL, &power, "%.0f");
		}
		snprintf(dest, remaining_space, temp, power);
		break;
	    }
	    /*}}} */
	    /*{{{  c --- ISO decimal unit prefix letters */
	case 'c':
	    {
		int power;

		t[0] = 'c';
		t[1] = 0;
		if (seen_mantissa) {
		    if (stored_power_base == 1.0) {
			power = stored_power;
		    } else {
			int_error(NO_CARET, "Format character mismatch: %%c is only valid with %%s");
		    }
		} else {
		    mant_exp(1.0, x, TRUE, NULL, &power, "%.0f");
		}

		if (power >= -24 && power <= 24) {
		    /* name  power   name  power
		       -------------------------
		       yocto  -24    yotta  24
		       zepto  -21    zetta  21
		       atto   -18    Exa    18
		       femto  -15    Peta   15
		       pico   -12    Tera   12
		       nano    -9    Giga    9
		       micro   -6    Mega    6
		       milli   -3    kilo    3   */
		    /* -18 -> 0, 0 -> 6, +18 -> 12, ... */
		    /* HBB 20010121: avoid division of -ve ints! */
		    power = (power + 24) / 3;
		    snprintf(dest, remaining_space, temp, "yzafpnum kMGTPEZY"[power]);
		} else {
		    /* please extend the range ! */
		    /* fall back to simple exponential */
		    snprintf(dest, remaining_space, "e%+02d", power);
		}
		break;
	    }
	    /*}}} */
	    /*{{{  B --- IEC 60027-2 A.2 / ISO/IEC 80000 binary unit prefix letters */
	case 'B':
	    {
		int power;

		t[0] = 'c';
		t[1] = 'i';
		t[2] = '\0';
		if (seen_mantissa) {
		    if (stored_power_base == log10_of_1024) {
			power = stored_power;
		    } else {
			int_error(NO_CARET, "Format character mismatch: %%B is only valid with %%b");
		    }
		} else {
			mant_exp(log10_of_1024, x, FALSE, NULL, &power, "%.0f");
		}

		if (power > 0 && power <= 8) {
		    /* name  power
		       -----------
		       Yobi   8
		       Zebi   7
		       Exbi   9
		       Pebi   5
		       Tebi   4
		       Gibi   3
		       Mebi   2
		       kibi   1   */
		    snprintf(dest, remaining_space, temp, " kMGTPEZY"[power]);
		} else if (power > 8) {
		    /* for the larger values, print x2^{10}Gi for example */
		    snprintf(dest, remaining_space, "x2^{%d}Yi", power-8);
		} else if (power < 0) {
		    snprintf(dest, remaining_space, "x2^{%d}", power*10);
		} else {
                    snprintf(dest, remaining_space, "  ");
                }

		break;
	    }
	    /*}}} */
	    /*{{{  P --- multiple of pi */
	case 'P':
	    {
		t[0] = 'f';
		t[1] = 0;
		snprintf(dest, remaining_space, temp, x / M_PI);
		break;
	    }
	    /*}}} */
	default:
	   reset_numeric_locale();
	   int_error(NO_CARET, "Bad format character");
	} /* switch */
	/*}}} */

	if (got_hash && (format != strpbrk(format,"oeEfFgG"))) {
	   reset_numeric_locale();
	   int_error(NO_CARET, "Bad format character");
	}

    /* change decimal '.' to the actual entry in decimalsign */
	if (decimalsign != NULL) {
	    char *dotpos1 = dest;
	    char *dotpos2;
	    size_t newlength = strlen(decimalsign);

	    /* dot is the default decimalsign we will be replacing */
	    int dot = *get_decimal_locale();

	    /* replace every dot by the contents of decimalsign */
	    while ((dotpos2 = strchr(dotpos1,dot)) != NULL) {
		if (newlength == 1) {	/* The normal case */
		    *dotpos2 = *decimalsign;
		    dotpos1++;
		} else {		/* Some multi-byte decimal marker */
		    size_t taillength = strlen(dotpos2);
		    dotpos1 = dotpos2 + newlength;
		    if (dotpos1 + taillength > limit)
			int_error(NO_CARET,
				  "format too long due to decimalsign string");
		    /* move tail end of string out of the way */
		    memmove(dotpos1, dotpos2 + 1, taillength);
		    /* insert decimalsign */
		    memcpy(dotpos2, decimalsign, newlength);
		}
	    }
	}

    /* EXPERIMENTAL
     * Some people prefer a "real" minus sign to the hyphen that standard
     * formatted input and output both use.  Unlike decimal signs, there is
     * no internationalization mechanism to specify this preference.
     * This code replaces all hyphens with the character string specified by
     * 'set minus_sign "..."'   typically unicode character U+2212 "−".
     * Use at your own risk.  Should be OK for graphical output, but text output
     * will not be readable by standard formatted input routines.
     */
	if (use_minus_sign		/* set minussign */
	    && minus_sign		/* current encoding provides one */
	    && !table_mode		/* not used inside "set table" */
	    && !(term->flags & TERM_IS_LATEX)	/* but LaTeX doesn't want it */
	   ) {

	    char *dotpos1 = dest;
	    char *dotpos2;
	    size_t newlength = strlen(minus_sign);

	    /* dot is the default hyphen we will be replacing */
	    int dot = '-';

	    /* replace every dot by the contents of minus_sign */
	    while ((dotpos2 = strchr(dotpos1,dot)) != NULL) {
		if (newlength == 1) {	/* The normal case */
		    *dotpos2 = *minus_sign;
		    dotpos1++;
		} else {		/* Some multi-byte minus marker */
		    size_t taillength = strlen(dotpos2);
		    dotpos1 = dotpos2 + newlength;
		    if (dotpos1 + taillength > limit)
			int_error(NO_CARET,
				  "format too long due to minus_sign string");
		    /* move tail end of string out of the way */
		    memmove(dotpos1, dotpos2 + 1, taillength);
		    /* insert minus_sign */
		    memcpy(dotpos2, minus_sign, newlength);
		}
	    }
	}

	/* this was at the end of every single case, before: */
	dest += strlen(dest);
	++format;
    } /* for ever */

done:

#if (0)
    /* Oct 2013 - Not safe because it fails to recognize LaTeX macros.	*/
    /* For LaTeX terminals, if the user has not already provided a   	*/
    /* format in math mode, wrap whatever we got by default in $...$ 	*/
    if (((term->flags & TERM_IS_LATEX)) && !strchr(tempdest, '$')) {
	*(outstring++) = '$';
	strcat(tempdest, "$");
	count -= 2;
    }
#endif

    /* Copy as much as fits */
    safe_strncpy(outstring, tempdest, count);

    reset_numeric_locale();
}

/*}}} */

/* some macros for the error and warning functions below
 * may turn this into a utility function later
 */
#define PRINT_MESSAGE_TO_STDERR				\
do {							\
    fprintf(stderr, "\n%s%s\n",				\
	    current_prompt ? current_prompt : "",	\
	    gp_input_line);				\
} while (0)

#define PRINT_SPACES_UNDER_PROMPT		\
do {						\
    const char *p;				\
						\
    if (!current_prompt)			\
	break;					\
    for (p = current_prompt; *p != '\0'; p++)	\
	(void) fputc(' ', stderr);		\
} while (0)

#define PRINT_SPACES_UPTO_TOKEN						\
do {									\
    int i;								\
									\
    for (i = 0; i < token[t_num].start_index; i++)			\
	(void) fputc((gp_input_line[i] == '\t') ? '\t' : ' ', stderr);	\
} while(0)

#define PRINT_CARET fputs("^\n",stderr);

#define PRINT_FILE_AND_LINE						\
if (!interactive) {							\
    if (lf_head && lf_head->name)                                       \
	fprintf(stderr, "\"%s\", line %d: ", lf_head->name, inline_num);\
    else fprintf(stderr, "line %d: ", inline_num);			\
}

/* TRUE if command just typed; becomes FALSE whenever we
 * send some other output to screen.  If FALSE, the command line
 * will be echoed to the screen before the ^ error message.
 */
TBOOLEAN screen_ok;

#if defined(VA_START) && defined(STDC_HEADERS)
void
os_error(int t_num, const char *str,...)
#else
void
os_error(int t_num, const char *str, va_dcl)
#endif
{
#ifdef VA_START
    va_list args;
#endif
#ifdef VMS
    static status[2] = { 1, 0 };		/* 1 is count of error msgs */
#endif /* VMS */

    /* reprint line if screen has been written to */

    if (t_num == DATAFILE) {
	df_showdata();
    } else if (t_num != NO_CARET) {	/* put caret under error */
	if (!screen_ok)
	    PRINT_MESSAGE_TO_STDERR;

	PRINT_SPACES_UNDER_PROMPT;
	PRINT_SPACES_UPTO_TOKEN;
	PRINT_CARET;
    }
    PRINT_SPACES_UNDER_PROMPT;

#ifdef VA_START
    VA_START(args, str);
# if defined(HAVE_VFPRINTF) || _LIBC
    vfprintf(stderr, str, args);
# else
    _doprnt(str, args, stderr);
# endif
    va_end(args);
#else
    fprintf(stderr, str, a1, a2, a3, a4, a5, a6, a7, a8);
#endif
    putc('\n', stderr);

    PRINT_SPACES_UNDER_PROMPT;
    PRINT_FILE_AND_LINE;

#ifdef VMS
    status[1] = vaxc$errno;
    sys$putmsg(status);
    (void) putc('\n', stderr);
#else /* VMS */
    perror("util.c");
    putc('\n', stderr);
#endif /* VMS */

    scanning_range_in_progress = FALSE;

    bail_to_command_line();
}


#if defined(VA_START) && defined(STDC_HEADERS)
void
int_error(int t_num, const char *str,...)
#else
void
int_error(int t_num, const char str[], va_dcl)
#endif
{
#ifdef VA_START
    va_list args;
#endif

    char error_message[128] = {'\0'};

    /* reprint line if screen has been written to */

    if (t_num == DATAFILE) {
	df_showdata();
    } else if (t_num != NO_CARET) { /* put caret under error */
	if (!screen_ok)
	    PRINT_MESSAGE_TO_STDERR;

	PRINT_SPACES_UNDER_PROMPT;
	PRINT_SPACES_UPTO_TOKEN;
	PRINT_CARET;
    }
    PRINT_SPACES_UNDER_PROMPT;
    PRINT_FILE_AND_LINE;

#ifdef VA_START
    VA_START(args, str);
# if defined(HAVE_VFPRINTF) || _LIBC
    vsnprintf(error_message, sizeof(error_message), str, args);
    fprintf(stderr,"%.120s",error_message);
# else
    _doprnt(str, args, stderr);
# endif
    va_end(args);
#else
    fprintf(stderr, str, a1, a2, a3, a4, a5, a6, a7, a8);
    snprintf(error_message, sizeof(error_message), str, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

    fputs("\n\n", stderr);

    /* We are bailing out of nested context without ever reaching */
    /* the normal cleanup code. Reset any flags before bailing.   */
    df_reset_after_error();
    eval_reset_after_error();
    clause_reset_after_error();
    parse_reset_after_error();
    scanning_range_in_progress = FALSE;
    inside_zoom = FALSE;

    /* Load error state variables */
    update_gpval_variables(2);
    fill_gpval_string("GPVAL_ERRMSG", error_message);

    bail_to_command_line();
}

/* Warn without bailing out to command line. Not a user error */
#if defined(VA_START) && defined(STDC_HEADERS)
void
int_warn(int t_num, const char *str,...)
#else
void
int_warn(int t_num, const char str[], va_dcl)
#endif
{
#ifdef VA_START
    va_list args;
#endif

    /* reprint line if screen has been written to */

    if (t_num == DATAFILE) {
	df_showdata();
    } else if (t_num != NO_CARET) { /* put caret under error */
	if (!screen_ok)
	    PRINT_MESSAGE_TO_STDERR;

	PRINT_SPACES_UNDER_PROMPT;
	PRINT_SPACES_UPTO_TOKEN;
	PRINT_CARET;
    }
    PRINT_SPACES_UNDER_PROMPT;
    PRINT_FILE_AND_LINE;

    fputs("warning: ", stderr);
#ifdef VA_START
    VA_START(args, str);
# if defined(HAVE_VFPRINTF) || _LIBC
    vfprintf(stderr, str, args);
# else
    _doprnt(str, args, stderr);
# endif
    va_end(args);
#else  /* VA_START */
    fprintf(stderr, str, a1, a2, a3, a4, a5, a6, a7, a8);
#endif /* VA_START */
    putc('\n', stderr);
}

/*{{{  graph_error() */
/* handle errors during graph-plot in a consistent way */
/* HBB 20000430: move here, from graphics.c */
#if defined(VA_START) && defined(STDC_HEADERS)
void
graph_error(const char *fmt, ...)
#else
void
graph_error(const char *fmt, va_dcl)
#endif
{
#ifdef VA_START
    va_list args;
#endif

    multiplot = FALSE;
    term_end_plot();

#ifdef VA_START
    VA_START(args, fmt);
    /* HBB 20001120: instead, copy the core code from int_error() to
     * here: */
    PRINT_SPACES_UNDER_PROMPT;
    PRINT_FILE_AND_LINE;

# if defined(HAVE_VFPRINTF) || _LIBC
    vfprintf(stderr, fmt, args);
# else
    _doprnt(fmt, args, stderr);
# endif
    va_end(args);
    fputs("\n\n", stderr);

    bail_to_command_line();
    va_end(args);
#else
    int_error(NO_CARET, fmt, a1, a2, a3, a4, a5, a6, a7, a8);
#endif

}

/*}}} */


/*
 * Reduce all multiple white-space chars to single spaces (if remain == 1)
 * or remove altogether (if remain == 0).  Modifies the original string.
 */
void
squash_spaces(char *s, int remain)
{
    char *r = s;	/* reading point */
    char *w = s;	/* writing point */
    TBOOLEAN space = FALSE;	/* TRUE if we've already copied a space */

    for (w = r = s; *r != NUL; r++) {
	if (isspace((unsigned char) *r)) {
	    /* white space; only copy if we haven't just copied a space */
	    if (!space && remain > 0) {
		space = TRUE;
		*w++ = ' ';
	    }			/* else ignore multiple spaces */
	} else {
	    /* non-space character; copy it and clear flag */
	    *w++ = *r;
	    space = FALSE;
	}
    }
    *w = NUL;			/* null terminate string */
}


/* postprocess single quoted strings: replace "''" by "'"
*/
void
parse_sq(char *instr)
{
    char *s = instr, *t = instr;

    /* the string will always get shorter, so we can do the
     * conversion in situ
     */

    while (*s != NUL) {
	if (*s == '\'' && *(s+1) == '\'')
	    s++;
	*t++ = *s++;
    }
    *t = NUL;
}


void
parse_esc(char *instr)
{
    char *s = instr, *t = instr;

    /* the string will always get shorter, so we can do the
     * conversion in situ
     */

    while (*s != NUL) {
	if (*s == '\\') {
	    s++;
	    if (*s == '\\') {
		*t++ = '\\';
		s++;
	    } else if (*s == 'n') {
		*t++ = '\n';
		s++;
	    } else if (*s == 'r') {
		*t++ = '\r';
		s++;
	    } else if (*s == 't') {
		*t++ = '\t';
		s++;
	    } else if (*s == '\"') {
		*t++ = '\"';
		s++;
	    } else if (*s >= '0' && *s <= '7') {
		int i, n;
		char *octal = (*s == '0' ? "%4o%n" : "%3o%n");
		if (sscanf(s, octal, &i, &n) > 0) {
		    *t++ = i;
		    s += n;
		} else {
		    /* int_error("illegal octal number ", c_token); */
		    *t++ = '\\';
		    *t++ = *s++;
		}
	    }
	} else if (df_separators && *s == '\"' && *(s+1) == '\"') {
	/* EAM Mar 2003 - For parsing CSV strings with quoted quotes */
	    *t++ = *s++; s++;
	} else {
	    *t++ = *s++;
	}
    }
    *t = NUL;
}


/* FIXME HH 20020915: This function does nothing if dirent.h and windows.h
 * not available. */
TBOOLEAN
existdir (const char *name)
{
#ifdef HAVE_DIRENT_H
    DIR *dp;
    if (! (dp = opendir(name) ) )
	return FALSE;

    closedir(dp);
    return TRUE;
#elif defined(_Windows)
    HANDLE FileHandle;
    WIN32_FIND_DATA finddata;

    FileHandle = FindFirstFile(name, &finddata);
    if (FileHandle != INVALID_HANDLE_VALUE) {
	if (finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	    return TRUE;
    }
    return FALSE;
#elif defined(VMS)
    return FALSE;
#else
    int_warn(NO_CARET,
	     "Test on directory existence not supported\n\t('%s!')",
	     name);
    return FALSE;
#endif
}


TBOOLEAN
existfile(const char *name)
{
#ifdef __MSC__
    return (_access(name, 0) == 0);
#else
    return (access(name, F_OK) == 0);
#endif
}


char *
getusername()
{
    char *username = NULL;

    username=getenv("USER");
    if (!username)
	username=getenv("USERNAME");

    return gp_strdup(username);
}

TBOOLEAN contains8bit(const char *s)
{
    while (*s) {
	if ((*s++ & 0x80))
	    return TRUE;
    }
    return FALSE;
}

#define INVALID_UTF8 0xfffful

/* Read from second byte to end of UTF-8 sequence.
   used by utf8toulong() */
static TBOOLEAN
utf8_getmore (unsigned long * wch, const char **str, int nbytes)
{
  int i;
  unsigned char c;
  unsigned long minvalue[] = {0x80, 0x800, 0x10000, 0x200000, 0x4000000};

  for (i = 0; i < nbytes; i++) {
    c = (unsigned char) **str;

    if ((c & 0xc0) != 0x80) {
      *wch = INVALID_UTF8;
      return FALSE;
    }
    *wch = (*wch << 6) | (c & 0x3f);
    (*str)++;
  }

  /* check for overlong UTF-8 sequences */
  if (*wch < minvalue[nbytes-1]) {
    *wch = INVALID_UTF8;
    return FALSE;
  }
  return TRUE;
}

/* Convert UTF-8 multibyte sequence from string to unsigned long character.
   Returns TRUE on success.
*/
TBOOLEAN
utf8toulong (unsigned long * wch, const char ** str)
{
  unsigned char c;

  c =  (unsigned char) *(*str)++;
  if ((c & 0x80) == 0) {
    *wch = (unsigned long) c;
    return TRUE;
  }

  if ((c & 0xe0) == 0xc0) {
    *wch = c & 0x1f;
    return utf8_getmore(wch, str, 1);
  }

  if ((c & 0xf0) == 0xe0) {
    *wch = c & 0x0f;
    return utf8_getmore(wch, str, 2);
  }

  if ((c & 0xf8) == 0xf0) {
    *wch = c & 0x07;
    return utf8_getmore(wch, str, 3);
  }

  if ((c & 0xfc) == 0xf8) {
    *wch = c & 0x03;
    return utf8_getmore(wch, str, 4);
  }

  if ((c & 0xfe) == 0xfc) {
    *wch = c & 0x01;
    return utf8_getmore(wch, str, 5);
  }

  *wch = INVALID_UTF8;
  return FALSE;
}

/*
 * Returns number of (possibly multi-byte) characters in a UTF-8 string
 */
size_t
strlen_utf8(const char *s)
{
    int i = 0, j = 0;
    while (s[i]) {
	if ((s[i] & 0xc0) != 0x80) j++;
	i++;
    }
    return j;
}

size_t
gp_strlen(const char *s)
{
    if (encoding == S_ENC_UTF8)
	return strlen_utf8(s);
    else
	return strlen(s);
}

/*
 * Returns a pointer to the Nth character of s
 * or a pointer to the trailing \0 if N is too large
 */
static char *
utf8_strchrn(const char *s, int N)
{
    int i = 0, j = 0;

    if (N <= 0)
	return (char *)s;
    while (s[i]) {
	if ((s[i] & 0xc0) != 0x80) {
	    if (j++ == N) return (char *)&s[i];
	}
	i++;
    }
    return (char *)&s[i];
}

char *
gp_strchrn(const char *s, int N)
{
    if (encoding == S_ENC_UTF8)
	return utf8_strchrn(s,N);
    else
	return (char *)&s[N];
}


/* TRUE if strings a and b are identical save for leading or trailing whitespace */
TBOOLEAN
streq(const char *a, const char *b)
{
    int enda, endb;

    while (isspace((unsigned char)*a))
	a++;
    while (isspace((unsigned char)*b))
	b++;

    enda = (*a) ? strlen(a) - 1 : 0;
    endb = (*b) ? strlen(b) - 1 : 0;

    while (isspace((unsigned char)a[enda]))
	enda--;
    while (isspace((unsigned char)b[endb]))
	endb--;

    return (enda == endb) ? !strncmp(a,b,++enda) : FALSE;
}


/* append string src to dest
   re-allocates memory if necessary, (re-)determines the length of the 
   destination string only if len==0
 */
size_t
strappend(char **dest, size_t *size, size_t len, const char *src)
{
    size_t destlen = (len != 0) ? len : strlen(*dest);
    size_t srclen = strlen(src);
    if (destlen + srclen + 1 > *size) {
	*size *= 2;
	*dest = (char *) gp_realloc(*dest, *size, "strappend");
    }
    memcpy(*dest + destlen, src, srclen + 1);
    return destlen + srclen;
}


/* convert a struct value to a string */
char *
value_to_str(struct value *val, TBOOLEAN need_quotes)
{
    static int i = 0;
    static char * s[4] = {NULL, NULL, NULL, NULL};
    static size_t c[4] = {0, 0, 0, 0};
    static const int minbufsize = 54;
    int j = i;

    i = (i + 1) % 4;
    if (s[j] == NULL) {
	s[j] = (char *) gp_alloc(minbufsize, "value_to_str");
	c[j] = minbufsize;
    }

    switch (val->type) {
    case INTGR:
	sprintf(s[j], "%d", val->v.int_val);
	break;
    case CMPLX:
	if (isnan(val->v.cmplx_val.real))
	    sprintf(s[j], "NaN");
	else if (val->v.cmplx_val.imag != 0.0)
	    sprintf(s[j], "{%s, %s}",
	            num_to_str(val->v.cmplx_val.real),
	            num_to_str(val->v.cmplx_val.imag));
	else
	    return num_to_str(val->v.cmplx_val.real);
	break;
    case STRING:
	if (val->v.string_val) {
	    if (!need_quotes) {
		return val->v.string_val;
	    } else {
		char * cstr = conv_text(val->v.string_val);
		size_t reqsize = strlen(cstr) + 3;
		if (reqsize > c[j]) {
		    /* Don't leave c[j[ non-zero if realloc fails */
		    s[j] = (char *) gp_realloc(s[j], reqsize + 20, NULL);
		    if (s[j] != NULL) {
			c[j] = reqsize + 20;
		    } else {
			c[j] = 0;
			int_error(NO_CARET, "out of memory");
		    }
		}
		sprintf(s[j], "\"%s\"", cstr);
	    }
	} else {
	    s[j][0] = NUL;
	}
	break;
    case DATABLOCK:
	{
	char **dataline = val->v.data_array;
	int nlines = 0;
	if (dataline != NULL) {
	    while (*dataline++ != NULL)
		nlines++;
	}
	sprintf(s[j], "<%d line data block>", nlines);
	break;
	}
    default:
	int_error(NO_CARET, "unknown type in value_to_str()");
    }

    return s[j];
}


/* Helper for value_to_str(): convert a single number to decimal
 * format. Rotates through 4 buffers 's[j]', and returns pointers to
 * them, to avoid execution ordering problems if this function is
 * called more than once between sequence points. */
char *
num_to_str(double r)
{
    static int i = 0;
    static char s[4][25];
    int j = i++;

    if (i > 3)
	i = 0;

    sprintf(s[j], "%.15g", r);
    if (strchr(s[j], '.') == NULL &&
	strchr(s[j], 'e') == NULL &&
	strchr(s[j], 'E') == NULL)
	strcat(s[j], ".0");

    return s[j];
}

