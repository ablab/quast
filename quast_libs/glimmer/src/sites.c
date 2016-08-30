//Copyright (c) 2003 by Mihaela Pertea

#include  <stdio.h>
#include  <math.h>
#include  <stdlib.h>
#include  <string.h>
#include  <ctype.h>
#include  <limits.h>
#include  <float.h>
#include  <time.h>
#include  <assert.h>

#include "sites.h"

#define  TRUE  1
#define  FALSE  0
#define  ACCEPTOR_LEN  29                    /* Positions +44,72 in a80 */
#define  ACCEPTOR_FILE_NAME "acc"
#define  ACCEPTOR_TREE_FILE "outex"
#define  ACCEPTOR_SIGNAL_OFFSET  24          /* Start of  AG  */

#define  ATG_LEN  19                    /* Positions +0,18 in a19 */
#define  ATG_SIGNAL_OFFSET  12          /* Start of  ATG  */
#define  ATG_FILE_NAME "atg.markov"

#define  STOP_LEN  19                    /* Positions +0,18 in a19 */
#define  STOP_SIGNAL_OFFSET  4          /* Start of  ATG  */
#define  STOP_FILE_NAME "stop.markov"
   
#define  DONOR_LEN  16                        /* Positions +5,20 in d80 */
#define  DONOR_FILE_NAME "don"
#define  DONOR_TREE_FILE "outin"
#define  DONOR_SIGNAL_OFFSET  5               /* Start of  GT  */

#define  AD_LEN  40                        /* Positions +5,20 in d80 */
#define  AD_FILE_NAME  "ad.markov"
#define  AD_SIGNAL_OFFSET  18              /* Start of  GT  */
#define  AD_THRESHOLD  0.084694            /* For 4 false negatives */
   

#define  MARKOV_DEGREE  3
#define  MARKOV_LEN  64                     /* ALPHABET_SIZE ^ MARKOV_DEGREE */
#define  LOG_PROB_ACCEPTOR  log (1682.0 / 30191.0)
#define  LOG_PROB_NONACCEPTOR  log (28509.0 / 30191.0)
#define  LOG_PROB_DONOR  log (1682.0 / 30191.0)         /* Change this */
#define  LOG_PROB_NONDONOR  log (28509.0 / 30191.0)     /* Change this */
#define  LOW_SCORE  -99.0  /* Score if pattern does not have GT or AG signal */
#define  RETURN_TRUE_PROB  0

#define CODING_LEN 80

#ifndef  EXIT_FAILURE
  #define  EXIT_FAILURE  -1
#endif
#ifndef  EXIT_SUCCESS
  #define  EXIT_SUCCESS  0
#endif

typedef struct tree {
	int val;
   	int consens;
   	int poz;
   	int no;
   	struct tree *left;
   	struct tree *right;
   } tree;

typedef unsigned int word;

int  Acc  (const int *, double *,double, tree *t,int ind, char *,int *leaf);
int  Don  (const int *, double *,double, tree *t,int ind, char *,int *leaf);
int comp(const void *a, const void *b);
int findfile(const int * S, tree *t);
int readtree(char *line, tree *t, int start);
int find(char *line, int start);
int  Is_Cod_NonCod  (const int * , double *, int ind, char *);
int  Is_Cod_NonCodATG  (const int * , double *, int ind, char *);
int  Is_Cod_NonCodSTOP  (const int * , double *, int ind, char *);
float ****Load4dim(int d1, int d2, int d3, int d4);
void free4dim(int ****ptr,int d1, int d2, int d3);

#define  Start_PosEx 56
#define  Stop_PosEx 84

#define  Start_PosIn 75
#define  Stop_PosIn 90

#define  Start_Cod 0
#define  Stop_Cod 79

#define Start_NoCod 82
#define Stop_NoCod 161


