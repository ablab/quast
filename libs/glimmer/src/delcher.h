//Copyright (c) 2003 by A. L. Delcher and  Mihaela Pertea


//   A. L. Delcher
//
//     File:  ~delcher/TIGR/delcher.h
//  Version:  1.01  31 Jul 97
//
//
//  Common generic routines.
//


#ifndef  __DELCHER_H_INCLUDED
#define  __DELCHER_H_INCLUDED


#include  <stdio.h>
#include  <stdlib.h>
#include  <math.h>
#include  <string.h>
#include  <ctype.h>
#include  <limits.h>
#include  <float.h>
#include  <time.h>
#include  <assert.h>
#include  <errno.h>


#define  TRUE  1
#define  FALSE  0
#ifndef  EXIT_FAILURE
  #define  EXIT_FAILURE  -1
#endif
#ifndef  EXIT_SUCCESS
  #define  EXIT_SUCCESS  0
#endif


FILE *  File_Open  (const char *, const char *);
template <class DT>
  DT  Max  (DT, DT);
template <class DT>
  DT  Min  (DT, DT);
void *  Safe_calloc  (size_t, size_t);
void *  Safe_malloc  (size_t);
void *  Safe_realloc  (void *, size_t);
template <class DT>
void  Swap  (DT &, DT &);





template <class DT>
DT  Max  (DT A, DT B)

/* Return the larger of  A  and  B . */

  {
   if  (A > B)
       return  A;
     else
       return  B;
  }



template <class DT>
DT  Min  (DT A, DT B)

/* Return the smaller of  A  and  B . */

  {
   if  (A < B)
       return  A;
     else
       return  B;
  }

  
template <class DT>
void  Swap  (DT & A, DT & B)

/* Swap the values in  A  and  B . */

  {
   DT  Save;

   Save = A;
   A = B;
   B = Save;

   return;
  }


#endif
