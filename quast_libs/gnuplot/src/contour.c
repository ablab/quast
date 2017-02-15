#ifndef lint
static char *RCSid() { return RCSid("$Id: contour.c,v 1.33 2014/04/02 22:43:39 sfeam Exp $"); }
#endif

/* GNUPLOT - contour.c */

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
 * AUTHORS
 *
 *   Original Software:
 *       Gershon Elber
 *
 *   Improvements to the numerical algorithms:
 *        Hans-Martin Keller, 1995,1997 (hkeller@gwdg.de)
 *
 */

#include "contour.h"

#include "alloc.h"
#include "axis.h"
/*  #include "setshow.h" */

/* exported variables (to be handled by the 'set' and friends): */

char contour_format[32] = "%8.3g";	/* format for contour key entries */
t_contour_kind contour_kind = CONTOUR_KIND_LINEAR;
t_contour_levels_kind contour_levels_kind = LEVELS_AUTO;
int contour_levels = DEFAULT_CONTOUR_LEVELS;
int contour_order = DEFAULT_CONTOUR_ORDER;
int contour_pts = DEFAULT_NUM_APPROX_PTS;

/* storage for z levels to draw contours at */
dynarray dyn_contour_levels_list;

/* position of edge in mesh */
typedef enum en_edge_position {
    INNER_MESH=1,
    BOUNDARY,
    DIAGONAL
} t_edge_position;


/* FIXME HBB 2000052: yet another local copy of 'epsilon'. Why? */
#define EPSILON  1e-5		/* Used to decide if two float are equal. */

#ifndef TRUE
#define TRUE     -1
#define FALSE    0
#endif

#define MAX_POINTS_PER_CNTR 	100

#define SQR(x)  ((x) * (x))

typedef struct edge_struct {
    struct poly_struct *poly[2]; /* Each edge belongs to up to 2 polygons */
    struct coordinate GPHUGE *vertex[2]; /* The two extreme points of this edge. */
    struct edge_struct *next;	/* To chain lists */
    TBOOLEAN is_active;		/* is edge is 'active' at certain Z level? */
    t_edge_position position;	/* position of edge in mesh */
} edge_struct;

typedef struct poly_struct {
    struct edge_struct *edge[3];	/* As we do triangolation here... */
    struct poly_struct *next;	/* To chain lists. */
} poly_struct;

/* Contours are saved using this struct list. */
typedef struct cntr_struct {		
    double X, Y;		/* The coordinates of this vertex. */
    struct cntr_struct *next;	/* To chain lists. */
} cntr_struct;

static struct gnuplot_contours *contour_list = NULL;
static double crnt_cntr[MAX_POINTS_PER_CNTR * 2];
static int crnt_cntr_pt_index = 0;
static double contour_level = 0.0;

/* Linear, Cubic interp., Bspline: */
static t_contour_kind interp_kind = CONTOUR_KIND_LINEAR;

static double x_min, y_min, z_min;	/* Minimum values of x, y, and z */
static double x_max, y_max, z_max;	/* Maximum values of x, y, and z */

static void add_cntr_point __PROTO((double x, double y));
static void end_crnt_cntr __PROTO((void));
static void gen_contours __PROTO((edge_struct *p_edges, double z_level,
				  double xx_min, double xx_max,
				  double yy_min, double yy_max));
static int update_all_edges __PROTO((edge_struct *p_edges,
				     double z_level));
static cntr_struct *gen_one_contour __PROTO((edge_struct *p_edges,
					     double z_level,
					     TBOOLEAN *contr_isclosed,
					     int *num_active));
static cntr_struct *trace_contour __PROTO((edge_struct *pe_start,
					   double z_level,
					   int *num_active,
					   TBOOLEAN contr_isclosed));
static cntr_struct *update_cntr_pt __PROTO((edge_struct *p_edge,
					    double z_level));
static int fuzzy_equal __PROTO((cntr_struct *p_cntr1,
				cntr_struct *p_cntr2));


static void gen_triangle __PROTO((int num_isolines,
				  struct iso_curve *iso_lines,
				  poly_struct **p_polys,
				  edge_struct **p_edges));
static void calc_min_max __PROTO((int num_isolines,
				  struct iso_curve *iso_lines,
				  double *xx_min, double *yy_min,
				  double *zz_min,
				  double *xx_max, double *yy_max,
				  double *zz_max));
static edge_struct *add_edge __PROTO((struct coordinate GPHUGE *point0,
					     struct coordinate GPHUGE *point1,
					     edge_struct
					     **p_edge,
					     edge_struct **pe_tail));
static poly_struct *add_poly __PROTO((edge_struct *edge0,
					     edge_struct *edge1,
					     edge_struct *edge2,
					     poly_struct **p_poly,
					     poly_struct **pp_tail));

static void put_contour __PROTO((cntr_struct *p_cntr,
				 double xx_min, double xx_max,
				 double yy_min, double yy_max,
				 TBOOLEAN contr_isclosed));
static void put_contour_nothing __PROTO((cntr_struct *p_cntr));
static int chk_contour_kind __PROTO((cntr_struct *p_cntr,
				     TBOOLEAN contr_isclosed));
static void put_contour_cubic __PROTO((cntr_struct *p_cntr,
				       double xx_min, double xx_max,
				       double yy_min, double yy_max,
				       TBOOLEAN contr_isclosed));
static void put_contour_bspline __PROTO((cntr_struct *p_cntr,
					 TBOOLEAN contr_isclosed));
static void free_contour __PROTO((cntr_struct *p_cntr));
static int count_contour __PROTO((cntr_struct *p_cntr));
static int gen_cubic_spline __PROTO((int num_pts, cntr_struct *p_cntr,
				     double d2x[], double d2y[],
				     double delta_t[],
				     TBOOLEAN contr_isclosed,
				     double unit_x, double unit_y));
static void intp_cubic_spline __PROTO((int n, cntr_struct *p_cntr,
				       double d2x[], double d2y[],
				       double delta_t[], int n_intpol));
static int solve_cubic_1 __PROTO((tri_diag m[], int n));
static void solve_cubic_2 __PROTO((tri_diag m[], double x[], int n));
static void gen_bspline_approx __PROTO((cntr_struct *p_cntr,
					int num_of_points, int order,
					TBOOLEAN contr_isclosed));
static void eval_bspline __PROTO((double t, cntr_struct *p_cntr,
				  int num_of_points, int order, int j,
				  TBOOLEAN contr_isclosed, double *x,
				  double *y));
static double fetch_knot __PROTO((TBOOLEAN contr_isclosed, int num_of_points,
				  int order, int i));

/*
 * Entry routine to this whole set of contouring module.
 */