int markov_degree;
int markov_len;
tree *tacc;
tree *tdon;
int readtacc=FALSE;
int readtdon=FALSE;
int accmax;
int donmax;
float  ****Acc_Positive_Table;
float  ****Acc_Negative_Table;
int *Acc_Tables_Loaded;
float  ****Don_Positive_Table;
float  ****Don_Negative_Table;
int *Don_Tables_Loaded;
float  Atg_Positive_Table [ATG_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
float  Atg_Negative_Table [ATG_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
int  Atg_Tables_Loaded = FALSE;
float  Cod_Positive_Table [4][CODING_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
float  Cod_Negative_Table [4][CODING_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
int  Cod_Tables_Loaded[4] = {FALSE,FALSE,FALSE,FALSE};
float  Stop_Positive_Table [STOP_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
float  Stop_Negative_Table [STOP_LEN] [ALPHABET_SIZE] [MARKOV_LEN];
int  Stop_Tables_Loaded = FALSE;

void free4dim(float ****ptr,int d1, int d2, int d3)
{
  int i,j,k;

  for(i=0;i<d1;i++) {
    for(j=0;j<d2;j++) {
      for(k=0;k<d3;k++) 
	if(ptr[i][j][k] != NULL ) free(ptr[i][j][k]);
      if(ptr[i][j] != NULL ) free(ptr[i][j]);
    }
    if(ptr[i] != NULL ) free(ptr[i]);
  }
  free(ptr);
}


void freetree(tree *t)
{
  if(t==NULL) return;
  freetree(t->left);
  freetree(t->right);
  free(t);
  t=NULL;
}

void UnLoadTables()
{
  int i;

  if(readtacc) {
    free4dim(Acc_Positive_Table,accmax,ACCEPTOR_LEN,ALPHABET_SIZE);
    free4dim(Acc_Negative_Table,accmax,ACCEPTOR_LEN,ALPHABET_SIZE);
    if(Acc_Tables_Loaded  != NULL ) free(Acc_Tables_Loaded);
  }

  if(readtdon) {
    free4dim(Don_Positive_Table,donmax,DONOR_LEN,ALPHABET_SIZE);
    free4dim(Don_Negative_Table,donmax,DONOR_LEN,ALPHABET_SIZE);
    if(Don_Tables_Loaded != NULL ) free(Don_Tables_Loaded);
  }

  readtacc=FALSE;
  readtdon=FALSE;
  freetree(tacc);
  freetree(tdon);

  Atg_Tables_Loaded = FALSE;
  Stop_Tables_Loaded = FALSE;
  for(i=0;i<4;i++) 
    Cod_Tables_Loaded[i]=FALSE;

}

float ****Load4dim(int d1, int d2, int d3, int d4)
{
  int i,j,k;
  float ****ptr;
  
  ptr = (float ****) malloc(d1 * sizeof(float ***));
  if(ptr==NULL) {
    fprintf(stderr,"Memory allocation for splice site tables failed.\n");
    abort();
  }
  for(i=0;i<d1;i++) {
    ptr[i] = (float ***) malloc(d2 * sizeof(float **));
    if(ptr[i]==NULL) {
      fprintf(stderr,"Memory allocation for splice site tables failed.\n");
      abort();
    }
    for(j=0;j<d2;j++) {
      ptr[i][j] = (float **) malloc(d3*sizeof(float *));
      if(ptr[i][j]==NULL) {
	fprintf(stderr,"Memory allocation for splice site tables failed.\n");
	abort();
      }
      for(k=0;k<d3;k++) {
	ptr[i][j][k] = (float *) malloc(d4*sizeof(float));
	if(ptr[i][j][k]==NULL) {
	  fprintf(stderr,"Memory allocation for splice site tables failed.\n");
	  abort();
	}
      }
    }
  }

  return(ptr);
}


int Is_Acceptor(const int *B, double *Return_Score,int * leaf, double ACCEPTOR_THRESHOLD, int istacc,char *TRAIN_DIR, int nocod,int md)
{
  FILE  * Infile;
  double Score,S1,S2;
  char line[5000];
  int i,ind;
  int T[100];
  double score1,score2,score3;
  char accname[600];
  
  markov_degree=md;
  markov_len=(int)pow(ALPHABET_SIZE,md);

  if(istacc && !readtacc) {
    
    
    // read the structure of the acceptor tree 

    sprintf(accname,"%s%s",TRAIN_DIR,ACCEPTOR_TREE_FILE);

    Infile = fopen (accname, "r");
    if  (Infile == NULL)
      {
	fprintf (stderr, "ERROR:  Unable to open file %s\n", ACCEPTOR_TREE_FILE);
	exit (EXIT_FAILURE);
      }
    
    tacc = (tree *) malloc(sizeof(tree));
    if (tacc == NULL) {fprintf(stderr," Memory allocation for tree failure.\n"); abort();}
    fgets(line, 5000, Infile);
    i=strlen(line);
    line[i-1]='\0';
    fclose(Infile);
    
    accmax=readtree(line, tacc, 0);
    
    readtacc=TRUE;
    
    // alloc memory for the tables
    Acc_Positive_Table=Load4dim(accmax,ACCEPTOR_LEN,ALPHABET_SIZE,markov_len);
    Acc_Negative_Table=Load4dim(accmax,ACCEPTOR_LEN,ALPHABET_SIZE,markov_len);
    Acc_Tables_Loaded=(int *) malloc(accmax*sizeof(int));
    if(Acc_Tables_Loaded == NULL) {
      fprintf(stderr,"Memory allocation for acceptor site tables failed.\n");
      abort();
    }
    for(i=0;i<accmax;i++) Acc_Tables_Loaded[i]=FALSE;      
  }
  else if(!istacc && !readtacc) {
    readtacc=TRUE;
    tacc=NULL;
    accmax=1;
    Acc_Positive_Table=Load4dim(1,ACCEPTOR_LEN,ALPHABET_SIZE,markov_len);
    Acc_Negative_Table=Load4dim(1,ACCEPTOR_LEN,ALPHABET_SIZE,markov_len);
    Acc_Tables_Loaded=(int *) malloc(sizeof(int));
    if(Acc_Tables_Loaded == NULL) {
      fprintf(stderr,"Memory allocation for acceptor site tables failed.\n");
      abort();
    }
    Acc_Tables_Loaded[0]=FALSE;   
  }

  for(i=0;i<=Stop_PosEx-Start_PosEx;i++)
    T[i]=B[i+Start_PosEx];

  ind=Acc(T, &S1, ACCEPTOR_THRESHOLD, tacc,0,TRAIN_DIR,leaf);
  if(ind==0) return(0);

  if(istacc) Acc(T, &S2, ACCEPTOR_THRESHOLD, tacc,1,TRAIN_DIR,leaf);
  else S2=S1;
  score1=(S1+S2)/2;

  //  if(score1<=THR_ACC) score1=-99;
  
  score2=0;
  score3=0;
    
  if(!nocod) {
    for(i=0;i<=Stop_NoCod-Start_NoCod;i++)
      T[i]=B[i+Start_NoCod];

    Is_Cod_NonCod(T,&score2,0,TRAIN_DIR);

    for(i=0;i<=Stop_Cod-Start_Cod;i++)
      T[i]=B[i+Start_Cod];
  

    //  if(score2<=THR_ACC_EX) score2=-99;

    Is_Cod_NonCod(T,&score3,1,TRAIN_DIR);
    
    //  if(score3<=THR_ACC_IN) score3=-99;
  }
    
  Score=score1+score2+score3;

  *Return_Score=Score;
  
  return Score >= ACCEPTOR_THRESHOLD;
	  
      
}  

int Is_Donor(const int *B, double *Return_Score, int *leaf, double DONOR_THRESHOLD, int istdon,char *TRAIN_DIR,int nocod,int md)
{
  FILE  * Infile;
  double Score,S1,S2;
  char line[5000];
  int ind,i;
  int T[100];
  double score1,score2,score3;
  char donname[600];

  markov_degree=md;
  markov_len=(int)pow(ALPHABET_SIZE,md);

  if(istdon && !readtdon) {
    
   
    // read the structure of the donor tree 
    sprintf(donname,"%s%s",TRAIN_DIR,DONOR_TREE_FILE);
    Infile = fopen (donname, "r");
    if  (Infile == NULL)
      {
	fprintf (stderr, "ERROR:  Unable to open file %s\n", DONOR_TREE_FILE);
	exit (EXIT_FAILURE);
      }
    
    tdon = (tree *) malloc(sizeof(tree));
    if (tdon == NULL) {fprintf(stderr,"Memory allocation for tree failure.\n"); abort();}
    fgets(line, 5000, Infile);
    i=strlen(line);
    line[i-1]='\0';
    fclose(Infile);
    
    donmax=readtree(line, tdon, 0);
    readtdon=TRUE;

    // alloc memory for the tables
    Don_Positive_Table=Load4dim(donmax,DONOR_LEN,ALPHABET_SIZE,markov_len);
    Don_Negative_Table=Load4dim(donmax,DONOR_LEN,ALPHABET_SIZE,markov_len);
    Don_Tables_Loaded=(int *) malloc(donmax*sizeof(int));
    if(Don_Tables_Loaded == NULL) {
      fprintf(stderr,"Memory allocation for donor site tables failed.\n");
      abort();
    }
    for(i=0;i<donmax;i++) Don_Tables_Loaded[i]=FALSE;   

  }
  else if(!istdon && !readtdon) {
    readtdon=TRUE;
    tdon=NULL;
    donmax=1;
    Don_Positive_Table=Load4dim(1,DONOR_LEN,ALPHABET_SIZE,markov_len);
    Don_Negative_Table=Load4dim(1,DONOR_LEN,ALPHABET_SIZE,markov_len);
    Don_Tables_Loaded=(int *) malloc(sizeof(int));
    if(Don_Tables_Loaded == NULL) {
      fprintf(stderr,"Memory allocation for donor site tables failed.\n");
      abort();
    }
    Don_Tables_Loaded[0]=FALSE;   
  }

  for(i=0;i<=Stop_PosIn-Start_PosIn;i++)
    T[i]=B[i+Start_PosIn];

  ind=Don(T, &S1, DONOR_THRESHOLD, tdon,0,TRAIN_DIR,leaf);
  if(ind==0) return(0);
  if(istdon) Don(T, &S2, DONOR_THRESHOLD, tdon,1,TRAIN_DIR,leaf);
  else S2=S1;
  score1=(S1+S2)/2;


  //  if(score1<=THR_DON) score1=-99;

  score2=0;
  score3=0;

  if(!nocod) {
    for(i=0;i<=Stop_Cod-Start_Cod;i++)
      T[i]=B[i+Start_Cod];

    Is_Cod_NonCod(T,&score2,2,TRAIN_DIR);

    //  if(score2<=THR_DON_EX) score2=-99;
  
    for(i=0;i<=Stop_NoCod-Start_NoCod;i++)
      T[i]=B[i+Start_NoCod];

    Is_Cod_NonCod(T,&score3,3,TRAIN_DIR);

    //  if(score3<=THR_DON_IN) score3=-99;
  }

  Score=score1+score2+score3;

  *Return_Score=Score;
  
  return Score >= DONOR_THRESHOLD;
	  
      
}  


    
int readtree(char *line, tree *t, int start)
{
 int len;
 int i,n;
 int val,valmax;
 char part[10];
 len=strlen(line);

 i=start;
 while((line[i]=='(')||(line[i]==' ')) i++;
 n=i;
 while(line[i]!=' ')
 {
	part[i-n]=line[i];
	i++;
 }
 part[i-n]='\0';
 t->val=atoi(part);
 valmax=t->val;

 i++;
 n=i;
 while(line[i]!=' ')
 { 
	part[i-n]=line[i];
	i++;
 }
 part[i-n]='\0';
 t->consens=atoi(part);

 i++;
 n=i;
 while(line[i]!=' ')
 { 
	part[i-n]=line[i];
	i++;
 }
 part[i-n]='\0';
 t->poz=atoi(part);

 i++;
 n=i;
 while(line[i]!=' ')
 { 
	part[i-n]=line[i];
	i++;
 }
 part[i-n]='\0';
 t->no=atoi(part);

 t->left=NULL;
 t->right=NULL;

 i+=2;n=i;
 if(line[i]=='(') 
   {
     i=find(line,i+1);
     t->left = (tree *) malloc(sizeof(tree));
     if (t->left == NULL) {fprintf(stderr,"Memory allocation for tree failure.\n"); abort();}
     val=readtree(line,t->left,n);
     if(val>valmax) valmax=val;
   }
	
 i+=2;n=i;
 if(line[i]=='(') 
   {
     i=find(line,i+1);
     t->right = (tree *) malloc(sizeof(tree));
     if (t->right == NULL) {fprintf(stderr,"Memory allocation for tree failure.\n"); abort();}
     val=readtree(line,t->right,n);
     if(val>valmax) valmax=val;
   }
 valmax++;
 return(valmax);
}

int find(char *line, int start)
{
 int stop,i;

 i=start;

 while(line[i]!=')')
 	if(line[i]=='(') i=find(line,i+1);
 	else i++;
 stop=i+1;
 return(stop);
}
 	

int comp(const void *a, const void *b)
{ 
  if(*(double *)a > *(double *)b) return(1);
  else if (*(double *)a==*(double *)b) return(0);
  else return(-1);

}  
  

int findfile(const int * S, tree *t)
{
	int val, cons, poz;
	val=t->val;

	cons=t->consens;
	if( cons !=-1)
	{ 
		poz=t->poz;
	    if(S[poz]==cons)
	    	val=findfile(S,t->left);
	    else val=findfile(S, t->right);
	}

	return(val);
}

int findleaf(tree *t, int n, int leaf, int *found) 
{
  int ret=n;

  if(t==NULL) { fprintf(stderr,"tree NULL\n");exit(0);}

  if(t->val == leaf) {*found=1; return(n+1);}

  if(t->left == NULL && t->right == NULL)  return(n+1);
  if(t->left != NULL) ret=findleaf(t->left,n,leaf,found);
  if(!(*found) && t->right != NULL) ret=findleaf(t->right,ret,leaf,found);
  
  return(ret);
}
   
  
  



int  Acc  (const int * S, double * Return_Score, double ACCEPTOR_THRESHOLD, tree *t,int ind,char *TRAIN_DIR,int *leaf)

/* Evaluate string  S [0 .. (ACCEPTOR_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely acceptor
*  site.  Also set  Return_Score  to the probability that it is an acceptor
*  site. */

{
  FILE  * Infile;
  double  Positive_Sum, Negative_Sum, Score;
  char accname[600];
#if  RETURN_TRUE_PROB
  double  X, Y;
#endif
  int  i, j, k, Ct, Sub, no;

/* see which acceptor you should use */

  if(ind) {
    no=findfile(S,t);
    sprintf(accname,"%s%s%d",TRAIN_DIR,ACCEPTOR_FILE_NAME,no);
    k=0;

    *leaf=findleaf(t,-1,no,&k);

    if(!k) { fprintf(stderr,"Leaf %d not found in acceptor tree!!!\n",no);exit(0);} 
  }
  else {
    strcpy(accname,TRAIN_DIR);
    strcat(accname,"acc1.mar");
    no=0;
  }

   if  (! Acc_Tables_Loaded[no])
       {
        Infile = fopen (accname, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open acceptor file \"%s\"\n",
                        accname);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < ACCEPTOR_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Acc_Positive_Table [no][i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading acceptor file \"%s\"\n", 
                                ACCEPTOR_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < ACCEPTOR_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Acc_Negative_Table [no][i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading acceptor file \"%s\"\n", 
                                ACCEPTOR_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Acc_Tables_Loaded[no]  = TRUE;
       }

   if  (S [ACCEPTOR_SIGNAL_OFFSET] != 0
           || S [ACCEPTOR_SIGNAL_OFFSET + 1] != 2)    /* AG */
       {
        * Return_Score = LOW_SCORE;
        return  FALSE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Acc_Positive_Table [no][markov_degree - 1] [0] [Sub];
   Negative_Sum = Acc_Negative_Table [no][markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < ACCEPTOR_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Acc_Positive_Table [no] [i] [j] [Sub];
      Negative_Sum += Acc_Negative_Table [no] [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }
  


   Score = Positive_Sum - Negative_Sum;

#if  RETURN_TRUE_PROB
   X = exp (Positive_Sum + LOG_PROB_ACCEPTOR);
   Y = exp (Negative_Sum + LOG_PROB_NONACCEPTOR);
   * Return_Score = log (X / (X + Y));
#else
   * Return_Score = Score;
#endif

   return(1);
  }



int  Don  (const int * S, double * Return_Score, double DONOR_THRESHOLD, tree *t,int ind,char *TRAIN_DIR, int *leaf)

/* Evaluate string  S [0 .. (DONOR_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely donor
*  site.  Also set  Return_Score  to the probability that it is an donor
*  site. */
{
   FILE  * Infile;
   double  Positive_Sum, Negative_Sum, Score;
   char donname[600];
   int no;

#if  RETURN_TRUE_PROB
   double  X, Y;
#endif
   int  i, j, k, Ct, Sub;

   /* see which donor file you should use */
   if(ind) {
     no=findfile(S,t);
     sprintf(donname,"%s%s%d",TRAIN_DIR,DONOR_FILE_NAME,no);
     k=0;
     *leaf=findleaf(t,-1,no,&k);
     if(!k) { fprintf(stderr,"Leaf %d not found in acceptor tree!!!\n",no);exit(0);} 
   }
   else 
     {
       strcpy(donname,TRAIN_DIR);
       strcat(donname,"don1.mar");
       no=0;
     }

   if  (! Don_Tables_Loaded[no] )
       {

        Infile = fopen (donname, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open donor file \"%s\"\n",
                        donname);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < DONOR_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Don_Positive_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading donor file \"%s\"\n", 
                                DONOR_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < DONOR_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Don_Negative_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading donor file \"%s\"\n", 
                                DONOR_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Don_Tables_Loaded [no] = TRUE;
       }


   if  (S [DONOR_SIGNAL_OFFSET] != 2
           || S [DONOR_SIGNAL_OFFSET + 1] != 3)    /* GT */
       {
        * Return_Score = LOW_SCORE;
        return  FALSE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Don_Positive_Table [no] [markov_degree - 1] [0] [Sub];
   Negative_Sum = Don_Negative_Table [no] [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < DONOR_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Don_Positive_Table [no] [i] [j] [Sub];
      Negative_Sum += Don_Negative_Table [no] [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }
 
   Score = Positive_Sum - Negative_Sum;

#if  RETURN_TRUE_PROB
   X = exp (Positive_Sum + LOG_PROB_DONOR);
   Y = exp (Negative_Sum + LOG_PROB_NONDONOR);
   * Return_Score = log (X / (X + Y));
#else
   * Return_Score = Score;
#endif

	if(Score==-99) printf("look one\n");

   return(1);
  }

int  Is_Atg162  (const int * S, double * Return_Score, double ATG_THRESHOLD,char *TRAIN_DIR,int md )

     /* Evaluate string  S [0 .. (ATG_LEN -1)] and
      *  return  TRUE  or  FALSE  as to whether it is a likely atg
      *  site.  Also set  Return_Score  to the probability that it is an atg
      *  site. */
     
{ 
  double score1,score2,score3;
  double Score;

  Is_Atg(S+68,&score1,ATG_THRESHOLD,TRAIN_DIR,md);

  Is_Cod_NonCodATG(S+82,&score2,0,TRAIN_DIR);

  Is_Cod_NonCodATG(S,&score3,1,TRAIN_DIR);
  
  Score=score1+score2+score3;
  * Return_Score = Score;

  return Score >= ATG_THRESHOLD;
}


int  Is_Atg  (const int * S, double * Return_Score, double ATG_THRESHOLD,char *TRAIN_DIR,int md )

/* Evaluate string  S [0 .. (ATG_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely atg
*  site.  Also set  Return_Score  to the probability that it is an atg
*  site. */

{
   FILE  * Infile;
   double  Positive_Sum, Negative_Sum, Score;
   int i, j, k, Ct, Sub;
   char atgname[600];

   markov_degree=md;
   markov_len=(int)pow(ALPHABET_SIZE,md);

   if  (! Atg_Tables_Loaded)
       {
	sprintf(atgname,"%s%s",TRAIN_DIR,ATG_FILE_NAME); 
        Infile = fopen (atgname, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open atg file \"%s\"\n",
                        ATG_FILE_NAME);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < ATG_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Atg_Positive_Table [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading atg file \"%s\"\n", 
                                atgname);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < ATG_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Atg_Negative_Table [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading atg file \"%s\"\n", 
                                ATG_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Atg_Tables_Loaded = TRUE;
       }

   if  (S [ATG_SIGNAL_OFFSET] != 0
	   || S [ATG_SIGNAL_OFFSET + 1] != 3
           || S [ATG_SIGNAL_OFFSET + 2] != 2)    /* ATG */
       {
        * Return_Score = LOW_SCORE;
        return  FALSE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Atg_Positive_Table [markov_degree - 1] [0] [Sub];
   Negative_Sum = Atg_Negative_Table [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < ATG_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Atg_Positive_Table [i] [j] [Sub];
      Negative_Sum += Atg_Negative_Table [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }

   Score = Positive_Sum - Negative_Sum;


   * Return_Score = Score;


   return  Score >= ATG_THRESHOLD;
  }




int  Is_Cod_NonCodATG  (const int * S, double * Return_Score, int ind,char *TRAIN_DIR)

/* Evaluate string  S [0 .. (CODING_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely donor
*  site.  Also set  Return_Score  to the probability that it is an donor
*  site. */

  {
   FILE  * Infile;
   static float  Positive_Table [2][CODING_LEN] [ALPHABET_SIZE] [4];
   static float  Negative_Table [2][CODING_LEN] [ALPHABET_SIZE] [4];
   static  int  Tables_Load[2] = {FALSE,FALSE};
   double  Positive_Sum, Negative_Sum, Score, Threshold;
   char filename[600];
   int no;

   int markov_len=4;
   int markov_degree=1;

#if  RETURN_TRUE_PROB
   double  X, Y;
#endif
   int  i, j, k, Ct, Sub;

   no=ind;

   switch (no) {
   case 0: // case of exon in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_ex.atg");
     Threshold = 0;
     break;
   case 1: // case of intron in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_in.atg");
     Threshold = 0;
     break;
   }

   if  (! Tables_Load[no] )
       {
        Infile = fopen (filename, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open atg file \"%s\"\n",
                        filename);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Positive_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading atg file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Negative_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading atg file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Tables_Load [no] = TRUE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Positive_Table [no] [markov_degree - 1] [0] [Sub];
   Negative_Sum = Negative_Table [no] [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < CODING_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Positive_Table [no] [i] [j] [Sub];
      Negative_Sum += Negative_Table [no] [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }
 


   Score = Positive_Sum - Negative_Sum;

#if  RETURN_TRUE_PROB
   X = exp (Positive_Sum + LOG_PROB_DONOR);
   Y = exp (Negative_Sum + LOG_PROB_NONDONOR);
   * Return_Score = log (X / (X + Y));
#else
   * Return_Score = Score;
#endif

	if(Score==-99) printf("look one\n");

   return  Score >= Threshold;
  }


int  Is_Cod_NonCodSTOP  (const int * S, double * Return_Score, int ind,char *TRAIN_DIR)

/* Evaluate string  S [0 .. (CODING_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely donor
*  site.  Also set  Return_Score  to the probability that it is an donor
*  site. */

  {
   FILE  * Infile;
   static float  Positive_Table [2][CODING_LEN] [ALPHABET_SIZE] [4];
   static float  Negative_Table [2][CODING_LEN] [ALPHABET_SIZE] [4];
   static  int  Tables_Load[2] = {FALSE,FALSE};
   double  Positive_Sum, Negative_Sum, Score, Threshold;
   char filename[600];
   int no;

   int markov_len=4;
   int markov_degree=1;

#if  RETURN_TRUE_PROB
   double  X, Y;
#endif
   int  i, j, k, Ct, Sub;

   no=ind;

   switch (no) {
   case 0: // case of exon in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_ex.stop");
     Threshold = 0;
     break;
   case 1: // case of intron in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_in.stop");
     Threshold = 0;
     break;
   }

   if  (! Tables_Load[no] )
       {
        Infile = fopen (filename, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open atg file \"%s\"\n",
                        filename);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Positive_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading stop file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Negative_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading stop file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Tables_Load [no] = TRUE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Positive_Table [no] [markov_degree - 1] [0] [Sub];
   Negative_Sum = Negative_Table [no] [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < CODING_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Positive_Table [no] [i] [j] [Sub];
      Negative_Sum += Negative_Table [no] [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }
 


   Score = Positive_Sum - Negative_Sum;

#if  RETURN_TRUE_PROB
   X = exp (Positive_Sum + LOG_PROB_DONOR);
   Y = exp (Negative_Sum + LOG_PROB_NONDONOR);
   * Return_Score = log (X / (X + Y));
#else
   * Return_Score = Score;
#endif

	if(Score==-99) printf("look one\n");

   return  Score >= Threshold;
  }

int  Is_Cod_NonCod  (const int * S, double * Return_Score, int ind,char *TRAIN_DIR)

/* Evaluate string  S [0 .. (CODING_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely donor
*  site.  Also set  Return_Score  to the probability that it is an donor
*  site. */

  {
   FILE  * Infile;
   double  Positive_Sum, Negative_Sum, Score, Threshold;
   char filename[600];
   int no;


#if  RETURN_TRUE_PROB
   double  X, Y;
#endif
   int  i, j, k, Ct, Sub;

   no=ind;

   switch (no) {
   case 0: // case of exon in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_ex.acc");
     break;
   case 1: // case of intron in acceptor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_in.acc");
     break;
   case 2: // case of exon in donor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_ex.don");
     break;
   case 3: // case of intron in donor
     strcpy(filename,TRAIN_DIR);
     strcat(filename,"score_in.don");
     break;
   }

   if  (! Cod_Tables_Loaded[no] )
       {
        Infile = fopen (filename, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open file \"%s\"\n",
                        filename);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Cod_Positive_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < CODING_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Cod_Negative_Table [no] [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading file \"%s\"\n", 
                                filename);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Cod_Tables_Loaded [no] = TRUE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Cod_Positive_Table [no] [markov_degree - 1] [0] [Sub];
   Negative_Sum = Cod_Negative_Table [no] [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < CODING_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Cod_Positive_Table [no] [i] [j] [Sub];
      Negative_Sum += Cod_Negative_Table [no] [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }
 


   Score = Positive_Sum - Negative_Sum;

#if  RETURN_TRUE_PROB
   X = exp (Positive_Sum + LOG_PROB_DONOR);
   Y = exp (Negative_Sum + LOG_PROB_NONDONOR);
   * Return_Score = log (X / (X + Y));
#else
   * Return_Score = Score;
#endif

   return (1);
  }



int  Is_Stop162  (const int * S, double * Return_Score, double STOP_THRESHOLD,char *TRAIN_DIR,int md )

     /* Evaluate string  S [0 .. (ATG_LEN -1)] and
      *  return  TRUE  or  FALSE  as to whether it is a likely atg
      *  site.  Also set  Return_Score  to the probability that it is an atg
      *  site. */
     
{ 
  double score1,score2,score3;
  double Score;

  Is_Stop(S+76,&score1,STOP_THRESHOLD,TRAIN_DIR,md);

  Is_Cod_NonCodSTOP(S,&score2,0,TRAIN_DIR);

  Is_Cod_NonCodSTOP(S+82,&score3,1,TRAIN_DIR);
  
  Score=score1+score2+score3;
  * Return_Score = Score;

  return Score >= STOP_THRESHOLD;
}


int  Is_Stop  (const int * S, double * Return_Score, double STOP_THRESHOLD,char *TRAIN_DIR,int md )

/* Evaluate string  S [0 .. (STOP_LEN -1)] and
*  return  TRUE  or  FALSE  as to whether it is a likely stop
*  site.  Also set  Return_Score  to the probability that it is an stop
*  site. */

{
   FILE  * Infile;
   double  Positive_Sum, Negative_Sum, Score;
   int i, j, k, Ct, Sub;
   char stopname[600];

   markov_degree=md;
   markov_len=(int)pow(ALPHABET_SIZE,md);

   if  (! Stop_Tables_Loaded)
       {
	sprintf(stopname,"%s%s",TRAIN_DIR,STOP_FILE_NAME); 
        Infile = fopen (stopname, "r");
        if  (Infile == NULL)
            {
             fprintf (stderr, "ERROR:  Unable to open stop file \"%s\"\n",
                        STOP_FILE_NAME);
             exit (EXIT_FAILURE);
            }

        for  (i = markov_degree - 1;  i < STOP_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Stop_Positive_Table [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading stop file \"%s\"\n", 
                                stopname);
                    exit (EXIT_FAILURE);
                   }
              }

        for  (i = markov_degree - 1;  i < STOP_LEN;  i ++)
          for  (k = 0;  k < markov_len;  k ++)
            for  (j = 0;  j < ALPHABET_SIZE;  j ++)
              {
               Ct = fscanf (Infile, "%f", & Stop_Negative_Table [i] [j] [k]);
               if  (Ct != 1)
                   {
                    fprintf (stderr, "ERROR reading stop file \"%s\"\n", 
                                STOP_FILE_NAME);
                    exit (EXIT_FAILURE);
                   }
              }

        fclose (Infile);

        Stop_Tables_Loaded = TRUE;
       }

   if  ((S [STOP_SIGNAL_OFFSET] != 3 || S [STOP_SIGNAL_OFFSET + 1] != 0 || S [STOP_SIGNAL_OFFSET + 2] != 0) &&    /* TAA */
       (S [STOP_SIGNAL_OFFSET] != 3 || S [STOP_SIGNAL_OFFSET + 1] != 0 || S [STOP_SIGNAL_OFFSET + 2] != 2) &&    /* TAG */
       (S [STOP_SIGNAL_OFFSET] != 3 || S [STOP_SIGNAL_OFFSET + 1] != 2 || S [STOP_SIGNAL_OFFSET + 2] != 0))      /* TGA */
       {
        * Return_Score = LOW_SCORE;
        return  FALSE;
       }

   Sub = 0;
   for  (i = 0;  i < markov_degree;  i ++)
     Sub = ALPHABET_SIZE * Sub + S [i];

   Positive_Sum = Stop_Positive_Table [markov_degree - 1] [0] [Sub];
   Negative_Sum = Stop_Negative_Table [markov_degree - 1] [0] [Sub];

   for  (i = markov_degree;  i < STOP_LEN;  i ++)
     {
      j = S [i];
      Positive_Sum += Stop_Positive_Table [i] [j] [Sub];
      Negative_Sum += Stop_Negative_Table [i] [j] [Sub];
      Sub = ALPHABET_SIZE * (Sub % (markov_len / ALPHABET_SIZE)) + j;
     }

   Score = Positive_Sum - Negative_Sum;


   * Return_Score = Score;


   return  Score >= STOP_THRESHOLD;
  }
