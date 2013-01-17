#ifndef  __SITES_H_INCLUDED
#define  __SITES_H_INCLUDED

#define  ALPHABET_SIZE  4

int  Is_Acceptor  (const int *, double *,int *,double,int,char *,int,int);
int  Is_Donor  (const int *, double *,int *,double,int,char *,int,int);
int  Is_Atg  (const int * , double *,double,char *,int);
int  Is_Atg162  (const int * , double *,double,char *,int);
int  Is_Stop162  (const int * , double *,double,char *,int);
int  Is_Stop  (const int * , double *,double,char *,int);
void UnLoadTables();

#endif
