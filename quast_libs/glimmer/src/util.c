//Copyright (c) 2003 by  Mihaela Pertea.

#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "gene.h"


/* prototypes */
int error(char *);
int *ivector(int, int);
float *vector(int, int);
double *dvector(int, int);
void free_ivector(int *, int, int);
void free_vector(float *, int,int);
void free_dvector(double *, int, int);
void usage(char *);

/****************************************************************/
/* Copyright 1993, 1994                                         */
/* Johns Hopkins University			                */
/* Department of Computer Science		                */
/****************************************************************/
/* Contact : murthy@cs.jhu.edu					*/
/****************************************************************/
/* File Name :	util.c						*/
/* Author : Sreerama K. Murthy					*/
/* Last modified : July 1994					*/
/* Contains modules : 	error					*/
/*			myrandom				*/
/*			vector					*/
/*			free_vector				*/
/*			ivector					*/
/*			free_ivector				*/
/*			dvector					*/
/*			free_dvector				*/
/* Uses modules in : None.					*/
/* Is used by modules in :	classify.c			*/
/*				compute_impurity.c		*/
/*				display.c			*/
/*				gendata.c			*/
/*				impurity_measure.c		*/
/*				load_data.c			*/
/*				mktree.c			*/
/*				perturb.c			*/
/*				tree_util.c			*/
/* Remarks       :	These are general-purpose utility       */
/*                      routines used by most of the modules    */
/*                      in the package.	                        */
/****************************************************************/	
#include <stdio.h>


/************************************************************************/
/* Module name : error							*/ 
/* Functionality :	Displays an error message, and exits execution	*/
/*			normally.					*/
/************************************************************************/
int error(char *error_text)
{
  printf("Runtime Error.\n%s.\nExecution Terminated.\n",error_text);
  exit(1);
  return(0);
}


/************************************************************************/
/* Module name : ivector						*/
/* Functionality :	Allocates a 1-D integer array, whose indices	*/
/*			range from "nl" through "nh", and returns a	*/
/*			pointer to this array.				*/
/* Parameters :	nl,nh : lowest and highest indices.			*/
/* Calls modules :	error						*/
/************************************************************************/
int *ivector(int nl, int nh)
{
  int *v;
  
  v=(int *)malloc((unsigned)(nh-nl+1)*sizeof(int));
  if (v==NULL) error("Ivector : Memory allocation failure.");
  return(v-nl);
}

/************************************************************************/
/* Module name : vector							*/
/* Functionality :	Allocates a 1-D float array, whose indices	*/
/*			range from "nl" through "nh", and returns a	*/
/*			pointer to this array.				*/
/* Parameters :	nl,nh : lowest and highest indices.			*/
/* Calls modules :	error						*/
/************************************************************************/
float *vector(int nl, int nh)
{
  float *v;
  
  v=(float *)malloc((unsigned)(nh-nl+1)*sizeof(float));
  if (v==NULL) error("Vector : Memory allocation failure.");
  return (v-nl);
}

/************************************************************************/
/* Module name : dvector						*/
/* Functionality :	Allocates a 1-D double array, whose indices	*/
/*			range from "nl" through "nh", and returns a	*/
/*			pointer to this array.				*/
/* Parameters :	nl,nh : lowest and highest indices.			*/
/* Calls modules :	error						*/
/************************************************************************/
double *dvector(int nl,int nh)
{
  double *v;
  
  v=(double *)malloc((unsigned)(nh-nl+1)*sizeof(double));
  if (v==NULL) error("Dvector : Memory allocation failure.");
  return (v-nl);
}

/************************************************************************/
/* Module name : free_ivector						*/
/* Functionality :	Frees a 1-D integer array. 			*/
/* Parameters :	v : pointer to the array				*/
/*		nl,nh : lowest and highest indices.			*/
/* Remarks: It is possible that the memory deallocation modules do not  */
/*          work well always. This should not be a major problem in most*/
/*          cases, however.                                             */
/************************************************************************/
void free_ivector(int *v, int nl, int nh)
{
  free((char*)(v+nl));
}

