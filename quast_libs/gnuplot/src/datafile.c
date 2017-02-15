#ifndef lint
static char *RCSid() { return RCSid("$Id: datafile.c,v 1.290.2.27 2016/09/03 23:18:57 sfeam Exp $"); }
#endif

/* GNUPLOT - datafile.c */

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

/* AUTHOR : David Denholm */

/*
 * this file provides the functions to handle data-file reading..
 * takes care of all the pipe / stdin / index / using worries
 */

/*{{{  notes */
/* couldn't decide how to implement 'thru' only for 2d and 'index'
 * for only 3d, so I did them for both - I can see a use for
 * index in 2d, especially for fit.
 *
 * I keep thru for backwards compatibility, and extend it to allow
 * more natural plot 'data' thru f(y) - I (personally) prefer
 * my syntax, but then I'm biased...
 *
 * - because I needed it, I have added a range of indexes...
 * (s)plot 'data' [index i[:j]]
 *
 * also every a:b:c:d:e:f  - plot every a'th point from c to e,
 * in every b lines from d to f
 * ie for (line=d; line<=f; line+=b)
 *     for (point=c; point >=e; point+=a)
 *
 *
 * I dont like mixing this with the time series hack... I am
 * very into modular code, so I would prefer to not have to
 * have _anything_ to do with time series... for example,
 * we just look at columns in file, and that is independent
 * of 2d/3d. I really dont want to have to pass a flag to
 * this is plot or splot.
 *
 * Now that df_2dbinary() and df_3dbinary() are here, I am seriously
 * tempted to move get_data() and get_3ddata() in here too
 *
 * public variables declared in this file.
 *    int df_no_use_specs - number of columns specified with 'using'
 *    int df_no_tic_specs - count of additional ticlabel columns
 *    int df_line_number  - for error reporting
 *    int df_datum        - increases with each data point
 *    int df_eof          - end of file
 *
 * functions
 *   int df_open(char *file_name, int max_using, plot_header *plot)
 *      parses thru / index / using on command line
 *      max_using is max no of 'using' columns allowed (obsolete?)
 *	plot_header is NULL if called from fit or set_palette code
 *      returns number of 'using' cols specified, or -1 on error (?)
 *
 *   int df_readline(double vector[], int max)
 *      reads a line, does all the 'index' and 'using' manipulation
 *      deposits values into vector[]
 *      returns
 *          number of columns parsed  [0 = not a blank line, but no valid data],
 *          DF_EOF - end of file
 *          DF_UNDEFINED - undefined result during eval of extended using spec
 *          DF_MISSING - requested column matched that of 'set missing <foo>'
 *          DF_FIRST_BLANK - first consecutive blank line
 *          DF_SECOND_BLANK - second consecutive blank line
 *          DF_FOUND_KEY_TITLE  - only relevant to first line of data
 *          DF_KEY_TITLE_MISSING  and only for 'set key autotitle columnhead'
 *          DF_STRINGDATA - not currently used by anyone
 *          DF_COLUMN_HEADERS - first row used as headers rather than data
 *
 * if a using spec was given, lines not fulfilling spec are ignored.
 * we will always return exactly the number of items specified
 *
 * if no spec given, we return number of consecutive columns we parsed.
 *
 * if we are processing indexes, separated by 'n' blank lines,
 * we will return n-1 blank lines before noticing the index change
 *
 *   void df_close()
 *     closes a currently open file.
 *
 *    void f_dollars(x)
 *    void f_column()    actions for expressions using $i, column(j), etc
 *    void f_valid()
 *
 *
 * line parsing slightly differently from previous versions of gnuplot...
 * given a line containing fewer columns than asked for, gnuplot used to make
 * up values... I say that if I have explicitly said 'using 1:2:3', then if
 * column 3 doesn't exist, I dont want this point...
 *
 * a column number of 0 means generate a value... as before, this value
 * is useful in 2d as an x value, and is reset at blank lines.
 * a column number of -1 means the (data) line number (not the file line
 * number).  splot 'file' using 1  is equivalent to
 * splot 'file' using 0:-1:1
 * column number -2 is the index. It was put in to kludge multi-branch
 * fitting.
 *
 * 20/5/95 : accept 1.23d4 in place of e (but not in scanf string)
 *         : autoextend data line buffer and MAX_COLS
 *
 * 11/8/96 : add 'columns' -1 for suggested y value, and -2 for
 *           current index.
 *           using 1:-1:-2  and  column(-1)  are supported.
 *           $-1 and $-2 are not yet supported, because of the
 *           way the parser works
 *
 */
/*}}} */

/* Daniel Sebald: added general binary 2d data support. (20 August 2004)
 */

#include "datafile.h"
#include "datablock.h"

#include "alloc.h"
#include "axis.h"
#include "command.h"
#include "eval.h"
#include "gp_time.h"
#include "graphics.h"
#include "misc.h"
#include "parse.h"
#include "plot.h"
#include "readline.h"
#include "util.h"
#include "breaders.h"
#include "variable.h" /* For locale handling */

/* test to see if the end of an inline datafile is reached */
#define is_EOF(c) ((c) == 'e' || (c) == 'E')

/* is it a comment line? */
#define is_comment(c) ((c) && (strchr(df_commentschars, (c)) != NULL))

/* Used to skip whitespace but not cross a field boundary */
#define NOTSEP (!df_separators || !strchr(df_separators,*s))

/*{{{  static fns */
static int check_missing __PROTO((char *s));

static void expand_df_column __PROTO((int));
static void clear_df_column_headers __PROTO((void));
static char *df_gets __PROTO((void));
static int df_tokenise __PROTO((char *s));
static float *df_read_matrix __PROTO((int *rows, int *columns));

static void plot_option_every __PROTO((void));
static void plot_option_index __PROTO((void));
static void plot_option_using __PROTO((int));
static TBOOLEAN valid_format __PROTO((const char *));
static void plot_ticlabel_using __PROTO((int));
static void add_key_entry __PROTO((char *temp_string, int df_datum));
static char * df_generate_pseudodata __PROTO((void));
static int df_skip_bytes __PROTO((off_t nbytes));

#ifdef BACKWARDS_COMPATIBLE
static void plot_option_thru __PROTO((void));
#endif
/*}}} */

/*{{{  variables */

enum COLUMN_TYPE { CT_DEFAULT, CT_STRING, CT_KEYLABEL,
		CT_XTICLABEL, CT_X2TICLABEL, CT_YTICLABEL, CT_Y2TICLABEL,
		CT_ZTICLABEL, CT_CBTICLABEL };

/* public variables client might access */

int df_no_use_specs;            /* how many using columns were specified */
int df_line_number;
int df_datum;                   /* suggested x value if none given */
int df_last_col = 0;		/* visible to user via STATS_columns */
AXIS_INDEX df_axis[MAXDATACOLS];
TBOOLEAN df_matrix = FALSE;     /* indicates if data originated from a 2D or 3D format */

void *df_pixeldata;		/* pixel data from an external library (e.g. libgd) */

#ifdef BACKWARDS_COMPATIBLE
/* jev -- the 'thru' function --- NULL means no dummy vars active */
struct udft_entry ydata_func;
#endif

/* string representing missing values in ascii datafiles */
char *missing_val = NULL;

/* input field separators, NULL if whitespace is the separator */
char *df_separators = NULL;

/* comments chars */
char *df_commentschars = 0;

/* If any 'inline data' are in use for the current plot, flag this */
TBOOLEAN plotted_data_from_stdin = FALSE;

/* Setting this allows the parser to recognize Fortran D or Q   */
/* format constants in the input file. But it slows things down */
TBOOLEAN df_fortran_constants = FALSE;

/* Setting this disables re-initialization of the floating point exception */
/* handler before every expression evaluation in a using spec.             */
TBOOLEAN df_nofpe_trap = FALSE;

/* private variables */

/* Bookkeeping for df_fgets() and df_gets().
 * Must be initialized before any callers to either function.
 */
static char *df_line = NULL;
static size_t max_line_len = 0;
#define DATA_LINE_BUFSIZ 160

static FILE *data_fp = NULL;
#if defined(PIPES)
static TBOOLEAN df_pipe_open = FALSE;
#endif
#if defined(HAVE_FDOPEN)
static int data_fd = -2;	/* only used for file redirection */
#endif
static TBOOLEAN mixed_data_fp = FALSE; /* inline data */
char *df_filename = NULL;      /* name of data file */
static int df_eof = 0;

static int df_no_tic_specs;     /* ticlabel columns not counted in df_no_use_specs */

#ifndef MAXINT                  /* should there be one already defined ? */
#  define MAXINT INT_MAX	/* from <limits.h> */
#endif

/* stuff for implementing index */
static int blank_count = 0;     /* how many blank lines recently */
static int df_lower_index = 0;  /* first mesh required */
static int df_upper_index = MAXINT;
static int df_index_step = 1;   /* 'every' for indices */
static int df_current_index;    /* current mesh */

/* stuff for named index support */
static char *indexname = NULL;
static TBOOLEAN index_found = FALSE;
static int df_longest_columnhead = 0;

/* stuff for every point:line */
static TBOOLEAN set_every = FALSE;
static int everypoint = 1;
static int firstpoint = 0;
static int lastpoint = MAXINT;
static int everyline = 1;
static int firstline = 0;
static int lastline = MAXINT;
static int point_count = -1;    /* point counter - preincrement and test 0 */
static int line_count = 0;      /* line counter */

/* for ascii file "skip" lines at head of file */
static int df_skip_at_front = 0;

/* for pseudo-data (1 if filename = '+'; 2 if filename = '++') */
static int df_pseudodata = 0;
static int df_pseudorecord = 0;
static int df_pseudospan = 0;
static double df_pseudovalue_0 = 0;
static double df_pseudovalue_1 = 0;

/* for datablocks */
static TBOOLEAN df_datablock = FALSE;
static char **df_datablock_line = NULL;

/* track dimensions of input matrix/array/image */
static unsigned int df_xpixels;
static unsigned int df_ypixels;
static TBOOLEAN df_transpose;

/* parsing stuff */
struct use_spec_s use_spec[MAXDATACOLS];
static char *df_format = NULL;
static char *df_binary_format = NULL;
TBOOLEAN evaluate_inside_using = FALSE;
TBOOLEAN df_warn_on_missing_columnheader = FALSE;

/* rather than three arrays which all grow dynamically, make one
 * dynamic array of this structure
 */

typedef struct df_column_struct {
    double datum;
    enum DF_STATUS good;
    char *position;	/* points to start of this field in current line */
    char *header;	/* points to copy of the header for this column */
} df_column_struct;

static df_column_struct *df_column = NULL;      /* we'll allocate space as needed */
static int df_max_cols = 0;     /* space allocated */
static int df_no_cols;          /* cols read */
static int fast_columns;        /* corey@cac optimization */

char *df_tokens[MAXDATACOLS];			/* filled in by df_tokenise */
static char *df_stringexpression[MAXDATACOLS];	/* filled in after evaluate_at() */
static struct curve_points *df_current_plot;	/* used to process histogram labels + key entries */

/* These control the handling of fields in the first row of a data file.
 * See also parse_1st_row_as_headers.
 */
#define NO_COLUMN_HEADER (-99)  /* some value that can never be a real column */
static int column_for_key_title = NO_COLUMN_HEADER;
static TBOOLEAN df_already_got_headers = FALSE;
char *df_key_title = NULL;     /* filled in from column header if requested */


/* Binary *read* variables used by df_readbinary().
 * There is a confusing difference between the ascii and binary "matrix" keywords.
 * Ascii matrix data by default is interpreted as having an implicit uniform grid
 * of x and y coords that are not actually present in the data file.
 * The equivalent binary data format is called "binary general".
 * In both of these cases the internal flag df_nonuniform_matrix is FALSE;
 * Binary matrix data contains explicit y values in the first row, and explicit x
 * values in the first column. This is signalled by "binary matrix".
 * In this case the internal flag df_nonuniform_matrix is TRUE.
 *
 * EAM May 2011 - Add a keyword "nonuniform matrix" to indicate ascii matrix data
 * in the same format as "binary matrix", i.e. with explicit x and y coordinates.
 * EAM Jul 2014 - Add keywords "columnheaders" and "rowheaders" to indicate ascii
 * matrix data in the uniform grid format containing labels in row 1 and column 1.
 */
TBOOLEAN df_read_binary;
TBOOLEAN df_nonuniform_matrix;
TBOOLEAN df_matrix_columnheaders, df_matrix_rowheaders;
int df_plot_mode;

static int df_readascii __PROTO((double [], int));
static int df_readbinary __PROTO((double [], int));

static void initialize_use_spec __PROTO((void));

static void initialize_binary_vars __PROTO((void));
static void df_insert_scanned_use_spec __PROTO((int));
static void adjust_binary_use_spec __PROTO((void));
static void clear_binary_records __PROTO((df_records_type));
static void plot_option_binary_format __PROTO((char *));
static void plot_option_binary __PROTO((TBOOLEAN, TBOOLEAN));
static void plot_option_array __PROTO((void));
static TBOOLEAN rotation_matrix_2D __PROTO((double R[][2], double));
static TBOOLEAN rotation_matrix_3D __PROTO((double P[][3], double *));
static int token2tuple __PROTO((double *, int));
static void df_determine_matrix_info __PROTO((FILE *));
static void df_swap_bytes_by_endianess __PROTO((char *, int, int));

typedef enum df_multivalue_type {
    DF_DELTA,
    DF_FLIP_AXIS,
    DF_FLIP,
    DF_SCAN,
    DF_ORIGIN,
    DF_CENTER,
    DF_ROTATION,
    DF_PERPENDICULAR,
    DF_SKIP
} df_multivalue_type;
static void plot_option_multivalued __PROTO((df_multivalue_type,int));

char *df_endian[DF_ENDIAN_TYPE_LENGTH] = {
    "little",
    "pdp (middle)",
    "swapped pdp (dimmle)",
    "big"
};

#define SUPPORT_MIDDLE_ENDIAN 1

#if SUPPORT_MIDDLE_ENDIAN
/* To generate a swap, take the bit-wise complement of the lowest two bits. */
typedef enum df_byte_read_order_type {
    DF_0123,
    DF_1032,
    DF_2301,
    DF_3210
} df_byte_read_order_type;

/* First argument, this program's endianess.  Second argument, file's endianess.
 * Don't use directly.  Use 'byte_read_order()' function instead.*/
static char df_byte_read_order_map[4][4] = {
    {DF_0123, DF_1032, DF_2301, DF_3210},
    {DF_1032, DF_0123, DF_1032, DF_2301},
    {DF_2301, DF_1032, DF_0123, DF_1032},
    {DF_3210, DF_2301, DF_1032, DF_0123}
};

static long long_0x2468 = 0x2468;
#define TEST_BIG_PDP         ( (((char *)&long_0x2468)[0] < 3) ? DF_BIG_ENDIAN : DF_PDP_ENDIAN )
#define THIS_COMPILER_ENDIAN ( (((char *)&long_0x2468)[0] < 5) ? TEST_BIG_PDP : DF_LITTLE_ENDIAN )


/* Argument is file's endianess type. */
static df_byte_read_order_type byte_read_order __PROTO((df_endianess_type));

/* Logical variables indicating information about data file. */
TBOOLEAN df_binary_file;
TBOOLEAN df_matrix_file;

int df_M_count;
int df_N_count;
int df_O_count;

/* Initially set to default and then possibly altered by command line. */
df_binary_file_record_struct *df_bin_record = 0;
/* Default settings. */
df_binary_file_record_struct *df_bin_record_default = 0;
/* Settings that are transferred to default upon reset. */
df_binary_file_record_struct df_bin_record_reset = {
    {-1, 0, 0},
    {1, 1, 1},
    {1, 1, 1},
    DF_TRANSLATE_DEFAULT,
    {0, 0, 0},
    0,
    {0, 0, 1},

    {DF_SCAN_POINT, DF_SCAN_LINE, DF_SCAN_PLANE},
    FALSE,
    {0, 0, 0},

    {0, 0, 0},
    {1, 1, 1},
    {0, 0, 0},
    DF_TRANSLATE_DEFAULT,
    {0, 0, 0},
    NULL           /* data_memory */
};

int df_max_num_bin_records = 0, df_num_bin_records, df_bin_record_count;
int df_max_num_bin_records_default = 0, df_num_bin_records_default;

/* Used to mark the location of a blank line in the original data input file */
struct coordinate blank_data_line = {UNDEFINED, -999, -999, -999, -999, -999, -999, -999};

static void gpbin_filetype_function __PROTO((void));
static void raw_filetype_function __PROTO((void));
static void avs_filetype_function __PROTO((void));

static void (*binary_input_function)(void);	/* Will point to one of the above */
static void auto_filetype_function(void){}	/* Just a placeholder for auto    */

struct gen_ftable df_bin_filetype_table[] = {
    {"avs", avs_filetype_function},
    {"bin", raw_filetype_function},
    {"edf", edf_filetype_function},
    {"ehf", edf_filetype_function},
    {"gif", gif_filetype_function},
    {"gpbin", gpbin_filetype_function},
    {"jpeg", jpeg_filetype_function},
    {"jpg", jpeg_filetype_function},
    {"png", png_filetype_function},
    {"raw", raw_filetype_function},
    {"rgb", raw_filetype_function},
    {"auto", auto_filetype_function},
    {NULL,   NULL}
};
#define RAW_FILETYPE 1

/* Initially set to default and then possibly altered by command line. */
int df_bin_filetype;
df_endianess_type df_bin_file_endianess;
/* Default setting. */
int df_bin_filetype_default;
df_endianess_type df_bin_file_endianess_default;
/* Setting that is transferred to default upon reset. */
int df_bin_filetype_reset = -1;
#define DF_BIN_FILE_ENDIANESS_RESET THIS_COMPILER_ENDIAN

typedef struct df_bin_scan_table_2D_struct {
    char *string;
    df_sample_scan_type scan[3];
} df_bin_scan_table_2D_struct;

df_bin_scan_table_2D_struct df_bin_scan_table_2D[] = {
    {"xy", {DF_SCAN_POINT, DF_SCAN_LINE,  DF_SCAN_PLANE}},
    {"yx", {DF_SCAN_LINE,  DF_SCAN_POINT, DF_SCAN_PLANE}},
    {"tr", {DF_SCAN_POINT, DF_SCAN_LINE,  DF_SCAN_PLANE}},
    {"rt", {DF_SCAN_LINE,  DF_SCAN_POINT, DF_SCAN_PLANE}}
};
#define TRANSPOSE_INDEX 1

typedef struct df_bin_scan_table_3D_struct {
    char *string;
    df_sample_scan_type scan[3];
} df_bin_scan_table_3D_struct;

df_bin_scan_table_3D_struct df_bin_scan_table_3D[] = {
    {"xyz", {DF_SCAN_POINT, DF_SCAN_LINE,  DF_SCAN_PLANE}},
    {"zxy", {DF_SCAN_LINE,  DF_SCAN_PLANE, DF_SCAN_POINT}},
    {"yzx", {DF_SCAN_PLANE, DF_SCAN_POINT, DF_SCAN_LINE}},
    {"yxz", {DF_SCAN_LINE,  DF_SCAN_POINT, DF_SCAN_PLANE}},
    {"xzy", {DF_SCAN_POINT, DF_SCAN_PLANE, DF_SCAN_LINE}},
    {"zyx", {DF_SCAN_PLANE, DF_SCAN_LINE,  DF_SCAN_POINT}},
    {"trz", {DF_SCAN_POINT, DF_SCAN_LINE,  DF_SCAN_PLANE}},
    {"ztr", {DF_SCAN_LINE,  DF_SCAN_PLANE, DF_SCAN_POINT}},
    {"rzt", {DF_SCAN_PLANE, DF_SCAN_POINT, DF_SCAN_LINE}},
    {"rtz", {DF_SCAN_LINE,  DF_SCAN_POINT, DF_SCAN_PLANE}},
    {"tzr", {DF_SCAN_POINT, DF_SCAN_PLANE, DF_SCAN_LINE}},
    {"zrt", {DF_SCAN_PLANE, DF_SCAN_LINE,  DF_SCAN_POINT}}
};

/* Names for machine dependent field sizes. */
char *ch_names[] = {"char","schar","c"};
char *uc_names[] = {"uchar"};
char *sh_names[] = {"short"};
char *us_names[] = {"ushort"};
char *in_names[] = {"int","sint","i","d"};
char *ui_names[] = {"uint","u"};
char *lo_names[] = {"long","ld"};
char *ul_names[] = {"ulong","lu"};
char *fl_names[] = {"float","f"};
char *db_names[] = {"double","lf"};

/* Machine independent names. */
char *byte_names[]   = {"int8","byte"};
char *ubyte_names[]  = {"uint8","ubyte"};
char *word_names[]   = {"int16","word"};
char *uword_names[]  = {"uint16","uword"};
char *word2_names[]  = {"int32"};
char *uword2_names[] = {"uint32"};
char *word4_names[]  = {"int64"};
char *uword4_names[] = {"uint64"};
char *float_names[]  = {"float32"};
char *float2_names[] = {"float64"};

typedef struct df_binary_details_struct {
    char **name;
    unsigned short no_names;
    df_binary_type_struct type;
} df_binary_details_struct;

typedef struct df_binary_tables_struct {
    df_binary_details_struct *group;
    unsigned short group_length;
} df_binary_tables_struct;

df_binary_details_struct df_binary_details[] = {
    {ch_names,sizeof(ch_names)/sizeof(ch_names[0]),{DF_CHAR,sizeof(char)}},
    {uc_names,sizeof(uc_names)/sizeof(uc_names[0]),{DF_UCHAR,sizeof(unsigned char)}},
    {sh_names,sizeof(sh_names)/sizeof(sh_names[0]),{DF_SHORT,sizeof(short)}},
    {us_names,sizeof(us_names)/sizeof(us_names[0]),{DF_USHORT,sizeof(unsigned short)}},
    {in_names,sizeof(in_names)/sizeof(in_names[0]),{DF_INT,sizeof(int)}},
    {ui_names,sizeof(ui_names)/sizeof(ui_names[0]),{DF_UINT,sizeof(unsigned int)}},
    {lo_names,sizeof(lo_names)/sizeof(lo_names[0]),{DF_LONG,sizeof(long)}},
    {ul_names,sizeof(ul_names)/sizeof(ul_names[0]),{DF_ULONG,sizeof(unsigned long)}},
    {fl_names,sizeof(fl_names)/sizeof(fl_names[0]),{DF_FLOAT,sizeof(float)}},
    {db_names,sizeof(db_names)/sizeof(db_names[0]),{DF_DOUBLE,sizeof(double)}},
    {NULL,0,                                       {DF_LONGLONG,sizeof(long long)}},
    {NULL,0,                                       {DF_ULONGLONG,sizeof(unsigned long long)}}
};

