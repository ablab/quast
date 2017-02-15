/*
 * $Id: boundary.h,v 1.2 2014/03/19 17:30:33 sfeam Exp $
 */

#ifndef GNUPLOT_BOUNDARY_H
# define GNUPLOT_BOUNDARY_H
#include "syscfg.h"

void boundary __PROTO((struct curve_points *plots, int count));
void do_key_bounds __PROTO((legend_key *key));
void do_key_layout __PROTO((legend_key *key));
int find_maxl_keys __PROTO((struct curve_points *plots, int count, int *kcnt));
void do_key_sample __PROTO((struct curve_points *this_plot,
			   legend_key *key, char *title,  int xl, int yl));
void do_key_sample_point __PROTO((struct curve_points *this_plot,
			   legend_key *key, int xl, int yl));
void draw_titles __PROTO((void));
void draw_key __PROTO((legend_key *key, TBOOLEAN key_pass, int *xl, int *yl));

/* Probably some of these could be made static */
extern int key_entry_height;
extern int key_point_offset;
extern int key_col_wth, yl_ref;
extern int ylabel_x, y2label_x, xlabel_y, x2label_y;
extern int ylabel_y, y2label_y, xtic_y, x2tic_y, ytic_x, y2tic_x;
extern int key_cols, key_rows;

#endif /* GNUPLOT_BOUNDARY_H */
