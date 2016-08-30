#ifndef  __GRAPH_H_INCLUDED
#define  __GRAPH_H_INCLUDED

//#define MESSAGE

#define FORWARD 0
#define REVERSE 1

#define log2(x) (log(x)/0.693147181)
#define exp2(x) (pow(2.0,x))

const double NO_SCORE=-99;

enum state {INTERG,I0PLUS,I1PLUS,I2PLUS,I0MINUS,I1MINUS,I2MINUS,ESGLPLUS,EINPLUS,E0PLUS,E1PLUS,E2PLUS,ETRPLUS,ESGLMINUS,EINMINUS,E0MINUS,E1MINUS,E2MINUS,ETRMINUS};

struct Site
{
  double *score;
  struct exon **prevex;
  int *prevstatetype;
  long int *prevstateno;
  int *prevpredno;
};

struct exon
{
  long int start;
  long int stop;
  int type; // 0=init+ 1=internal+ 2=terminal+ 3=singl+ 4=init- 5=internal- 6=terminal- 7=single-
  int lphase;
  int rphase;
  double score;
  struct exon *prev;
  int exon_no;
};

Site **graph (char *PData,long int PData_Len,char *TRAIN_DIR, long int *splicesiteno, char *ProtDomFile,long int offset,int ese,char *Name,int PREDNO,int force);

#endif