df_binary_details_struct df_binary_details_independent[] = {
    {byte_names,sizeof(byte_names)/sizeof(byte_names[0]),{SIGNED_TEST(1),1}},
    {ubyte_names,sizeof(ubyte_names)/sizeof(ubyte_names[0]),{UNSIGNED_TEST(1),1}},
    {word_names,sizeof(word_names)/sizeof(word_names[0]),{SIGNED_TEST(2),2}},
    {uword_names,sizeof(uword_names)/sizeof(uword_names[0]),{UNSIGNED_TEST(2),2}},
    {word2_names,sizeof(word2_names)/sizeof(word2_names[0]),{SIGNED_TEST(4),4}},
    {uword2_names,sizeof(uword2_names)/sizeof(uword2_names[0]),{UNSIGNED_TEST(4),4}},
    {word4_names,sizeof(word4_names)/sizeof(word4_names[0]),{SIGNED_TEST(8),8}},
    {uword4_names,sizeof(uword4_names)/sizeof(uword4_names[0]),{UNSIGNED_TEST(8),8}},
    {float_names,sizeof(float_names)/sizeof(float_names[0]),{FLOAT_TEST(4),4}},
    {float2_names,sizeof(float2_names)/sizeof(float2_names[0]),{FLOAT_TEST(8),8}}
};

int df_no_bin_cols;             /* binary columns to read */

df_binary_tables_struct df_binary_tables[] = {
    {df_binary_details,sizeof(df_binary_details)/sizeof(df_binary_details[0])},
    {df_binary_details_independent,sizeof(df_binary_details_independent)/sizeof(df_binary_details_independent[0])}
};

/* Information about binary data structure, to be determined by the
 * using and format options.  This should be one greater than df_no_bin_cols.
 */
static df_column_bininfo_struct *df_column_bininfo = NULL;      /* allocate space as needed */
static int df_max_bininfo_cols = 0;     /* space allocated */

static const char *matrix_general_binary_conflict_msg
    = "Conflict between some matrix binary and general binary keywords";

#endif

/*}}} */


/*{{{  static char *df_gets() */
static char *
df_gets()
{
    /* Initialization must happen in all paths */
    if (max_line_len < DATA_LINE_BUFSIZ) {
	max_line_len = DATA_LINE_BUFSIZ;
	df_line = gp_alloc(max_line_len, "datafile line buffer");
    }

    /* HBB 20000526: prompt user for inline data, if in interactive mode */
    if (mixed_data_fp && interactive)
	fputs("input data ('e' ends) > ", stderr);

    /* Special pseudofiles '+' and '++' return coords of sample */
    if (df_pseudodata)
	return df_generate_pseudodata();

    if (df_datablock)
	return *(df_datablock_line++);

    return df_fgets(data_fp);
}

/*}}} */

/*{{{  char *df_gets() */
/* 
 * This one is shared by df_gets() and by datablock.c:datablock_command
 */
char *
df_fgets( FILE *fin )
{
    int len = 0;

    /* Initialization must happen in all paths */
    if (max_line_len < DATA_LINE_BUFSIZ) {
	max_line_len = DATA_LINE_BUFSIZ;
	df_line = gp_alloc(max_line_len, "datafile line buffer");
    }

    if (!fgets(df_line, max_line_len, fin))
	return NULL;

    if (mixed_data_fp)
	++inline_num;

    for (;;) {
	len += strlen(df_line + len);

	if (len > 0 && df_line[len - 1] == '\n') {
	    /* we have read an entire text-file line.
	     * Strip the trailing linefeed and return
	     */
	    df_line[len - 1] = 0;
	    return df_line;
	}

	if ((max_line_len - len) < 32)
	    df_line = gp_realloc(df_line, max_line_len *= 2, "datafile line buffer");

	if (!fgets(df_line + len, max_line_len - len, fin))
	    return df_line;        /* unexpected end of file, but we have something to do */
    }

    /* NOTREACHED */
    return NULL;
}

/*}}} */

/*{{{  static int df_tokenise(s) */
static int
df_tokenise(char *s)
{
    /* implement our own sscanf that takes 'missing' into account,
     * and can understand fortran quad format
     */
    TBOOLEAN in_string;
    int i;

    /* "here data" lines may end in \n rather than \0. */
    /* DOS/Windows lines may end in \r rather than \0. */
    if (s[strlen(s)-1] == '\n' || s[strlen(s)-1] == '\r')
	s[strlen(s)-1] = '\0';

    for (i = 0; i<MAXDATACOLS; i++)
	df_tokens[i] = NULL;

    df_no_cols = 0;

    while (*s) {
	/* We may poke at 2 new fields before coming back here - make sure there is room */
	if (df_max_cols <= df_no_cols + 2)
	    expand_df_column((df_max_cols < 20) ? df_max_cols+20 : 2*df_max_cols);

	/* have always skipped spaces at this point */
	df_column[df_no_cols].position = s;
	in_string = FALSE;

	/* Keep pointer to start of this token if user wanted it for
	 * anything, particularly if it is a string */
	for (i = 0; i<MAXDATACOLS; i++) {
	    if (df_no_cols == use_spec[i].column-1) {
		df_tokens[i] = s;
		if (use_spec[i].expected_type == CT_STRING)
		    df_column[df_no_cols].good = DF_GOOD;
	    }
	}

	/* CSV files must accept numbers inside quotes also,
	 * so we step past the quote */
	if (*s == '"' && df_separators != NULL) {
	    in_string = TRUE;
	    df_column[df_no_cols].position = ++s;
	}

	if (*s == '"') {
	    /* treat contents of a quoted string as single column */
	    in_string = !in_string;
	    df_column[df_no_cols].good = DF_STRINGDATA;
	} else if (check_missing(s)) {
	    df_column[df_no_cols].good = DF_MISSING;
	    df_column[df_no_cols].datum = not_a_number();
	    df_column[df_no_cols].position = NULL;
	} else {
	    int used;
	    int count;
	    int dfncp1 = df_no_cols + 1;

	    /* optimizations by Corey Satten, corey@cac.washington.edu */
	    /* only scanf the field if it is mentioned in one of the using specs */
	    if ((fast_columns == 0)
		|| (df_no_use_specs == 0)
		|| ((df_no_use_specs > 0)
		    && (use_spec[0].column == dfncp1
			|| (df_no_use_specs > 1
			    && (use_spec[1].column == dfncp1
				|| (df_no_use_specs > 2
				    && (use_spec[2].column == dfncp1
				|| (df_no_use_specs > 3
				    && (use_spec[3].column == dfncp1
				        || (df_no_use_specs > 4
				            && (use_spec[4].column == dfncp1
				                || df_no_use_specs > 5)
				            )
				        )
				    )
				)
				    )
				)
			    )
			)
		    )
		) {


		/* This was the [slow] code used through version 4.0
		 *   count = sscanf(s, "%lf%n", &df_column[df_no_cols].datum, &used);
		 */

		/* Use strtod() because
		 *  - it is faster than sscanf()
		 *  - sscanf(... %n ...) may not be portable
		 *  - it allows error checking
		 *  - atof() does not return a count or new position
		 */
		 char *next;
		 df_column[df_no_cols].datum = strtod(s, &next);
		 used = next - s;
		 count = (used) ? 1 : 0;

	    } else {
		/* skip any space at start of column */
		while (isspace((unsigned char) *s) && NOTSEP)
		    ++s;
		count = (*s && NOTSEP) ? 1 : 0;
		/* skip chars to end of column */
		used = 0;
		if (df_separators != NULL && in_string) {
		    do
			++s;
		    while (*s && *s != '"');
		    in_string = FALSE;
		}
		while (!isspace((unsigned char) *s)
		       && (*s != NUL) && NOTSEP)
		    ++s;
	    }

	    /* it might be a fortran double or quad precision.
	     * 'used' is only safe if count is 1
	     */
	    if (df_fortran_constants && count == 1 &&
		(s[used] == 'd' || s[used] == 'D' ||
		 s[used] == 'q' || s[used] == 'Q')) {
		/* HBB 20001221: avoid breaking parsing of time/date
		 * strings like 01Dec2000 that would be caused by
		 * overwriting the 'D' with an 'e'... */
		char *endptr;
		char save_char = s[used];

		/* might be fortran double */
		s[used] = 'e';
		/* and try again */
		df_column[df_no_cols].datum = strtod(s, &endptr);
		count = (endptr == s) ? 0 : 1;
		s[used] = save_char;
	    }

	    df_column[df_no_cols].good = count == 1 ? DF_GOOD : DF_BAD;

	    if (isnan(df_column[df_no_cols].datum)) {
		df_column[df_no_cols].good = DF_UNDEFINED;
		FPRINTF((stderr,"NaN in column %d\n", df_no_cols));
	    }
	}

	++df_no_cols;

	/* If we are in a quoted string, skip to end of quote */
	if (in_string) {
	    do
		s++;
	    while (*s && (unsigned char) *s != '"');
	}

	/* skip to 1st character in the next field */
	if (df_separators != NULL) {
	    /* skip to next separator or end of line */
	    while ((*s != '\0') && (*s != '\n') && NOTSEP)
		++s;
	    if ((*s == '\0') || (*s == '\n'))	/* End of line; we're done */
		break;
	    /* step over field separator */
		++s;
	    /* skip whitespace at start of next field */
	    while ((*s == ' ' || *s == '\t') && NOTSEP)
		++s;
	    if ((*s == '\0') || (*s == '\n'))	{ /* Last field is empty */
		df_column[df_no_cols].good = DF_MISSING;
		df_column[df_no_cols].datum = not_a_number();
		++df_no_cols;
		break;
	    }
	} else {
	    /* skip trash chars remaining in this column */
	    while ((*s != '\0') && (*s != '\n') && !isspace((unsigned char) *s))
		++s;
	    /* skip whitespace to start of next column */
	    while (isspace((unsigned char) *s) && *s != '\n')
		++s;
	}

    }

    return df_no_cols;
}

/*}}} */

/*{{{  static float *df_read_matrix() */
/* Reads a matrix from a text file and stores it as floats in allocated
 * memory.
 *
 * IMPORTANT NOTE:  The routine returns the memory pointer for that matrix,
 * but does not retain the pointer.  Maintenance of the memory is left to
 * the calling code.
 */
static float *
df_read_matrix(int *rows, int *cols)
{
    int max_rows = 0;
    int c;
    float *linearized_matrix = NULL;
    int bad_data = 0;
    char *s;
    int index = 0;

    *rows = 0;
    *cols = 0;

    for (;;) {
	if (!(s = df_gets())) {
	    df_eof = 1;
	    /* NULL if we have not read anything yet */
	    return linearized_matrix;   
	}

	/* skip leading spaces */
	while (isspace((unsigned char) *s) && NOTSEP)
	    ++s;

	/* skip blank lines and comments */
	if (!*s || is_comment(*s)) {
	    /* except that some comments hide an index name */
	    if (indexname) {
		while (is_comment(*s) || isspace((unsigned char)*s))
		    ++s;
		if (*s && !strncmp(s, indexname, strlen(indexname)))
		    index_found = TRUE;
	    }
	    if (linearized_matrix)
		return linearized_matrix;
	    else
		continue;
	}

	if (mixed_data_fp && is_EOF(*s)) {
	    df_eof = 1;
	    return linearized_matrix;
	}
	c = df_tokenise(s);

	if (!c)
	    return linearized_matrix;

	/* If the first row of matrix data contains column headers */
	if (!df_already_got_headers && df_matrix_columnheaders && *rows == 0) {
	    int i;
	    char *temp_string;
	    df_already_got_headers = TRUE;

	    for (i = (df_matrix_rowheaders ? 1 :0); i < c; i++) {
		double xpos = df_matrix_rowheaders ? (i-1) : i;
		if (use_spec[0].at) {
		    struct value a;
		    df_column[0].datum = xpos;
		    df_column[0].good = DF_GOOD;
		    evaluate_inside_using = TRUE;
		    evaluate_at(use_spec[0].at, &a);
		    evaluate_inside_using = FALSE;
		    xpos = real(&a);
		}
		temp_string = df_parse_string_field(df_column[i].position);
		add_tic_user(FIRST_X_AXIS, temp_string, xpos, -1);
		free(temp_string);
	    }
	    continue;
	}

	if (*cols && c != *cols) {
	    /* it's not regular */
	    if (linearized_matrix)
		free(linearized_matrix);
	    int_error(NO_CARET, "Matrix does not represent a grid");
	}
	*cols = c;

	++*rows;
	if (*rows > max_rows) {
	    max_rows = GPMAX(2*max_rows,1);
	    linearized_matrix = gp_realloc(linearized_matrix,
				   *cols * max_rows * sizeof(float),
				   "df_matrix");
	}

	/* store data */
	{
	    int i;
	    
	    for (i = 0; i < c; ++i) {

		/* First column in "matrix rowheaders" is a ytic label */
		if (df_matrix_rowheaders && i == 0) {
		    char *temp_string;
		    double ypos = *rows - 1;
		    if (use_spec[1].at) {
			/* The save/restore is to make sure 1:(f($2)):3 works */
			struct value a;
			double save = df_column[1].datum;
			df_column[1].datum = ypos;
			evaluate_inside_using = TRUE;
			evaluate_at(use_spec[1].at, &a);
			evaluate_inside_using = FALSE;
			ypos = real(&a);
			df_column[1].datum = save;
		    }
		    temp_string = df_parse_string_field(df_column[0].position);
		    add_tic_user(FIRST_Y_AXIS, temp_string, ypos, -1);
		    free(temp_string);
		    continue;
		}

		if (i < firstpoint && df_column[i].good != DF_GOOD) {
		    /* It's going to be skipped anyhow, so... */
		    linearized_matrix[index++] = 0;
		} else
		    linearized_matrix[index++] = (float) df_column[i].datum;

		if (df_column[i].good != DF_GOOD) {
		    if (bad_data++ == 0)
			int_warn(NO_CARET,"matrix contains missing or undefined values");
		}
	    }
	}
    }
}
/*}}} */


static void
initialize_use_spec()
{
    int i;
    
    df_no_use_specs = 0;
    for (i = 0; i < MAXDATACOLS; ++i) {
	use_spec[i].column = i + 1; /* default column */
	use_spec[i].expected_type = CT_DEFAULT; /* no particular expectation */
	if (use_spec[i].at) {
	    free_at(use_spec[i].at);
	    use_spec[i].at = NULL;  /* no expression */
	}
	df_axis[i] = NO_AXIS; /* no timefmt for this output column */
    }
}


/*{{{  int df_open(char *file_name, int max_using, plot_header *plot) */

/* open file, parsing using/thru/index stuff return number of using
 * specs [well, we have to return something !]
 */
int
df_open(const char *cmd_filename, int max_using, struct curve_points *plot)
{
    int name_token = c_token - 1;
    TBOOLEAN duplication = FALSE;
    TBOOLEAN set_index = FALSE, set_skip = FALSE;
    TBOOLEAN set_using = FALSE;
    TBOOLEAN set_matrix = FALSE;

    fast_columns = 1;           /* corey@cac */

    /* close file if necessary */
    if (data_fp) {
	df_close();
	data_fp = NULL;
    }

    free(df_format);
    df_format = NULL;         /* no format string */

    df_no_tic_specs = 0;
    free(df_key_title);
    df_key_title = NULL;

    initialize_use_spec();
    clear_df_column_headers();

    df_datum = -1;              /* it will be preincremented before use */
    df_line_number = 0;         /* ditto */

    df_lower_index = 0;
    df_index_step = 1;
    df_upper_index = MAXINT;
    free(indexname);
    indexname = NULL;

    df_current_index = 0;
    blank_count = 2;
    /* by initialising blank_count, leading blanks will be ignored */

    set_every = FALSE;
    everypoint = everyline = 1; /* unless there is an every spec */
    firstpoint = firstline = 0;
    lastpoint = lastline = MAXINT;

    df_binary_file = df_matrix_file = FALSE;
    df_pixeldata = NULL;
    df_num_bin_records = 0;
    df_matrix = FALSE;
    df_nonuniform_matrix = FALSE;
    df_matrix_columnheaders = FALSE;
    df_matrix_rowheaders = FALSE;
    df_skip_at_front = 0;

    df_xpixels = 0;
    df_ypixels = 0;
    df_transpose = FALSE;

    df_eof = 0;

    /* Save for use by df_readline(). */
    /* Perhaps it should be a parameter to df_readline? */
    df_current_plot = plot;

    /* If 'set key autotitle columnhead' is in effect we always treat the
     * first data row as non-data (df_readline() will return DF_COLUMNHEADERS
     * rather than the column count).  This is true even if the key is off
     * or the data is read from 'stats' or from 'fit' rather than plot.
     * FIXME:  This should probably be controlled by an option to 
     *         'set datafile' rather than 'set key'.  Or maybe both?
     */
    column_for_key_title = NO_COLUMN_HEADER;
    df_already_got_headers = FALSE;
    if ((&keyT)->auto_titles == COLUMNHEAD_KEYTITLES)
	parse_1st_row_as_headers = TRUE;
    else
	parse_1st_row_as_headers = FALSE;

    if (!cmd_filename)
	int_error(c_token, "missing filename");
    if (!cmd_filename[0]) {
	if (!df_filename || !*df_filename)
	    int_error(c_token, "No previous filename");
    } else {
	free(df_filename);
	df_filename = gp_strdup(cmd_filename);
    }

    /* defer opening until we have parsed the modifiers... */

#ifdef BACKWARDS_COMPATIBLE
    free_at(ydata_func.at);
    ydata_func.at = NULL;
#endif

    /* pm 25.11.2001 allow any order of options */
    while (!END_OF_COMMAND) {

	/* look for binary / matrix */
	if (almost_equals(c_token, "bin$ary")) {
	    if (df_filename[0] == '$')
		int_error(c_token, "data blocks cannot be binary");
	    c_token++;
	    if (df_binary_file || set_skip) {
		duplication=TRUE;
		break;
	    }
	    df_binary_file = TRUE;
	    /* Up to the time of adding the general binary code, only matrix
	     * binary for 3d was defined.  So, use matrix binary by default.
	     */
	    df_matrix_file = TRUE;
	    initialize_binary_vars();
	    plot_option_binary(set_matrix, FALSE);
	    continue;
	}

	/* deal with matrix */
	if (almost_equals(c_token, "mat$rix")) {
	    c_token++;
	    if (set_matrix) {
		duplication=TRUE;
		break;
	    }
	    /* `binary` default is both df_matrix_file and df_binary_file.
	     * So if df_binary_file is true, but df_matrix_file isn't, then
	     * some keyword specific to general binary has been given.
	     */
	    if (!df_matrix_file && df_binary_file)
		int_error(c_token, matrix_general_binary_conflict_msg);
	    df_matrix_file = TRUE;
	    set_matrix = TRUE;
	    fast_columns = 0;
	    continue;
	}

	/* May 2011 - "nonuniform matrix" indicates an ascii data file
	 * with the same row/column layout as "binary matrix" */
	if (almost_equals(c_token, "nonuni$form")) {
	    c_token++;
	    df_matrix_file = TRUE;
	    df_nonuniform_matrix = TRUE;
	    fast_columns = 0;
	    if (df_matrix_rowheaders || df_matrix_columnheaders)
		duplication = TRUE;
	    continue;
	}

	/* Jul 2014 - "matrix columnheaders" indicates an ascii data file
	 * in uniform grid format but with column labels in row 1 */
	if (almost_equals(c_token, "columnhead$ers")) {
	    c_token++;
	    df_matrix_file = TRUE;
	    df_matrix_columnheaders = TRUE;
	    if (df_nonuniform_matrix || !set_matrix)
		duplication = TRUE;
	    continue;
	}

	/* Jul 2014 - "matrix rowheaders" indicates an ascii data file
	 * in uniform grid format but with row labels in column 1 */
	if (almost_equals(c_token, "rowhead$ers")) {
	    c_token++;
	    df_matrix_file = TRUE;
	    df_matrix_rowheaders = TRUE;
	    if (df_nonuniform_matrix || !set_matrix)
		duplication = TRUE;
	    continue;
	}

	/* deal with index */
	if (almost_equals(c_token, "i$ndex")) {
	    if (set_index) { duplication=TRUE; break; }
	    plot_option_index();
	    set_index = TRUE;
	    continue;
	}

	/* deal with every */
	if (almost_equals(c_token, "ev$ery")) {
	    if (set_every) { duplication=TRUE; break; }
	    plot_option_every();
	    set_every = TRUE;
	    continue;
	}

	/* deal with skip */
	if (equals(c_token, "skip")) {
	    if (set_skip || df_binary_file) { duplication=TRUE; break; }
	    set_skip = TRUE;
	    c_token++;
	    df_skip_at_front = int_expression();
	    if (df_skip_at_front < 0)
		df_skip_at_front = 0;
	    continue;
	}

#ifdef BACKWARDS_COMPATIBLE
	/* deal with thru */
	/* jev -- support for passing data from file thru user function */
	if (almost_equals(c_token, "thru$")) {
	    plot_option_thru();
	    continue;
	}
#endif

	/* deal with using */
	if (almost_equals(c_token, "u$sing")) {
	    if (set_using) { duplication=TRUE; break; }
	    plot_option_using(max_using);
	    set_using = TRUE;
	    continue;
	}

	/* deal with volatile */
	if (almost_equals(c_token, "volatile")) {
	    c_token++;
	    volatile_data = TRUE;
	    continue;
	}

	/* Allow this plot not to affect autoscaling */
	if (almost_equals(c_token, "noauto$scale")) {
	    c_token++;
	    plot->noautoscale = TRUE;
	    continue;
	}

	break; /* unknown option */

    } /* while (!END_OF_COMMAND) */

    if (duplication)
	int_error(c_token,
		  "duplicated or contradicting arguments in datafile options");

    /* Check for auto-generation of key title from column header  */
    /* Mar 2009:  This may no longer be the best place for this!  */
    if ((&keyT)->auto_titles == COLUMNHEAD_KEYTITLES) {
	if (df_no_use_specs == 1)
	    column_for_key_title = use_spec[0].column;
	else if (plot && plot->plot_type == DATA3D)
	    column_for_key_title = use_spec[2].column;
	else
	    column_for_key_title = use_spec[1].column;
    }

    /*{{{  more variable inits */
    point_count = -1;           /* we preincrement */
    line_count = 0;
    df_pseudodata = 0;
    df_pseudorecord = 0;
    df_pseudospan = 0;
    df_datablock = FALSE;
    df_datablock_line = NULL;

    if (plot) {
	/* Save the matrix/array/image dimensions for binary image plot styles	*/
	plot->image_properties.ncols = df_xpixels;
	plot->image_properties.nrows = df_ypixels;
	FPRINTF((stderr,"datafile.c:%d (ncols,nrows) set to (%d,%d)\n", __LINE__,
		df_xpixels, df_ypixels));

	if (set_every && df_xpixels && df_ypixels) {
	    plot->image_properties.ncols = 1 + 
		    ((int)(GPMIN(lastpoint,df_xpixels-1)) - firstpoint) / everypoint;
	    plot->image_properties.nrows = 1 +
		    ((int)(GPMIN(lastline,df_ypixels-1)) - firstline) / everyline;
	    FPRINTF((stderr,"datafile.c:%d  adjusting to (%d, %d)\n", __LINE__,
		    plot->image_properties.ncols, plot->image_properties.nrows));
	}
	if (df_transpose) {
	    unsigned int temp = plot->image_properties.ncols;
	    plot->image_properties.ncols = plot->image_properties.nrows;
	    plot->image_properties.nrows = temp;
	    FPRINTF((stderr,"datafile.c:%d  adjusting to (%d, %d)\n", __LINE__,
		    plot->image_properties.ncols, plot->image_properties.nrows));
	}
    }

    /*}}} */

    /*{{{  open file */
#if defined(HAVE_FDOPEN)
    if (*df_filename == '<' && strlen(df_filename) > 1 && df_filename[1] == '&') {
	char *substr;
	/* read from an already open file descriptor */
	data_fd = strtol(df_filename + 2, &substr, 10);
	if (*substr != '\0' || data_fd < 0 || substr == df_filename+2)
	    int_error(name_token, "invalid file descriptor integer");
	else if (data_fd == fileno(stdin)
	     ||  data_fd == fileno(stdout)
	     ||  data_fd == fileno(stderr))
	    int_error(name_token, "cannot plot from stdin/stdout/stderr");
	else if ((data_fp = fdopen(data_fd, "r")) == (FILE *) NULL)
	    int_error(name_token, "cannot open file descriptor for reading data");

	/* if this stream isn't seekable, set it to volatile */
        if (fseek(data_fp, 0, SEEK_CUR) < 0)
	    volatile_data = TRUE;

    } else
#endif /* HAVE_FDOPEN */
#if defined(PIPES)
    if (*df_filename == '<') {
	restrict_popen();
	if ((data_fp = popen(df_filename + 1, "r")) == (FILE *) NULL)
	    os_error(name_token, "cannot create pipe for data");
	else
	    df_pipe_open = TRUE;
    } else
#endif /* PIPES */

    /* Special filenames '-' '+' '++' '$DATABLOCK' */
    if (*df_filename == '-' && strlen(df_filename) == 1) {
	plotted_data_from_stdin = TRUE;
	volatile_data = TRUE;
	data_fp = lf_top();
	if (!data_fp)
	    data_fp = stdin;
	mixed_data_fp = TRUE;   /* don't close command file */
    } else if (!strcmp(df_filename,"+")) {
	    df_pseudodata = 1;
    } else if (!strcmp(df_filename,"++")) {
	    df_pseudodata = 2;
    } else if (df_filename[0] == '$') {
	df_datablock = TRUE;
	df_datablock_line = get_datablock(df_filename);
    } else {

	/* filename cannot be static array! */
	gp_expand_tilde(&df_filename);
#ifdef HAVE_SYS_STAT_H
	{
	    struct stat statbuf;
	    if ((stat(df_filename, &statbuf) > -1) &&
		S_ISDIR(statbuf.st_mode)) {
		os_error(name_token, "\"%s\" is a directory",
			df_filename);
	    }
	}
#endif /* HAVE_SYS_STAT_H */

	if ((data_fp = loadpath_fopen(df_filename, df_binary_file ? "rb" : "r")) == NULL) {
	    int_warn(NO_CARET, "Cannot find or open file \"%s\"", df_filename);
	    df_eof = 1;
	    return DF_EOF;
	}
    }
/*}}} */


    /* If the data is in binary matrix form, read in some values
     * to determine the nubmer of columns and rows.  If data is in
     * ASCII matrix form, read in all the data to memory in preparation
     * for using df_readbinary() routine.
     */
    if (df_matrix_file) {
	df_determine_matrix_info(data_fp);

	/* Image size bookkeeping for ascii uniform matrices */
	/* NB: If we're inside a 'stats' command there is no plot */
	if (!df_binary_file && plot) {
	    plot->image_properties.ncols = df_xpixels;
	    plot->image_properties.nrows = df_ypixels;
	    FPRINTF((stderr,"datafile.c:%d  ascii uniform matrix dimensions %d x %d \n",
		    __LINE__, df_xpixels, df_ypixels));
	}
    }    

    /* General binary, matrix binary and ASCII matrix all use the
     * df_readbinary() routine.
     */
    if (df_binary_file || df_matrix_file) {
	df_read_binary = TRUE;
	adjust_binary_use_spec();
    } else
	df_read_binary = FALSE;

    /* Make information about whether the data forms a grid or not
     * available to the outside world.  */
    df_matrix = (df_matrix_file
		 || ((df_num_bin_records == 1) 
		     && ((df_bin_record[0].cart_dim[1] > 0)
			 || (df_bin_record[0].scan_dim[1] > 0))));

    return df_no_use_specs;
}