struct gnuplot_contours *
contour(int num_isolines, struct iso_curve *iso_lines)
{
    int i;
    int num_of_z_levels;	/* # Z contour levels. */
    poly_struct *p_polys, *p_poly;
    edge_struct *p_edges, *p_edge;
    double z = 0, dz = 0;
    struct gnuplot_contours *save_contour_list;

    /* HBB FIXME 20050804: The number of contour_levels as set by 'set
     * cnrparam lev inc a,b,c' is almost certainly wrong if z axis is
     * logarithmic */
    num_of_z_levels = contour_levels;
    interp_kind = contour_kind;

    contour_list = NULL;

    /*
     * Calculate min/max values :
     */
    calc_min_max(num_isolines, iso_lines,
		 &x_min, &y_min, &z_min, &x_max, &y_max, &z_max);

    /*
     * Generate list of edges (p_edges) and list of triangles (p_polys):
     */
    gen_triangle(num_isolines, iso_lines, &p_polys, &p_edges);
    crnt_cntr_pt_index = 0;

    if (contour_levels_kind == LEVELS_AUTO) {
	dz = fabs(z_max - z_min);
	if (dz == 0)
	    return NULL;	/* empty z range ? */
	/* Find a tic step that will generate approximately the
	 * desired number of contour levels. The "* 2" is historical.
	 * */
	dz = quantize_normal_tics(dz, ((int) contour_levels + 1) * 2);
	z = floor(z_min / dz) * dz;
	num_of_z_levels = (int) floor((z_max - z) / dz);
    }
    for (i = 0; i < num_of_z_levels; i++) {
	switch (contour_levels_kind) {
	case LEVELS_AUTO:
	    z += dz;
	    z = CheckZero(z,dz);
	    break;
	case LEVELS_INCREMENTAL:
	    z = AXIS_LOG_VALUE(FIRST_Z_AXIS, contour_levels_list[0]) +
		i * AXIS_LOG_VALUE(FIRST_Z_AXIS, contour_levels_list[1]);
	    break;
	case LEVELS_DISCRETE:
	    z = AXIS_LOG_VALUE(FIRST_Z_AXIS, contour_levels_list[i]);
	    break;
	}
	contour_level = z;
	save_contour_list = contour_list;
	gen_contours(p_edges, z, x_min, x_max, y_min, y_max);
	if (contour_list != save_contour_list) {
	    contour_list->isNewLevel = 1;
	    /* Nov-2011 Use gprintf rather than sprintf so that LC_NUMERIC is used */
	    gprintf(contour_list->label, sizeof(contour_list->label), 
		    contour_format, 1.0, AXIS_DE_LOG_VALUE(FIRST_Z_AXIS,z));
	    contour_list->z = z;
	}
    }

    /* Free all contouring related temporary data. */
    while (p_polys) {
	p_poly = p_polys->next;
	free(p_polys);
	p_polys = p_poly;
    }
    while (p_edges) {
	p_edge = p_edges->next;
	free(p_edges);
	p_edges = p_edge;
    }

    return contour_list;
}

/*
 * Adds another point to the currently build contour.
 */
static void
add_cntr_point(double x, double y)
{
    int index;

    if (crnt_cntr_pt_index >= MAX_POINTS_PER_CNTR - 1) {
	index = crnt_cntr_pt_index - 1;
	end_crnt_cntr();
	crnt_cntr[0] = crnt_cntr[index * 2];
	crnt_cntr[1] = crnt_cntr[index * 2 + 1];
	crnt_cntr_pt_index = 1;	/* Keep the last point as first of this one. */
    }
    crnt_cntr[crnt_cntr_pt_index * 2] = x;
    crnt_cntr[crnt_cntr_pt_index * 2 + 1] = y;
    crnt_cntr_pt_index++;
}

/*
 * Done with current contour - create gnuplot data structure for it.
 */
static void
end_crnt_cntr()
{
    int i;
    struct gnuplot_contours *cntr =
	gp_alloc(sizeof(struct gnuplot_contours), "gnuplot_contour");
    cntr->coords =
	gp_alloc(sizeof(struct coordinate) * crnt_cntr_pt_index,
		 "contour coords");

    for (i = 0; i < crnt_cntr_pt_index; i++) {
	cntr->coords[i].x = crnt_cntr[i * 2];
	cntr->coords[i].y = crnt_cntr[i * 2 + 1];
	cntr->coords[i].z = contour_level;
    }
    cntr->num_pts = crnt_cntr_pt_index;
    cntr->label[0] = '\0';

    cntr->next = contour_list;
    contour_list = cntr;
    contour_list->isNewLevel = 0;

    crnt_cntr_pt_index = 0;
}

/*
 * Generates all contours by tracing the intersecting triangles.
 */
static void
gen_contours(
    edge_struct *p_edges,
    double z_level,
    double xx_min, double xx_max,
    double yy_min, double yy_max)
{
    int num_active;		/* Number of edges marked ACTIVE. */
    TBOOLEAN contr_isclosed;	/* Is this contour a closed line? */
    cntr_struct *p_cntr;

    num_active = update_all_edges(p_edges, z_level);	/* Do pass 1. */

    contr_isclosed = FALSE;	/* Start to look for contour on boundaries. */

    while (num_active > 0) {	/* Do Pass 2. */
	/* Generate One contour (and update NumActive as needed): */
	p_cntr = gen_one_contour(p_edges, z_level, &contr_isclosed, &num_active);
	/* Emit it in requested format: */
	put_contour(p_cntr, xx_min, xx_max, yy_min, yy_max, contr_isclosed);
    }
}

/*
 * Does pass 1, or marks the edges which are active (crosses this z_level)
 * Returns number of active edges (marked ACTIVE).
 */
static int
update_all_edges(edge_struct *p_edges, double z_level)
{
    int count = 0;

    while (p_edges) {
	/* use the same test at both vertices to avoid roundoff errors */
	if ((p_edges->vertex[0]->z >= z_level) !=
	    (p_edges->vertex[1]->z >= z_level)) {
	    p_edges->is_active = TRUE;
	    count++;
	} else
	    p_edges->is_active = FALSE;
	p_edges = p_edges->next;
    }

    return count;
}

/*
 * Does pass 2, or find one complete contour out of the triangulation
 * data base:
 *
 * Returns a pointer to the contour (as linked list), contr_isclosed
 * tells if the contour is a closed line or not, and num_active is
 * updated.
 */
