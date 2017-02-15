#ifndef lint
static char *RCSid() { return RCSid("$Id: doc2tex.c,v 1.26 2014/03/22 19:35:21 sfeam Exp $"); }
#endif

/* GNUPLOT - doc2tex.c */

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
 * doc2tex.c  -- program to convert Gnuplot .DOC format to LaTeX document
 * Also will work for VMS .HLP files.
 * Modified by Russell Lang from hlp2ms.c by Thomas Williams
 * Extended by David Kotz to support quotes ("), backquotes, tables.
 * Extended by Jens Emmerich to handle '_', '---', paired single
 * quotes. Changed "-handling. Added pre/post-verbatim hooks.
 *
 *
 * usage:  doc2tex [file.doc [file.tex]]
 *
 *   where file.doc is a Gnuplot .DOC file, and file.tex will be an
 *     article document suitable for printing with LaTeX.
 *
 * typical usage for GNUPLOT:
 *
 *   doc2tex gnuplot.doc gnuplot.tex
 *   latex gnuplot.tex ; latex gnuplot.tex
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "syscfg.h"
#include "stdfn.h"
#include "doc2x.h"

void init __PROTO((FILE *));
void convert __PROTO((FILE *, FILE *));
void process_line __PROTO((char *, FILE *));
void section __PROTO((char *, FILE *));
void puttex __PROTO((char *, FILE *));
void finish __PROTO((FILE *));

static TBOOLEAN intable = FALSE;
static TBOOLEAN verb = FALSE;
static TBOOLEAN see = FALSE;
static TBOOLEAN inhref = FALSE;
static TBOOLEAN figures = FALSE;

int
main (int argc, char **argv)
{
    FILE *infile;
    FILE *outfile;

    int inarg = 1;

    infile = stdin;
    outfile = stdout;

    if (argc > 1 && !strcmp(argv[1],"-figures")) {
	figures = TRUE;
	inarg = 2;
    }

    if (argc > (figures ? 4 : 3)) {
	fprintf(stderr, "Usage: %s [-figures] [infile [outfile]]\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    if (argc > inarg) {
	if ((infile = fopen(argv[inarg], "r")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for reading\n",
		    argv[0], argv[inarg]);
	    exit(EXIT_FAILURE);
	}
    }
    if (argc == inarg+2) {
	if ((outfile = fopen(argv[inarg+1], "w")) == (FILE *) NULL) {
	    fprintf(stderr, "%s: Can't open %s for writing\n",
		    argv[0], argv[inarg+1]);
	    exit(EXIT_FAILURE);
	}
    }
    init(outfile);
    convert(infile, outfile);
    finish(outfile);
    return EXIT_SUCCESS;
}


void
init(FILE *b)
{
    (void) fputs("\\input{titlepag.tex}\n", b);
}


void
convert(FILE *a, FILE *b)
{
    static char line[MAX_LINE_LEN+1];

    while (get_line(line, sizeof(line), a))
	process_line(line, b);

}

void
process_line( char *line, FILE *b)
{
    char string[MAX_LINE_LEN+1], c;
    int i, initlen;
    char *ind;
    static TBOOLEAN parsed = FALSE;

    initlen = strlen(line);
    switch (line[0]) {		/* control character */
    case '?':			/* interactive help entry */
                                /* convert '?xxx' to '\label{xxx}' */
	    line[strlen(line)-1]=NUL;
            (void) fputs("\\label{",b);
	    fputs(line+1, b);
            (void) fputs("}\n",b);
	    if (!strpbrk(line+1," ")) {	/* Make an index entry also */
		(void) fputs("\\index{",b);
		while ((ind = strpbrk(line+1,"-_")))
		    *ind = ' ';
		fputs(line+1, b);
		(void) fputs("}\n",b);
	    }
	    break;		/* ignore */ /* <- don't ignore */

    case '=':			/* explicit index entry */
	    line[strlen(line)-1]=NUL;
	    (void) fputs("\\index{",b);
	    while ((ind = strpbrk(line+1,"-_")))
		*ind = ' ';
	    fputs(line+1, b);
	    (void) fputs("}\n",b);
	    break;

    case 'F':			/* embedded figure */
	    if (figures) {
		line[strlen(line)-1]=NUL;
		(void) fputs("\\parpic[r][rt]{\\includegraphics[width=3in,keepaspectratio]{",b);
		fputs(line+1, b);
		(void) fputs("}}\n",b);
	    }
	    break;

    case '@':{			/* start/end table */
	    if (intable) {
		(void) fputs("\\hline\n\\end{tabular}\n", b);
		(void) fputs("\\end{center}\n", b);
		intable = FALSE;
	    } else {
		if (verb) {
		    (void) fputs("\\end{verbatim}\n", b);
		    (void) fputs("\\postverbatim\n", b);
		    verb = FALSE;
		}
		(void) fputs("\n\\begin{center}\n", b);
		/* moved to gnuplot.doc by RCC
		   (void) fputs("\\begin{tabular}{|ccl|} \\hline\n", b);
		 */
		intable = TRUE;
	    }
	    /* ignore rest of line */
	    break;
	}
    case '#':{			/* latex table entry */
	    if (intable)
		(void) fputs(line + 1, b);	/* copy directly */
	    else {
		/* Itemized list outside of table */
		if (line[1] == 's')
		    (void) fputs("\\begin{itemize}\\setlength{\\itemsep}{0pt}\n", b);
		else if (line[1] == 'e')
		    (void) fputs("\\end{itemize}\n", b);
		else {
		    if (strchr(line, '\n'))
			*(strchr(line, '\n')) = '\0';
		    fprintf(b, "\\item\n\\begin{verbatim}%s\\end{verbatim}\n", line + 1);
		}
	    }
	    break;
	}
    case '^':{			/* external link escape */
                                /* internal link escape */
             /* convert '^ <a href="xxx">yyy</a>' to '\href{xxx}{yyy}' */
	     /* convert '^ <a href="#xxx"></a> to '\ref{xxx}' */
	     /* convert '^ <a name="xxx"></a> to '\label{xxx}' */
            switch (line[3]) {
            case 'a':{
                    switch (line[5]) {
                    case 'h':{
	                    if (line[11] == '#') {
                                fputs("{\\bf ",b);
                                parsed = 0;
		                for (i = 12; (c = line[i]) != '"'; i++) {
                                    string[i-12] = c;
                                }
                                string[i-12]= NUL;
                                i++;i++;
                                for ( ; i < initlen-5; i++) {
                                     fputc(line[i],b);
                                }
                                fputs(" (p.~\\pageref{",b);
                                fputs(string,b);
                                fputs("})}} ",b);
                                inhref = FALSE;
                            } else {
	                        inhref = TRUE;
                                if (strstr(line,"</a>") == NULL){
                                   fputs("\\par\\hskip2.7em\\href{",b);
                                } else {
                                   fputs("\\href{",b);
                                }
	                        parsed = 0;
                                for (i = 11; i < initlen-1 ; i++){
                                    c = line[i];
                                    if (c == '"') {
                                         ;
                                    } else if ( c == '>' && parsed == 0) {
                                        fputs("}{\\tt ",b); parsed = 1;
                                    } else if ( c == '~') {
                                        fputs("\\~",b);
                                    } else if ( c == '_' && parsed == 1) {
                                        fputs("\\_",b);
                                    } else if ( c == '<' && parsed == 1) {
		                        fputs("}{\n",b);
                                        i += 5;
                                        inhref = FALSE;
                                    } else {
                                        fputc(c,b);
                                    }
                                }
		            }
                            break;
		    }
                    case 'n': {
                            fputs("\\label{",b);
		            for (i = 11; (c = *(line +i)) != '"'; i++) {
                                 fputc(c,b);
                            }
                            fputs("}\n",b);
	                    break;
		       }
	            default:
                            break;
		    }
                    break;
    	       }
            case '/':
		    if ( line[4] == 'a') {
		        fputs("}\n\n",b);
                        inhref = FALSE;
                    }
		    break;
            default:
   	            break;		/* ignore */
	    }
            break;
        }
    case '%':{			/* troff table entry */
	    break;		/* ignore */
	}
    case '\n':			/* empty text line */
    case ' ':{			/* normal text line */
	    if (intable)
		break;		/* ignore while in table */
            if ( inhref == TRUE){
                 puttex(line+1,b);
                 break;
            }
	    if (line[1] == ' ') {
		/* verbatim mode */
		if (!verb) {
		    (void) fputs("\\preverbatim\n", b);
		    (void) fputs("\\begin{verbatim}\n", b);
		    verb = TRUE;
		}
		(void) fputs(line + 2, b);
	    } else {
		if (verb) {
		    (void) fputs("\\end{verbatim}\n", b);
		    (void) fputs("\\postverbatim\n", b);
		    verb = FALSE;
		}
		if (line[0] == '\n')
		    puttex(line, b);	/* handle totally blank line */
		else
		    puttex(line + 1, b);
	    }
	    break;
	}
    default:{
	    if (isdigit((int) line[0])) {	/* start of section */
		if (!intable) {	/* ignore while in table */
		    if (line[0] == '1')
			fputs("\\newpage", b);
		    section(line, b);
		}
	    } else
		fprintf(stderr, "unknown control code '%c' in column 1\n",
			line[0]);
	    break;
	}
    }
}

/* process a line with a digit control char */
/* starts a new [sub]section */

void
section(char *line, FILE *b)
{
    static char string[MAX_LINE_LEN+1];
    int sh_i;

    if (verb) {
	(void) fputs("\\end{verbatim}\n", b);
	(void) fputs("\\postverbatim\n", b);
	verb = FALSE;
    }
    (void) sscanf(line, "%d %[^\n]s", &sh_i, string);
    switch (sh_i) {
    case 1:
	(void) fprintf(b, "\\part{");
	break;
    case 2:
	(void) fprintf(b, "\\section*{");
	break;
    case 3:
	(void) fprintf(b, "\\subsection*{");
	break;
    case 4:
	(void) fprintf(b, "\\subsubsection*{");
	break;
    case 5:
	(void) fprintf(b, "\\paragraph*{");
	break;
    case 6:
	(void) fprintf(b, "\\subparagraph{");
	break;
    default:
        break;

    }
    if (islower((int) string[0]))
	string[0] = toupper(string[0]);
    puttex(string, b);
    (void) fprintf(b, "}\n");

    switch (sh_i) {
    case 2:
	(void) fprintf(b, "\\addcontentsline{toc}{section}{");
	puttex(string, b);
	(void) fprintf(b, "}\n");
	break;
    case 3:
	(void) fprintf(b, "\\addcontentsline{toc}{subsection}{");
	puttex(string, b);
	(void) fprintf(b, "}\n");
	break;
    case 4:
	(void) fprintf(b, "\\addcontentsline{toc}{subsubsection}{");
	puttex(string, b);
	(void) fprintf(b, "}\n");
	break;
    case 5:
	(void) fprintf(b, "\\addcontentsline{toc}{paragraph}{");
	puttex(string, b);
	(void) fprintf(b, "}\n");
	break;
    default:
        break;

    }

}

/* put text in string str to file while buffering special TeX characters */
void
puttex( char *str, FILE *file)
{
    register char ch;
    char string[MAX_LINE_LEN+1], c;
    static TBOOLEAN inquote = FALSE;
    int i;

    while ((ch = *str++) != NUL) {
	switch (ch) {
	case '#':
	case '$':
	case '%':
	case '&':
	case '{':
	case '}':
	    (void) fputc('\\', file);
	    (void) fputc(ch, file);
	    break;
	case '\\':
	    (void) fputs("$\\backslash$", file);
	    break;
	case '~':
	    (void) fputs("\\~{\\ }", file);
	    break;
	case '^':
	    (void) fputs("\\verb+^+", file);
	    break;
    	case '>':
    	case '<':
	case '|':
	    (void) fputc('$', file);
	    (void) fputc(ch, file);
	    (void) fputc('$', file);
	    break;
	case '"':
	    (void) fputs("{\\tt\"}", file);
	    break;
	case '\'':
	    if (*str == '\'') {
		(void) fputs("{'\\,'}", file);
		str++;
	    } else {
		(void) fputc(ch, file);
	    }
	    break;
	case '-':
	    if ((*str == '-') && (*(str + 1) == '-')) {
		(void) fputs(" --- ", file);
		str += 2;
	    } else {
		(void) fputc(ch, file);
	    }
	    break;
	case '`':    /* backquotes mean boldface */
	    if (inquote) {
                if (see){
		    char *index = string;
		    char *s;
                    (void) fputs(" (p.~\\pageref{", file);
                    (void) fputs(string, file);
		    (void) fputs("})", file);
#ifndef NO_CROSSREFS
		    /* Make the final word an index entry also */
		    fputs("\\index{",file);
#if 0
		    /* Aug 2006: no need to split index words at - or _ */
		    if (strrchr(index,'-'))
			index = strrchr(index,'-')+1;
		    if (strrchr(index,'_'))
			index = strrchr(index,'_')+1;
#endif
		    if (strrchr(index,' '))
			index = strrchr(index,' ')+1;
		    while ((s = strchr(index,'_')) != NULL) /* replace _ by space */
			*s = ' ';
		    fputs(index,file);
		    fputs("}",file);
#endif
                    /* see = FALSE; */
                }
                (void) fputs("}", file);
		inquote = FALSE;
	    } else {
		(void) fputs("{\\bf ", file);
		for (i=0; i<MAX_LINE_LEN && ((c=str[i]) != '`') ; i++){
                    string[i] = c;
                }
		string[i] = NUL;
		inquote = TRUE;
	    }
	    break;
	case '_':		/* emphasised text ? */
	    for (i = 0; isalpha((int) (*(str + i))); i++);
	    if ((i > 0) && (*(str + i) == '_') &&
		           isspace((int) (*(str + i + 1)))) {
		(void) fputs("{\\em ", file);
		for (; *str != '_'; str++) {
		    (void) fputc(*str, file);
		}
		str++;
		(void) fputs("\\/}", file);
	    } else {
		(void) fputs("\\_", file);
	    }
	    break;
        case 's':    /* find backquote after 'see' {see `} */
        case 'S':
            (void) fputc(ch, file);
	    if ( str[0] == 'e' && str[1] == 'e' && isspace(str[2])){
                see = TRUE;
            }
            break;
        case ')':
        case '.':
            see = FALSE;
	default:
	    (void) fputc(ch, file);
	    break;
	}
    }
}


void finish(FILE *b)
{
    (void) fputs("\\part{Index}\n", b);
    (void) fputs("\\printindex\n", b);
    (void) fputs("\\end{document}\n", b);
}