/*}}} */

/*{{{  void df_close() */
void
df_close()
{
    int i;

    /* paranoid - mark $n and column(n) as invalid */
    df_no_cols = 0;

    if (!data_fp && !df_datablock)
	return;

#ifdef BACKWARDS_COMPATIBLE
    free_at(ydata_func.at);
    ydata_func.at = NULL;
#endif

    /* free any use expression storage */
    for (i = 0; i < MAXDATACOLS; ++i)
	if (use_spec[i].at) {
	    free_at(use_spec[i].at);
	    use_spec[i].at = NULL;
	}

    /* free binary matrix data */
    if (df_matrix) {
	for (i = 0; i < df_num_bin_records; i++) {
	    free(df_bin_record[i].memory_data);
	    df_bin_record[i].memory_data = NULL;
	}
    }

    if (!mixed_data_fp && !df_datablock) {
#if defined(HAVE_FDOPEN)
	if (data_fd == fileno(data_fp)) {
	    /* This will allow replotting if this stream is backed by a file,
	     * and hopefully is harmless if it connects to a pipe.
	     * Leave it open in either case.
	     */
	    rewind(data_fp);
	    fprintf(stderr,"Rewinding fd %d\n", data_fd);
	} else
#endif
#if defined(PIPES)
	if (df_pipe_open) {
	    (void) pclose(data_fp);
	    df_pipe_open = FALSE;
	} else
#endif /* PIPES */
	    (void) fclose(data_fp);
    }
    mixed_data_fp = FALSE;
    data_fp = NULL;
}

/*}}} */

/*{{{  void df_showdata() */
/* display the current data file line for an error message
 */
void
df_showdata()
{
  if (data_fp && df_filename && df_line) {
    /* display no more than 77 characters */
    fprintf(stderr, "%.77s%s\n%s:%d:", df_line,
	    (strlen(df_line) > 77) ? "..." : "",
	    df_filename, df_line_number);
  }
}

/*}}} */


static void
plot_option_every()
{
    fast_columns = 0;           /* corey@cac */
    /* allow empty fields - every a:b:c::e we have already established
     * the defaults */

    if (!equals(++c_token, ":")) {
	everypoint = int_expression();
	if (everypoint < 0) everypoint = 1;
	else if (everypoint < 1)
	    int_error(c_token, "Expected positive integer");
    }
    /* if it fails on first test, no more tests will succeed. If it
     * fails on second test, next test will succeed with correct
     * c_token */
    if (equals(c_token, ":") && !equals(++c_token, ":")) {
	everyline = int_expression();
	if (everyline < 0) everyline = 1;
	else if (everyline < 1)
	    int_error(c_token, "Expected positive integer");
    }
    if (equals(c_token, ":") && !equals(++c_token, ":")) {
	firstpoint = int_expression();
	if (firstpoint < 0) firstpoint = 0;
    }
    if (equals(c_token, ":") && !equals(++c_token, ":")) {
	firstline = int_expression();
	if (firstline < 0) firstline = 0;
    }
    if (equals(c_token, ":") && !equals(++c_token, ":")) {
	lastpoint = int_expression();
	if (lastpoint < 0) lastpoint = MAXINT;
	else if (lastpoint < firstpoint)
	    int_error(c_token, "Last point must not be before first point");
    }
    if (equals(c_token, ":")) {
	++c_token;
	lastline = int_expression();
	if (lastline < 0) lastline = MAXINT;
	else if (lastline < firstline)
	    int_error(c_token, "Last line must not be before first line");
    }
}


static void
plot_option_index()
{
    if (df_binary_file && df_matrix_file)
	int_error(c_token, "Binary matrix file format does not allow more than one surface per file");

    ++c_token;
    /* Check for named index */
    if ((indexname = try_to_get_string())) {
	index_found = FALSE;
	return;
    }

    /* Numerical index list */
    df_lower_index = int_expression();
    if (df_lower_index < 0)
	int_error(c_token,"index must be non-negative");
    if (equals(c_token, ":")) {
	++c_token;
	if (equals(c_token, ":")) {
	    df_upper_index = MAXINT;    /* If end index not specified */
	} else {
	    df_upper_index = int_expression();
	    if (df_upper_index < df_lower_index)
		int_error(c_token, "Upper index should be bigger than lower index");
	}
	if (equals(c_token, ":")) {
	    ++c_token;
	    df_index_step = abs(int_expression());
	    if (df_index_step < 1)
		int_error(c_token, "Index step must be positive");
	}
    } else
	df_upper_index = df_lower_index;
}

#ifdef BACKWARDS_COMPATIBLE
static void
plot_option_thru()
{
    c_token++;
    strcpy(c_dummy_var[0], set_dummy_var[0]);
    /* allow y also as a dummy variable.
     * during plot, c_dummy_var[0] and [1] are 'sacred'
     * ie may be set by  splot [u=1:2] [v=1:2], and these
     * names are stored only in c_dummy_var[]
     * so choose dummy var 2 - can anything vital be here ?
     */
    dummy_func = &ydata_func;
    strcpy(c_dummy_var[2], "y");
    ydata_func.at = perm_at();
    dummy_func = NULL;
}
#endif


static void
plot_option_using(int max_using)
{
    int no_cols = 0;  /* For general binary only. */
    char *column_label;

    /* The filetype function may have set the using specs, so reset
     * them before processing tokens. */
    if (df_binary_file)
	initialize_use_spec();

    /* Try to distinguish between 'using "A":"B"' and 'using "%lf %lf" */
    if (!END_OF_COMMAND && isstring(++c_token)) {
	int save_token = c_token;
	df_format = try_to_get_string();
	if (valid_format(df_format))
	    return;
	free(df_format);
	df_format = NULL;
	c_token = save_token;
    }

    if (!END_OF_COMMAND) {
	do {                    /* must be at least one */
	    if (df_no_use_specs >= MAXDATACOLS)
		int_error(c_token, "at most %d columns allowed in using spec", MAXDATACOLS);

	    if (df_no_use_specs >= max_using)
		int_error(c_token, "Too many columns in using specification");

	    if (equals(c_token, ":")) {
		/* empty specification - use default */
		use_spec[df_no_use_specs].column = df_no_use_specs;
		if (df_no_use_specs > no_cols)
		    no_cols = df_no_use_specs;
		++df_no_use_specs;
		/* do not increment c+token ; let while() find the : */

	    } else if (equals(c_token, "(")) {
		fast_columns = 0;       /* corey@cac */
		dummy_func = NULL;      /* no dummy variables active */
		/* this will match ()'s: */
		at_highest_column_used = NO_COLUMN_HEADER;
		use_spec[df_no_use_specs].at = perm_at();
		if (no_cols < at_highest_column_used)
		    no_cols = at_highest_column_used;
		/* Catch at least the simplest case of 'autotitle columnhead' using an expression */
		use_spec[df_no_use_specs++].column = at_highest_column_used;

	    /* It would be nice to handle these like any other      */
	    /* internal function via perm_at() but it doesn't work. */
	    } else if (almost_equals(c_token, "xtic$labels")) {
		plot_ticlabel_using(CT_XTICLABEL);
	    } else if (almost_equals(c_token, "x2tic$labels")) {
		plot_ticlabel_using(CT_X2TICLABEL);
	    } else if (almost_equals(c_token, "ytic$labels")) {
		plot_ticlabel_using(CT_YTICLABEL);
	    } else if (almost_equals(c_token, "y2tic$labels")) {
		plot_ticlabel_using(CT_Y2TICLABEL);
	    } else if (almost_equals(c_token, "ztic$labels")) {
		plot_ticlabel_using(CT_ZTICLABEL);
	    } else if (almost_equals(c_token, "cbtic$labels")) {
		plot_ticlabel_using(CT_CBTICLABEL);
	    } else if (almost_equals(c_token, "key")) {
		plot_ticlabel_using(CT_KEYLABEL);

	    } else if ((column_label = try_to_get_string())) {
		/* ...using "A"... Dummy up a call to column(column_label) */
		use_spec[df_no_use_specs].at = create_call_column_at(column_label);
		use_spec[df_no_use_specs++].column = NO_COLUMN_HEADER;
		parse_1st_row_as_headers = TRUE;
		fast_columns = 0;
		/* FIXME - is it safe to always take the title from the 2nd use spec? */
		if (df_no_use_specs == 2) {
		    free(df_key_title);
		    df_key_title = gp_strdup(column_label);
		}

	    } else {
		int col = int_expression();

		if (col == -3)	/* pseudocolumn -3 means "last column" */	
		    fast_columns = 0;
		else if (col < -2)
		    int_error(c_token, "Column must be >= -2");

		use_spec[df_no_use_specs++].column = col;

		/* Supposedly only happens for binary files, but don't bet on it */
		if (col > no_cols)
		    no_cols = col;
	    }
	} while (equals(c_token, ":") && ++c_token);
    }

    if (df_binary_file) {
	/* If the highest user column number is greater than number of binary
	 * columns, set the unitialized columns binary info to that of the last
	 * specified column or the default.
	 */
	df_extend_binary_columns(no_cols);
    }

    /* Allow a format specifier after the enumeration of columns. */
    /* Note: This was left out by mistake in versions 4.6.0 + 4.6.1 */
    if (!END_OF_COMMAND && isstring(c_token)) {
	df_format = try_to_get_string();
	if (!valid_format(df_format))
	    int_error(c_token, "format must have 1-7 conversions of type double (%%lf)");
    }

}


static void
plot_ticlabel_using(int axis)
{
    int col = 0;
    
    c_token ++;
    if (!equals(c_token,"("))
	int_error(c_token, "missing '('");
    c_token++;

    /* FIXME: What we really want is a test for a constant expression as  */
    /* opposed to a dummy expression. This is similar to the problem with */
    /* with parsing the first argument of the plot command itself.        */
    if (isanumber(c_token) || type_udv(c_token)==INTGR) {
	col = int_expression();
	use_spec[df_no_use_specs+df_no_tic_specs].at = NULL;
    } else {
	use_spec[df_no_use_specs+df_no_tic_specs].at = perm_at();
	fast_columns = 0;	/* Force all columns to be evaluated */
	col = 1;		/* Redundant because of the above */
    }

    if (col < 1)
	int_error(c_token, "ticlabels must come from a real column");
    if (!equals(c_token,")"))
	int_error(c_token, "missing ')'");
    c_token++;
    use_spec[df_no_use_specs+df_no_tic_specs].expected_type = axis;
    use_spec[df_no_use_specs+df_no_tic_specs].column = col;
    df_no_tic_specs++;
}


/*{{{  int df_readline(v, max) */
int
df_readline(double v[], int max)
{
    if (!data_fp && !df_pseudodata && !df_datablock)
	return DF_EOF;

    if (df_read_binary)
	/* General binary, matrix binary or matrix ascii
	 * that's been converted to binary.
	 */
	return df_readbinary(v, max);
    else
	return df_readascii(v, max);
}
/*}}} */


/* do the hard work... read lines from file,
 * - use blanks to get index number
 * - ignore lines outside range of indices required
 * - fill v[] based on using spec if given
 */

