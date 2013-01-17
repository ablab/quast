//Copyright (c) 2003 by A. L. Delcher and Mihaela Pertea 
  


#ifndef  __GENE_H_INCLUDED
#define  __GENE_H_INCLUDED

#define FALSE 0
#define TRUE 1

const long int  INCR_SIZE = 10000;
const long int  INIT_SIZE = 1000000;
const int  MAX_LINE = 300;


char  Complement  (char);
char  Filter  (char);
int  Read_String  (FILE *, char * &, long int &, char [], int);
int  Read_Multi_String  (FILE *, char * &, long int &, char [], int,int);




#endif