/************************************************************************/
/* Module name : free_vector						*/
/* Functionality :	Frees a 1-D float array. 			*/
/* Parameters :	v : pointer to the array				*/
/*		nl,nh : lowest and highest indices.			*/
/* Remarks: It is possible that the memory deallocation modules do not  */
/*          work well always. This should not be a major problem in most*/
/*          cases, however.                                             */
/************************************************************************/
void free_vector(float *v,int nl,int nh)
{
  free((char*)(v+nl));
}

/************************************************************************/
/* Module name : free_dvector						*/
/* Functionality :	Frees a 1-D double array. 			*/
/* Parameters :	v : pointer to the array				*/
/*		nl,nh : lowest and highest indices.			*/
/* Remarks: It is possible that the memory deallocation modules do not  */
/*          work well always. This should not be a major problem in most*/
/*          cases, however.                                             */
/************************************************************************/
void free_dvector(double *v, int nl, int nh)
{
  free((char*)(v+nl));
}

/************************************************************************/
/* Module name : Usage                                                  */ 
/* Functionality : Displays a list of possible options for MKTREE, GENDATA*/
/*                 and DISPLAY modules. Is activated in irritatingly    */
/*                 many situations. More precisely, whenever an incorrect*/
/*                 option is specified, or an option is accompanied by  */
/*                 incorrect argument, or when incorrect combinations of*/
/*                 options are used, this module is activated.          */
/* Parameters : pname: name of the program whose options are to be shown*/           
/************************************************************************/
void usage(char *pname)
{
  if (!strcmp(pname, "mktree"))
    {
      fprintf(stderr,"\n\nUsage: mktree aA:b:Bc:d:D:i:j:Kl:m:M:n:Nop:r:R:s:t:T:uvV:");
      fprintf(stderr,"\nOptions :");
      fprintf(stderr,"\n    -a : Only axis parallel splits.");
      fprintf(stderr,"\n    -A<file to output animation information to>");
      fprintf(stderr,"\n      (Default = No output)");
      fprintf(stderr,"\n    -b : bias towards axis parallel splits (>=1.0)");
      fprintf(stderr,"\n      (Default = 1.0)");
      fprintf(stderr,"\n    -B : Order of coeff. perturbation= Best First");
      fprintf(stderr,"\n    -c<number of classes> ");
      fprintf(stderr,"\n      (Default: computed from data or decision tree)");
      fprintf(stderr,"\n    -d<number of attributes> ");
      fprintf(stderr,"\n      (Default: computed from data or decision tree)");
      fprintf(stderr,"\n    -D<decision tree file>");
      fprintf(stderr,"\n      (Default=<training data>.dt, for outputting.)");
      fprintf(stderr,"\n    -i<#restarts for the perturbation alg.>");
      fprintf(stderr,"\n      (Default=20)");
      fprintf(stderr,"\n    -j<maximum number of random jumps");
      fprintf(stderr,"\n       tried at each local minimum> (Default = 5)");
      fprintf(stderr,"\n    -K : CART-linear combinations mode");
      fprintf(stderr,"\n    -l<log file>  (Default=oc1.log)");
      fprintf(stderr,"\n    -m<maximum number of random jumps");
      fprintf(stderr,"\n       tried at each local minimum> (Default = 5)");
      fprintf(stderr,"\n    -M<file to output misclassified instances to>");
      fprintf(stderr,"\n      (Default = No output)");
      fprintf(stderr,"\n    -n<number of training examples> ");
      fprintf(stderr,"\n    -N : No normalization at each tree node.");
      fprintf(stderr,"\n    -o : Only oblique splits.");
      fprintf(stderr,"\n    -p<portion of training set to be used in pruning>");
      fprintf(stderr,"\n      (Default=0.10 i.e., 10%%)");
      fprintf(stderr,"\n    -r<#restarts for the perturbation alg.>");
      fprintf(stderr,"\n      (Default=20)");
      fprintf(stderr,"\n    -R<cycle_count>");
      fprintf(stderr,"\n      Order of coeff. pert.= Random. Perturb Cycle_Count times.");
      fprintf(stderr,"\n    -s<integer seed for the random number generator>");
      fprintf(stderr,"\n    -t<file containing training data> (Default=None)");
      fprintf(stderr,"\n    -T<file containing testing data> (Default=None)");
      fprintf(stderr,"\n    -u : test data is unlabelled. Label it!");
      fprintf(stderr,"\n    -v : verbose if specified once.");
      fprintf(stderr,"\n         very verbose if specified more than once.");
      fprintf(stderr,"\n    -V<#partitions for cross validation>  (Default=0)");
      fprintf(stderr,"\n       (-1 : leave-one-out, 0 = no CV)");
    }
  
 if (!strcmp(pname,"display"))
    {
      fprintf (stderr,"\n\nUsage : display -d:D:eh:o:t:T:vw:x:X:y:Y:");
      fprintf (stderr,"\nOptions :");
      fprintf (stderr,"\n    -d<#dimensions> (Has to be 2)");
      fprintf (stderr,"\n    -D<File containing the Decision tree>");
      fprintf (stderr,"\n      (Default: None)");
      fprintf (stderr,"\n    -e : Erase Mode OFF.");
      fprintf (stderr,"\n       Produce animation without erasing any hyperplanes.");
      fprintf (stderr,"\n    -h<header (title) for the display>");
      fprintf (stderr,"\n      (Default=<datafile>-<decision tree file>)");
      fprintf (stderr,"\n    -o<file to write the PostScript(R) output>");
      fprintf (stderr,"\n      (Default=stdout)");
      fprintf (stderr,"\n    -t or -T <File containing the data points>");
      fprintf (stderr,"\n      (Default: None)");
      fprintf (stderr,"\n    -v : Verbose (Default=FALSE)");
      fprintf (stderr,"\n    -w<wait time between erasing one hyperplane and");
      fprintf (stderr,"\n       showing another, in the animation mode>");
      fprintf (stderr,"\n    -x<minimum x value>");
      fprintf (stderr,"\n      (Default=calculated from point set or 0)");
      fprintf (stderr,"\n    -X<maximum x coord for the display>");
      fprintf (stderr,"\n      (Default=calculated from point set or 1)");
      fprintf (stderr,"\n    -y<minimum y coord for the display>");
      fprintf (stderr,"\n      (Default=calculated from point set or 0)");
      fprintf (stderr,"\n    -Y<maximum y coord for the display>");
      fprintf (stderr,"\n      (Default=calculated from point set or 1)");
    }
  
  if (!strcmp(pname,"gendata"))
    {
      fprintf (stderr,"\n\nUsage : gendata -a:b:c:d:D:n:N:o:s:t:T:uv");
      fprintf (stderr,"\nOptions :");
      fprintf (stderr,"\n    -a<all attribute values >= this number>");
      fprintf (stderr,"\n         (Default=0)");
      fprintf (stderr,"\n    -b<all attribute values <= this number>");
      fprintf (stderr,"\n         (Default=1)");
      fprintf (stderr,"\n    -c<#categories. (Default=2)");
      fprintf (stderr,"\n    -d<#dimensions> (Default=2)");
      fprintf (stderr,"\n    -D<File containing the Decision tree>");
      fprintf (stderr,"\n      (Default: None)");
      fprintf (stderr,"\n    -n or N <number of points to be generated>");
      fprintf (stderr,"\n      (Default: None)");
      fprintf (stderr,"\n    -o<file to write the generated data> (Default=stdout)");
      fprintf (stderr,"\n    -t<file to write the generated data> (Default=stdout)");
      fprintf (stderr,"\n    -T<file to write the generated data> (Default=stdout)");
      fprintf (stderr,"\n    -s<integer seed for the random number generator>");
      fprintf (stderr,"\n    -u : Unlabeled Data. (Default=FALSE)");
      fprintf (stderr,"\n    -v : Verbose (Default=FALSE)");
    }

  fprintf (stderr,"\n\n");
  exit(0);
}