int
df_readascii(double v[], int max)
{
    char *s;
    /* Version 5:
     * We used to return DF_MISSING or DF_UNDEFINED immediately if any column
     * could not be parsed.  Now we note this failure in return_value but
     * continue to process any additional requested columns before returning.
     * This is a CHANGE.
     */
    int return_value = DF_GOOD;

    assert(max <= MAXDATACOLS);

    /* catch attempt to read past EOF on mixed-input */
    if (df_eof)
	return DF_EOF;

	/*{{{  process line */
    while ((s = df_gets()) != NULL) {
	int line_okay = 1;
	int output = 0;         /* how many numbers written to v[] */
	return_value = DF_GOOD;

	/* "skip" option */
	if (df_skip_at_front > 0) {
	    df_skip_at_front--;
	    continue;
	}

	++df_line_number;
	df_no_cols = 0;

	/*{{{  check for blank lines, and reject by index/every */
	/*{{{  skip leading spaces */
	while (isspace((unsigned char) *s) && NOTSEP)
	    ++s;                /* will skip the \n too, to point at \0  */
	/*}}} */

	/*{{{  skip comments */
	if (is_comment(*s)) {
	    if (indexname) { /* Look for index name in comment */
		while (is_comment(*s) || isspace((unsigned char)*s))
		    ++s;
		if (*s && !strncmp(s, indexname, strlen(indexname)))
		    index_found = TRUE;
	    }
	    continue;           /* ignore comments */
	}
	/*}}} */

	/*{{{  check EOF on mixed data */
	if (mixed_data_fp && is_EOF(*s)) {
	    df_eof = 1;         /* trap attempts to read past EOF */
	    return DF_EOF;
	}
	/*}}} */

	/*{{{  its a blank line - update counters and continue or return */
	if (*s == 0) {
	    /* argh - this is complicated !  we need to
	     *   ignore it if we haven't reached first index
	     *   report EOF if passed last index
	     *   report blank line unless we've already done 2 blank lines
	     *
	     * - I have probably missed some obvious way of doing all this,
	     * but its getting late
	     */

	    point_count = -1;   /* restart counter within line */

	    if (++blank_count == 1) {
		/* first blank line */
		++line_count;
	    }
	    /* just reached end of a group/surface */
	    if (blank_count == 2) {
		++df_current_index;
		line_count = 0;
		df_datum = -1;

		/* Found two blank lines after a block of data with a named index */
		if (indexname && index_found) {
		    df_eof = 1;
		    return DF_EOF;
		}

		/* ignore line if current_index has just become
		 * first required one - client doesn't want this
		 * blank line. While we're here, check for <=
		 * - we need to do it outside this conditional, but
		 * probably no extra cost at assembler level
		 */
		if (df_current_index <= df_lower_index)
		    continue;   /* dont tell client */

		/* df_upper_index is MAXINT-1 if we are not doing index */
		if (df_current_index > df_upper_index) {
		    /* oops - need to gobble rest of input if mixed */
		    if (mixed_data_fp)
			continue;
		    else {
			df_eof = 1;
			return DF_EOF;  /* no point continuing */
		    }
		}
	    }
	    /* dont tell client if we haven't reached first index */
	    if (indexname && !index_found)
		continue;
	    if (df_current_index < df_lower_index)
		continue;

	    /* ignore blank lines after blank_index */
	    if (blank_count > 2)
		continue;

	    return DF_FIRST_BLANK - (blank_count - 1);
	}
	/*}}} */

	/* get here => was not blank */

	blank_count = 0;

	/*{{{  ignore points outside range of index */
	/* we try to return end-of-file as soon as we pass upper index,
	 * but for mixed input stream, we must skip garbage
	 */

	if (indexname && !index_found)
	    continue;
	if (df_current_index < df_lower_index ||
	    df_current_index > df_upper_index ||
	    ((df_current_index - df_lower_index) % df_index_step) != 0)
	    continue;
	/*}}} */

	/* Bookkeeping for the plot ... every N:M:etc option */
	if ((parse_1st_row_as_headers || column_for_key_title > 0)
	&&  !df_already_got_headers) {
		FPRINTF((stderr,"skipping 'every' test in order to read column headers\n"));
	} else {
		/* Accept only lines with (line_count%everyline) == 0 */
		if (line_count < firstline || line_count > lastline ||
		    (line_count - firstline) % everyline != 0)
		    continue;
		/* update point_count. ignore point if point_count%everypoint != 0 */
		if (++point_count < firstpoint || point_count > lastpoint ||
		    (point_count - firstpoint) % everypoint != 0)
		    continue;
	}
	/*}}} */

	++df_datum;

	if (df_format) {
	    /*{{{  do a sscanf */
	    int i;

	    /* check we have room for at least 7 columns */
	    if (df_max_cols < 7)
		expand_df_column(7);

	    df_no_cols = sscanf(s, df_format,
				&df_column[0].datum,
				&df_column[1].datum,
				&df_column[2].datum,
				&df_column[3].datum,
				&df_column[4].datum,
				&df_column[5].datum,
				&df_column[6].datum);

	    if (df_no_cols == EOF) {
		df_eof = 1;
		return DF_EOF;  /* tell client */
	    }
	    for (i = 0; i < df_no_cols; ++i) {  /* may be zero */
		df_column[i].good = DF_GOOD;
		df_column[i].position = NULL;   /* cant get a time */
	    }
	    /*}}} */
	} else
	    df_tokenise(s);

	/* df_tokenise already processed everything, but in the case of pseudodata
	 * '+' or '++' the value itself was passed as an ascii string formatted by
	 * "%g".  We can do better than this by substituting in the binary value.
	 */
	if (df_pseudodata > 0)
	    df_column[0].datum = df_pseudovalue_0;
	if (df_pseudodata > 1)
	    df_column[1].datum = df_pseudovalue_1;

	/* Always save the contents of the first row in case it is needed for
	 * later access via column("header").  However, unless we know for certain that
	 * it contains headers only, e.g. via parse_1st_row_as_headers or 
	 * (column_for_key_title > 0), also treat it as a data row.
	 */
	if (df_datum == 0 && !df_already_got_headers) {
	    int j;
	    for (j=0; j<df_no_cols; j++) {
		free(df_column[j].header);
		df_column[j].header = df_parse_string_field(df_column[j].position);
		if (df_column[j].header) {
		    if (df_longest_columnhead < strlen(df_column[j].header))
			df_longest_columnhead = strlen(df_column[j].header);
		    FPRINTF((stderr,"Col %d: \"%s\"\n",j,df_column[j].header));
		}
	    }
	    df_already_got_headers = TRUE;

	    /* Restrict the column number to possible values */
	    if (column_for_key_title > df_no_cols)
		column_for_key_title = df_no_cols;
	    if (column_for_key_title == -3)	/* last column in file */ 
		column_for_key_title = df_no_cols;

	    if (column_for_key_title > 0) {
		df_key_title = gp_strdup(df_column[column_for_key_title-1].header);
		if (!df_key_title) {
		    FPRINTF((stderr,
			 "df_readline: missing column head for key title\n"));
		    return(DF_KEY_TITLE_MISSING);
		}
		df_datum--;
		column_for_key_title = NO_COLUMN_HEADER;
		parse_1st_row_as_headers = FALSE;
		return DF_FOUND_KEY_TITLE;
	    } else if (parse_1st_row_as_headers) {
		df_datum--;
		parse_1st_row_as_headers = FALSE;
		return DF_COLUMN_HEADERS;
	    }
	}

	/* Used by stats to set STATS_columns */
	if (df_datum == 0)
		df_last_col = df_no_cols;

	/*{{{  copy column[] to v[] via use[] */
	{
	    int limit = (df_no_use_specs
			 ? df_no_use_specs + df_no_tic_specs
			 : MAXDATACOLS);
	    
	    if (limit > max + df_no_tic_specs)
		limit = max + df_no_tic_specs;

	    for (output = 0; output < limit; ++output) {
		/* if there was no using spec, column is output+1 and at=NULL */
		int column = use_spec[output].column;

		if (column == -3) /* pseudocolumn -3 means "last column" */
		    column = use_spec[output].column = df_no_cols;

		/* Handle cases where column holds a meta-data string */
		/* Axis labels, plot titles, etc.                     */
		if (use_spec[output].expected_type >= CT_XTICLABEL) {
		    int axis, axcol;
		    double xpos;
		   
		    /* EAM FIXME - skip columnstacked histograms also */
		    if (df_current_plot) {
			if (df_current_plot->plot_style == BOXPLOT)
				continue;
		    }
		    switch (use_spec[output].expected_type) {
			default:
			case CT_XTICLABEL:
			    axis = FIRST_X_AXIS;
			    axcol = 0;
			    break;
			case CT_X2TICLABEL:
			    axis = SECOND_X_AXIS;
			    axcol = 0;
			    break;
			case CT_YTICLABEL:
			    axis = FIRST_Y_AXIS;
			    axcol = 1;
			    break;
			case CT_Y2TICLABEL:
			    axis = SECOND_Y_AXIS;
			    axcol = 1;
			    break;
			case CT_ZTICLABEL:
			    axis = FIRST_Z_AXIS;
			    axcol = 2;
			    break;
			case CT_CBTICLABEL:
			    /* EAM FIXME - Which column to set for cbtic? */
			    axis = COLOR_AXIS;
			    axcol = 3;
			    break;
		    }
		    /* FIXME EAM - Trap special case of only a single
		     * 'using' column. But really we need to handle
		     * general case of implicit column 0 */
		    if (output == 1)
			xpos = (axcol == 0) ? df_datum : v[axcol-1];
		    else
			xpos = v[axcol];

		    if (df_current_plot
			&& df_current_plot->plot_style == HISTOGRAMS) {
			if (output > 1) /* Can only happen for HT_ERRORBARS */
			    xpos = (axcol == 0) ? df_datum : v[axcol-1];
			xpos += df_current_plot->histogram->start;
		    }

		    /* Tic label is generated by a string-valued function */
		    if (use_spec[output].at) {
			struct value a;
			evaluate_inside_using = TRUE;
			evaluate_at(use_spec[output].at, &a);
			evaluate_inside_using = FALSE;
			if (a.type == STRING) {
			    add_tic_user(axis, a.v.string_val, xpos, -1);
			    gpfree_string(&a);
			} else {
			    /* Version 5: In this case do not generate a tic at all. */
			    /* E.g. plot $FOO using 1:2:(filter(3) ? strcol(3) : NaN) */
			    /*
			    add_tic_user(axis, "", xpos, -1);
			    int_warn(NO_CARET,"Tic label does not evaluate as string!\n");
			     */
			}
		    } else {
			char *temp_string = df_parse_string_field(df_tokens[output]);
			add_tic_user(axis, temp_string, xpos, -1);
			free(temp_string);
		    }

		} else if (use_spec[output].expected_type == CT_KEYLABEL) {
		    char *temp_string = df_parse_string_field(df_tokens[output]);
		    if (df_current_plot)
			add_key_entry(temp_string,df_datum);
		    free(temp_string);

		} else

		if (use_spec[output].at) {
		    struct value a;
		    TBOOLEAN timefield = FALSE;
		    /* no dummy values to set up prior to... */
		    evaluate_inside_using = TRUE;
		    evaluate_at(use_spec[output].at, &a);
		    evaluate_inside_using = FALSE;
		    if (undefined) {
			return_value = DF_UNDEFINED;
			v[output] = not_a_number();
			continue;
		    }

		    if ((df_axis[output] != NO_AXIS)
			   && axis_array[df_axis[output]].datatype == DT_TIMEDATE)
			timefield = TRUE;

		    if (timefield && (a.type != STRING)
		    && !strcmp(timefmt,"%s")) {
			/* Handle the case of timefmt "%s" which expects a string */
			/* containing a number. If evaluate_at() above returned a */
			/* bare number then we must convert it to a sting before  */
			/* falling through to the usual processing case.          */
			/* NB: We only accept time values of +/- 10^12 seconds.   */
			char *timestring = gp_alloc(20,"timestring");
			sprintf(timestring,"%16.3f",real(&a));
			a.type = STRING;
			a.v.string_val = timestring;
		    }

		    if (a.type == STRING) {
			v[output] = not_a_number();	/* found a string, not a number */

			/* This string value will get parsed as if it were a data column */
			/* so put it in quotes to allow embedded whitespace.             */
			if (use_spec[output].expected_type == CT_STRING) {
			    char *s = gp_alloc(strlen(a.v.string_val)+3,"quote");
			    *s = '"';
			    strcpy(s+1, a.v.string_val);
			    strcat(s, "\"");
			    free(df_stringexpression[output]);
			    df_tokens[output] = df_stringexpression[output] = s;
			}

			/* Check for timefmt string generated by a function */
			if (timefield) {
			    struct tm tm;
			    double usec = 0.0;
			    if (gstrptime(a.v.string_val, timefmt, &tm, &usec))
				v[output] = (double) gtimegm(&tm) + usec;
			    else
				return_value = DF_BAD;
			}
			gpfree_string(&a);
		    }

		    else {
			v[output] = real(&a);
			if (isnan(v[output]))
			    return_value = DF_UNDEFINED;
		    }

		} else if (column == -2) {
		    v[output] = df_current_index;
		} else if (column == -1) {
		    v[output] = line_count;
		} else if (column == 0) {
		    v[output] = df_datum;       /* using 0 */
		} else if (column <= 0) /* really < -2, but */
		    int_error(NO_CARET, "internal error: column <= 0 in datafile.c");
		else if ((df_axis[output] != NO_AXIS)
			 && (axis_array[df_axis[output]].datatype == DT_TIMEDATE)) {
		    struct tm tm;
		    double usec = 0.0;
		    if (column > df_no_cols ||
			df_column[column - 1].good == DF_MISSING ||
			!df_column[column - 1].position ||
			!gstrptime(df_column[column - 1].position, timefmt, &tm, &usec)
			) {
			/* line bad only if user explicitly asked for this column */
			if (df_no_use_specs)
			    line_okay = 0;

			/* return or ignore line depending on line_okay */
			break;
		    }
		    v[output] = (double) gtimegm(&tm) + usec;

		} else if (use_spec[output].expected_type == CT_STRING) {
		    /* Do nothing. */
		    /* String tokens were loaded into df_tokens already. */

		} else {
		    /* column > 0 */
		    if ((column <= df_no_cols)
			&& df_column[column - 1].good == DF_GOOD) {
			v[output] = df_column[column - 1].datum;

		    /* Version 5:
		     * Do not return immediately on DF_MISSING or DF_UNDEFINED.
		     * THIS IS A CHANGE.
		     */
		    } else if ((column <= df_no_cols)
			     && (df_column[column - 1].good == DF_MISSING)) {
			v[output] = not_a_number();
			return_value = DF_MISSING;
		    } else if ((column <= df_no_cols)
			     && (df_column[column - 1].good == DF_UNDEFINED)) {
			v[output] = df_column[column - 1].datum;
			return_value = DF_UNDEFINED;
		    } else {
			/* line bad only if user explicitly asked for this column */
			if (df_no_use_specs)
			    line_okay = 0;
			break;      /* return or ignore depending on line_okay */
		    }
		}
		/* Special case to make 'using 0' et al. to work with labels */
		if (use_spec[output].expected_type == CT_STRING
		    && (!(use_spec[output].at) || !df_tokens[output])
		    && (column == -2 || column == -1 || column == 0)) {
		    char *s = gp_alloc(32*sizeof(char), 
			"temp string for label hack");
		    sprintf(s, "%d", (int)v[output]);
		    free(df_stringexpression[output]);
		    df_tokens[output] = df_stringexpression[output] = s;
		}
	    }
	}
	/*}}} */

	if (!line_okay) /* Ignore this line (pretend we never read it) */
	    continue;

	/* output == df_no_use_specs if using was specified -
	 * actually, smaller of df_no_use_specs and max */
	/* FIXME EAM - In theory it might be useful for the caller to
	 * know whether or not tic specs were read from this line, but
	 * all callers would have to be modified to deal with it one
	 * way or the other. */
	output -= df_no_tic_specs;
	assert(df_no_use_specs == 0
	       || output == df_no_use_specs
	       || output == max);

	/*
	 * EAM Apr 2012 - If there is no using spec, then whatever we found on
	 * the first line becomes the expectation for the rest of the input file.
	 * THIS IS A CHANGE!
	 */
	 if (df_no_use_specs == 0)
		df_no_use_specs = output;

	/* Version 5:
	 * If all requested values were OK, return number of columns read.
	 * If a requested column was bad, return an error but nevertheless
	 * return the other requested columns. The number of columns is
	 * available to the caller in df_no_use_specs.
	 * THIS IS A CHANGE!
	 */
	switch (return_value) {
	case DF_MISSING:
	case DF_UNDEFINED:
	case DF_BAD:
			return return_value;
			break;
	default:
			return output;
			break;
	}

    }
    /*}}} */

    /* get here => fgets failed */

    /* no longer needed - mark column(x) as invalid */
    df_no_cols = 0;

    df_eof = 1;
    return DF_EOF;
}
/*}}} */


char *read_error_msg = "Data file read error";
double df_matrix_corner[2][2]; /* First argument is corner, second argument is x (0) or y(1). */

float
df_read_a_float(FILE *fin) {
    float fdummy;
    if (fread(&fdummy, sizeof(fdummy), 1, fin) != 1) {
	if (feof(fin))
	    int_error(NO_CARET, "Data file is empty");
	else
	    int_error(NO_CARET, read_error_msg);
    }
    df_swap_bytes_by_endianess((char *)&fdummy, byte_read_order(df_bin_file_endianess), sizeof(fdummy));
    return fdummy;
}

void
df_determine_matrix_info(FILE *fin)
{

    if (df_binary_file) {

	/* Binary matrix format. */
	float fdummy;
	off_t nc, nr;	/* off_t because they contribute to fseek offset */
	off_t flength;

	/* Read first value for number of columns. */
	fdummy = df_read_a_float(fin);
	nc = ((size_t) fdummy);
	if (nc == 0)
	    int_error(NO_CARET, "Read grid of zero width");
	else if (nc > 1e8)
	    int_error(NO_CARET, "Read grid width too large");

	/* Read second value for corner_0 x. */
	fdummy = df_read_a_float(fin);
	df_matrix_corner[0][0] = fdummy;

	/* Read nc+1 value for corner_1 x. */
	if (nc > 1) {
	    fseek(fin, (nc-2)*sizeof(float), SEEK_CUR);
	    fdummy = df_read_a_float(fin);
	}
	df_matrix_corner[1][0] = fdummy;

	/* Read nc+2 value for corner_0 y. */
	df_matrix_corner[0][1] = df_read_a_float(fin);

	/* Compute length of file and number of columns. */
	fseek(fin, 0L, SEEK_END);
	flength = ftell(fin)/sizeof(float);
	nr = flength/(nc + 1);
	if (nr*(nc + 1) != flength)
	    int_error(NO_CARET, "File doesn't factorize into full matrix");

	/* Read last value for corner_1 y */
	fseek(fin, -(nc + 1)*sizeof(float), SEEK_END);
	df_matrix_corner[1][1] = df_read_a_float(fin);

	/* Set up scan information for df_readbinary(). */
	df_bin_record[0].scan_dim[0] = nc;
	df_bin_record[0].scan_dim[1] = nr;

	/* Reset counter file pointer. */
	fseek(fin, 0L, SEEK_SET);

    } else {

	/* ASCII matrix format, converted to binary memory format. */
	static float *matrix = NULL;
	int nr, nc;

	/* Insurance against creating a matrix with df_read_matrix()
	 * and then erroring out through df_add_binary_records().
	 */
	if (matrix)
	    free(matrix);

	/* Set important binary variables, then free memory for all default
	 * binary records and set number of records to 0. */
	initialize_binary_vars();
	clear_binary_records(DF_CURRENT_RECORDS);

	/* If the user has set an explicit locale for numeric input, apply it */
	/* here so that it affects data fields read from the input file.      */
	set_numeric_locale();

	/* "skip" option to skip lines at start of ascii file */
	while (df_skip_at_front > 0) {
	    df_gets();
	    df_skip_at_front--;
	}

	/* Keep reading matrices until file is empty. */
	while (1) {
	    if ((matrix = df_read_matrix(&nr, &nc)) != NULL) {
		int index = df_num_bin_records;

		/* Ascii matrix with explicit y in first row, x in first column */
		if (df_nonuniform_matrix) {
		    nc--;
		    nr--;
		}
		/* First column contains row labels, not data */
		if (df_matrix_rowheaders)
		    nc--;

		df_add_binary_records(1, DF_CURRENT_RECORDS);
		df_bin_record[index].memory_data = (char *) matrix;
		matrix = NULL;
		df_bin_record[index].scan_dim[0] = nc;
		df_bin_record[index].scan_dim[1] = nr;
		df_bin_record[index].scan_dim[2] = 0;
		df_bin_file_endianess = THIS_COMPILER_ENDIAN;

		/* Save matrix dimensions in case it contains an image */
		df_xpixels = nc;
		df_ypixels = nr;

		if (set_every) {
		    df_xpixels = 1 + ((int)(GPMIN(lastpoint,df_xpixels-1)) - firstpoint) / everypoint;
		    df_ypixels = 1 + ((int)(GPMIN(lastline,df_ypixels-1)) - firstline) / everyline;
		    FPRINTF((stderr,"datafile.c:%d filtering (%d,%d) to (%d,%d)\n",
				 __LINE__, nc, nr, df_xpixels, df_ypixels));
		}

		/* This matrix is the one (and only) requested by name.	*/
		/* Dummy up index range and skip rest of file.		*/
		if (indexname) {
		    if (index_found) {
			df_lower_index = df_upper_index = index;
			break;
			}
		    else
			df_lower_index = index+1;
		}

	    } else
		break;
	}

	/* We are finished reading user input; return to C locale for internal use */
	reset_numeric_locale();

	/* Data from file is now in memory.  Make the rest of gnuplot think
	 * that the data stream has not yet reached the end of file.
	 */
	df_eof = 0;

    }

}


/* stuff for implementing the call-backs for picking up data values
 * do it here so we can make the variables private to this file
 */

/*{{{  void f_dollars(x) */
void
f_dollars(union argument *x)
{
    int column = x->v_arg.v.int_val;
    struct value a;

    if (column == -3)	/* pseudocolumn -3 means "last column" */
	column = df_no_cols;

    if (column == 0) {
	push(Gcomplex(&a, (double) df_datum, 0.0));     /* $0 */
    } else if (column > df_no_cols || df_column[column-1].good != DF_GOOD) {
	undefined = TRUE;
	/* Nov 2014: This is needed in case the value is referenced */
	/* in an expression inside a 'using' clause.		    */
	push(Gcomplex(&a, not_a_number(), 0.0));
    } else
	push(Gcomplex(&a, df_column[column-1].datum, 0.0));
}

/*}}} */

/*{{{  void f_column() */
void
f_column(union argument *arg)
{
    struct value a;
    int column;

    (void) arg;                 /* avoid -Wunused warning */
    (void) pop(&a);

    if (!evaluate_inside_using)
	int_error(c_token-1, "column() called from invalid context");

    if (a.type == STRING) {
	int j;
	char *name = a.v.string_val;
	column = DF_COLUMN_HEADERS;
	for (j=0; j<df_no_cols; j++) {
	    if (df_column[j].header) {
		int offset = (*df_column[j].header == '"') ? 1 : 0;
		if (streq(name, df_column[j].header + offset)) {
		    column = j+1;
		    if (!df_key_title)
			df_key_title = gp_strdup(df_column[j].header);
		    break;
		}
	    }
	}
	/* This warning should only trigger once per problematic input file */
	if (column == DF_COLUMN_HEADERS && (*name)
	&&  df_warn_on_missing_columnheader) {
	    df_warn_on_missing_columnheader = FALSE;
	    int_warn(NO_CARET,"no column with header \"%s\"", a.v.string_val);
	    for (j=0; j<df_no_cols; j++) {
		if (df_column[j].header) {
		    int offset = (*df_column[j].header == '"') ? 1 : 0;
		    if (!strncmp(name, df_column[j].header + offset,strlen(name)))
			int_warn(NO_CARET, "partial match against column %d header \"%s\"",
				j+1, df_column[j].header);
		}
	    }
	}
	gpfree_string(&a);
    } else
	column = (int) real(&a);

    if (column == -2)
	push(Ginteger(&a, df_current_index));
    else if (column == -1)
	push(Ginteger(&a, line_count));
    else if (column == 0)       /* $0 = df_datum */
	push(Gcomplex(&a, (double) df_datum, 0.0));
    else if (column == -3)	/* pseudocolumn -3 means "last column" */
	push(Gcomplex(&a, df_column[df_no_cols - 1].datum, 0.0));
    else if (column < 1
	     || column > df_no_cols
	     || df_column[column - 1].good != DF_GOOD
	     ) {
	undefined = TRUE;
	/* Nov 2014: This is needed in case the value is referenced */
	/* in an expression inside a 'using' clause.		    */
	push(Gcomplex(&a, not_a_number(), 0.0));
    } else
	push(Gcomplex(&a, df_column[column - 1].datum, 0.0));
}

/* Called from int_error() */
void
df_reset_after_error()
{
    reset_numeric_locale();
    evaluate_inside_using = FALSE;
}

void
f_stringcolumn(union argument *arg)
{
    struct value a;
    int column;

    (void) arg;                 /* avoid -Wunused warning */
    (void) pop(&a);

    if (!evaluate_inside_using || df_matrix)
	int_error(c_token-1, "stringcolumn() called from invalid context");

    if (a.type == STRING) {
	int j;
	char *name = a.v.string_val;
	column = DF_COLUMN_HEADERS;
	for (j=0; j<df_no_cols; j++) {
	    if (df_column[j].header) {
		int offset = (*df_column[j].header == '"') ? 1 : 0;
		if (streq(name, df_column[j].header + offset)) {
		    column = j+1;
		    if (!df_key_title)
			df_key_title = gp_strdup(df_column[j].header);
		    break;
		}
	    }
	}
	/* This warning should only trigger once per problematic input file */
	if (column == DF_COLUMN_HEADERS && (*name)
	&&  df_warn_on_missing_columnheader) {
	    df_warn_on_missing_columnheader = FALSE;
	    int_warn(NO_CARET,"no column with header \"%s\"", a.v.string_val);
	    for (j=0; j<df_no_cols; j++) {
		if (df_column[j].header) {
		    int offset = (*df_column[j].header == '"') ? 1 : 0;
		    if (!strncmp(name, df_column[j].header + offset,strlen(name)))
			int_warn(NO_CARET, "partial match against column %d header \"%s\"",
				j+1, df_column[j].header);
		}
	    }
	}
	gpfree_string(&a);
    } else
	column = (int) real(&a);

    if (column == -3)	/* pseudocolumn -3 means "last column" */
	column = df_no_cols;

    if (column == -2) {
	char temp_string[32];
	sprintf(temp_string, "%d", df_current_index);
	push(Gstring(&a, temp_string ));
    } else if (column == -1) {
	char temp_string[32];
	sprintf(temp_string, "%d", line_count);
	push(Gstring(&a, temp_string ));
    } else if (column == 0) {      /* $0 = df_datum */
	char temp_string[32];
	sprintf(temp_string, "%d", df_datum);
	push(Gstring(&a, temp_string ));
    } else if (column < 1 || column > df_no_cols) {
	undefined = TRUE;
	push(&a);               /* any objection to this ? */
    } else {
	char *temp_string = df_parse_string_field(df_column[column-1].position);
	push(Gstring(&a, temp_string ? temp_string : ""));
	free(temp_string);
    }
}

/*{{{  void f_columnhead() */
void
f_columnhead(union argument *arg)
{
    static char placeholder[] = "@COLUMNHEAD0000@";
    struct value a;

    if (!evaluate_inside_using)
	int_error(c_token-1, "columnhead() called from invalid context");

    (void) arg;                 /* avoid -Wunused warning */
    (void) pop(&a);
    column_for_key_title = (int) real(&a);
    if (column_for_key_title < 0 || column_for_key_title > 9999)
	column_for_key_title = 0;
    snprintf(placeholder+11, 6, "%4d@", column_for_key_title);
    push(Gstring(&a, placeholder));
}


/*{{{  void f_valid() */
void
f_valid(union argument *arg)
{
    struct value a;
    int column, good;

    (void) arg;                 /* avoid -Wunused warning */
    (void) pop(&a);
    column = (int) magnitude(&a) - 1;
    good = column >= 0
	&& column < df_no_cols
	&& df_column[column].good == DF_GOOD;
    push(Ginteger(&a, good));
}

/*}}} */

/*{{{  void f_timecolumn() */
/* Version 5 - replace the old and very broken timecolumn(N) with 
 * a 2-parameter version that requires an explicit time format
 * timecolumn(N, "format").
 */
void
f_timecolumn(union argument *arg)
{
    struct value a;
    struct value b;
    struct tm tm;
    int num_param;
    int column;
    double usec = 0.0;

    (void) arg;                 /* avoid -Wunused warning */
    (void) pop(&b);		/* this is the number of parameters */
    num_param = b.v.int_val;
    (void) pop(&b);		/* this is the time format string */

    switch (num_param) {
    case 2:
	column = (int) magnitude(pop(&a));
	break;
    case 1:
	/* No format parameter passed (v4-style call) */
	/* Only needed for backward compatibility */
	column = magnitude(&b);
	b.v.string_val = gp_strdup(timefmt);
	b.type = STRING;
	break;
    default:
	int_error(NO_CARET,"wrong number of parameters to timecolumn");
    }

    if (!evaluate_inside_using)
	int_error(c_token-1, "timecolumn() called from invalid context");
    if (b.type != STRING)
	int_error(NO_CARET, "non-string passed as a format to timecolumn");

    if (column < 1
	|| column > df_no_cols
	|| !df_column[column - 1].position
	|| !gstrptime(df_column[column - 1].position, b.v.string_val, &tm, &usec)) {
	undefined = TRUE;
	push(&a);
    } else {
	push(Gcomplex(&a, gtimegm(&tm) + usec, 0.0));
    }

    gpfree_string(&b);
}

/*}}} */


/*{{{  static int check_missing(s) */
static int
check_missing(char *s)
{
    /* Match the string specified by 'set datafile missing' */
    if (missing_val != NULL) {
	size_t len = strlen(missing_val);
	if (strncmp(s, missing_val, len) == 0) {
	    s += len;
	    if (!(*s))
		return 1;
	    if (!df_separators && isspace((unsigned char) *s))
		return 1;
	    /* s now points to the character after the "missing" sequence */
	}
    }
    /* April 2013 - Treat an empty csv field as "missing" */
    if (df_separators && strchr(df_separators,*s))
	return 1;

    return (0);
}

/*}}} */


/* formerly in misc.c, but only used here */
/* check user defined format strings for valid double conversions */
/* HBB 20040601: Added check that the number of format specifiers is
 * workable (between 0 and 7) */
