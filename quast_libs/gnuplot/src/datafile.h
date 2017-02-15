/*
 * $Id: datafile.h,v 1.49.2.2 2016/08/25 04:28:36 sfeam Exp $
 */

/* GNUPLOT - datafile.h */

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

#ifndef GNUPLOT_DATAFILE_H
# define GNUPLOT_DATAFILE_H

/* #if... / #include / #define collection: */

#include "axis.h"
#include "graph3d.h"
#include "graphics.h"

/* returns from DF_READLINE in datafile.c */
/* +ve is number of columns read */
enum DF_STATUS {
    DF_BAD = 0,
    DF_GOOD = 1,
    DF_EOF = -1,
    DF_UNDEFINED = -2,
    DF_FIRST_BLANK = -3,
    DF_SECOND_BLANK = -4,
    DF_MISSING = -5,
    DF_FOUND_KEY_TITLE = -6,
    DF_KEY_TITLE_MISSING = -7,
    DF_STRINGDATA = -8,
    DF_COLUMN_HEADERS = -9
};

/* large file support (offsets potentially > 2GB) */
#if defined(HAVE_FSEEKO) && defined(HAVE_OFF_T)
#  define fseek(stream,pos,whence) fseeko(stream,pos,whence)
#  define ftell(stream) ftello(stream)
#elif defined(_MSC_VER)
#  define fseek(stream,pos,whence) _fseeki64(stream,pos,whence)
#  define ftell(stream) _ftelli64(stream)
#elif defined(__MINGW32__)
#  define fseek(stream,pos,whence) fseeko64(stream,pos,whence)
#  define ftell(stream) ftello64(stream)
#endif

/* Variables of datafile.c needed by other modules: */

/* how many using columns were specified in the current command */
extern int df_no_use_specs;

/* Maximum number of columns returned to caller by df_readline		*/
/* Various data structures are dimensioned to hold this many entries.	*/
/* As of June 2013, plot commands never ask for more than 7 columns of	*/
/* data, but fit commands can use more. "fit" is also limited by	*/
/* the number of parameters that can be passed	to a user function, so	*/
/* let's try setting MAXDATACOLS to match.				*/
/* At present this bumps it from 7 to 14.				*/
#define MAXDATACOLS (MAX_NUM_VAR+2)

/* suggested x value if none given */
extern int df_datum;

/* is this a matrix splot? */
extern TBOOLEAN df_matrix;

/* is this a binary file? */
extern TBOOLEAN df_binary;

extern char *df_filename;
extern int df_line_number;
extern AXIS_INDEX df_axis[];

#ifdef BACKWARDS_COMPATIBLE
extern struct udft_entry ydata_func; /* deprecated "thru" function */
#endif

/* Returned to caller by df_readline() */
extern char *df_tokens[];

/* number of columns in first row of data return to user in STATS_columns */
extern int df_last_col;

/* string representing missing values, ascii datafiles */
extern char *missing_val;

/* input field separators, NULL if whitespace is the separator */
extern char *df_separators;

/* comments chars */
extern char *df_commentschars;

/* flag if any 'inline' data are in use, for the current plot */
extern TBOOLEAN plotted_data_from_stdin;

/* Setting this allows the parser to recognize Fortran D or Q   */
/* format constants in the input file. But it slows things down */
extern TBOOLEAN df_fortran_constants;

/* Setting this disables initialization of the floating point exception */
/* handler before every expression evaluation in a using specifier.   	 */
/* This can speed data input significantly, but assumes valid input.    */
extern TBOOLEAN df_nofpe_trap;
extern TBOOLEAN evaluate_inside_using;
extern TBOOLEAN df_warn_on_missing_columnheader;

/* Used by plot title columnhead, stats name columnhead */
extern char *df_key_title;

/* Prototypes of functions exported by datafile.c */

int df_open __PROTO((const char *, int, struct curve_points *));
int df_readline __PROTO((double [], int));
void df_close __PROTO((void));
char * df_fgets __PROTO((FILE *));
void df_showdata __PROTO((void));
int df_2dbinary __PROTO((struct curve_points *));
int df_3dmatrix __PROTO((struct surface_points *, int));
void df_set_key_title __PROTO((struct curve_points *));
void df_set_key_title_columnhead __PROTO((struct curve_points *));
char * df_parse_string_field __PROTO((char *));
int expect_string __PROTO((const char column ));

void df_reset_after_error __PROTO((void));
void f_dollars __PROTO((union argument *x));
void f_column  __PROTO((union argument *x));
void f_columnhead  __PROTO((union argument *x));
void f_valid   __PROTO((union argument *x));
void f_timecolumn   __PROTO((union argument *x));
void f_stringcolumn   __PROTO((union argument *x));

struct use_spec_s {
    int column;
    int expected_type;
    struct at_type *at;
};


/* Details about the records contained in a binary data file. */

typedef enum df_translation_type {
    DF_TRANSLATE_DEFAULT,     /* Gnuplot will position in first quadrant at origin. */
    DF_TRANSLATE_VIA_ORIGIN,
    DF_TRANSLATE_VIA_CENTER
} df_translation_type;

typedef enum df_sample_scan_type {
    DF_SCAN_POINT = -3,  /* fastest */
    DF_SCAN_LINE  = -4,
    DF_SCAN_PLANE = -5   /* slowest */
} df_sample_scan_type;

/* To generate a swap, take the bit-wise complement of the lowest two bits. */
typedef enum df_endianess_type {
    DF_LITTLE_ENDIAN,
    DF_PDP_ENDIAN,
    DF_DPD_ENDIAN,
    DF_BIG_ENDIAN,
    DF_ENDIAN_TYPE_LENGTH  /* Must be last */
} df_endianess_type;