void *  Safe_calloc  (size_t N, size_t Len)

/* Allocate and return a pointer to enough memory to hold an
*  array with  N  entries of  Len  bytes each.  All memory is
*  cleared to 0.  Exit if fail. */

  {
   void  * P;

   P = calloc (N, Len);
   if  (P == NULL)
       {
        fprintf (stderr, "ERROR:  calloc failed\n");
        exit (EXIT_FAILURE);
       }

   return  P;
  }

void *  Safe_malloc  (size_t Len)

/* Allocate and return a pointer to  Len  bytes of memory.
*  Exit if fail. */

  {
   void  * P;

   P = malloc (Len);
   if  (P == NULL)
       {
        fprintf (stderr, "ERROR:  malloc failed\n");
        exit (EXIT_FAILURE);
       }

   return  P;
  }



void *  Safe_realloc  (void * Q, size_t Len)

/* Reallocate memory for  Q  to  Len  bytes and return a
*  pointer to the new memory.  Exit if fail. */

  {
   void  * P;

   P = realloc (Q, Len);
   if  (P == NULL)
       {
        fprintf (stderr, "ERROR:  realloc failed\n");
        exit (EXIT_FAILURE);
       }

   return  P;
  }





FILE *  File_Open  (const char * Filename, const char * Mode)