static TBOOLEAN
valid_format(const char *format)
{
    int formats_found = 0;

    if (!format)
	return FALSE;

    for (;;) {
	if (!(format = strchr(format, '%')))    /* look for format spec  */
	    return (formats_found > 0 && formats_found <= 7);

	/* Found a % to check --- scan past option specifiers: */
	do {
	    format++;
	} while (strchr("+-#0123456789.", *format));

	/* Now at format modifier */
	switch (*format) {
	case '*':               /* Ignore '*' statements */
	case '%':               /* Char   '%' itself     */
	    format++;
	    continue;
	case 'l':               /* Now we found it !!! */
	    if (!strchr("fFeEgG", format[1]))   /* looking for a valid format */
		return FALSE;
	    formats_found++;
	    format++;
	    break;
	default:
	    return FALSE;
	}
    }
}

/*
 * Plotting routines can call this prior to invoking df_readline() to indicate
 * that they expect a certain column to contain an ascii string rather than a
 * number.
 */
int
expect_string(const char column)
{
    use_spec[column-1].expected_type = CT_STRING;
    /* Nasty hack to make 'plot "file" using "A":"B":"C" with labels' work.
     * The case of named columns is handled by create_call_column_at(),
     * which fakes an action table as if '(column("string"))' was written 
     * in the using spec instead of simply "string". In this specific case, however,
     * we need the values as strings - so we change the action table to call 
     * f_stringcolumn() instead of f_column. */
    if (use_spec[column-1].at 
    && (use_spec[column-1].at->a_count == 2)
    && (use_spec[column-1].at->actions[1].index == COLUMN))
        use_spec[column-1].at->actions[1].index = STRINGCOLUMN;
    return(use_spec[column-1].column);
}

/*
 * Load plot title for key box from the string found earlier by df_readline.
 * Called from get_data().
 */
void
df_set_key_title(struct curve_points *plot)
{
    if (!df_key_title)
	return;

    if (plot->plot_style == HISTOGRAMS
    &&  histogram_opts.type == HT_STACKED_IN_TOWERS) {
	/* In this case it makes no sense to treat key titles in the usual */
	/* way, so we assume that it is supposed to be an xtic label.      */
	/* FIXME EAM - This style should default to notitle!               */
	double xpos = plot->histogram_sequence + plot->histogram->start;
	add_tic_user(FIRST_X_AXIS, df_key_title, xpos, -1);
	free(df_key_title);
	df_key_title = NULL;
	return;
    }

    /* What if there was already a title specified? */
    if (plot->title && !plot->title_is_filename) {
	int columnhead;
	char *placeholder = strstr(plot->title, "@COLUMNHEAD");

	while (placeholder) {
	    char *newtitle = gp_alloc(strlen(plot->title) + df_longest_columnhead, "plot title");
	    char *trailer = NULL;
	    columnhead = strtol(placeholder+11, &trailer, 0);
	    *placeholder = '\0';
	    if (trailer && *trailer == '@')
		trailer++;
	    sprintf(newtitle, "%s%s%s", plot->title,
		    (columnhead <= 0) ? df_key_title :
		    (columnhead <= df_no_cols) ? df_column[columnhead-1].header : "",
		    trailer ? trailer : "");
	    free(plot->title);
	    plot->title = newtitle;
	    placeholder = strstr(newtitle, "@COLUMNHEAD");
	}
	return;
    }
    if (plot->title_is_suppressed)
	return;
    if (plot->title)
	free(plot->title);

    plot->title_no_enhanced = !keyT.enhanced;
    plot->title = df_key_title;
    df_key_title = NULL;
}

/*
 * Load plot title for key box from columnheader.
 * Called from eval_plots(), eval_3dplots() while parsing the plot title option
 */
void
df_set_key_title_columnhead(struct curve_points *plot)
{
    c_token++;
    if (equals(c_token,"(")) {
	c_token++;
	column_for_key_title = int_expression();
	c_token++;
    } else if (!END_OF_COMMAND && isanumber(c_token)) {
	column_for_key_title = int_expression();
    } else {
	if (!plot) /* stats "name" option rather than plot title */
	    column_for_key_title = use_spec[0].column;
	else if (df_no_use_specs == 1)
	    column_for_key_title = use_spec[0].column;
	else if (plot->plot_type == DATA3D)
	    column_for_key_title = use_spec[2].column;
	else
	    column_for_key_title = use_spec[1].column;
    }
    /* This results from  plot 'foo' using (column("name")) title columnhead */
    if (column_for_key_title == NO_COLUMN_HEADER)
	plot->title = gp_strdup("@COLUMNHEAD-1@");
}

char *
df_parse_string_field(char *field)
{
    char *temp_string;
    int length;

    if (!field) {
	return NULL;
    } else if (*field == '"') {
	field++;
	length = strcspn(field, "\"");
    } else if (df_separators != NULL) {
	length = strcspn(field, df_separators);
	if (length > strcspn(field, "\""))	/* Why? */
	    length = strcspn(field, "\"");
    } else {
	length = strcspn(field,"\t ");
    }

    /* If we are fed a file with unrecognized line termination then */
    /* memory use can become excessive. Truncate and report error.  */
    if (length > MAX_LINE_LEN) {
	length = MAX_LINE_LEN;
	int_warn(NO_CARET, "input file contains very long line with no separators, truncating");
	if (strcspn(field, "\r") < MAX_LINE_LEN)
	    int_error(NO_CARET, "      line contains embedded <CR>, wrong file format?");
    }

    temp_string = malloc(length+1);
    strncpy(temp_string, field, length);
    temp_string[length] = '\0';

    parse_esc(temp_string);

    return temp_string;
}

static void
add_key_entry(char *temp_string, int df_datum)
{
    text_label *new_entry = gp_alloc(sizeof(text_label), "key entry");

    /* Associate this key list with the histogram it belongs to. */
    if (!df_current_plot->labels) {
	/* The first text_label structure in the list is a place-holder */
	df_current_plot->labels = gp_alloc(sizeof(text_label), "key entry");
	memset(df_current_plot->labels, 0, sizeof(text_label));
	df_current_plot->labels->tag  = -1;
    }

    new_entry->text = gp_strdup(temp_string);
    new_entry->tag = df_datum;
    new_entry->font = NULL;
    new_entry->next = df_current_plot->labels->next;
    df_current_plot->labels->next = new_entry;
}


/* Construct 2D rotation matrix. */
/* R - Matrix to construct. */
/* alpha - Rotation angle. */
/* return - TRUE means a translation is required. */
TBOOLEAN
rotation_matrix_2D(double R[][2], double alpha)
{
    static double I[2][2] = {{1, 0},
			     {0, 1}};
#define ANGLE_TOLERANCE 0.001
    if (fabs(alpha) < ANGLE_TOLERANCE) {
	/* Zero angle.  Unity rotation. */
	memcpy(R, I, sizeof(I));
	return FALSE;
    } else {
	R[0][0] = cos(alpha);
	R[0][1] = -sin(alpha);
	R[1][0] = sin(alpha);
	R[1][1] = cos(alpha);
	return TRUE;
    }
}


/* Construct 3D rotation matrix. */
/* P - Matrix to construct. */
/* p - Pointer to perpendicular vector. */
/* return - TRUE means a translation is required. */
TBOOLEAN
rotation_matrix_3D(double P[][3], double *p)
{
    static double I[3][3] = {{1, 0, 0},
			     {0, 1, 0},
			     {0, 0, 1}};
    double scale, C1, C2;
#define x p[0]
#define y p[1]
#define z p[2]
    C1 = sqrt(x*x + y*y + z*z);
    C2 = sqrt(x*x + y*y);
    /* ????? Is there a precision constant for doubles similar to what is in limits.h for other types? */
    if ((C1 < 10e-10) || (C2 < (10e-5*C1))) {
	/* Zero vector (invalid) || vector perpendiculat to x/y plane.  Unity rotation. */
	memcpy(P, I, sizeof(I));
	return FALSE;
    } else {
	scale = 1.0/(C1*C2);
	P[0][0] =    x*z * scale;
	P[0][1] =  -y*C1 * scale;
	P[0][2] =   x*C2 * scale;
	P[1][0] =    y*z * scale;
	P[1][1] =   x*C1 * scale;
	P[1][2] =   y*C2 * scale;
	P[2][0] = -C2*C2 * scale;
	P[2][1] =      0;
	P[2][2] =   z*C2 * scale;
	return TRUE;
    }
#undef x
#undef y
#undef z
}


df_byte_read_order_type
byte_read_order (df_endianess_type file_endian)
{
    /* Range limit file endianess to ensure that future file type function
     * programmer doesn't incorrectly access array and cause segmentation
     * fault unknowingly.
     */
    return df_byte_read_order_map[THIS_COMPILER_ENDIAN][GPMIN(file_endian, DF_ENDIAN_TYPE_LENGTH-1)];
}


void
df_unset_datafile_binary(void)
{
    clear_binary_records(DF_DEFAULT_RECORDS);
    df_bin_filetype_default = df_bin_filetype_reset;
    df_bin_file_endianess_default = DF_BIN_FILE_ENDIANESS_RESET;
}


void
df_set_datafile_binary()
{
    c_token++;
    if (END_OF_COMMAND)
	int_error(c_token, "option expected");
    clear_binary_records(DF_CURRENT_RECORDS);
    /* Set current records to default in order to retain current default settings. */
    if (df_bin_record_default) {
	df_bin_filetype = df_bin_filetype_default;
	df_bin_file_endianess = df_bin_file_endianess_default;
	df_add_binary_records(df_num_bin_records_default, DF_CURRENT_RECORDS);
	memcpy(df_bin_record, df_bin_record_default, df_num_bin_records*sizeof(df_binary_file_record_struct));
    } else {
	df_bin_filetype = df_bin_filetype_reset;
	df_bin_file_endianess = DF_BIN_FILE_ENDIANESS_RESET;
	df_add_binary_records(1, DF_CURRENT_RECORDS);
    }
    /* Process the binary tokens. */
    df_set_plot_mode(MODE_QUERY);
    plot_option_binary(FALSE, TRUE);
    /* Copy the modified settings as the new default settings. */
    df_bin_filetype_default = df_bin_filetype;
    df_bin_file_endianess_default = df_bin_file_endianess;
    clear_binary_records(DF_DEFAULT_RECORDS);
    df_add_binary_records(df_num_bin_records, DF_DEFAULT_RECORDS);
    memcpy(df_bin_record_default, df_bin_record, df_num_bin_records_default*sizeof(df_binary_file_record_struct));
}


void
gpbin_filetype_function(void)
{
    /* Gnuplot binary. */
    df_matrix_file = TRUE;
    df_binary_file = TRUE;
}


void
raw_filetype_function(void)
{
    /* No information in file, just data. */
    df_matrix_file = FALSE;
    df_binary_file = TRUE;
}


void
avs_filetype_function(void)
{
    /* A very simple file format: 
     * 8 byte header (width and height, 4 bytes each), unknown endian
     * followed by 4 bytes per pixel (alpha, red, green, blue).
     */

    FILE *fp;
    unsigned long M, N;
    int read_order = 0;

    /* open (header) file */
    fp = loadpath_fopen(df_filename, "rb");
    if (!fp)
	os_error(NO_CARET, "Can't open data file \"%s\"", df_filename);

    /* read header: it is only 8 bytes */
    if (!fread(&M, 4, 1, fp))
	os_error(NO_CARET, "Can't read first dimension in data file \"%s\"", df_filename);
    if (M > 0xFFFF)
	read_order = DF_3210;
    df_swap_bytes_by_endianess((char *) &M, read_order, 4);
    if (!fread(&N, 4, 1, fp))
	os_error(NO_CARET, "Can't read second dimension in data file \"%s\"", df_filename);
    df_swap_bytes_by_endianess((char *) &N, read_order, 4);

    fclose(fp);

    df_matrix_file = FALSE;
    df_binary_file = TRUE;

    df_bin_record[0].scan_skip[0] = 8;
    df_bin_record[0].scan_dim[0] = M;
    df_bin_record[0].scan_dim[1] = N;
 
    df_bin_record[0].scan_dir[0] = 1;
    df_bin_record[0].scan_dir[1] = -1;
    df_bin_record[0].scan_generate_coord = TRUE;
    df_bin_record[0].cart_scan[0] = DF_SCAN_POINT;
    df_bin_record[0].cart_scan[1] = DF_SCAN_LINE;

    /* The four components are 1 byte each. Permute ARGB to RGBA */ 
    df_extend_binary_columns(4);
    df_set_read_type(1, DF_UCHAR);
    df_set_read_type(2, DF_UCHAR);
    df_set_read_type(3, DF_UCHAR);
    df_set_read_type(4, DF_UCHAR);
    df_set_skip_before(1,0);

    df_no_use_specs = 4;
    use_spec[0].column = 2;
    use_spec[1].column = 3;
    use_spec[2].column = 4;
    use_spec[3].column = 1;

}

static void
initialize_binary_vars()
{
    /* Initialize for the df_readline() routine. */
    df_bin_record_count = 0;
    df_M_count = df_N_count = df_O_count = 0;

    /* Set default binary data widths and skip paratemers. */
    df_no_bin_cols = 0;
    df_set_skip_before(1, 0);

    /* Copy the default binary records to the active binary records.  The number
     * of records will always be at least one in case "record", "array",
     * or "filetype" are not issued by the user.
     */
    clear_binary_records(DF_CURRENT_RECORDS);
    if (df_num_bin_records_default) {
	df_bin_filetype = df_bin_filetype_default;
	df_bin_file_endianess = df_bin_file_endianess_default;
	df_add_binary_records(df_num_bin_records_default, DF_CURRENT_RECORDS);
	memcpy(df_bin_record, df_bin_record_default, df_num_bin_records*sizeof(df_binary_file_record_struct));
    } else {
	df_bin_filetype = df_bin_filetype_reset;
	df_bin_file_endianess = DF_BIN_FILE_ENDIANESS_RESET;
	df_add_binary_records(1, DF_CURRENT_RECORDS);
    }
}


static char *too_many_cols_msg = "Too many columns in using specification and implied sampling array";


/* Place a special marker in the using list to derive the x/y/z value
 * from the appropriate dimensional counter.
 */
void
df_insert_scanned_use_spec(int uspec)
{
    /* Place a special marker in the using list to derive the z value
     * from the third dimensional counter, which will be zero.
     */
    if (df_no_use_specs >= MAXDATACOLS)
	int_error(NO_CARET, too_many_cols_msg);
    else {
	int j;
	for (j=df_no_use_specs; j > uspec; j--)
	    use_spec[j] = use_spec[j - 1];
	use_spec[uspec].column = (uspec == 2 ? DF_SCAN_PLANE : DF_SCAN_LINE);
	/* The at portion is set to NULL here, but this doesn't mash
	 * a valid memory pointer because any valid memory pointers
	 * were copied to new locations in the previous for loop.
	 */
	use_spec[uspec].at = NULL; /* Not a bad memory pointer overwrite!! */
	df_no_use_specs++;
    }
}


/* Not the most elegant way of defining the default columns, but I prefer
 * this to switch and conditional statements when there are so many styles.
 */
typedef struct df_bin_default_columns {
    PLOT_STYLE plot_style;
    short excluding_gen_coords; /* Number of columns of information excluding generated coordinates. */
    short dimen_in_2d;          /* Number of additional columns required (in 2D plot) if coordinates not generated. */
} df_bin_default_columns;
df_bin_default_columns default_style_cols[] = {
    {LINES, 1, 1},
    {POINTSTYLE, 1, 1},
    {IMPULSES, 1, 1},
    {LINESPOINTS, 1, 1},
    {DOTS, 1, 1},
    {XERRORBARS, 2, 1},
    {YERRORBARS, 2, 1},
    {XYERRORBARS, 3, 1},
    {BOXXYERROR, 3, 1},
    {BOXES, 1, 1},
    {BOXERROR, 3, 1},
    {STEPS, 1, 1},
    {FSTEPS, 1, 1},
    {FILLSTEPS, 1, 1},
    {HISTEPS, 1, 1},
    {VECTOR, 2, 2},
    {CANDLESTICKS, 4, 1},
    {FINANCEBARS, 4, 1},
    {BOXPLOT, 2, 1},
    {XERRORLINES, 2, 1},
    {YERRORLINES, 2, 1},
    {XYERRORLINES, 3, 1},
    {FILLEDCURVES, 1, 1},
    {PM3DSURFACE, 1, 2},
    {LABELPOINTS, 2, 1},
    {HISTOGRAMS, 1, 0},
    {IMAGE, 1, 2},
    {RGBIMAGE, 3, 2},
    {RGBA_IMAGE, 4, 2}
#ifdef EAM_OBJECTS
    , {CIRCLES, 2, 1}
    , {ELLIPSES, 2, 3}
#endif
    , {TABLESTYLE, 0, 0}
};


/* FIXME!!!
 * EAM Feb 2008:
 * This whole routine is a disaster.  It makes so many broken assumptions it's not funny.
 * Other than filling in the first two columns of an implicit matrix, I suspect we can
 * do away with it altogether. Frankly, we _don't care_ how many columns there are,
 * so long as the ones that are present are mapped to the right ordering.
 */

static void
adjust_binary_use_spec()
{

    char *nothing_known = "No default columns known for that plot style";
    enum PLOT_STYLE plot_style;
    unsigned int ps_index;
    int c_token_copy;

    /* The default binary matrix format is nonuniform, i.e. 
     * it has an extra row and column for sample coordinates.
     */
    if (df_matrix_file && df_binary_file)
	df_nonuniform_matrix = TRUE;

    c_token_copy = c_token;
 
    for ( ; !END_OF_COMMAND; c_token++)
	if (almost_equals(c_token, "w$ith"))
	    break;
    if (!END_OF_COMMAND)
	plot_style = get_style();
    else
	plot_style = LINES;
    c_token = c_token_copy;

    /* Determine index. */
    for (ps_index = 0; ps_index < sizeof(default_style_cols)/sizeof(default_style_cols[0]); ps_index++) {
	if (default_style_cols[ps_index].plot_style == plot_style)
	    break;
    }
    if (ps_index == sizeof(default_style_cols)/sizeof(default_style_cols[0]))
	int_error(c_token_copy, nothing_known);

    /* Matrix format is interpretted as always having three columns. */
    if (df_matrix_file) {
	if (df_no_bin_cols > 3)
	    int_error(NO_CARET, "Matrix data contains only three columns");
	df_extend_binary_columns(3);
    }

    /* If nothing has been done to set the using specs, use the default using
     * characteristics for the style.
     */
    if (!df_no_use_specs) {

	if (!df_matrix_file) {

	    int no_cols = default_style_cols[ps_index].excluding_gen_coords;
	    if (!no_cols)
		int_error(c_token_copy, nothing_known);

	    /* If coordinates are generated, make sure this plot style allows it.
	     * Otherwise, add in the number of generated coordinates and add an
	     * extra column if using `splot`.
	     */
	    if (df_num_bin_records && df_bin_record[0].scan_generate_coord) {
		if (default_style_cols[ps_index].dimen_in_2d == 0)
		    int_error(c_token_copy, "Cannot generate coords for that plot style");
	    } else {
		/* If there aren't generated coordinates, then add the
		 * amount of columns that would be generated.
		 */
		no_cols += default_style_cols[ps_index].dimen_in_2d;
		if (df_plot_mode == MODE_SPLOT)
		    no_cols++;
	    }

	    assert(no_cols <= MAXDATACOLS);

	    /* Nothing need be done here to set the using specs because they
	     * will have been initialized appropriately and left unaltered.
	     * So just set the number of specs.
	     */
	    df_no_use_specs = no_cols;
	    df_extend_binary_columns(no_cols);

	} else {

	    /* Number of columns is fixed at three and no using specs given.  Do what we can.
	     * The obvious best combination is two dimensional coordinates and one information
	     * value.  One wonders what to do if a matrix is only one column; can be treated
	     * as linear?  This isn't implemented here, but if it were, this is where it
	     * should go.
	     */

	    if ((default_style_cols[ps_index].dimen_in_2d == 2)
		&& (default_style_cols[ps_index].excluding_gen_coords == 1)) {
		df_no_use_specs = 3;
	    } else if ((default_style_cols[ps_index].dimen_in_2d == 1)
		   &&  (default_style_cols[ps_index].excluding_gen_coords == 1) ) {
		if (df_plot_mode == MODE_SPLOT)
		    df_no_use_specs = 3;
		else {
		    /* Command:  plot 'foo' matrix       with no using spec */
		    /* Matix element treated as y value rather than z value */
		    df_no_use_specs = 2;
		    use_spec[1].column = 3;
		}
	    } else
		int_error(NO_CARET, "Plot style does not conform to three column data in this graph mode");
	}

    }

    if (df_num_bin_records && df_bin_record[0].scan_generate_coord && !df_matrix_file) {

	int i;

	struct use_spec_s original_use_spec[MAXDATACOLS];
	int added_columns = 0;

	/* Keep record of the original using specs. */
	memcpy(original_use_spec, use_spec, sizeof(use_spec));

	/* Put in columns at front for generated variables. */
	for (i = 0; i < 3; i++) {
	    if (df_bin_record[0].cart_dim[i] || df_bin_record[0].scan_dim[i])
		added_columns++;
	    else
		break;
	}
	if ((df_no_use_specs + added_columns) >= MAXDATACOLS)
	    int_error(NO_CARET, too_many_cols_msg);
	else {

	    /* Shift the original columns over by added number of columns, but only
	     * if not matrix data.
	     */
	    memcpy(&use_spec[added_columns], original_use_spec, df_no_use_specs*sizeof(use_spec[0]));

	    /* The at portion is set to NULL here, but this doesn't mash
	     * a valid memory pointer because any valid memory pointers
	     * were copied to new locations in the previous memcpy().
	     */
	    for (i = 0; i < added_columns; i++) {
		use_spec[i].column = df_bin_record[0].cart_scan[i];
		use_spec[i].at = NULL; /* Not a bad memory pointer overwrite!! */
	    }

	    df_no_use_specs += added_columns; /* Do not extend columns for generated coordinates. */
	}

	if (df_plot_mode == MODE_SPLOT) {

	    /* For binary data having an implied uniformly sampled grid, treat
	     * less than three-dimensional data in special ways based upon what
	     * is being plotted.
	     */
	    int k;
	    for (k = 0; k < df_num_bin_records; k++) {
		if ((df_bin_record[k].cart_dim[2] == 0) && (df_bin_record[k].scan_dim[2] == 0)) {
		    if (default_style_cols[ps_index].dimen_in_2d > 2)
			int_error(NO_CARET, "Plot style requires higher than two-dimensional sampling array");
		    else {
			if ((df_bin_record[k].cart_dim[1] == 0) && (df_bin_record[k].scan_dim[1] == 0)) {
			    if (default_style_cols[ps_index].dimen_in_2d > 1)
				int_error(NO_CARET, "Plot style requires higher than one-dimensional sampling array");
			    else {
				/* Place a special marker in the using list to derive the y value
				 * from the second dimensional counter.
				 */
				df_insert_scanned_use_spec(1);
			    }
			}
			/* Place a special marker in the using list to derive the z value
			 * from the third dimensional counter.
			 */
			df_insert_scanned_use_spec(2);
		    }
		}
	    }
	}
    }
}