static cntr_struct *
gen_one_contour(
    edge_struct *p_edges,	/* list of edges input */
    double z_level,			/* Z level of contour input */
    TBOOLEAN *contr_isclosed,	/* open or closed contour, in/out */
    int *num_active)		/* number of active edges     in/out */
{
    edge_struct *pe_temp;

    if (! *contr_isclosed) {
	/* Look for something to start with on boundary: */
	pe_temp = p_edges;
	while (pe_temp) {
	    if (pe_temp->is_active && (pe_temp->position == BOUNDARY))
		break;
	    pe_temp = pe_temp->next;
	}
	if (!pe_temp)
	    *contr_isclosed = TRUE;	/* No more contours on boundary. */
	else {
	    return trace_contour(pe_temp, z_level, num_active, *contr_isclosed);
	}
    }
    if (*contr_isclosed) {
	/* Look for something to start with inside: */
	pe_temp = p_edges;
	while (pe_temp) {
	    if (pe_temp->is_active && (pe_temp->position != BOUNDARY))
		break;
	    pe_temp = pe_temp->next;
	}
	if (!pe_temp) {
	    *num_active = 0;
	    fprintf(stderr, "gen_one_contour: no contour found\n");
	    return NULL;
	} else {
	    *contr_isclosed = TRUE;
	    return trace_contour(pe_temp, z_level, num_active, *contr_isclosed);
	}
    }
    return NULL;		/* We should never be here, but lint... */
}

/*
 * Search the data base along a contour starts at the edge pe_start until
 * a boundary edge is detected or until we close the loop back to pe_start.
 * Returns a linked list of all the points on the contour
 * Also decreases num_active by the number of points on contour.
 */
static cntr_struct *
trace_contour(
    edge_struct *pe_start, /* edge to start contour input */
    double z_level,		/* Z level of contour input */
    int *num_active,		/* number of active edges in/out */
    TBOOLEAN contr_isclosed)	/* open or closed contour line (input) */
{
    cntr_struct *p_cntr, *pc_tail;
    edge_struct *p_edge, *p_next_edge;
    poly_struct *p_poly, *PLastpoly = NULL;
    int i;

    p_edge = pe_start;		/* first edge to start contour */

    /* Generate the header of the contour - the point on pe_start. */
    if (! contr_isclosed) {
	pe_start->is_active = FALSE;
	(*num_active)--;
    }
    if (p_edge->poly[0] || p_edge->poly[1]) {	/* more than one point */

	p_cntr = pc_tail = update_cntr_pt(pe_start, z_level);	/* first point */

	do {
	    /* Find polygon to continue (Not where we came from - PLastpoly): */
	    if (p_edge->poly[0] == PLastpoly)
		p_poly = p_edge->poly[1];
	    else
		p_poly = p_edge->poly[0];
	    p_next_edge = NULL;	/* In case of error, remains NULL. */
	    for (i = 0; i < 3; i++)	/* Test the 3 edges of the polygon: */
		if (p_poly->edge[i] != p_edge)
		    if (p_poly->edge[i]->is_active)
			p_next_edge = p_poly->edge[i];
	    if (!p_next_edge) {	/* Error exit */
		pc_tail->next = NULL;
		free_contour(p_cntr);
		fprintf(stderr, "trace_contour: unexpected end of contour\n");
		return NULL;
	    }
	    p_edge = p_next_edge;
	    PLastpoly = p_poly;
	    p_edge->is_active = FALSE;
	    (*num_active)--;

	    /* Do not allocate contour points on diagonal edges */
	    if (p_edge->position != DIAGONAL) {

		pc_tail->next = update_cntr_pt(p_edge, z_level);

		/* Remove nearby points */
		if (fuzzy_equal(pc_tail, pc_tail->next)) {

		    free(pc_tail->next);
		} else
		    pc_tail = pc_tail->next;
	    }
	} while ((p_edge != pe_start) && (p_edge->position != BOUNDARY));

	pc_tail->next = NULL;

	/* For closed contour the first and last point should be equal */
	if (pe_start == p_edge) {
	    (p_cntr->X) = (pc_tail->X);
	    (p_cntr->Y) = (pc_tail->Y);
	}
    } else {			/* only one point, forget it */
	p_cntr = NULL;
    }

    return p_cntr;
}

/*
 * Allocates one contour location and update it to to correct position
 * according to z_level and edge p_edge.
 */
static cntr_struct *
update_cntr_pt(edge_struct *p_edge, double z_level)
{
    double t;
    cntr_struct *p_cntr;

    t = (z_level - p_edge->vertex[0]->z) /
	(p_edge->vertex[1]->z - p_edge->vertex[0]->z);

    /* test if t is out of interval [0:1] (should not happen but who knows ...) */
    t = (t < 0.0 ? 0.0 : t);
    t = (t > 1.0 ? 1.0 : t);

    p_cntr = gp_alloc(sizeof(cntr_struct), "contour cntr_struct");

    p_cntr->X = p_edge->vertex[1]->x * t +
	p_edge->vertex[0]->x * (1 - t);
    p_cntr->Y = p_edge->vertex[1]->y * t +
	p_edge->vertex[0]->y * (1 - t);
    return p_cntr;
}

/* Simple routine to decide if two contour points are equal by
 * calculating the relative error (< EPSILON).  */
static int
fuzzy_equal(cntr_struct *p_cntr1, cntr_struct *p_cntr2)
{
    double unit_x, unit_y;
    unit_x = fabs(x_max - x_min);		/* reference */
    unit_y = fabs(y_max - y_min);
    return ((fabs(p_cntr1->X - p_cntr2->X) < unit_x * EPSILON)
	    && (fabs(p_cntr1->Y - p_cntr2->Y) < unit_y * EPSILON));
}

/*
 * Generate the triangles.
 * Returns the lists (edges & polys) via pointers to their heads.
 */