/* Open  Filename  in  Mode  and return a pointer to its control
*  block.  If fail, print a message and exit. */

  {
   FILE  *  fp;

   fp = fopen (Filename, Mode);
   if  (fp == NULL)
       {
        fprintf (stderr, "ERROR:  Could not open file  %s \n", Filename);
        exit (EXIT_FAILURE);
       }

   return  fp;
  }



char  Complement  (char Ch)

/* Returns the DNA complement of  Ch . */

  {
   switch  (tolower (Ch))
     {
      case  'a' :
        return  't';
      case  'c' :
        return  'g';
      case  'g' :
        return  'c';
      case  't' :
        return  'a';
      case  'r' :          // a or g
        return  'y';
      case  'y' :          // c or t
        return  'r';
      case  's' :          // c or g
        return  's';
      case  'w' :          // a or t
        return  'w';
      case  'm' :          // a or c
        return  'k';
      case  'k' :          // g or t
        return  'm';
      case  'b' :          // c, g or t
        return  'v';
      case  'd' :          // a, g or t
        return  'h';
      case  'h' :          // a, c or t
        return  'd';
      case  'v' :          // a, c or g
        return  'b';
      default :            // anything
        return  'n';
     }
  }



char  Filter  (char Ch)

//  Return a single  a, c, g or t  for  Ch .  Choice is to minimize likelihood
//  of a stop codon on the primary strand.

  {
   switch  (tolower (Ch))
     {
      case  'a' :
      case  'c' :
      case  'g' :
      case  't' :
        return  Ch;
      case  'r' :     // a or g
        return  'g';
      case  'y' :     // c or t
        return  'c';
      case  's' :     // c or g
        return  'c';
      case  'w' :     // a or t
        return  't';
      case  'm' :     // a or c
        return  'c';
      case  'k' :     // g or t
        return  't';
      case  'b' :     // c, g or t
        return  'c';
      case  'd' :     // a, g or t
        return  'g';
      case  'h' :     // a, c or t
        return  'c';
      case  'v' :     // a, c or g
        return  'c';
      default :       // anything
        return  'c';
    }
  }



int  Read_String  (FILE * fp, char * & T, long int & Size, char Name [],
                   int Partial)