char *equal_symbol_msg = "Equal ('=') symbol required";


static void
plot_option_binary(TBOOLEAN set_matrix, TBOOLEAN set_default)
{
    TBOOLEAN duplication = FALSE;
    TBOOLEAN set_record = FALSE;
    TBOOLEAN set_array = FALSE, set_dx = FALSE, set_dy = FALSE, set_dz = FALSE;
    TBOOLEAN set_center = FALSE, set_origin = FALSE, set_skip = FALSE, set_endian = FALSE;
    TBOOLEAN set_rotation = FALSE, set_perpendicular = FALSE;
    TBOOLEAN set_flip = FALSE, set_noflip = FALSE;
    TBOOLEAN set_flipx = FALSE, set_flipy = FALSE, set_flipz = FALSE;
    TBOOLEAN set_scan = FALSE;
    TBOOLEAN set_format = FALSE;

	/* Binary file type must be the first word in the command following `binary`" */
	if (df_bin_filetype_default >= 0)
	    df_bin_filetype = df_bin_filetype_default;
	if (almost_equals(c_token, "file$type") || (df_bin_filetype >= 0)) {
	    int i;
	    char file_ext[8] = {'\0','\0','\0','\0','\0','\0','\0','\0'};

	    /* Above keyword not part of pre-existing binary definition.
	     * So use general binary. */
	    if (set_matrix)
		int_error(c_token, matrix_general_binary_conflict_msg);
	    df_matrix_file = FALSE;

	    if (almost_equals(c_token, "file$type")) {
		if (!equals(++c_token, "="))
		    int_error(c_token, equal_symbol_msg);

		copy_str(file_ext, ++c_token, 8);
		for (i=0; df_bin_filetype_table[i].key; i++)
		    if (!strcasecmp(file_ext, df_bin_filetype_table[i].key)) {
			binary_input_function = df_bin_filetype_table[i].value;
			df_bin_filetype = i;
			break;
		    }

		if (df_bin_filetype != i)
		    /* Maybe set to "auto" and continue? */
		    int_error(c_token, "Unrecognized filetype; try \"show datafile binary filetypes\"");

		c_token++;
	    }

	    if (df_plot_mode != MODE_QUERY  
	    && !strcmp("auto", df_bin_filetype_table[df_bin_filetype].key)) {
		int i;
		char *file_ext = strrchr(df_filename, '.');
		if (file_ext++) {
		    for (i=0; df_bin_filetype_table[i].key; i++)
			if (!strcasecmp(file_ext, df_bin_filetype_table[i].key))
			    binary_input_function = df_bin_filetype_table[i].value;
		}
		if (binary_input_function == auto_filetype_function)
		    int_error(NO_CARET, "Unrecognized filename extension; try \"show datafile binary filetypes\"");
	    }

	    /* Unless only querying settings, call the routine to prep binary data parameters. */
	    if (df_plot_mode != MODE_QUERY) {
		(*binary_input_function)();
		df_xpixels = df_bin_record[0].scan_dim[0];
		df_ypixels = df_bin_record[0].scan_dim[1];
		FPRINTF((stderr,"datafile.c:%d  image dimensions %d x %d\n", __LINE__,
			df_xpixels, df_ypixels));
	    }

	    /* Now, at this point anything that was filled in for "scan" should
	     * override the "cart" variables.
	     */
	    for (i=0; i < df_num_bin_records; i++) {
		int j;
		/* Dimension */
		if (df_bin_record[i].scan_dim[0] != df_bin_record_reset.scan_dim[0])
		    for (j=0; j < 3; j++)
			df_bin_record[i].cart_dim[j] = 0;
		/* Delta */
		for (j=0; j < 3; j++)
		    if (df_bin_record[i].scan_delta[j] != 0.0) {
			int k;
			for (k=0; k < 3; k++)
			    if (df_bin_record[i].cart_scan[k] == (DF_SCAN_POINT - j))
				df_bin_record[i].cart_delta[k] = 0;
		    }
		/* Translation */
		if (df_bin_record[i].scan_trans != DF_TRANSLATE_DEFAULT)
		    df_bin_record[i].cart_trans = DF_TRANSLATE_DEFAULT;
	    }
	}


    while (!END_OF_COMMAND) {
	char origin_and_center_conflict_message[] = "Can specify `origin` or `center`, but not both";

	/* look for record */
	if (almost_equals(c_token, "rec$ord")) {
	    if (set_record) { duplication=TRUE; break; }
	    c_token++;
	    /* Above keyword not part of pre-existing binary definition.  So use general binary. */
	    if (set_matrix)
		int_error(c_token, matrix_general_binary_conflict_msg);
	    df_matrix_file = FALSE;
	    plot_option_array();
	    set_record = TRUE;
	    df_xpixels = df_bin_record[df_num_bin_records - 1].cart_dim[1];
	    df_ypixels = df_bin_record[df_num_bin_records - 1].cart_dim[0];
	    FPRINTF((stderr,"datafile.c:%d  record dimensions %d x %d\n", __LINE__,
		df_xpixels, df_ypixels));
	    continue;
	}

	/* look for array */
	if (almost_equals(c_token, "arr$ay")) {
	    int i;
	    if (set_array) { duplication=TRUE; break; }
	    c_token++;
	    /* Above keyword not part of pre-existing binary definition.  So use general binary. */
	    if (set_matrix)
		int_error(c_token, matrix_general_binary_conflict_msg);
	    df_matrix_file = FALSE;
	    plot_option_array();
	    for (i = 0; i < df_num_bin_records; i++) {
		/* Indicate that coordinate info should be generated internally */
		df_bin_record[i].scan_generate_coord = TRUE;
	    }
	    set_array = TRUE;
	    df_xpixels = df_bin_record[df_num_bin_records - 1].cart_dim[0];
	    df_ypixels = df_bin_record[df_num_bin_records - 1].cart_dim[1];
	    FPRINTF((stderr,"datafile.c:%d  array dimensions %d x %d\n", __LINE__,
		df_xpixels, df_ypixels));
	    continue;
	}

	/* deal with spacing between array points */
	if (equals(c_token, "dx") || equals(c_token, "dt")) {
	    if (set_dx) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_DELTA, 0);
	    if (!set_dy) {
		int i;
		for (i = 0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_delta[1] = df_bin_record[i].cart_delta[0];
	    }
	    if (!set_dz) {
		int i;
		for (i = 0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_delta[2] = df_bin_record[i].cart_delta[0];
	    }
	    set_dx = TRUE;
	    continue;
	}

	if (equals(c_token, "dy") || equals(c_token, "dr")) {
	    if (set_dy) { duplication=TRUE; break; }
	    if (!set_array && !df_bin_record)
		int_error(c_token, "Must specify a sampling array size before indicating spacing in second dimension");
	    c_token++;
	    plot_option_multivalued(DF_DELTA, 1);
	    if (!set_dz) {
		int i;
		for (i = 0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_delta[2] = df_bin_record[i].cart_delta[1];
	    }
	    set_dy = TRUE;
	    continue;
	}

	if (equals(c_token, "dz")) {
	    int_error(c_token, "Currently not supporting three-dimensional sampling");
	    if (set_dz) { duplication=TRUE; break; }
	    if (!set_array && !df_bin_record)
		int_error(c_token, "Must specify a sampling array size before indicating spacing in third dimension");
	    c_token++;
	    plot_option_multivalued(DF_DELTA, 2);
	    set_dz = TRUE;
	    continue;
	}

	/* deal with direction in which sampling increments */
	if (equals(c_token, "flipx")) {
	    if (set_flipx) { duplication=TRUE; break; }
	    c_token++;
	    /* If no equal sign, then set flip true for all records. */
	    if (!equals(c_token, "=")) {
		int i;
		for (i = 0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_dir[0] = -1;
	    } else {
		plot_option_multivalued(DF_FLIP_AXIS, 0);
	    }
	    set_flipx = TRUE;
	    continue;
	}

	if (equals(c_token, "flipy")) {
	    if (set_flipy) { duplication=TRUE; break; }
	    if (!set_array && !df_bin_record)
		int_error(c_token, "Must specify a sampling array size before indicating flip in second dimension");
	    c_token++;
	    /* If no equal sign, then set flip true for all records. */
	    if (!equals(c_token, "=")) {
		int i;
		for (i = 0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_dir[1] = -1;
	    } else {
		plot_option_multivalued(DF_FLIP_AXIS, 1);
	    }
	    set_flipy = TRUE;
	    continue;
	}

	if (equals(c_token, "flipz")) {
	    int_error(c_token, "Currently not supporting three-dimensional sampling");
	    if (set_flipz) { duplication=TRUE; break; }
	    if (!set_array && !df_bin_record)
		int_error(c_token, "Must specify a sampling array size before indicating spacing in third dimension");
	    c_token++;
	    /* If no equal sign, then set flip true for all records. */
	    if (!equals(c_token, "=")) {
		int i;
		for (i=0; i < df_num_bin_records; i++)
		    df_bin_record[i].cart_dir[2] = -1;
	    } else {
		plot_option_multivalued(DF_FLIP_AXIS, 2);
	    }
	    set_flipz = TRUE;
	    continue;
	}

	/* Deal with flipping data for individual records. */
	if (equals(c_token, "flip")) {
	    if (set_flip) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_FLIP, -1);
	    set_flip = TRUE;
	    continue;
	}

	/* Deal with flipping data for individual records. */
	if (equals(c_token, "noflip")) {
	    if (set_noflip) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_FLIP, 1);
	    set_noflip = TRUE;
	    continue;
	}

	/* Deal with manner in which dimensions are scanned from file. */
	if (equals(c_token, "scan")) {
	    if (set_scan) { duplication=TRUE; break; }
	    c_token++;
	    if (almost_equals(c_token+1, "yx$z"))
		df_transpose = TRUE;
	    plot_option_multivalued(DF_SCAN, 0);
	    set_scan = TRUE;
	    continue;
	}

	/* Deal with manner in which dimensions are scanned from file. */
	if (almost_equals(c_token, "trans$pose")) {
	    int i;
	    if (set_scan) { duplication=TRUE; break; }
	    c_token++;
	    for (i=0; i < df_num_bin_records; i++)
		memcpy(df_bin_record[i].cart_scan, df_bin_scan_table_2D[TRANSPOSE_INDEX].scan, sizeof(df_bin_record[0].cart_scan));
	    set_scan = TRUE;
	    df_transpose = TRUE;
	    continue;
	}

	/* deal with origin */
	if (almost_equals(c_token, "orig$in")) {
	    if (set_center)
		int_error(c_token, origin_and_center_conflict_message);
	    if (set_origin) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_ORIGIN, df_plot_mode);
	    set_origin = TRUE;
	    continue;
	}

	/* deal with origin */
	if (almost_equals(c_token, "cen$ter")) {
	    if (set_origin)
		int_error(c_token, origin_and_center_conflict_message);
	    if (set_center) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_CENTER, df_plot_mode);
	    set_center = TRUE;
	    continue;
	}

	/* deal with rotation angle */
	if (almost_equals(c_token, "rot$ation") || almost_equals(c_token, "rot$ate")) {
	    if (set_rotation) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_ROTATION, 0);
	    set_rotation = TRUE;
	    continue;
	}

	/* deal with rotation angle */
	if (almost_equals(c_token, "perp$endicular")) {
	    if (df_plot_mode == MODE_PLOT)
		int_error(c_token, "Key word `perpendicular` is not allowed with `plot` command");
	    if (set_perpendicular) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_PERPENDICULAR, 0);
	    set_perpendicular = TRUE;
	    continue;
	}

	/* deal with number of bytes to skip before record */
	if (almost_equals(c_token, "skip")) {
	    if (set_skip) { duplication=TRUE; break; }
	    c_token++;
	    plot_option_multivalued(DF_SKIP, 0);
	    set_skip = TRUE;
	    continue;
	}

	/* deal with byte order */
	if (almost_equals(c_token, "end$ian")) {
	    if (set_endian) { duplication=TRUE; break; }
	    c_token++;

	    /* Require equal symbol. */
	    if (!equals(c_token, "="))
		int_error(c_token, equal_symbol_msg);
	    c_token++;

	    if (almost_equals(c_token, "def$ault"))
		df_bin_file_endianess = THIS_COMPILER_ENDIAN;
	    else if (equals(c_token, "swap") || equals(c_token, "swab"))
		df_bin_file_endianess = (~df_bin_file_endianess)&3; /* complement and isolate lowest two bits */
	    else if (almost_equals(c_token, "lit$tle"))
		df_bin_file_endianess = DF_LITTLE_ENDIAN;
	    else if (equals(c_token, "big"))
		df_bin_file_endianess = DF_BIG_ENDIAN;
#if SUPPORT_MIDDLE_ENDIAN
	    else if (almost_equals(c_token, "mid$dle") || equals(c_token, "pdp"))
		df_bin_file_endianess = DF_PDP_ENDIAN;
	    else
		int_error(c_token, "Options are default, swap (swab), little, big, middle (pdp)");
#else
	    else
		int_error(c_token, "Options are default, swap (swab), little, big");
#endif
	    c_token++;
	    set_endian = TRUE;
	    continue;
	}

	/* deal with various types of binary files */
	if (almost_equals(c_token, "form$at")) {
	    if (set_format) { duplication=TRUE; break; }
	    c_token++;
	    /* Format string not part of pre-existing binary definition.  So use general binary. */
	    if (set_matrix)
		int_error(c_token, matrix_general_binary_conflict_msg);
	    df_matrix_file = FALSE;

	    /* Require equal sign */
	    if (!equals(c_token, "="))
		int_error(c_token, equal_symbol_msg);
	    c_token++;

	    if (set_default) {
		free(df_binary_format);
		df_binary_format = try_to_get_string();
	    } else {
		char *format_string = try_to_get_string();
		if (!format_string)
		    int_error(c_token, "missing format string");
		plot_option_binary_format(format_string);
		free(format_string);
	    }
	    set_format = TRUE;
	    continue;
	}

	break; /* unknown option */

    } /* while (!END_OF_COMMAND) */

    if (duplication)
	int_error(c_token, "Duplicated or contradicting arguments in datafile options");

    if (!set_default && !set_matrix && df_num_bin_records_default) {
	int_warn(NO_CARET, "using default binary record/array structure");
    }

    if (!set_format && !df_matrix_file) {
	if (df_binary_format) {
	    plot_option_binary_format(df_binary_format);
	    int_warn(NO_CARET, "using default binary format");
	}
    }

}


void
df_add_binary_records(int num_records_to_add, df_records_type records_type)
{
    int i;
    int new_number;
    df_binary_file_record_struct **bin_record;
    int *num_bin_records;
    int *max_num_bin_records;

    if (records_type == DF_CURRENT_RECORDS) {
	bin_record = &df_bin_record;
	num_bin_records = &df_num_bin_records;
	max_num_bin_records = &df_max_num_bin_records;
    } else {
	bin_record = &df_bin_record_default;
	num_bin_records = &df_num_bin_records_default;
	max_num_bin_records = &df_max_num_bin_records_default;
    }

    new_number = *num_bin_records + num_records_to_add;

    if (new_number > *max_num_bin_records) {
	*bin_record
	    = gp_realloc(*bin_record,
			 new_number * sizeof(df_binary_file_record_struct),
			 "binary file data records");
	if (!*bin_record) {
	    *max_num_bin_records = 0;
	    int_error(c_token,
		      "Error assigning memory for binary file data records");
	}
	*max_num_bin_records = new_number;
    }

    for (i = 0; i < num_records_to_add; i++) {
	memcpy(*bin_record + *num_bin_records,
	       &df_bin_record_reset,
	       sizeof(df_binary_file_record_struct));
	(*num_bin_records)++;
    }
}


static void
clear_binary_records(df_records_type records_type)
{
    df_binary_file_record_struct *temp_bin_record;
    int *temp_num_bin_records;
    int i;

    if (records_type == DF_CURRENT_RECORDS) {
	temp_bin_record = df_bin_record;
	temp_num_bin_records = &df_num_bin_records;
    } else {
	temp_bin_record = df_bin_record_default;
	temp_num_bin_records = &df_num_bin_records_default;
    }

    for (i = 0; i < *temp_num_bin_records; i++) {
	if (temp_bin_record[i].memory_data != NULL) {
	    free(temp_bin_record[i].memory_data);
	    temp_bin_record[i].memory_data = NULL;
	}
    }
    *temp_num_bin_records = 0;
}


/* 
 * Syntax is:   array=(xdim,ydim):(xdim,ydim):CONST:(xdim) etc
 */
static void
plot_option_array(void)
{
    int number_of_records = 0;

    if (!equals(c_token, "="))
	int_error(c_token, equal_symbol_msg);

#if (0)
    /* Removing this call reduces the order-dependence of binary options.	*/
    /* However, it may also introduce some option persistance across plots.	*/
    /* See Bug #3408082 */
    clear_binary_records(DF_CURRENT_RECORDS);
#endif

    do {
	c_token++;

	/* Partial backward compatibility with syntax up to 4.2.4 */
	if (isanumber(c_token)) {
	    if (++number_of_records > df_num_bin_records)
		df_add_binary_records(1, DF_CURRENT_RECORDS);
	    df_bin_record[df_num_bin_records - 1].cart_dim[0] = int_expression();
	    /* Handle the old syntax:  array=123x456 */
	    if (!END_OF_COMMAND) {
		char xguy[8]; int itmp=0;
		copy_str(xguy, c_token, 6);
		if (xguy[0] == 'x') {
		    sscanf(&xguy[1],"%d",&itmp);
		    df_bin_record[df_num_bin_records - 1].cart_dim[1] = itmp;
		    c_token++;
		}
	    }
	} else

	if (equals(c_token, "(")) {
	    c_token++;
	    if (++number_of_records > df_num_bin_records)
		df_add_binary_records(1, DF_CURRENT_RECORDS);
	    df_bin_record[df_num_bin_records - 1].cart_dim[0] = int_expression();
	    if (equals(c_token, ",")) {
		c_token++;
		df_bin_record[df_num_bin_records - 1].cart_dim[1] = int_expression();
	    }
	    if (!equals(c_token, ")"))
		int_error(c_token, "tuple syntax error");
	    c_token++;
	}

    } while (equals(c_token, ":"));
}


/* Evaluate a tuple of up to specified dimension. */
#define TUPLE_SEPARATOR_CHAR ":"
#define LEFT_TUPLE_CHAR "("
#define RIGHT_TUPLE_CHAR ")"

int
token2tuple(double *tuple, int dimension)
{
    if (equals(c_token, LEFT_TUPLE_CHAR)) {
	TBOOLEAN expecting_number = TRUE;
	int N = 0;

	c_token++;
	while (!END_OF_COMMAND) {
	    if (expecting_number) {
		if (++N <= dimension)
		    *tuple++ = real_expression();
		else
		    int_error(c_token-1, "More than %d elements", N);
		expecting_number = FALSE;
	    } else if (equals(c_token, ",")) {
		    c_token++;
		    expecting_number = TRUE;
	    } else if (equals(c_token, RIGHT_TUPLE_CHAR)) {
		    c_token++;
		    return N;
	    } else
		    int_error(c_token, "Expecting ',' or '" RIGHT_TUPLE_CHAR "'");
	}
    }

    /* Not a tuple */
    return 0;
}