static void
gen_triangle(
    int num_isolines,		/* number of iso-lines input */
    struct iso_curve *iso_lines,	/* iso-lines input */
    poly_struct **p_polys,	/* list of polygons output */
    edge_struct **p_edges)	/* list of edges output */
{
    int i, j, grid_x_max = iso_lines->p_count;
    edge_struct *p_edge1, *p_edge2, *edge0, *edge1, *edge2, *pe_tail, *pe_tail2, *pe_temp;
    poly_struct *pp_tail;
    struct coordinate GPHUGE *p_vrtx1, GPHUGE * p_vrtx2;

    (*p_polys) = pp_tail = NULL;	/* clear lists */
    (*p_edges) = pe_tail = NULL;

    p_vrtx1 = iso_lines->points;	/* first row of vertices */
    p_edge1 = pe_tail = NULL;	/* clear list of edges */

    /* Generate edges of first row */
    for (j = 0; j < grid_x_max - 1; j++)
	add_edge(p_vrtx1 + j, p_vrtx1 + j + 1, &p_edge1, &pe_tail);

    (*p_edges) = p_edge1;	/* update main list */


    /*
     * Combines vertices to edges and edges to triangles:
     * ==================================================
     * The edges are stored in the edge list, referenced by p_edges
     * (pe_tail points on last edge).
     *
     * Temporary pointers:
     * 1. p_edge2: Top horizontal edge list:      +-----------------------+ 2
     * 2. p_tail : end of middle edge list:       |\  |\  |\  |\  |\  |\  |
     *                                            |  \|  \|  \|  \|  \|  \|
     * 3. p_edge1: Bottom horizontal edge list:   +-----------------------+ 1
     *
     * pe_tail2  : end of list beginning at p_edge2
     * pe_temp   : position inside list beginning at p_edge1
     * p_edges   : head of the master edge list (part of our output)
     * p_vrtx1   : start of lower row of input vertices
     * p_vrtx2   : start of higher row of input vertices
     *
     * The routine generates two triangle            Lower      Upper 1
     * upper one and lower one:                     | \           ----
     * (Nums. are edges order in polys)            0|   \1       0\   |2
     * The polygons are stored in the polygon        ----           \ |
     * list (*p_polys) (pp_tail points on             2
     * last polygon).
     *                                                        1
     *                                                   -----------
     * In addition, the edge lists are updated -        | \   0     |
     * each edge has two pointers on the two            |   \       |
     * (one active if boundary) polygons which         0|1   0\1   0|1
     * uses it. These two pointer to polygons           |       \   |
     * are named: poly[0], poly[1]. The diagram         |    1    \ |
     * on the right show how they are used for the       -----------
     * upper and lower polygons (INNER_MESH polygons only).  0
     */

    for (i = 1; i < num_isolines; i++) {
	/* Read next column and gen. polys. */
	iso_lines = iso_lines->next;

	p_vrtx2 = iso_lines->points;	/* next row of vertices */
	p_edge2 = pe_tail2 = NULL;	/* clear top horizontal list */
	pe_temp = p_edge1;	/* pointer in bottom list */

	/*
	 * Generate edges and triagles for next row:
	 */

	/* generate first vertical edge */
	edge2 = add_edge(p_vrtx1, p_vrtx2, p_edges, &pe_tail);

	for (j = 0; j < grid_x_max - 1; j++) {

	    /* copy vertical edge for lower triangle */
	    edge0 = edge2;

	    if (pe_temp && pe_temp->vertex[0] == p_vrtx1 + j) {
		/* test lower edge */
		edge2 = pe_temp;
		pe_temp = pe_temp->next;
	    } else {
		edge2 = NULL;	/* edge is undefined */
	    }

	    /* generate diagonal edge */
	    edge1 = add_edge(p_vrtx1 + j + 1, p_vrtx2 + j, p_edges, &pe_tail);
	    if (edge1)
		edge1->position = DIAGONAL;

	    /* generate lower triangle */
	    add_poly(edge0, edge1, edge2, p_polys, &pp_tail);

	    /* copy diagonal edge for upper triangle */
	    edge0 = edge1;

	    /* generate upper edge */
	    edge1 = add_edge(p_vrtx2 + j, p_vrtx2 + j + 1, &p_edge2, &pe_tail2);

	    /* generate vertical edge */
	    edge2 = add_edge(p_vrtx1 + j + 1, p_vrtx2 + j + 1, p_edges, &pe_tail);

	    /* generate upper triangle */
	    add_poly(edge0, edge1, edge2, p_polys, &pp_tail);
	}

	if (p_edge2) {
	    /* HBB 19991130 bugfix: if p_edge2 list is empty,
	     * don't change p_edges list! Crashes by access
	     * to NULL pointer pe_tail, the second time through,
	     * otherwise */
	    if ((*p_edges)) {	/* Chain new edges to main list. */
		pe_tail->next = p_edge2;
		pe_tail = pe_tail2;
	    } else {
		(*p_edges) = p_edge2;
		pe_tail = pe_tail2;
	    }
	}

	/* this row finished, move list heads up one row: */
	p_edge1 = p_edge2;
	p_vrtx1 = p_vrtx2;
    }

    /* Update the boundary flag, saved in each edge, and update indexes: */

    pe_temp = (*p_edges);

    while (pe_temp) {
	if ((!(pe_temp->poly[0])) || (!(pe_temp->poly[1])))
	    (pe_temp->position) = BOUNDARY;
	pe_temp = pe_temp->next;
    }
}

/*
 * Calculate minimum and maximum values
 */
static void
calc_min_max(
    int num_isolines,		/* number of iso-lines input */
    struct iso_curve *iso_lines, /* iso-lines input */
    double *xx_min, double *yy_min, double *zz_min,
    double *xx_max, double *yy_max, double *zz_max) /* min/max values in/out */
{
    int i, j, grid_x_max;
    struct coordinate GPHUGE *vertex;

    grid_x_max = iso_lines->p_count;	/* number of vertices per iso_line */

    (*xx_min) = (*yy_min) = (*zz_min) = VERYLARGE;	/* clear min/max values */
    (*xx_max) = (*yy_max) = (*zz_max) = -VERYLARGE;

    for (j = 0; j < num_isolines; j++) {

	vertex = iso_lines->points;

	for (i = 0; i < grid_x_max; i++) {
	    if (vertex[i].type != UNDEFINED) {
		if (vertex[i].x > (*xx_max))
		    (*xx_max) = vertex[i].x;
		if (vertex[i].y > (*yy_max))
		    (*yy_max) = vertex[i].y;
		if (vertex[i].z > (*zz_max))
		    (*zz_max) = vertex[i].z;
		if (vertex[i].x < (*xx_min))
		    (*xx_min) = vertex[i].x;
		if (vertex[i].y < (*yy_min))
		    (*yy_min) = vertex[i].y;
		if (vertex[i].z < (*zz_min))
		    (*zz_min) = vertex[i].z;
	    }
	}
	iso_lines = iso_lines->next;
    }

    /*
     * fprintf(stderr," x: %g, %g\n", (*xx_min), (*xx_max));
     * fprintf(stderr," y: %g, %g\n", (*yy_min), (*yy_max));
     * fprintf(stderr," z: %g, %g\n", (*zz_min), (*zz_max));
     */
}

/*
 * Generate new edge and append it to list, but only if both vertices are
 * defined. The list is referenced by p_edge and pe_tail (p_edge points on
 * first edge and pe_tail on last one).
 * Note, the list may be empty (pe_edge==pe_tail==NULL) on entry and exit.
 */