/* Read next string from  fp  (assuming FASTA format) into  T [1 ..]
*  which has  Size  characters.  Allocate extra memory if needed
*  and adjust  Size  accordingly.  Return  TRUE  if successful,  FALSE
*  otherwise (e.g., EOF).  Partial indicates if first line has
*  numbers indicating a subrange of characters to read.  If  Partial  is
*  true, then the first line must have 2 integers indicating positions
*  in the string and only those positions will be put into  T .  If
*  Partial  is false, the entire string is put into  T .  Sets  Name
*  to the first string after the starting '>' character. */

  {
   char  * P, Line [MAX_LINE];
   long int  Len, Lo, Hi;
   int  Ch, Ct = FALSE;

   while  ((Ch = fgetc (fp)) != EOF && Ch != '>')
     ;

   if  (Ch == EOF)
       return  FALSE;

   fgets (Line, MAX_LINE, fp);
   Len = strlen (Line);
   assert (Len > 0 && Line [Len - 1] == '\n');
   P = strtok (Line, " \t\n");
   if  (P != NULL)
       strcpy (Name, P);
     else
       Name [0] = '\0';
   Lo = 0;  Hi = LONG_MAX;
   if  (Partial)
       {
        P = strtok (NULL, " \t\n");
        if  (P != NULL)
            {
             Lo = strtol (P, NULL, 10);
             P = strtok (NULL, " \t\n");
             if  (P != NULL)
                 Hi = strtol (P, NULL, 10);
            }
        assert (Lo <= Hi);
       }

   Ct = 0;
   T [0] = '\0';
   Len = 1;
   while  ((Ch = fgetc (fp)) != EOF && Ch != '>')
     {
      if  (isspace (Ch))
          continue;

      Ct ++;
      if  (Ct < Lo || Ct > Hi)
          continue;

      if  (Len >= Size)
          {
           Size += INCR_SIZE;
           T = (char *) Safe_realloc (T, Size);
          }
      Ch = tolower (Ch);
      switch  (Ch)
        {
         case  'a' :
         case  'c' :
         case  'g' :
         case  't' :
         case  's' :
         case  'w' :
         case  'r' :
         case  'y' :
         case  'm' :
         case  'k' :
         case  'b' :
         case  'd' :
         case  'h' :
         case  'v' :
         case  'n' :
           break;
         default :
           fprintf (stderr, "Unexpected character `%c\' in string %s\n",
                                 Ch, Name);
           Ch = 'n';
        }
      T [Len ++] = Ch;
     }

   T [Len] = '\0';
   if  (Ch == '>')
       ungetc (Ch, fp);

   return  TRUE;
  }

int  Read_Multi_String  (FILE * fp, char * & T, long int & Size, char Name [],
                   int Partial, int no)

/* Read next string #no from  fp  (assuming MULTIFASTA format) into  T [1 ..]
*  which has  Size  characters.  Allocate extra memory if needed
*  and adjust  Size  accordingly.  Return  TRUE  if successful,  FALSE
*  otherwise (e.g., EOF).  Partial indicates if first line has
*  numbers indicating a subrange of characters to read.  If  Partial  is
*  true, then the first line must have 2 integers indicating positions
*  in the string and only those positions will be put into  T .  If
*  Partial  is false, the entire string is put into  T .  Sets  Name
*  to the first string after the starting '>' character. */

  {
   char  * P, Line [MAX_LINE];
   long int  Len, Lo, Hi;
   int  Ch, Ct = FALSE;
   int i;

   i=0;

   while(i<no) {
     while  ((Ch = fgetc (fp)) != EOF && Ch != '>')
       ;

     if  (Ch == EOF)
       return  FALSE;
     if (Ch == '>') i++;
   }

   fgets (Line, MAX_LINE, fp);
   Len = strlen (Line);
   assert (Len > 0 && Line [Len - 1] == '\n');
   P = strtok (Line, " \t\n");
   if  (P != NULL)
       strcpy (Name, P);
     else
       Name [0] = '\0';
   Lo = 0;  Hi = LONG_MAX;
   if  (Partial)
       {
        P = strtok (NULL, " \t\n");
        if  (P != NULL)
            {
             Lo = strtol (P, NULL, 10);
             P = strtok (NULL, " \t\n");
             if  (P != NULL)
                 Hi = strtol (P, NULL, 10);
            }
        assert (Lo <= Hi);
       }

   Ct = 0;
   T [0] = '\0';
   Len = 1;
   while  ((Ch = fgetc (fp)) != EOF && Ch != '>')
     {
      if  (isspace (Ch))
          continue;

      Ct ++;
      if  (Ct < Lo || Ct > Hi)
          continue;

      if  (Len >= Size)
          {
           Size += INCR_SIZE;
           T = (char *) Safe_realloc (T, Size);
          }
      Ch = tolower (Ch);
      switch  (Ch)
        {
         case  'a' :
         case  'c' :
         case  'g' :
         case  't' :
         case  's' :
         case  'w' :
         case  'r' :
         case  'y' :
         case  'm' :
         case  'k' :
         case  'b' :
         case  'd' :
         case  'h' :
         case  'v' :
         case  'n' :
           break;
         default :
           fprintf (stderr, "Unexpected character `%c\' in string %s\n",
                                 Ch, Name);
           Ch = 'n';
        }
      T [Len ++] = Ch;
     }

   T [Len] = '\0';
   if  (Ch == '>')
       ungetc (Ch, fp);

   return  TRUE;
  }


/************************************************************************/
/************************************************************************/