/* Determine the 2D rotational matrix from the "rotation" qualifier. */
void
plot_option_multivalued(df_multivalue_type type, int arg)
{
    int bin_record_count = 0;
    int test_val;

    /* Require equal symbol. */
    if (!equals(c_token, "="))
	int_error(c_token, equal_symbol_msg);
    c_token++;

    while (!END_OF_COMMAND) {
	double tuple[3];

	switch (type) {
	    case DF_ORIGIN:
	    case DF_CENTER:
	    case DF_PERPENDICULAR:
		test_val = token2tuple(tuple, sizeof(tuple)/sizeof(tuple[0]));
		break;
	    case DF_SCAN:
	    case DF_FLIP:
		/* Will check later */
		test_val = 1;
		break;
	    default: {
		/* Check if a valid number. */
		tuple[0] = real_expression();
		test_val = 1;
	    }
	}

	if (test_val) {
	    char const * cannot_flip_msg = "Cannot flip a non-existent dimension";
	    char flip_list[4];

	    if (bin_record_count >= df_num_bin_records)
		int_error(c_token, "More parameters specified than data records specified");

	    switch (type) {
		case DF_DELTA:
		    /* Set the spacing between grid points in the
		     * specified dimension. */
		    *(df_bin_record[bin_record_count].cart_delta + arg) = tuple[0];
		    if (df_bin_record[bin_record_count].cart_delta[arg] <= 0)
			int_error(c_token - 2, "Sample period must be positive. Try `flip` for changing direction");
		    break;

		case DF_FLIP_AXIS:
		    /* Set the direction of grid points increment in
		     * the specified dimension. */
		    if (df_bin_record[bin_record_count].cart_dim[0] != 0) {
			if (tuple[0] == 0.0)
			    df_bin_record[bin_record_count].cart_dir[arg] = 0;
			else if (tuple[0] == 1.0)
			    df_bin_record[bin_record_count].cart_dir[arg] = 1;
			else
			    int_error(c_token-1, "Flipping dimension direction must be 1 or 0");
		    } else
			int_error(c_token, cannot_flip_msg);
		    break;

		case DF_FLIP:
		    /* Set the direction of grid points increment in
		     * based upon letters for axes. Check if there are
		     * any characters in string that shouldn't be. */
		    copy_str(flip_list, c_token, 4);
		    if (strlen(flip_list) != strspn(flip_list, "xXyYzZ"))
			int_error(c_token, "Can only flip x, y, and/or z");
		    /* Check for valid dimensions. */
		    if (strpbrk(flip_list, "xX")) {
			if (df_bin_record[bin_record_count].cart_dim[0] != 0)
			    df_bin_record[bin_record_count].cart_dir[0] = arg;
			else
			    int_error(c_token, cannot_flip_msg);
		    }
		    if (strpbrk(flip_list, "yY")) {
			if (df_bin_record[bin_record_count].cart_dim[1] != 0)
			    df_bin_record[bin_record_count].cart_dir[1] = arg;
			else
			    int_error(c_token, cannot_flip_msg);
		    }
		    if (strpbrk(flip_list, "zZ")) {
			if (df_bin_record[bin_record_count].cart_dim[2] != 0)
			    df_bin_record[bin_record_count].cart_dir[2] = arg;
			else
			    int_error(c_token, cannot_flip_msg);
		    }
		    c_token++;
		    break;
		
		case DF_SCAN: {
		    /* Set the method in which data is scanned from
		     * file.  Compare against a set number of strings.  */
		    int i;
		
		    if (!(df_bin_record[bin_record_count].cart_dim[0]
			|| df_bin_record[bin_record_count].scan_dim[0])
			|| !(df_bin_record[bin_record_count].cart_dim[1]
			|| df_bin_record[bin_record_count].scan_dim[1]))
			int_error(c_token, "Cannot alter scanning method for one-dimensional data");
		    else if (df_bin_record[bin_record_count].cart_dim[2]
			     || df_bin_record[bin_record_count].scan_dim[2]) {
			for (i = 0;
			     i < sizeof(df_bin_scan_table_3D)
				 /sizeof(df_bin_scan_table_3D_struct);
			     i++)
			    if (equals(c_token, df_bin_scan_table_3D[i].string)) {
				memcpy(df_bin_record[bin_record_count].cart_scan,
				       df_bin_scan_table_3D[i].scan,
				       sizeof(df_bin_record[0].cart_scan));
				break;
			    }
			if (i == sizeof(df_bin_scan_table_3D) / sizeof(df_bin_scan_table_3D_struct))
			    int_error(c_token, "Improper scanning string. Try 3 character string for 3D data");
		    } else {
			for (i = 0;
			     i < sizeof(df_bin_scan_table_2D)
				 /sizeof(df_bin_scan_table_2D_struct); i++)
			    if (equals(c_token, df_bin_scan_table_2D[i].string)) {
				memcpy(df_bin_record[bin_record_count].cart_scan,
				       df_bin_scan_table_2D[i].scan,
				       sizeof(df_bin_record[0].cart_scan));
				break;
			    }
			if (i == sizeof(df_bin_scan_table_2D) / sizeof(df_bin_scan_table_2D_struct))
			    int_error(c_token, "Improper scanning string. Try 2 character string for 2D data");
		    }
		    /* Remove the file supplied scan direction. */
		    memcpy(df_bin_record[bin_record_count].scan_dir,
			   df_bin_record_reset.scan_dir,
			   sizeof(df_bin_record[0].scan_dir));
		    c_token++;
		    break;
		}

		case DF_SKIP:
		    /* Set the number of bytes to skip before reading
		     * record. */
		    df_bin_record[bin_record_count].scan_skip[0] = tuple[0];
		    if ((df_bin_record[bin_record_count].scan_skip[0] != tuple[0])
		    ||  (df_bin_record[bin_record_count].scan_skip[0] < 0))
			int_error(c_token, "Number of bytes to skip must be positive integer");
		    break;

		case DF_ORIGIN:
		case DF_CENTER:
		    /* Set the origin or center of the image based upon
		     * the plot mode. */
		    if (type == DF_ORIGIN)
			df_bin_record[bin_record_count].cart_trans
			    = DF_TRANSLATE_VIA_ORIGIN;
		    else
			df_bin_record[bin_record_count].cart_trans
			    = DF_TRANSLATE_VIA_CENTER;
		    if (arg == MODE_PLOT) {
			if (test_val != 2)
			    int_error(c_token, "Two-dimensional tuple required for 2D plot");
			tuple[2] = 0.0;
		    } else if (arg == MODE_SPLOT) {
			if (test_val != 3)
			    int_error(c_token, "Three-dimensional tuple required for 3D plot");
		    } else if (arg == MODE_QUERY) {
			if (test_val != 3)
			    int_error(c_token, "Three-dimensional tuple required for setting binary parameters");
		    } else {
			int_error(c_token, "Internal error (datafile.c): Unknown plot mode");
		    }
		    memcpy(df_bin_record[bin_record_count].cart_cen_or_ori,
			   tuple, sizeof(tuple));
		    break;
		
		case DF_ROTATION:
		    /* Allow user to enter angle in terms of pi or degrees. */
		    if (equals(c_token, "pi")) {
			tuple[0] *= M_PI;
			c_token++;
		    } else if (almost_equals(c_token, "d$egrees")) {
			tuple[0] *= M_PI/180;
			c_token++;
		    }
		    /* Construct 2D rotation matrix. */
		    df_bin_record[bin_record_count].cart_alpha = tuple[0];
		    break;

		case DF_PERPENDICULAR:
		    /* Make sure in three dimensional plotting mode before
		     * accepting the perpendicular vector for translation. */
		    if (test_val != 3)
			int_error(c_token, "Three-dimensional tuple required");
		    /* Compare vector length against variable precision
		     * to determine if this is the null vector */
		    if ((tuple[0]*tuple[0]
			 + tuple[1]*tuple[1]
			 + tuple[2]*tuple[2]) < 100.*DBL_EPSILON)
			int_error(c_token, "Perpendicular vector cannot be zero");
		    memcpy(df_bin_record[bin_record_count].cart_p,
			   tuple,
			   sizeof(tuple));
		    break;

		default:
		    int_error(NO_CARET, "Internal error: Invalid comma separated type");
	    } /* switch() */
	} else {
	    int_error(c_token, "Invalid numeric or tuple form");
	}

	if (equals(c_token, TUPLE_SEPARATOR_CHAR)) {
	    bin_record_count++;
	    c_token++;
	} else
	    break;

    } /* while(!EOC) */

    return;
}


/* Set the 'bytes' to skip before column 'col'. */
void
df_set_skip_before(int col, int bytes)
{
    assert(col > 0);
    /* Check if we have room at least col columns */
    if (col > df_max_bininfo_cols) {
	df_column_bininfo = gp_realloc(df_column_bininfo,
				       col * sizeof(df_column_bininfo_struct),
				       "datafile columns binary information");
	df_max_bininfo_cols = col;
    }
    df_column_bininfo[col-1].skip_bytes = bytes;
}


/* Set the column data type. */
void
df_set_read_type(int col, df_data_type type)
{
    assert(col > 0);
    assert(type < DF_BAD_TYPE);
    /* Check if we have room at least col columns */
    if (col > df_max_bininfo_cols) {
	df_column_bininfo = gp_realloc(df_column_bininfo,
				       col * sizeof(df_column_bininfo_struct),
				       "datafile columns binary information");
	df_max_bininfo_cols = col;
    }
    df_column_bininfo[col-1].column.read_type = type;
    df_column_bininfo[col-1].column.read_size
	= df_binary_details[type].type.read_size;
}


/* Get the column data type. */
df_data_type
df_get_read_type(int col)
{
    assert(col > 0);
    /* Check if we have room at least col columns */
    if (col < df_max_bininfo_cols)
	return(df_column_bininfo[col].column.read_type);
    else
	return -1;
}


/* Get the binary column data size. */
int
df_get_read_size(int col)
{
    assert(col > 0);
    /* Check if we have room at least col columns */
    if (col < df_max_bininfo_cols)
	return(df_column_bininfo[col].column.read_size);
    else
	return -1;
}


/* If the column number is greater than number of binary columns, set
 * the unitialized columns binary info to that of the last specified
 * column or the default if none were set.  */
void
df_extend_binary_columns(int no_cols)
{
    if (no_cols > df_no_bin_cols) {
	int i;
	df_data_type type;
	if (df_no_bin_cols > 0)
	    type = df_column_bininfo[df_no_bin_cols-1].column.read_type;
	else
	    type = DF_DEFAULT_TYPE;
	for (i = no_cols; i > df_no_bin_cols; i--) {
	    df_set_skip_after(i, 0);
	    df_set_read_type(i, type);
	}
	df_no_bin_cols = no_cols;
    }
}


/* Determine binary data widths from the `using` (or `binary`) format
 * specification. */
void
plot_option_binary_format(char *format_string)
{

    int prev_read_type = DF_DEFAULT_TYPE; /* Defaults when none specified. */
    int no_fields = 0;
    char *substr = format_string;

    while (*substr != '\0' && *substr != '\"' && *substr != '\'') {

	if (*substr == ' ') {
	    substr++;
	    continue;
	}  /* Ignore spaces. */

	if (*substr == '%') {
	    int ignore, field_repeat, j=0, k=0, m=0, breakout;
	    
	    substr++;
	    ignore = (*substr == '*');
	    if(ignore)
		substr++;

	    /* Check for field repeat number. */
	    field_repeat = isdigit((unsigned char)*substr) ? strtol(substr, &substr, 10) : 1;

	    /* Try finding the word among the valid type names. */
	    for (j = 0, breakout = 0;
		 j < (sizeof(df_binary_tables)
		      /sizeof(df_binary_tables[0]));
		 j++) {
		for (k = 0, breakout = 0;
		     k < df_binary_tables[j].group_length;
		     k++) {
		    for (m = 0;
			 m < df_binary_tables[j].group[k].no_names;
			 m++) {
			int strl
			    = strlen(df_binary_tables[j].group[k].name[m]);

			/* Check for exact match, which includes character
			 * after the substring being non-alphanumeric. */
			if (!strncmp(substr,
				     df_binary_tables[j].group[k].name[m],
				     strl)
			    && strchr("%\'\" ", *(substr + strl)) ) {
			    substr += strl;  /* Advance pointer in array to next text. */
			    if (!ignore) {
				int n;
				
				for (n = 0; n < field_repeat; n++) {
				    no_fields++;
				    df_set_skip_after(no_fields, 0);
				    df_set_read_type(no_fields,
					     df_binary_tables[j].group[k].type.read_type);
				    prev_read_type = df_binary_tables[j].group[k].type.read_type;
				}
			    } else {
				if (!df_column_bininfo)
				    int_error(NO_CARET,"Failure in binary table initialization");
				df_column_bininfo[no_fields].skip_bytes
				    += field_repeat * df_binary_tables[j].group[k].type.read_size;
			    }
			    breakout = 1;
			    break;
			}
		    }
		    if (breakout)
			break;
		}
		if (breakout)
		    break;
	    }

	    if (j == (sizeof(df_binary_tables)
		      /sizeof(df_binary_tables[0]))
		&& (k == df_binary_tables[j-1].group_length)
		&& (m == df_binary_tables[j-1].group[k-1].no_names)) {
		int_error(c_token, "Unrecognized binary format specification");
	    }
	} else {
	    int_error(c_token, "Format specifier must begin with '%'");
	}
    }

    /* Any remaining unspecified fields are assumed to be of the same type
     * as the last specified field.
     */
    for ( ; no_fields < df_no_bin_cols; no_fields++) {
	df_set_skip_after(no_fields, 0);
	df_set_skip_before(no_fields, 0);
	df_set_read_type(no_fields, prev_read_type);
    }
    df_no_bin_cols = no_fields;

}


void
df_show_binary(FILE *fp)
{
    int i, num_record;
    df_binary_file_record_struct *bin_record;

    fprintf(fp, "\tDefault binary data file settings (in-file settings may override):\n");

    if (!df_num_bin_records_default) {
	bin_record = &df_bin_record_reset;
	num_record = 1;
    } else {
	bin_record = df_bin_record_default;
	num_record = df_num_bin_records_default;
    }

    fprintf(fp, "\n\t  File Type: ");
    if (df_bin_filetype_default >= 0)
	fprintf(fp, "%s", df_bin_filetype_table[df_bin_filetype_default].key);
    else
	fprintf(fp, "none");

    fprintf(fp, "\n\t  File Endianness: %s",
	    df_endian[df_bin_file_endianess_default]);

    fprintf(fp, "\n\t  Default binary format: %s",
	    df_binary_format ? df_binary_format : "none");

    for (i = 0; i < num_record; i++) {
	int dimension = 1;
	
	fprintf(fp, "\n\t  Record %d:\n", i);
	fprintf(fp, "\t    Dimension: ");
	if (bin_record[i].cart_dim[0] < 0)
	    fprintf(fp, "Inf");
	else {
	    fprintf(fp, "%d", bin_record[i].cart_dim[0]);
	    if (bin_record[i].cart_dim[1] > 0) {
		dimension = 2;
		fprintf(fp, "x%d", bin_record[i].cart_dim[1]);
		if (bin_record[i].cart_dim[2] > 0) {
		    dimension = 3;
		    fprintf(fp, "x%d", bin_record[i].cart_dim[2]);
		}
	    }
	}
	fprintf(fp, "\n\t    Generate coordinates: %s",
		(bin_record[i].scan_generate_coord ? "yes" : "no"));
	if (bin_record[i].scan_generate_coord) {
	    int j;
	    TBOOLEAN no_flip = TRUE;
	    
	    fprintf(fp, "\n\t    Direction: ");
	    if (bin_record[i].cart_dir[0] == -1) {
		fprintf(fp, "flip x");
		no_flip = FALSE;
	    }
	    if ((dimension > 1) && (bin_record[i].cart_dir[1] == -1)) {
		fprintf(fp, "%sflip y", (no_flip ? "" : ", "));
		no_flip = FALSE;
	    }
	    if ((dimension > 2) && (bin_record[i].cart_dir[2] == -1)) {
		fprintf(fp, "%sflip z", (no_flip ? "" : ", "));
		no_flip = FALSE;
	    }
	    if (no_flip)
		fprintf(fp, "all forward");
	    fprintf(fp, "\n\t    Sample periods: dx=%f",
		    bin_record[i].cart_delta[0]);
	    if (dimension > 1)
		fprintf(fp, ", dy=%f", bin_record[i].cart_delta[1]);
	    if (dimension > 2)
		fprintf(fp, ", dz=%f", bin_record[i].cart_delta[2]);
	    if (bin_record[i].cart_trans == DF_TRANSLATE_VIA_ORIGIN)
		fprintf(fp, "\n\t    Origin:");
	    else if (bin_record[i].cart_trans == DF_TRANSLATE_VIA_CENTER)
		fprintf(fp, "\n\t    Center:");
	    if ((bin_record[i].cart_trans == DF_TRANSLATE_VIA_ORIGIN)
		|| (bin_record[i].cart_trans == DF_TRANSLATE_VIA_CENTER))
		fprintf(fp, " (%f, %f, %f)",
			bin_record[i].cart_cen_or_ori[0],
			bin_record[i].cart_cen_or_ori[1],
			bin_record[i].cart_cen_or_ori[2]);
	    fprintf(fp, "\n\t    2D rotation angle: %f",
		    bin_record[i].cart_alpha);
	    fprintf(fp, "\n\t    3D normal vector: (%f, %f, %f)",
		    bin_record[i].cart_p[0],
		    bin_record[i].cart_p[1],
		    bin_record[i].cart_p[2]);
	    for (j = 0;
		 j < (sizeof(df_bin_scan_table_3D)
		      /sizeof(df_bin_scan_table_3D[0]));
		 j++) {
		if (!strncmp((char *)bin_record[i].cart_scan,
			     (char *)df_bin_scan_table_3D[j].scan,
			     sizeof(bin_record[0].cart_scan)) ) {
		    fprintf(fp, "\n\t    Scan: ");
		    fprintf(fp,
			    (bin_record[i].cart_dim[2] ? "%s" : "%2.2s"),
			    df_bin_scan_table_3D[j].string);
		    break;
		}
	    }
	    fprintf(fp, "\n\t    Skip bytes: %lld before record",
		    (long long)bin_record[i].scan_skip[0]);
	    if (dimension > 1)
		fprintf(fp, ", %lld before line",
		    (long long)bin_record[i].scan_skip[1]);
	    if (dimension > 2)
		fprintf(fp, ", %lld before plane",
		    (long long) bin_record[i].scan_skip[2]);
	}
	fprintf(fp, "\n");
    }
}


void
df_show_datasizes(FILE *fp)
{
    int i;

    fprintf(fp,"\tThe following binary data sizes are machine dependent:\n\n"
	    "\t  name (size in bytes)\n\n");
    for (i = 0;
	 i < sizeof(df_binary_details)/sizeof(df_binary_details[0]);
	 i++) {
	int j;
	
	fprintf(fp,"\t  ");
	for (j = 0; j < df_binary_details[i].no_names; j++) {
	    fprintf(fp,"\"%s\" ",df_binary_details[i].name[j]);
	}
	fprintf(fp,"(%d)\n",df_binary_details[i].type.read_size);
    }

    fprintf(fp,"\n\
\tThe following binary data sizes attempt to be machine independent:\n\n\
\t  name (size in bytes)\n\n");
    for (i = 0;
	 i < sizeof(df_binary_details_independent)
	     /sizeof(df_binary_details_independent[0]);
	 i++) {
	int j;
	
	fprintf(fp,"\t  ");
	for (j = 0; j < df_binary_details_independent[i].no_names; j++) {
	    fprintf(fp,"\"%s\" ",df_binary_details_independent[i].name[j]);
	}
	fprintf(fp,"(%d)",df_binary_details_independent[i].type.read_size);
	if (df_binary_details_independent[i].type.read_type == DF_BAD_TYPE)
	    fprintf(fp," -- processor does not support this size");
	fputc('\n', fp);
    }
}


void
df_show_filetypes(FILE *fp)
{
    int i = 0;
    
    fprintf(fp,"\tThis version of gnuplot understands the following binary file types:\n");
    while (df_bin_filetype_table[i].key)
	fprintf(fp, "\t  %s", df_bin_filetype_table[i++].key);
    fputs("\n",fp);
}


void
df_swap_bytes_by_endianess(char *data, int read_order, int read_size)
{
    if ((read_order == DF_3210) 
#if SUPPORT_MIDDLE_ENDIAN
	|| (read_order == DF_2301)
#endif
	) {
	int j = 0;
	int k = read_size - 1;
	
	for (; j < k; j++, k--) {
	    char temp = data[j];
	    
	    data[j] = data[k];
	    data[k] = temp;
	}
    }
    
#if SUPPORT_MIDDLE_ENDIAN
    if ((read_order == DF_1032) || (read_order == DF_2301)) {
	int j= read_size - 1;
	
	for (; j > 0; j -= 2) {
	    char temp = data[j-1];
	    
	    data[j-1] = data[j];
	    data[j] = temp;
	}
    }
#endif
}


static int
df_skip_bytes(off_t nbytes)
{
#if defined(PIPES)
    char cval;

    if (df_pipe_open || plotted_data_from_stdin) {
	while (nbytes--) {
	    if (1 == fread(&cval, 1, 1, data_fp))
		continue;
	    if (feof(data_fp)) {
		df_eof = 1;
		return DF_EOF;
	    }
	    int_error(NO_CARET, read_error_msg);
	}
    } else
#endif
    if (fseek(data_fp, nbytes, SEEK_CUR)) {
	if (feof(data_fp)) {
	    df_eof = 1;
	    return DF_EOF;
	}
	int_error(NO_CARET, read_error_msg);
    }

    return 0;
}


/*{{{  int df_readbinary(v, max) */
/* do the hard work... read lines from file,
 * - use blanks to get index number
 * - ignore lines outside range of indices required
 * - fill v[] based on using spec if given
 */