static edge_struct *
add_edge(
    struct coordinate GPHUGE *point0,	/* 2 vertices input */
    struct coordinate GPHUGE *point1,
    edge_struct **p_edge, /* pointers to edge list in/out */
    edge_struct **pe_tail)
{
    edge_struct *pe_temp = NULL;

#if 1
    if (point0->type == INRANGE && point1->type == INRANGE)
#else
    if (point0->type != UNDEFINED && point1->type != UNDEFINED)
#endif
    {
	pe_temp = gp_alloc(sizeof(edge_struct), "contour edge");

	pe_temp->poly[0] = NULL;	/* clear links           */
	pe_temp->poly[1] = NULL;
	pe_temp->vertex[0] = point0;	/* First vertex of edge. */
	pe_temp->vertex[1] = point1;	/* Second vertex of edge. */
	pe_temp->next = NULL;
	pe_temp->position = INNER_MESH;		/* default position in mesh */

	if ((*pe_tail)) {
	    (*pe_tail)->next = pe_temp;		/* Stick new record as last one. */
	} else {
	    (*p_edge) = pe_temp;	/* start new list if empty */
	}
	(*pe_tail) = pe_temp;	/* continue to last record. */

    }
    return pe_temp;		/* returns NULL, if no edge allocated */
}

/*
 * Generate new triangle and append it to list, but only if all edges are defined.
 * The list is referenced by p_poly and pp_tail (p_poly points on first ploygon
 * and pp_tail on last one).
 * Note, the list may be empty (pe_ploy==pp_tail==NULL) on entry and exit.
 */
static poly_struct *
add_poly(
    edge_struct *edge0,
    edge_struct *edge1,
    edge_struct *edge2,	/* 3 edges input */
    poly_struct **p_poly,
    poly_struct **pp_tail) /* pointers to polygon list in/out */
{
    poly_struct *pp_temp = NULL;

    if (edge0 && edge1 && edge2) {
	pp_temp = gp_alloc(sizeof(poly_struct), "contour polygon");

	pp_temp->edge[0] = edge0;	/* First edge of triangle */
	pp_temp->edge[1] = edge1;	/* Second one             */
	pp_temp->edge[2] = edge2;	/* Third one              */
	pp_temp->next = NULL;

	if (edge0->poly[0])	/* update edge0 */
	    edge0->poly[1] = pp_temp;
	else
	    edge0->poly[0] = pp_temp;

	if (edge1->poly[0])	/* update edge1 */
	    edge1->poly[1] = pp_temp;
	else
	    edge1->poly[0] = pp_temp;

	if (edge2->poly[0])	/* update edge2 */
	    edge2->poly[1] = pp_temp;
	else
	    edge2->poly[0] = pp_temp;

	if ((*pp_tail))		/* Stick new record as last one. */
	    (*pp_tail)->next = pp_temp;
	else
	    (*p_poly) = pp_temp;	/* start new list if empty */

	(*pp_tail) = pp_temp;	/* continue to last record. */

    }
    return pp_temp;		/* returns NULL, if no edge allocated */
}



/*
 * Calls the (hopefully) desired interpolation/approximation routine.
 */
static void
put_contour(
    cntr_struct *p_cntr,	/* contour structure input */
    double xx_min, double xx_max,
    double yy_min, double yy_max, /* minimum/maximum values input */
    TBOOLEAN contr_isclosed)	/* contour line closed? (input) */
{

    if (!p_cntr)
	return;			/* Nothing to do if it is empty contour. */

    switch (interp_kind) {
    case CONTOUR_KIND_LINEAR:	/* No interpolation/approximation. */
	put_contour_nothing(p_cntr);
	break;
    case CONTOUR_KIND_CUBIC_SPL: /* Cubic spline interpolation. */
	put_contour_cubic(p_cntr, xx_min, xx_max, yy_min, yy_max,
			  chk_contour_kind(p_cntr, contr_isclosed));

	break;
    case CONTOUR_KIND_BSPLINE:	/* Bspline approximation. */
	put_contour_bspline(p_cntr,
			    chk_contour_kind(p_cntr, contr_isclosed));
	break;
    }
    free_contour(p_cntr);
}

/*
 * Simply puts contour coordinates in order with no interpolation or
 * approximation.
 */
static void
put_contour_nothing(cntr_struct *p_cntr)
{
    while (p_cntr) {
	add_cntr_point(p_cntr->X, p_cntr->Y);
	p_cntr = p_cntr->next;
    }
    end_crnt_cntr();
}

/*
 * for some reason contours are never flagged as 'isclosed'
 * if first point == last point, set flag accordingly
 *
 */

static int
chk_contour_kind(cntr_struct *p_cntr, TBOOLEAN contr_isclosed)
{
    cntr_struct *pc_tail = NULL;
    TBOOLEAN current_contr_isclosed;

    current_contr_isclosed = contr_isclosed;

    if (! contr_isclosed) {
	pc_tail = p_cntr;
	while (pc_tail->next)
	    pc_tail = pc_tail->next;	/* Find last point. */

	/* test if first and last point are equal */
	if (fuzzy_equal(pc_tail, p_cntr))
	    current_contr_isclosed = TRUE;
    }
    return (current_contr_isclosed);
}

/*
 * Generate a cubic spline curve through the points (x_i,y_i) which are
 * stored in the linked list p_cntr.
 * The spline is defined as a 2d-function s(t) = (x(t),y(t)), where the
 * parameter t is the length of the linear stroke.
 */
static void
put_contour_cubic(
    cntr_struct *p_cntr,
    double xx_min, double xx_max,
    double yy_min, double yy_max,
    TBOOLEAN contr_isclosed)
{
    int num_pts, num_intpol;
    double unit_x, unit_y;	/* To define norm (x,y)-plane */
    double *delta_t;		/* Interval length t_{i+1}-t_i */
    double *d2x, *d2y;		/* Second derivatives x''(t_i), y''(t_i) */
    cntr_struct *pc_tail;

    num_pts = count_contour(p_cntr);	/* Number of points in contour. */

    pc_tail = p_cntr;		/* Find last point. */
    while (pc_tail->next)
	pc_tail = pc_tail->next;

    if (contr_isclosed) {
	/* Test if first and last point are equal (should be) */
	if (!fuzzy_equal(pc_tail, p_cntr)) {
	    pc_tail->next = p_cntr;	/* Close contour list - make it circular. */
	    num_pts++;
	}
    }
    delta_t = gp_alloc(num_pts * sizeof(double), "contour delta_t");
    d2x = gp_alloc(num_pts * sizeof(double), "contour d2x");
    d2y = gp_alloc(num_pts * sizeof(double), "contour d2y");

    /* Width and height of the grid is used as a unit length (2d-norm) */
    unit_x = xx_max - xx_min;
    unit_y = yy_max - yy_min;
    /* FIXME HBB 20010121: 'zero' should not be used as an absolute
     * figure to compare to data */
    unit_x = (unit_x > zero ? unit_x : zero);	/* should not be zero */
    unit_y = (unit_y > zero ? unit_y : zero);

    if (num_pts > 2) {
	/*
	 * Calculate second derivatives d2x[], d2y[] and interval lengths delta_t[]:
	 */
	if (!gen_cubic_spline(num_pts, p_cntr, d2x, d2y, delta_t,
			      contr_isclosed, unit_x, unit_y)) {
	    free(delta_t);
	    free(d2x);
	    free(d2y);
	    if (contr_isclosed)
		pc_tail->next = NULL;	/* Un-circular list */
	    return;
	}
    }
    /* If following (num_pts > 1) is TRUE then exactly 2 points in contour.  */
    else if (num_pts > 1) {
	/* set all second derivatives to zero, interval length to 1 */
	d2x[0] = 0.;
	d2y[0] = 0.;
	d2x[1] = 0.;
	d2y[1] = 0.;
	delta_t[0] = 1.;
    } else {			/* Only one point ( ?? ) - ignore it. */
	free(delta_t);
	free(d2x);
	free(d2y);
	if (contr_isclosed)
	    pc_tail->next = NULL;	/* Un-circular list */
	return;
    }

    /* Calculate "num_intpol" interpolated values */
    num_intpol = 1 + (num_pts - 1) * contour_pts;	/* global: contour_pts */
    intp_cubic_spline(num_pts, p_cntr, d2x, d2y, delta_t, num_intpol);

    free(delta_t);
    free(d2x);
    free(d2y);

    if (contr_isclosed)
	pc_tail->next = NULL;	/* Un-circular list */

    end_crnt_cntr();
}