/* The various types of numerical types that can be read from a data file. */
typedef enum df_data_type {
    DF_CHAR, DF_UCHAR, DF_SHORT, DF_USHORT, DF_INT,
    DF_UINT, DF_LONG,  DF_ULONG, DF_FLOAT,  DF_DOUBLE,
    DF_LONGLONG, DF_ULONGLONG,
    DF_BAD_TYPE
} df_data_type;
#define DF_DEFAULT_TYPE DF_FLOAT

/* Some macros for making the compiler figure out what function
 * the "machine independent" names should execute to read the
 * appropriately sized variable from a data file.
 */ 
#define SIGNED_TEST(val) ((val)==sizeof(long) ? DF_LONG : \
			 ((val)==sizeof(long long) ? DF_LONGLONG : \
			 ((val)==sizeof(int) ? DF_INT : \
			 ((val)==sizeof(short) ? DF_SHORT : \
			 ((val)==sizeof(char) ? DF_CHAR : DF_BAD_TYPE)))))
#define UNSIGNED_TEST(val) ((val)==sizeof(unsigned long) ? DF_ULONG : \
			   ((val)==sizeof(unsigned long long) ? DF_ULONGLONG : \
			   ((val)==sizeof(unsigned int) ? DF_UINT : \
			   ((val)==sizeof(unsigned short) ? DF_USHORT : \
			   ((val)==sizeof(unsigned char) ? DF_UCHAR : DF_BAD_TYPE)))))
#define FLOAT_TEST(val) ((val)==sizeof(float) ? DF_FLOAT : \
			((val)==sizeof(double) ? DF_DOUBLE : DF_BAD_TYPE))

typedef enum df_records_type {
    DF_CURRENT_RECORDS,
    DF_DEFAULT_RECORDS
} df_records_type;

typedef struct df_binary_type_struct {
    df_data_type read_type;
    unsigned short read_size;
} df_binary_type_struct;

typedef struct df_column_bininfo_struct {
    long skip_bytes;
    df_binary_type_struct column;
} df_column_bininfo_struct;

/* NOTE TO THOSE WRITING FILE TYPE FUNCTIONS
 *
 * "cart" means Cartesian, i.e., the (x,y,z) [or (r,t,z)] coordinate
 * system of the plot.  "scan" refers to the scanning method of the
 * file in question, i.e., first points, then lines, then planes.
 * The important variables for a file type function to fill in are
 * those beginning with "scan".  There is a tricky set of rules
 * related to the "scan_cart" mapping, the file-specified variables,
 * the default variables, and the command-line variables.  Basically,
 * command line overrides data file which overrides default.  (Yes,
 * like a confusing version of rock, paper, scissors.) So, from the
 * file type function perspective, it is better to leave those
 * variables which are not specifically known from file data or
 * otherwise (e.g., sample periods "scan_delta") unaltered in case
 * the user has issued "set datafile" to define defaults.
 */
typedef struct df_binary_file_record_struct {
    int cart_dim[3];                  /* dimension array size, x/y/z */
    int cart_dir[3];                  /* 1 scan in positive direction, -1 negative, x/y/z */
    double cart_delta[3];             /* spacing between array points, x/y/z */
    df_translation_type cart_trans;   /* translate via origin, center or default */
    double cart_cen_or_ori[3];        /* vector representing center or origin, x/y/z */
    double cart_alpha;                /* 2D rotation angle (rotate) */
    double cart_p[3];                 /* 3D rotation normal vector (perpendicular) */

    df_sample_scan_type cart_scan[3]; /* how to assign the dimensions read from file when generating coordinates */
    TBOOLEAN scan_generate_coord;     /* whether or not Gnuplot should generate coordinates. */
    off_t scan_skip[3];               /* skip bytes before the record, line, plane */

    /* Not controllable by the user, only by file type functions.
     * These are all points/lines/planes format.
     */
    int scan_dim[3];                  /* number of points, lines, planes */
    int scan_dir[3];                  /* 1 scan in positive direction wrt Cartesian coordinate system, -1 negative */
    double scan_delta[3];             /* sample period along points, lines, planes */
    df_translation_type scan_trans;   /* translate via origin, center or default */
    double scan_cen_or_ori[3];        /* vector representing center or origin, x/y/z */

    /* *** Do not modify outside of datafile.c!!! *** */
    char GPFAR *memory_data;
} df_binary_file_record_struct;

extern df_binary_file_record_struct *df_bin_record;
extern int df_num_bin_records;
extern struct coordinate blank_data_line;

extern struct use_spec_s use_spec[];

/* Prototypes of functions exported by datafile.c */

void df_show_binary __PROTO((FILE *fp));
void df_show_datasizes __PROTO((FILE *fp));
void df_show_filetypes __PROTO((FILE *fp));
void df_set_datafile_binary __PROTO((void)); 
void df_unset_datafile_binary __PROTO((void));
void df_add_binary_records __PROTO((int, df_records_type));
void df_extend_binary_columns __PROTO((int));
void df_set_skip_before __PROTO((int col, int bytes));                /* Number of bytes to skip before a binary column. */
#define df_set_skip_after(col,bytes) df_set_skip_before(col+1,bytes)  /* Number of bytes to skip after a binary column. */
void df_set_read_type __PROTO((int col, df_data_type type));          /* Type of data in the binary column. */
df_data_type df_get_read_type __PROTO((int col));                     /* Type of data in the binary column. */
int df_get_read_size __PROTO((int col));                              /* Size of data in the binary column. */
int df_get_num_matrix_cols __PROTO((void));
void df_set_plot_mode __PROTO((int));

#endif /* GNUPLOT_DATAFILE_H */