int
df_readbinary(double v[], int max)
{
    /* For general column structured binary. */
    static int scan_size[3];
    static double delta[3];      /* sampling periods */
    static double o[3];          /* add after rotations */
    static double c[3];          /* subtract before doing rotations */
    static double P[3][3];       /* 3D rotation matrix (perpendicular) */
    static double R[2][2];       /* 2D rotation matrix (rotate) */
    static int read_order;
    static off_t record_skip;
    static TBOOLEAN end_of_scan_line;
    static TBOOLEAN end_of_block;
    static TBOOLEAN translation_required;
    static char *memory_data;

    /* For matrix data structure (i.e., gnuplot binary). */
    static double first_matrix_column;
    static float *scanned_matrix_row = 0;
    static int first_matrix_row_col_count;
    TBOOLEAN saved_first_matrix_column = FALSE;

    assert(max <= MAXDATACOLS);
    assert(df_max_bininfo_cols > df_no_bin_cols);
    assert(df_no_bin_cols);

    /* catch attempt to read past EOF on mixed-input */
    if (df_eof)
	return DF_EOF;

    /* Check if we have room for at least df_no_bin_cols columns */
    if (df_max_cols < df_no_bin_cols)
	expand_df_column(df_no_bin_cols);

    /* In binary mode, the number of user specs was increased by the
     * number of dimensions in the underlying uniformly sampled grid
     * previously.  Fill in those values.  Also, compute elements of
     * formula x' = P*R*(x - c) + o */
    if (!df_M_count && !df_N_count && !df_O_count) {
	int i;
	TBOOLEAN D2, D3;
	df_binary_file_record_struct *this_record
	    = df_bin_record + df_bin_record_count;

	scan_size[0] = scan_size[1] = scan_size[2] = 0;

	D2 = rotation_matrix_2D(R, this_record->cart_alpha);
	D3 = rotation_matrix_3D(P, this_record->cart_p);
	translation_required = D2 || D3;

	if (df_matrix_file) {
	    /* Dimensions */
	    scan_size[0] = this_record->scan_dim[0];
	    scan_size[1] = this_record->scan_dim[1];

	    FPRINTF((stderr,"datafile.c:%d matrix dimensions %d x %d\n",
			__LINE__, scan_size[1], scan_size[0]));
	    df_xpixels = scan_size[1];
	    df_ypixels = scan_size[0];

	    if (scan_size[0] == 0)
		int_error(NO_CARET, "Scan size of matrix is zero");

	    /* To accomplish flipping in this case, multiply the
	     * appropriate column of the rotation matrix by -1.  */
	    for (i = 0; i < 2; i++) {
		int j;
		
		for (j = 0; j < 2; j++) {
		    R[i][j] *= this_record->cart_dir[i];
		}
	    }
	    /* o */
	    for (i = 0; i < 3; i++) {
		if (this_record->cart_trans != DF_TRANSLATE_DEFAULT) {
		    o[i] = this_record->cart_cen_or_ori[i];
		} else {
		    /* Default is translate by center. */
		    if (i < 2)
			o[i] = (df_matrix_corner[1][i]
				+ df_matrix_corner[0][i]) / 2;
		    else
			o[i] = 0;
		}
	    }
	    /* c */
	    for (i = 0; i < 3; i++) {
		if (this_record->cart_trans == DF_TRANSLATE_VIA_ORIGIN) {
		    if (i < 2)
			c[i] = df_matrix_corner[0][i];
		    else
			c[i] = 0;
		} else {
		    if (i < 2)
			c[i] = (df_matrix_corner[1][i]
				+ df_matrix_corner[0][i]) / 2;
		    else
			c[i] = 0;
		}
	    }

	    first_matrix_row_col_count = 0;
	} else { /* general binary */

	  
	    for (i = 0; i < 3; i++) {
		int map;
		
		/* How to direct the generated coordinates in regard
		 * to scan direction */
		if (this_record->cart_dim[i] || this_record->scan_dim[i]) {
		    if (this_record->scan_generate_coord)
			use_spec[i].column = this_record->cart_scan[i];
		}
		/* Dimensions */
		map = DF_SCAN_POINT - this_record->cart_scan[i];
		if (this_record->cart_dim[i] > 0)
		    scan_size[map] = this_record->cart_dim[i];
		else if (this_record->cart_dim[i] < 0)
		    scan_size[map] = MAXINT;
		else
		    scan_size[map] = this_record->scan_dim[map];
		/* Sample periods */
		if (this_record->cart_delta[i])
		    delta[map] = this_record->cart_delta[i];
		else
		    delta[map] = this_record->scan_delta[map];
		delta[map] *= this_record->scan_dir[map] * this_record->cart_dir[i];
		/* o */
		if (this_record->cart_trans != DF_TRANSLATE_DEFAULT)
		    o[i] = this_record->cart_cen_or_ori[i];
		else if (this_record->scan_trans != DF_TRANSLATE_DEFAULT)
		    o[i] = this_record->scan_cen_or_ori[map];
		else if (scan_size[map] > 0)
		    o[i] = (scan_size[map] - 1)*fabs(delta[map])/2;
		else
		    o[i] = 0;
		/* c */
		if (this_record->cart_trans == DF_TRANSLATE_VIA_ORIGIN
		    || (this_record->cart_trans == DF_TRANSLATE_DEFAULT
			&& this_record->scan_trans == DF_TRANSLATE_VIA_ORIGIN)
		    ) {
		    if ((scan_size[map] > 0) && (delta[map] < 0))
			c[i] = (scan_size[map] - 1)*delta[map];
		    else
			c[i] = 0;
		} else {
		    if (scan_size[map] > 0)
			c[i] = (scan_size[map] - 1)*(delta[map]/2);
		    else
			c[i] = 0;
		}
	    }
	}

	/* Check if c and o are the same. */
	for (i = 0; i < 3; i++)
	    translation_required = translation_required || (c[i] != o[i]);

	/* Should data come from memory? */
	memory_data = this_record->memory_data;

	/* byte read order */
	read_order = byte_read_order(df_bin_file_endianess);

	/* amount to skip before first record */
	record_skip = this_record->scan_skip[0];

	end_of_scan_line = FALSE;
	end_of_block = FALSE;
	point_count = -1;
	line_count = 0;
	df_current_index = df_bin_record_count;

	/* Craig DeForest Feb 2013 - Fast version of uniform binary matrix.
	 * Don't apply this to ascii input or special filetypes.
	 * Slurp all data from file or pipe in one shot to minimize fread calls.
	 */
	if (!memory_data && !(df_bin_filetype > 0)
	&&  df_binary_file &&  df_matrix && !df_nonuniform_matrix) {
	    int i;
	    unsigned long int bytes_per_point = 0;
	    unsigned long int bytes_per_line = 0;
	    unsigned long int bytes_per_plane = 0;
	    unsigned long int bytes_total = 0;
	    size_t fread_ret;

	    /* Accumulate total number of bytes in this tuple */
	    for (i=0; i<df_no_bin_cols; i++)
		bytes_per_point +=  
		  df_column_bininfo[i].skip_bytes +
		  df_column_bininfo[i].column.read_size;
	    bytes_per_point += df_column_bininfo[df_no_bin_cols].skip_bytes;
	  
	    bytes_per_line  = bytes_per_point
			    * (  (scan_size[0] > 0) ? scan_size[0] : 1 );
	    bytes_per_plane = bytes_per_line
			    * ( (scan_size[1] > 0) ? scan_size[1] : 1 );
	    bytes_total     = bytes_per_plane
			    * ( (scan_size[2]>0) ? scan_size[2] : 1);
	    bytes_total    += record_skip;

	    /* Allocate a chunk of memory and stuff it */
	    /* EAM FIXME: Is this a leak if the plot errors out? */
	    memory_data = gp_alloc(bytes_total, "df_readbinary slurper");
	    this_record->memory_data = memory_data; 
	 
	    FPRINTF((stderr,"Fast matrix code:\n"));
	    FPRINTF((stderr,"\t\t skip %d bytes, read %ld bytes as %d x %d array\n",
		    record_skip, bytes_total, scan_size[0], scan_size[1]));
 
	    /* Do the actual slurping */
	    fread_ret = fread(memory_data, 1, bytes_total, data_fp);
	    if (fread_ret != bytes_total) {
		int_warn(NO_CARET, "Couldn't slurp %ld bytes (return was %zd)\n",
			bytes_total, fread_ret);
		df_eof = 1;
		return DF_EOF;
	    }
	}
    }

    while (!df_eof) {
	/*{{{  process line */
	int line_okay = 1;
	int output = 0;             /* how many numbers written to v[] */
	int i, fread_ret = 0;
	int m_value, n_value, o_value;
	union io_val {
	    char ch;
	    unsigned char uc;
	    short sh;
	    unsigned short us;
	    int in;
	    unsigned int ui;
	    long lo;
	    unsigned long ul;
	    long long llo;
	    unsigned long long ull;
	    float fl;
	    double db;
	} io_val;

	/* Scan in a number of floats based upon the largest index in
	 * the use_specs array.  If the largest index in the array is
	 * greater than maximum columns then issue an error.
	 */

	/* Handle end of line or end of block on previous read. */
	if (end_of_scan_line) {
	    end_of_scan_line = FALSE;
	    point_count = -1;
	    line_count++;
	    return DF_FIRST_BLANK;
	}
	if (end_of_block) {
	    end_of_block = FALSE;
	    line_count = 0;
	    return DF_SECOND_BLANK;
	}

	/* Possibly skip bytes before starting to read record. */
	if (record_skip) {
	    if (memory_data)
		memory_data += record_skip;
	    else if (df_skip_bytes(record_skip))
		return DF_EOF;
	    record_skip = 0;
	}

	/* Bring in variables as described by the field parameters.
	 * If less than than the appropriate number of bytes have been
	 * read, issue an error stating not enough columns were found.  */
	for (i = 0; ; i++) {
	    off_t skip_bytes = df_column_bininfo[i].skip_bytes;

	    if (skip_bytes) {
		if (memory_data)
		    memory_data += skip_bytes;
		else if (df_skip_bytes(skip_bytes))
		    return DF_EOF;
	    }

	    /* Last entry only has skip bytes, no data. */
	    if (i == df_no_bin_cols)
		break;

	    /* Read in a "column", i.e., a binary value of various types. */
	    if (df_pixeldata) {
		io_val.uc = df_libgd_get_pixel(df_M_count, df_N_count, i);
	    } else

	    if (memory_data) {
		for (fread_ret = 0;
		     fread_ret < df_column_bininfo[i].column.read_size;
		     fread_ret++)
		    (&io_val.ch)[fread_ret] = *memory_data++;
	    } else {
		fread_ret = fread(&io_val.ch,
				  df_column_bininfo[i].column.read_size,
				  1, data_fp);
		if (fread_ret != 1) {
		    df_eof = 1;
		    return DF_EOF;
		}
	    }

	    if (read_order != 0)
		df_swap_bytes_by_endianess(&io_val.ch, read_order,
				       df_column_bininfo[i].column.read_size);

	    switch (df_column_bininfo[i].column.read_type) {
		case DF_CHAR:
		    df_column[i].datum = io_val.ch;
		    break;
		case DF_UCHAR:
		    df_column[i].datum = io_val.uc;
		    break;
		case DF_SHORT:
		    df_column[i].datum = io_val.sh;
		    break;
		case DF_USHORT:
		    df_column[i].datum = io_val.us;
		    break;
		case DF_INT:
		    df_column[i].datum = io_val.in;
		    break;
		case DF_UINT:
		    df_column[i].datum = io_val.ui;
		    break;
		case DF_LONG:
		    df_column[i].datum = io_val.lo;
		    break;
		case DF_ULONG:
		    df_column[i].datum = io_val.ul;
		    break;
		case DF_LONGLONG:
		    df_column[i].datum = io_val.llo;
		    break;
		case DF_ULONGLONG:
		    df_column[i].datum = io_val.ull;
		    break;
		case DF_FLOAT:
		    df_column[i].datum = io_val.fl;
		    break;
		case DF_DOUBLE:
		    df_column[i].datum = io_val.db;
		    break;
		default:
		    int_error(NO_CARET, "Binary data type unknown");
	    }

	    df_column[i].good = DF_GOOD;
	    df_column[i].position = NULL;   /* cant get a time */

	    /* Matrix file data is a special case. After reading in just
	     * one binary value, stop and decide on what to do with it. */
	    if (df_matrix_file)
		break;

	} /* for(i) */

	if (df_matrix_file) {
	    if (df_nonuniform_matrix) {
		/* Store just first column? */
		if (!df_M_count && !saved_first_matrix_column) {
		    first_matrix_column = df_column[i].datum;
		    saved_first_matrix_column = TRUE;
		    continue;
		}

		/* Read reset of first row? */
		if (!df_M_count && !df_N_count && !df_O_count
		    && first_matrix_row_col_count < scan_size[0]) {
		    if (!first_matrix_row_col_count
			&& ! (scanned_matrix_row =
			      gp_realloc(scanned_matrix_row,
					 scan_size[0]*sizeof(float),
					 "gpbinary matrix row")))
			int_error(NO_CARET, "not enough memory to create vector");
		    scanned_matrix_row[first_matrix_row_col_count] = df_column[i].datum;
		    first_matrix_row_col_count++;
		    if (first_matrix_row_col_count == scan_size[0]) {
			/* Start of the second row. */
			saved_first_matrix_column = FALSE;
		    }
		    continue;
		}

	    }

	    /* Update all the binary columns.  Matrix binary and
	     * matrix ASCII is a slight abuse of notation.  At the
	     * command line, 1 means first row, 2 means first
	     * column.  There can only be one column of data input
	     * because it is a matrix of data, not columns.  */
	    {
		int j;

		df_datum = df_column[i].datum;

		/* Fill backward so that current read value is not
		 * overwritten. */
		for (j = df_no_bin_cols-1; j >= 0; j--) {
		    if (j == 0)
			df_column[j].datum = df_nonuniform_matrix ? scanned_matrix_row[df_M_count] : df_M_count;
		    else if (j == 1)
			df_column[j].datum = df_nonuniform_matrix ? first_matrix_column : df_N_count;
		    else
			df_column[j].datum = df_column[i].datum;
		    df_column[j].good = DF_GOOD;
		    df_column[j].position = NULL;
		}
	    }
	} else { /* Not matrix file, general binray. */
	    df_datum = point_count + 1;
	    if (i != df_no_bin_cols) {
		if (feof(data_fp)) {
		    if (i != 0) 
			int_error(NO_CARET, "Last point in the binary file did not match the specified `using` columns");
		    df_eof = 1;
		    return DF_EOF;
		} else {
		    int_error(NO_CARET, read_error_msg);
		}
	    }
	}

	m_value = df_M_count;
	n_value = df_N_count;
	o_value = df_O_count;
	df_M_count++;
	if ((scan_size[0] > 0) && (df_M_count >= scan_size[0])) {
	    /* This is a new "line". */
	    df_M_count = 0;
	    df_N_count++;
	    end_of_scan_line = TRUE;
	    if ((scan_size[1] >= 0) && (df_N_count >= scan_size[1])) {
		/* This is a "block". */
		df_N_count = 0;
		df_O_count++;
		if ((scan_size[2] >= 0) && (df_O_count >= scan_size[2])) {
		    df_O_count = 0;
		    end_of_block = TRUE;
		    if (++df_bin_record_count >= df_num_bin_records) {
			df_eof = 1;
		    }
		}
	    }
	}

	/*{{{  ignore points outside range of index */
	/* we try to return end-of-file as soon as we pass upper
	 * index, but for mixed input stream, we must skip garbage */

	if (df_current_index < df_lower_index
	    || df_current_index > df_upper_index
	    || ((df_current_index - df_lower_index) % df_index_step) != 0)
	    continue;
	/*}}} */

	/*{{{  reject points by every */
	/* accept only lines with (line_count%everyline) == 0 */
	if (line_count < firstline
	    || line_count > lastline
	    || (line_count - firstline) % everyline != 0)
	    continue;

	/* update point_count. ignore point if
	   point_count%everypoint != 0 */
	if (++point_count < firstpoint
	    || point_count > lastpoint
	    || (point_count - firstpoint) % everypoint != 0)
	    continue;
	/*}}} */

	/* At this point the binary columns have been read
	 * successfully.  Set df_no_cols to df_no_bin_cols for use
	 * in the interpretation code.  */
	df_no_cols = df_no_bin_cols;

	/*{{{  copy column[] to v[] via use[] */
	{
	    int limit = (df_no_use_specs ? df_no_use_specs : MAXDATACOLS);
		
	    if (limit > max)
		limit = max;

	    for (output = 0; output < limit; ++output) {
		int column = use_spec[output].column;

		/* if there was no using spec, column is output+1 and at=NULL */
		if (use_spec[output].at) {
		    struct value a;
			
		    /* no dummy values to set up prior to... */
		    evaluate_inside_using = TRUE;
		    evaluate_at(use_spec[output].at, &a);
		    evaluate_inside_using = FALSE;
		    if (undefined)
			return DF_UNDEFINED; 

		    if (a.type == STRING) {
			if (use_spec[output].expected_type == CT_STRING) {
			    char *s = gp_alloc(strlen(a.v.string_val)+3,"quote");
			    *s = '"';
			    strcpy(s+1, a.v.string_val);
			    strcat(s, "\"");
			    free(df_stringexpression[output]);
			    df_tokens[output] = df_stringexpression[output] = s;
			}
			gpfree_string(&a);
		    }
		    else
			v[output] = real(&a);

		} else if (column == DF_SCAN_PLANE) {
		    if ((df_current_plot->plot_style == IMAGE)
		    ||  (df_current_plot->plot_style == RGBIMAGE))
			v[output] = o_value*delta[2];
		    /* EAM August 2009
		     * This was supposed to be "z" in a 3D grid holding a binary
		     * value at each voxel.  But in fact the binary code does not
		     * support 3D grids, only 2D. So this always got set to 0,
		     * making the whole thing pretty useless except for inherently.
		     * planar objects like 2D images.
		     * Now I set Z to be the pixel value, which allows you
		     * to draw surfaces described by a 2D binary array.
		     */ 
		    else
			v[output] = df_column[0].datum;
		} else if (column == DF_SCAN_LINE) {
		    v[output] = n_value*delta[1];
		} else if (column == DF_SCAN_POINT) {
		    v[output] = m_value*delta[0];
		} else if (column == -2) {
		    v[output] = df_current_index;
		} else if (column == -1) {
		    v[output] = line_count;
		} else if (column == 0) {
		    v[output] = df_datum;
		} else if (column <= 0) {
		    int_error(NO_CARET, "internal error: unknown column type");

		/* July 2010 - We used to have special code to handle time data. */
		/* But time data in a binary file is just one more binary value, */
		/* so let the general case code handle it.                       */
		} else if ((column <= df_no_cols)
			   && df_column[column - 1].good == DF_GOOD)
		    v[output] = df_column[column - 1].datum;
			
		/* EAM - Oct 2002 Distinguish between DF_MISSING
		 * and DF_BAD.  Previous versions would never
		 * notify caller of either case.  Now missing data
		 * will be noted. Bad data should arguably be
		 * noted also, but that would change existing
		 * default behavior.  */
		else if ((column <= df_no_cols)
			 && (df_column[column - 1].good == DF_MISSING))
		    return DF_MISSING;
		else {
		    /* line bad only if user explicitly asked for this column */
		    if (df_no_use_specs)
			line_okay = 0;
		    break;  /* return or ignore depending on line_okay */
		}

		if (isnan(v[output])) {
			/* EAM April 2012 - return, continue, or ignore??? */
			FPRINTF((stderr,"NaN input value"));
			if (!df_matrix)
				return DF_UNDEFINED;
		}

	    }

	    /* Linear translation. */
	    if (translation_required) {
		double x, y, z;

		x = v[0] - c[0];
		y = v[1] - c[1];

		v[0] = R[0][0] * x + R[0][1] * y;
		v[1] = R[1][0] * x + R[1][1] * y;
		if (df_plot_mode == MODE_SPLOT) {
		    x = v[0];
		    y = v[1];
		    z = v[2] - c[2];
		    v[0] = P[0][0] * x + P[0][1] * y + P[0][2] * z;
		    v[1] = P[1][0] * x + P[1][1] * y + P[1][2] * z;
		    v[2] = P[2][0] * x + P[2][1] * y + P[2][2] * z;
		}

		v[0] += o[0];
		v[1] += o[1];
		if (df_plot_mode == MODE_SPLOT)
		    v[2] += o[2];
	    }

	}
	/*}}} */

	if (!line_okay)
	    continue;

	for (i=df_no_use_specs; i<df_no_use_specs+df_no_tic_specs; i++) {
	    if (use_spec[i].expected_type >= CT_XTICLABEL
	    &&  use_spec[i].at != NULL) {
		struct value a;
		int axis, axcol;
		evaluate_inside_using = TRUE;
		evaluate_at(use_spec[i].at, &a);
		evaluate_inside_using = FALSE;
		switch (use_spec[i].expected_type) {
		    default:
		    case CT_XTICLABEL:
			axis = FIRST_X_AXIS;
			axcol = 0;
			break;
		    case CT_X2TICLABEL:
			axis = SECOND_X_AXIS;
			axcol = 0;
			break;
		    case CT_YTICLABEL:
			axis = FIRST_Y_AXIS;
			axcol = 1;
			break;
		    case CT_Y2TICLABEL:
			axis = SECOND_Y_AXIS;
			axcol = 1;
			break;
		    case CT_ZTICLABEL:
			axis = FIRST_Z_AXIS;
			axcol = 2;
			break;
		    case CT_CBTICLABEL:
			/* EAM FIXME - Which column to set for cbtic? */
			axis = COLOR_AXIS;
			axcol = 2;
			break;
		}
		if (a.type == STRING) {
		    add_tic_user(axis, a.v.string_val, v[axcol], -1);
		    gpfree_string(&a);
		}
	    }
	}

	/* output == df_no_use_specs if using was specified -
	 * actually, smaller of df_no_use_specs and max */
	assert(df_no_use_specs == 0
	       || output == df_no_use_specs
	       || output == max);

	return output;

    }
    /*}}} */

    df_eof = 1;
    return DF_EOF;

}

void
df_set_plot_mode(int mode)
{
    df_plot_mode = mode;
}

/* Special pseudofile '+' returns x coord of sample for 2D plots,
 * Special pseudofile '++' returns x and y coordinates of grid for 3D plots.
 */
static char *
df_generate_pseudodata()
{
    /* Pseudofile '+' returns a set of (samples) x coordinates */
    /* This code copied from that in second pass through eval_plots() */
    if (df_pseudodata == 1) {
	static double t, t_min, t_max, t_step;
	if (df_pseudorecord >= samples_1)
	    return NULL;
	if (df_pseudorecord == 0) {
	    if ((axis_array[SAMPLE_AXIS].range_flags & RANGE_SAMPLED)) {
		t_min = axis_array[SAMPLE_AXIS].min;
		t_max = axis_array[SAMPLE_AXIS].max;
		/* FIXME:  Do we need to handle log-scaled SAMPLE_AXIS? */
	    } else if (parametric || polar) {
		t_min = axis_array[T_AXIS].min;
		t_max = axis_array[T_AXIS].max;
	    } else {
		if (axis_array[FIRST_X_AXIS].max == -VERYLARGE)
		    axis_array[FIRST_X_AXIS].max = 10;
		if (axis_array[FIRST_X_AXIS].min == VERYLARGE)
		    axis_array[FIRST_X_AXIS].min = -10;
		t_min = X_AXIS.min;
		t_max = X_AXIS.max;
		axis_unlog_interval(x_axis, &t_min, &t_max, 1);
	    }
	    t_step = (t_max - t_min) / (samples_1 - 1);
	}
	t = t_min + df_pseudorecord * t_step;
	/* FIXME:  Is it safe to assume SAMPLE_AXIS and x_axis are distinct? */
	if (!parametric && !(axis_array[SAMPLE_AXIS].range_flags & RANGE_SAMPLED))
	    t = AXIS_DE_LOG_VALUE(x_axis, t);
	if (df_current_plot && df_current_plot->sample_var)
	    Gcomplex(&(df_current_plot->sample_var->udv_value), t, 0.0);
	df_pseudovalue_0 = t;
	sprintf(df_line,"%g",t);
	++df_pseudorecord;
    }

    /* Pseudofile '++' returns a (samples X isosamples) grid of x,y coordinates */
    /* This code copied from that in second pass through eval_3dplots */
    if (df_pseudodata == 2) {
	static double u_min, u_max, u_step, v_min, v_max, v_isostep;
	static int nusteps, nvsteps;
	double u, v;
	AXIS_INDEX u_axis = FIRST_X_AXIS;
	AXIS_INDEX v_axis = FIRST_Y_AXIS;

	/* Fill in the static variables only once per plot */
	if (df_pseudospan == 0 && df_pseudorecord == 0) {
	    if (samples_1 < 2 || samples_2 < 2 || iso_samples_1 < 2 || iso_samples_2 < 2)
		int_error(NO_CARET, "samples or iso_samples < 2. Must be at least 2.");
	    if (parametric) {
		u_min = axis_array[U_AXIS].min;
		u_max = axis_array[U_AXIS].max;
		v_min = axis_array[V_AXIS].min;
		v_max = axis_array[V_AXIS].max;
	    } else {
		axis_checked_extend_empty_range(FIRST_X_AXIS, "x range is invalid");
		axis_checked_extend_empty_range(FIRST_Y_AXIS, "y range is invalid");
		u_min = axis_log_value_checked(u_axis, axis_array[u_axis].min, "x range");
		u_max = axis_log_value_checked(u_axis, axis_array[u_axis].max, "x range");
		v_min = axis_log_value_checked(v_axis, axis_array[v_axis].min, "y range");
		v_max = axis_log_value_checked(v_axis, axis_array[v_axis].max, "y range");
	    }

	    if (hidden3d) {
		 u_step = (u_max - u_min) / (iso_samples_1 - 1);
		 nusteps = iso_samples_1;
	    } else {
		 u_step = (u_max - u_min) / (samples_1 - 1);
		 nusteps = samples_1;
	    }
	    v_isostep = (v_max - v_min) / (iso_samples_2 - 1);
	    nvsteps = iso_samples_2;
	}

	/* wrap at end of each line */
	if (df_pseudorecord >= nusteps) {
	    df_pseudorecord = 0;
	    if (++df_pseudospan >= nvsteps)
		return NULL;
	    else
		return ""; /* blank record for end of scan line */
	}

	/* Duplicate algorithm from calculate_set_of_isolines() */
	u = u_min + df_pseudorecord * u_step;
	v = v_max - df_pseudospan * v_isostep;

	/* Round-off error is most visible at the border */
	if (df_pseudorecord == nusteps-1)
	    u = u_max;
	if (df_pseudospan == nvsteps-1)
	    v = v_min;

	if (parametric) {
	    df_pseudovalue_0 = u;
	    df_pseudovalue_1 = v;
	} else {
	    df_pseudovalue_0 = AXIS_DE_LOG_VALUE(u_axis,u);
	    df_pseudovalue_1 = AXIS_DE_LOG_VALUE(v_axis,v);
	}
	sprintf(df_line,"%g %g", df_pseudovalue_0, df_pseudovalue_1);
	++df_pseudorecord;
    }

    return df_line;
}

/* Allocate space for more data columns as needed */
void
expand_df_column(int new_max)
{
    df_column = gp_realloc(df_column,
			new_max * sizeof(df_column_struct),
			"datafile column");
    for (; df_max_cols < new_max; df_max_cols++) {
	df_column[df_max_cols].datum = 0;
	df_column[df_max_cols].header = NULL;
	df_column[df_max_cols].position = NULL;
    }
}

/* Clear column headers stored for previous plot */
void
clear_df_column_headers()
{
    int i;
    for (i=0; i<df_max_cols; i++) {
	free(df_column[i].header);
	df_column[i].header = NULL;
    }
    df_longest_columnhead = 0;
}