/*
 * Find Bspline approximation for this data set.
 * Uses global variable contour_pts to determine number of samples per
 * interval, where the knot vector intervals are assumed to be uniform, and
 * global variable contour_order for the order of Bspline to use.
 */
static void
put_contour_bspline(cntr_struct *p_cntr, TBOOLEAN contr_isclosed)
{
    int num_pts;
    int order = contour_order - 1;

    num_pts = count_contour(p_cntr);	/* Number of points in contour. */
    if (num_pts < 2)
	return;			/* Can't do nothing if empty or one points! */
    /* Order must be less than number of points in curve - fix it if needed. */
    if (order > num_pts - 1)
	order = num_pts - 1;

    gen_bspline_approx(p_cntr, num_pts, order, contr_isclosed);
    end_crnt_cntr();
}

/*
 * Free all elements in the contour list.
 */
static void
free_contour(cntr_struct *p_cntr)
{
    cntr_struct *pc_temp;

    while (p_cntr) {
	pc_temp = p_cntr;
	p_cntr = p_cntr->next;
	free(pc_temp);
    }
}

/*
 * Counts number of points in contour.
 */
static int
count_contour(cntr_struct *p_cntr)
{
    int count = 0;

    while (p_cntr) {
	count++;
	p_cntr = p_cntr->next;
    }
    return count;
}

/*
 * Find second derivatives (x''(t_i),y''(t_i)) of cubic spline interpolation
 * through list of points (x_i,y_i). The parameter t is calculated as the
 * length of the linear stroke. The number of points must be at least 3.
 * Note: For closed contours the first and last point must be equal.
 */
static int
gen_cubic_spline(
    int num_pts,		/* Number of points (num_pts>=3), input */
    cntr_struct *p_cntr,	/* List of points (x(t_i),y(t_i)), input */
    double d2x[], double d2y[],	/* Second derivatives (x''(t_i),y''(t_i)), output */
    double delta_t[],		/* List of interval lengths t_{i+1}-t_{i}, output */
    TBOOLEAN contr_isclosed,	/* Closed or open contour?, input  */
    double unit_x, double unit_y) /* Unit length in x and y (norm=1), input */
{
    int n, i;
    double norm;
    tri_diag *m;		/* The tri-diagonal matrix is saved here. */
    cntr_struct *pc_temp;

    m = gp_alloc(num_pts * sizeof(tri_diag), "contour tridiag m");

    /*
     * Calculate first differences in (d2x[i], d2y[i]) and interval lengths
     * in delta_t[i]:
     */
    pc_temp = p_cntr;
    for (i = 0; i < num_pts - 1; i++) {
	d2x[i] = pc_temp->next->X - pc_temp->X;
	d2y[i] = pc_temp->next->Y - pc_temp->Y;
	/*
	 * The norm of a linear stroke is calculated in "normal coordinates"
	 * and used as interval length:
	 */
	delta_t[i] = sqrt(SQR(d2x[i] / unit_x) + SQR(d2y[i] / unit_y));

	d2x[i] /= delta_t[i];	/* first difference, with unit norm: */
	d2y[i] /= delta_t[i];	/*   || (d2x[i], d2y[i]) || = 1      */

	pc_temp = pc_temp->next;
    }

    /*
     * Setup linear system:  m * x = b
     */
    n = num_pts - 2;		/* Without first and last point */
    if (contr_isclosed) {
	/* First and last points must be equal for closed contours */
	delta_t[num_pts - 1] = delta_t[0];
	d2x[num_pts - 1] = d2x[0];
	d2y[num_pts - 1] = d2y[0];
	n++;			/* Add last point (= first point) */
    }
    for (i = 0; i < n; i++) {
	/* Matrix M, mainly tridiagonal with cyclic second index ("j = j+n mod n") */
	m[i][0] = delta_t[i];	/* Off-diagonal element M_{i,i-1} */
	m[i][1] = 2. * (delta_t[i] + delta_t[i + 1]);	/* M_{i,i} */
	m[i][2] = delta_t[i + 1];	/* Off-diagonal element M_{i,i+1} */

	/* Right side b_x and b_y */
	d2x[i] = (d2x[i + 1] - d2x[i]) * 6.;
	d2y[i] = (d2y[i + 1] - d2y[i]) * 6.;

	/*
	 * If the linear stroke shows a cusps of more than 90 degree, the right
	 * side is reduced to avoid oscillations in the spline:
	 */
	norm = sqrt(SQR(d2x[i] / unit_x) + SQR(d2y[i] / unit_y)) / 8.5;

	if (norm > 1.) {
	    d2x[i] /= norm;
	    d2y[i] /= norm;
	    /* The first derivative will not be continuous */
	}
    }

    if (!contr_isclosed) {
	/* Third derivative is set to zero at both ends */
	m[0][1] += m[0][0];	/* M_{0,0}     */
	m[0][0] = 0.;		/* M_{0,n-1}   */
	m[n - 1][1] += m[n - 1][2];	/* M_{n-1,n-1} */
	m[n - 1][2] = 0.;	/* M_{n-1,0}   */
    }
    /* Solve linear systems for d2x[] and d2y[] */


    if (solve_cubic_1(m, n)) {	/* Calculate Cholesky decomposition */
	solve_cubic_2(m, d2x, n);	/* solve M * d2x = b_x */
	solve_cubic_2(m, d2y, n);	/* solve M * d2y = b_y */

    } else {			/* Should not happen, but who knows ... */
	free(m);
	return FALSE;
    }

    /* Shift all second derivatives one place right and abdate end points */
    for (i = n; i > 0; i--) {
	d2x[i] = d2x[i - 1];
	d2y[i] = d2y[i - 1];
    }
    if (contr_isclosed) {
	d2x[0] = d2x[n];
	d2y[0] = d2y[n];
    } else {
	d2x[0] = d2x[1];	/* Third derivative is zero in */
	d2y[0] = d2y[1];	/*     first and last interval */
	d2x[n + 1] = d2x[n];
	d2y[n + 1] = d2y[n];
    }

    free(m);
    return TRUE;
}

