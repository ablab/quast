//Copyright (c) 2003 by  Mihaela Pertea


#include <malloc.h>

#define MAX_COEFFICIENT 	1.0
#define MAX_NO_OF_ATTRIBUTES	100
#define MAX_DT_DEPTH 		50 

#define LINESIZE 		80

#define TRUE 			1
#define FALSE 			0

typedef struct point
 {
   float *dimension;
   int category;
   double prob[2]; /* New for prob. classification, Added by Xin Chen   */
                   /* probability of point being classified as category */
   
   double val; /*Value obtained by substituting this point in the 
                equation of the hyperplane under consideration.
                This field is maintained to avoid redundant
                computation. */
 }POINT;

struct endpoint
 {
  float x,y;
 };

typedef struct edge
 {
  struct endpoint from,to;
 }EDGE;

struct tree_node
 {
  float *coefficients;
  int *left_count, *right_count;
  int left_total, right_total;  /* New for prob. classification */
                                /* Added by Xin Chen            */
  struct tree_node *parent,*left,*right;
  int left_cat,right_cat;
  char label[MAX_DT_DEPTH];
  float alpha; /* used only in error_complexity pruning. */
  int no_of_points;
  EDGE edge; /* used only in the display module. */
 };

void error(char *);
void free_ivector(int *,int,int);
void free_vector(float *,int,int);
void free_dvector(double*,int,float);
float myrandom(float,float);
float *vector(int,int);
double *dvector(int,int);
int *ivector(int,int);
float average(float*,int);
float sdev(float*,int);