/*
 * Calculate interpolated values of the spline function (defined via p_cntr
 * and the second derivatives d2x[] and d2y[]). The number of tabulated
 * values is n. On an equidistant grid n_intpol values are calculated.
 */
static void
intp_cubic_spline(
    int n,
    cntr_struct *p_cntr,
    double d2x[], double d2y[], double delta_t[],
    int n_intpol)
{
    double t, t_skip, t_max;
    double x0, x1, x, y0, y1, y;
    double d, hx, dx0, dx01, hy, dy0, dy01;
    int i;

    /* The length of the total interval */
    t_max = 0.;
    for (i = 0; i < n - 1; i++)
	t_max += delta_t[i];

    /* The distance between interpolated points */
    t_skip = (1. - 1e-7) * t_max / (n_intpol - 1);

    t = 0.;			/* Parameter value */
    x1 = p_cntr->X;
    y1 = p_cntr->Y;
    add_cntr_point(x1, y1);	/* First point. */
    t += t_skip;

    for (i = 0; i < n - 1; i++) {
	p_cntr = p_cntr->next;

	d = delta_t[i];		/* Interval length */
	x0 = x1;
	y0 = y1;
	x1 = p_cntr->X;
	y1 = p_cntr->Y;
	hx = (x1 - x0) / d;
	hy = (y1 - y0) / d;
	dx0 = (d2x[i + 1] + 2 * d2x[i]) / 6.;
	dy0 = (d2y[i + 1] + 2 * d2y[i]) / 6.;
	dx01 = (d2x[i + 1] - d2x[i]) / (6. * d);
	dy01 = (d2y[i + 1] - d2y[i]) / (6. * d);
	while (t <= delta_t[i]) {	/* t in current interval ? */
	    x = x0 + t * (hx + (t - d) * (dx0 + t * dx01));
	    y = y0 + t * (hy + (t - d) * (dy0 + t * dy01));
	    add_cntr_point(x, y);	/* next point. */
	    t += t_skip;
	}
	t -= delta_t[i];	/* Parameter t relative to start of next interval */
    }
}

/*
 * The following two procedures solve the special linear system which arise
 * in cubic spline interpolation. If x is assumed cyclic ( x[i]=x[n+i] ) the
 * equations can be written as (i=0,1,...,n-1):
 *     m[i][0] * x[i-1] + m[i][1] * x[i] + m[i][2] * x[i+1] = b[i] .
 * In matrix notation one gets M * x = b, where the matrix M is tridiagonal
 * with additional elements in the upper right and lower left position:
 *   m[i][0] = M_{i,i-1}  for i=1,2,...,n-1    and    m[0][0] = M_{0,n-1} ,
 *   m[i][1] = M_{i, i }  for i=0,1,...,n-1
 *   m[i][2] = M_{i,i+1}  for i=0,1,...,n-2    and    m[n-1][2] = M_{n-1,0}.
 * M should be symmetric (m[i+1][0]=m[i][2]) and positiv definite.
 * The size of the system is given in n (n>=1).
 *
 * In the first procedure the Cholesky decomposition M = C^T * D * C
 * (C is upper triangle with unit diagonal, D is diagonal) is calculated.
 * Return TRUE if decomposition exist.
 */
static int
solve_cubic_1(tri_diag m[], int n)
{
    int i;
    double m_ij, m_n, m_nn, d;

    if (n < 1)
	return FALSE;		/* Dimension should be at least 1 */

    d = m[0][1];		/* D_{0,0} = M_{0,0} */
    if (d <= 0.)
	return FALSE;		/* M (or D) should be positiv definite */
    m_n = m[0][0];		/*  M_{0,n-1}  */
    m_nn = m[n - 1][1];		/* M_{n-1,n-1} */
    for (i = 0; i < n - 2; i++) {
	m_ij = m[i][2];		/*  M_{i,1}  */
	m[i][2] = m_ij / d;	/* C_{i,i+1} */
	m[i][0] = m_n / d;	/* C_{i,n-1} */
	m_nn -= m[i][0] * m_n;	/* to get C_{n-1,n-1} */
	m_n = -m[i][2] * m_n;	/* to get C_{i+1,n-1} */
	d = m[i + 1][1] - m[i][2] * m_ij;	/* D_{i+1,i+1} */
	if (d <= 0.)
	    return FALSE;	/* Elements of D should be positiv */
	m[i + 1][1] = d;
    }
    if (n >= 2) {		/* Complete last column */
	m_n += m[n - 2][2];	/* add M_{n-2,n-1} */
	m[n - 2][0] = m_n / d;	/* C_{n-2,n-1} */
	m[n - 1][1] = d = m_nn - m[n - 2][0] * m_n;	/* D_{n-1,n-1} */
	if (d <= 0.)
	    return FALSE;
    }
    return TRUE;
}

/*
 * The second procedure solves the linear system, with the Choleky
 * decomposition calculated above (in m[][]) and the right side b given
 * in x[]. The solution x overwrites the right side in x[].
 */
static void
solve_cubic_2(tri_diag m[], double x[], int n)
{
    int i;
    double x_n;

    /* Division by transpose of C : b = C^{-T} * b */
    x_n = x[n - 1];
    for (i = 0; i < n - 2; i++) {
	x[i + 1] -= m[i][2] * x[i];	/* C_{i,i+1} * x_{i} */
	x_n -= m[i][0] * x[i];	/* C_{i,n-1} * x_{i} */
    }
    if (n >= 2)
	x[n - 1] = x_n - m[n - 2][0] * x[n - 2];	/* C_{n-2,n-1} * x_{n-1} */

    /* Division by D: b = D^{-1} * b */
    for (i = 0; i < n; i++)
	x[i] /= m[i][1];

    /* Division by C: b = C^{-1} * b */
    x_n = x[n - 1];
    if (n >= 2)
	x[n - 2] -= m[n - 2][0] * x_n;	/* C_{n-2,n-1} * x_{n-1} */
    for (i = n - 3; i >= 0; i--) {
	/*      C_{i,i+1} * x_{i+1} + C_{i,n-1} * x_{n-1} */
	x[i] -= m[i][2] * x[i + 1] + m[i][0] * x_n;
    }
    return;
}

/*
 * Solve tri diagonal linear system equation. The tri diagonal matrix is
 * defined via matrix M, right side is r, and solution X i.e. M * X = R.
 * Size of system given in n. Return TRUE if solution exist.
 */
/* not used any more in "contour.c", but in "spline.c" (21. Dec. 1995) ! */

int
solve_tri_diag(tri_diag m[], double r[], double x[], int n)
{
    int i;
    double t;

    for (i = 1; i < n; i++) {	/* Eliminate element m[i][i-1] (lower diagonal). */
	if (m[i - 1][1] == 0)
	    return FALSE;
	t = m[i][0] / m[i - 1][1];	/* Find ratio between the two lines. */
/*      m[i][0] = m[i][0] - m[i-1][1] * t; */
/* m[i][0] is not used any more (and set to 0 in the above line) */
	m[i][1] = m[i][1] - m[i - 1][2] * t;
	r[i] = r[i] - r[i - 1] * t;
    }
    /* Now do back subtitution - update the solution vector X: */
    if (m[n - 1][1] == 0)
	return FALSE;
    x[n - 1] = r[n - 1] / m[n - 1][1];	/* Find last element. */
    for (i = n - 2; i >= 0; i--) {
	if (m[i][1] == 0)
	    return FALSE;
	x[i] = (r[i] - x[i + 1] * m[i][2]) / m[i][1];
    }
    return TRUE;
}

/*
 * Generate a Bspline curve defined by all the points given in linked list p:
 * Algorithm: using deBoor algorithm
 * Note: if Curvekind is open contour than Open end knot vector is assumed,
 *       else (closed contour) Float end knot vector is assumed.
 * It is assumed that num_of_points is at least 2, and order of Bspline is less
 * than num_of_points!
 */
static void
gen_bspline_approx(
    cntr_struct *p_cntr,
    int num_of_points,
    int order,
    TBOOLEAN contr_isclosed)
{
    int knot_index = 0, pts_count = 1;
    double dt, t, next_t, t_min, t_max, x, y;
    cntr_struct *pc_temp = p_cntr, *pc_tail = NULL;

    /* If the contour is Closed one we must update few things:
     * 1. Make the list temporary circular, so we can close the contour.
     * 2. Update num_of_points - increase it by "order-1" so contour will be
     *    closed. This will evaluate order more sections to close it!
     */
    if (contr_isclosed) {
	pc_tail = p_cntr;
	while (pc_tail->next)
	    pc_tail = pc_tail->next;	/* Find last point. */

	/* test if first and last point are equal */
	if (fuzzy_equal(pc_tail, p_cntr)) {
	    /* Close contour list - make it circular. */
	    pc_tail->next = p_cntr->next;
	    num_of_points += order - 1;
	} else {
	    pc_tail->next = p_cntr;
	    num_of_points += order;
	}
    }
    /* Find first (t_min) and last (t_max) t value to eval: */
    t = t_min = fetch_knot(contr_isclosed, num_of_points, order, order);
    t_max = fetch_knot(contr_isclosed, num_of_points, order, num_of_points);
    next_t = t_min + 1.0;
    knot_index = order;
    dt = 1.0 / contour_pts;	/* Number of points per one section. */


    while (t < t_max) {
	if (t > next_t) {
	    pc_temp = pc_temp->next;	/* Next order ctrl. pt. to blend. */
	    knot_index++;
	    next_t += 1.0;
	}
	eval_bspline(t, pc_temp, num_of_points, order, knot_index,
		     contr_isclosed, &x, &y);	/* Next pt. */
	add_cntr_point(x, y);
	pts_count++;
	/* As we might have some real number round off problems we do      */
	/* the last point outside the loop                                 */
	if (pts_count == contour_pts * (num_of_points - order) + 1)
	    break;
	t += dt;
    }

    /* Now do the last point */
    eval_bspline(t_max - EPSILON, pc_temp, num_of_points, order, knot_index,
		 contr_isclosed, &x, &y);
    add_cntr_point(x, y);	/* Complete the contour. */

    if (contr_isclosed)	/* Update list - un-circular it. */
	pc_tail->next = NULL;
}

/*
 * The routine to evaluate the B-spline value at point t using knot vector
 * from function fetch_knot(), and the control points p_cntr.
 * Returns (x, y) of approximated B-spline. Note that p_cntr points on the
 * first control point to blend with. The B-spline is of order order.
 */
static void
eval_bspline(
    double t,
    cntr_struct *p_cntr,
    int num_of_points, int order, int j,
    TBOOLEAN contr_isclosed,
    double *x, double *y)
{
    int i, p;
    double ti, tikp, *dx, *dy;	/* Copy p_cntr into it to make it faster. */

    dx = gp_alloc((order + j) * sizeof(double), "contour b_spline");
    dy = gp_alloc((order + j) * sizeof(double), "contour b_spline");

    /* Set the dx/dy - [0] iteration step, control points (p==0 iterat.): */
    for (i = j - order; i <= j; i++) {
	dx[i] = p_cntr->X;
	dy[i] = p_cntr->Y;
	p_cntr = p_cntr->next;
    }

    for (p = 1; p <= order; p++) {	/* Iteration (b-spline level) counter. */
	for (i = j; i >= j - order + p; i--) {	/* Control points indexing. */
	    ti = fetch_knot(contr_isclosed, num_of_points, order, i);
	    tikp = fetch_knot(contr_isclosed, num_of_points, order, i + order + 1 - p);
	    if (ti == tikp) {	/* Should not be a problems but how knows... */
	    } else {
		dx[i] = dx[i] * (t - ti) / (tikp - ti) +	/* Calculate x. */
		    dx[i - 1] * (tikp - t) / (tikp - ti);
		dy[i] = dy[i] * (t - ti) / (tikp - ti) +	/* Calculate y. */
		    dy[i - 1] * (tikp - t) / (tikp - ti);
	    }
	}
    }
    *x = dx[j];
    *y = dy[j];
    free(dx);
    free(dy);
}

/*
 * Routine to get the i knot from uniform knot vector. The knot vector
 * might be float (Knot(i) = i) or open (where the first and last "order"
 * knots are equal). contr_isclosed determines knot kind - open contour means
 * open knot vector, and closed contour selects float knot vector.
 * Note the knot vector is not exist and this routine simulates it existance
 * Also note the indexes for the knot vector starts from 0.
 */
static double
fetch_knot(TBOOLEAN contr_isclosed, int num_of_points, int order, int i)
{
    if(! contr_isclosed) {
	if (i <= order)
	    return 0.0;
	else if (i <= num_of_points)
	    return (double) (i - order);
	else
	    return (double) (num_of_points - order);
    } else {
	return (double) i;
    }
}
