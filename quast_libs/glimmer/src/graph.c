// Copyright (c) 2003, Mihaela Pertea 

// GlimmerHMM 

//#define MESSAGE

#include  "delcher.h"
#include  "gene.h"
#include "oc1.h"
#include "sites.h"
#include "graph.h"


// ********************* CONSTANT DECLARATIONS **********************

const int NSTATES = 19; // total number of states
const int NCSTATES = 7; // number of nocoding states
const int MODEL_NO=1;  // this was supposed to allow different models for being used during processing of file; this apply to coding sensor, not to signal sensors
int  MODEL_LEN[MODEL_NO];

#include  "context.h"
#include "icm.h"

// ******************** STRUCTURE DEFINITIONS ***********************

struct Specif
{
  double score;
  double sp;
};

struct SpliceSite 
{
  int type;  // atg=1, gt=2, ag=3, stop=4; undefined=0;
  double score;
  long int poz;
};


// ******************** FUNCTION DECLARATIONS ***********************

void  Read_Coding_Model(char *,int);
void  Read_AAC(char *,int);
tModel * Read_NonCoding_Model(char *);
void DP(long int start, long int stop,int PREDNO,int force);
int check_for_stops(long int prevstop,long int newstart,int prevrphase);
int basetoint(char c, int dir);
double score_signalp(long int start, long int stop);
int stopcodon(int frame,int phase,long int stop,long int start);
int addexon(int number);
void freetreenode(struct tree_node *t);
exon *copyexon(exon *initexon);
int insertmaxscore(double score,exon *newexon,int state,long int pos,int *changed,int PREDNO,int prevstate,long int prevstateno,int prevpredno);
void initparam(double *, double *, double *, double *);
void loadmatrix(char *,double *,double **,int,int);
void loadweights(double *W,char *,int);
int loadscaledata(double *,double *,int,char *, int *,int *);
double scale(double,int,int, int,double,double);
int ok_gene_length(exon *newexon, int curstatetype,long int curstateno,int curpredno,int forw);

// ********************* VARIABLE DECLARATIONS **********************

double Acc_Thr;
double Don_Thr;
double Atg_Thr;
double Stop_Thr;
//double Acc_Max;
//double Don_Max;
//double Atg_Max;
//double Stop_Max;
int Use_Filter = 0;
int Use_Intron_Distrib[MODEL_NO];
int Use_Protein[MODEL_NO];
int use_dts[MODEL_NO];
int end_partial_penalty=0;
double *Protein[MODEL_NO][2];
int Win_Len=60;
char CODING_FILE[MODEL_NO][500];
char PROTEIN_FILE[MODEL_NO][500];
char NONCODING_FILE[MODEL_NO][500];
char MATRIXA_FILE[500];
char WEIGHTA_FILE[500];
char SCALEA_FILE[500];
char MATRIXD_FILE[500];
char WEIGHTD_FILE[500];
char SCALED_FILE[500];
char SIGNALP_FILE[500];
int ifSP=0;
int Use_Exon_Count;
int SignalP[32][4];
double boostexon[MODEL_NO]; 
double boostdistr[MODEL_NO];
double snglfactor[MODEL_NO];
double boostsplice[MODEL_NO];
int CG[MODEL_NO];
int ExNo;
tModel * NMODEL[MODEL_NO];
long int Data_Len;
char *Data;
int intergval[MODEL_NO];
int intergpen[MODEL_NO];
double splitpenalty[MODEL_NO];
int partial[MODEL_NO];
int sscore[MODEL_NO];
int onlytga=0;
int onlytaa=0;
int onlytag=0;
int min_gene_len=100;

enum aac {A,B,C,D,E,F,G,H,I,K,L,M,N,P,Q,R,S,T,V,W,Y,Z,X};
// aaa=0  aac=1  aag=2  aat=3  aca=4  acc=5  acg=6  act=7  aga=8  agc=9  agg=10 agt=11 ata=12 atc=13 atg=14 att=15
// caa=16 cac=17 cag=18 cat=19 cca=20 ccc=21 ccg=22 cct=23 cga=24 cgc=25 cgg=26 cgt=27 cta=28 ctc=29 ctg=30 ctt=31
// gaa=32 gac=33 gag=34 gat=35 gca=36 gcc=37 gcg=38 gct=39 gga=40 ggc=41 ggg=42 ggt=43 gta=44 gtc=45 gtg=46 gtt=47
// taa=48 tac=49 tag=50 tat=51 tca=52 tcc=53 tcg=54 tct=55 tga=56 tgc=57 tgg=58 tgt=59 tta=60 ttc=61 ttg=62 ttt=63
int valaac[64]={K,N,K,N,T,T,T,T,R,S,R,S,I,I,M,I,
		Q,H,Q,H,P,P,P,P,R,R,R,R,L,L,L,L,
		E,D,E,D,A,A,A,A,G,G,G,G,V,V,V,V,
		X,Y,X,Y,S,S,S,S,X,C,W,C,L,F,L,F};
double Facc1[MODEL_NO][23];
double Facc2[MODEL_NO][529];
double Facc[MODEL_NO][12167];


// States

double Trans[MODEL_NO][NSTATES][NSTATES];
char EXON_DISTR[MODEL_NO][500];
char EXONNO_DISTR[500]="";
double *LenDistr[MODEL_NO][6]; // 0=initial, 1=internal, 2=terminal, 3=single, 4=intron, 5=intergenic
double *ExDistr;
double *ExTail;
int LenNo[MODEL_NO][4]; // remember it just for exons
Site *Sites[NCSTATES];

// ********************* DECISION TREES DECLARATIONS *****************

int no_of_dimensions=1;
int no_of_categories = 2;
struct tree_node *atgroot;
struct tree_node *tagroot;
struct tree_node *accroot;
struct tree_node *donroot;


void classify(POINT *,struct tree_node **,int no_of_trees);
void classifysgl(POINT *,struct tree_node *);
extern struct tree_node *read_tree(char *);


// ********************* FALSE VAR DECLARATIONS *****************

int AtgNo,AccNo,DonNo,StopNo;
struct Specif *AtgFalse,*AccFalse,*DonFalse,*StopFalse;
int loadfalse=0;
double loadbin(struct Specif *,int,int,double);

// ************************** Scoring ***************************

double *Cod[MODEL_NO][3];     // scoring in six frames; these are log odds probabilities of coding vs. noncoding
SpliceSite *SS;     // all splice sites on both strands: atg=1, gt=2, ag=3, stop=4; undefined=0;
                    // splice site position is always on first and last base of signal 
                    // for forward and reverse strand respectively


exon *scoreinitexon(long int start,long int stop, int *prevstate, int *nextstate, int dir, int m);
exon *scoretermexon(long int start,long int stop, int *prevstate, int *nextstate, int dir, int m);
exon *scoreinternexon(long int start,long int stop, int rphase, int *prevstate, int *nextstate, int dir, int m);
exon *scoresglexon(long int start,long int stop, int *prevstate, int *nextstate, int dir, int m);


// ************************** Main PROGRAM ***************************

Site **graph ( char *PData,long int PData_Len,char *TRAIN_DIR, long int *splicesiteno, char *ProtDomFile,long int offset,int ese,char *Name,int PREDNO,int force)
{
  FILE  * fp;
  int md=1,multiple=0;
  char  File_Name [MAX_LINE], Line [MAX_LINE], Param[MAX_LINE];
  char CONFIG_FILE[MODEL_NO][MAX_LINE];
  float readval,cgperc;
  double eval;
  long int acgt;
  SpliceSite *SST[2];
  long int ssFno,ssRno; // number of splice sites detected on forward and reverse strand respectively
  long int ssno;      // number of splice sites detected on forward and reverse strand +2 (start and stop)
  int istacc=0, istdon=0, isatg=0, isstop=0, nocod=0;
  double score, minterg[MODEL_NO], m5utr[MODEL_NO], m3utr[MODEL_NO], mintron[MODEL_NO], i5utr, i3utr, iinterg,iintron, iin[3];
  double noncoding,coding;
  int ret,model;
  int m,l,ti;
  long int i,j,k,Input_Size;
  int val;
  int B[200];
  double *TCod[MODEL_NO][6];
  double *PDom[MODEL_NO][6];
  int esea=0;
  int esed=0;
  int motiflena=0;
  int flanklena=0;
  int featnoa=0;
  int motiflend=0;
  int flanklend=0;
  int featnod=0;
  int leafnoa=0;
  int leafnod=0;
  int leaf;
  int begscalea,endscalea;
  double **Ma;
  double Bga[4];
  double *Wa;
  double *MinSa;
  double *MaxSa;
  int begscaled,endscaled;
  double **Md;
  double Bgd[4];
  double *Wd;
  double *MinSd;
  double *MaxSd;
  int scaledataa=1;
  int scaledatad=1;
  double motifscore,prob;
  double *maxmotif;
  long int begdom,enddom;
  float bitscore;
  double baseprob;


  initparam(mintron,minterg,m5utr,m3utr);

  Data=PData;
  Data_Len = PData_Len;

  Acc_Thr = NO_SCORE;
  Don_Thr = NO_SCORE;
  Atg_Thr = NO_SCORE;
  Stop_Thr = NO_SCORE;
  Use_Exon_Count = FALSE; // default

  cgperc=0;
  acgt=0;

  for  (i = 1;  i <= Data_Len;  i ++) {

    // Converts all characters to  acgt
    Data [i] = Filter (tolower (Data [i]));

    switch  (Data [i]) {
    case  'c' :
    case  'g' :
      cgperc++;
    case  'a' :
    case  't' :
      acgt++;
      break;
    }

  }

  cgperc/=acgt;
  cgperc*=100;

  // ----------------------------------
  //         read config_file
  // ----------------------------------

  strcpy(File_Name,TRAIN_DIR);
  strcat(File_Name,"config.file"); 
  fp = fopen (File_Name, "r");
  if  (fp == NULL)
    {
      fprintf (stderr, "ERROR 1:  Unable to open file %s\n", File_Name);
      exit (0);
    }

  File_Name[0]='\0';


  i=0;
  Param[0]='\0';
  while(fgets (Line, MAX_LINE, fp) != NULL) {
    sscanf(Line,"%*s %*s %f %s %s",&readval,File_Name,Param);

    if(!readval) { multiple=1;} 
    else {
      if(!multiple) {
	if(cgperc<=readval) { 
	  strcpy(CONFIG_FILE[0],File_Name);
	  if(strcmp(Param,"")) {
	    TRAIN_DIR=(char *) realloc(TRAIN_DIR,(strlen(TRAIN_DIR)+strlen(Param)+1)*sizeof(char));
	    strcat(TRAIN_DIR,Param);
	    strcat(TRAIN_DIR,"/");	
	  }

	  break; 
	}
      }
      else {
	strcpy(CONFIG_FILE[i],File_Name);
	if(strcmp(Param,"")) {
	  TRAIN_DIR=(char *) realloc(TRAIN_DIR,(strlen(TRAIN_DIR)+strlen(Param)+1)*sizeof(char));
	  strcat(TRAIN_DIR,Param);
	  strcat(TRAIN_DIR,"/");	 
	}
	//	CG[i]=(int)floorf(readval);
	i++;
      }
    }

  }

  fclose(fp);

  strcpy(MATRIXA_FILE,"");
  strcpy(WEIGHTA_FILE,"");
  strcpy(SCALEA_FILE,"");
  strcpy(MATRIXD_FILE,"");
  strcpy(WEIGHTD_FILE,"");
  strcpy(SCALED_FILE,"");


  for(i=0;i<MODEL_NO;i++) {

    assert(CONFIG_FILE[i][0] != '\0');

    strcpy(Line,TRAIN_DIR);
    strcat(Line,CONFIG_FILE[i]);

    fp = fopen (Line, "r");
    if  (fp == NULL)
      {
	fprintf (stderr, "ERROR 2:  Unable to open file %s\n", CONFIG_FILE[i]);
	exit (0);
      }

    while(fgets (Line, MAX_LINE, fp) != NULL) {
      sscanf(Line,"%s %s", Param, File_Name);
      if(strcmp (Param,"coding_model_file") == 0 ) {
	strcpy(CODING_FILE[i],File_Name); 
	continue;
      }
      if(strcmp (Param,"use_protein")==0) {
	Use_Protein[i]=1;
	strcpy(PROTEIN_FILE[i],File_Name);
	continue;
      }
      if(strcmp (Param,"use_dts")==0) {
	use_dts[i]=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"end_partial_penalty")==0) {
	end_partial_penalty=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"noncoding_model_file")==0) {
	strcpy(NONCODING_FILE[i],File_Name); 
	continue;
      }
      if(strcmp (Param,"acceptor_MDD_tree")==0) {
	istacc=atoi(File_Name);
	assert(istacc == 0 || istacc ==1);
	continue;
      }
      if(ese && strcmp (Param,"acceptor_ese")==0) {
	esea=atoi(File_Name);
	assert(esea==0 || esea==1);
	continue;
      }
      if(ese && strcmp (Param,"donor_ese")==0) {
	esed=atoi(File_Name);
	assert(esed==0 || esed==1);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_motiflen")==0) {
	motiflena=atoi(File_Name);
	assert(motiflena>0);
	continue;
      }
      if(ese && strcmp(Param,"MDD_acceptor_leafno")==0) {
	leafnoa=atoi(File_Name);
	assert(leafnoa>0);
	continue;
      }
      if(ese && strcmp(Param,"MDD_donor_leafno")==0) {
	leafnod=atoi(File_Name);
	assert(leafnod>0);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_featno")==0) {
	featnoa=atoi(File_Name);
	assert(featnoa>1);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_flanklen")==0) {
	flanklena=atoi(File_Name);
	assert(flanklena>0);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_matrixfile")==0) {
	strcpy(MATRIXA_FILE,File_Name);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_weightfile")==0) {
	strcpy(WEIGHTA_FILE,File_Name);
	continue;
      }
      if(ese && strcmp(Param,"acceptor_scalefile")==0) {
	strcpy(SCALEA_FILE,File_Name);
	continue;
      }
      if(ese && strcmp(Param,"donor_motiflen")==0) {
	motiflend=atoi(File_Name);
	assert(motiflend>0);
	continue;
      }
      if(ese && strcmp(Param,"donor_featno")==0) {
	featnod=atoi(File_Name);
	assert(featnod>1);
	continue;
      }
      if(ese && strcmp(Param,"donor_flanklen")==0) {
	flanklend=atoi(File_Name);
	assert(flanklend>0);
	continue;
      }
      if(ese && strcmp(Param,"donor_matrixfile")==0) {
	strcpy(MATRIXD_FILE,File_Name);
	continue;
      }
      if(ese && strcmp(Param,"donor_weightfile")==0) {
	strcpy(WEIGHTD_FILE,File_Name);
	continue;
      }
      if(ese && strcmp(Param,"donor_scalefile")==0) {
	strcpy(SCALED_FILE,File_Name);
	continue;
      }
      if(strcmp (Param,"model_len")==0) {
	MODEL_LEN[i]=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"partial")==0) {
	partial[i]=atoi(File_Name);
	assert(partial[i] == 0 || partial[i] ==1);
	continue;
      }
      if(strcmp (Param,"no_cod_noncod")==0) {
	nocod=atoi(File_Name);
	assert(nocod == 0 || nocod ==1);
	continue;
      }
      if(strcmp (Param,"onlytga")==0) {
	onlytga=atoi(File_Name);
	assert(onlytga == 0 || onlytga ==1);
	continue;
      }
      if(strcmp (Param,"onlytaa")==0) {
	onlytaa=atoi(File_Name);
	assert(onlytaa == 0 || onlytaa ==1);
	continue;
      }
      if(strcmp (Param,"onlytag")==0) {
	onlytag=atoi(File_Name);
	assert(onlytag == 0 || onlytag ==1);
	continue;
      }
      if(strcmp (Param,"splice_score")==0) {
	sscore[i]=atoi(File_Name);
	assert(sscore[i] == 0 || sscore[i] ==1);
	continue;
      }
      if(strcmp (Param,"use_intron_distrib")==0) {
	Use_Intron_Distrib[i]=atoi(File_Name);
	assert(Use_Intron_Distrib[i] == 0 || Use_Intron_Distrib[i] ==1);
	continue;
      }
      if(strcmp (Param,"window_len")==0) {
	Win_Len=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"use_filter")==0) {
	Use_Filter=atoi(File_Name);
	assert(Use_Filter == 0 || Use_Filter ==1);
	continue;
      }
      if(strcmp (Param,"intergenic_val")==0) {
	intergval[i]=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"intergenic_penalty")==0) {
	intergpen[i]=atoi(File_Name);
	continue;
      }
      if(strcmp (Param,"load_falses")==0) {
	loadfalse=atoi(File_Name);
	assert(loadfalse == 0 || loadfalse ==1);
	continue;
      }
      if(strcmp (Param,"donor_MDD_tree")==0) {
	istdon=atoi(File_Name);
	assert(istdon == 0 || istdon ==1);
	continue;
      }
      if(strcmp (Param,"markov_degree")==0) {
	md=atoi(File_Name);
	assert(md>=1 && md<=3);
	continue;
      }
      if(strcmp (Param,"acceptor_threshold")==0) {
	if(Acc_Thr==NO_SCORE) {
	  score = strtod (File_Name,NULL);
	  if  (errno == ERANGE)
	    fprintf (stderr, "ERROR:  Bad acceptor threshold score in config_file: %s\n", File_Name);
	  else Acc_Thr=score;
	}
	continue;
      }
      /*
	if(strcmp (Param,"acceptor_max")==0) {
	Acc_Max = strtod (File_Name,NULL);
	if  (errno == ERANGE)
	fprintf (stderr, "ERROR:  Bad acceptor threshold score in config_file: %s\n", File_Name);
	continue;
	}
	if(strcmp (Param,"donor_max")==0) {
	Don_Max = strtod (File_Name,NULL);
	if  (errno == ERANGE)
	fprintf (stderr, "ERROR:  Bad acceptor threshold score in config_file: %s\n", File_Name);
	continue;
	}
	if(strcmp (Param,"atg_max")==0) {
	Atg_Max = strtod (File_Name,NULL);
	if  (errno == ERANGE)
	fprintf (stderr, "ERROR:  Bad acceptor threshold score in config_file: %s\n", File_Name);
	continue;
	}
	if(strcmp (Param,"stop_max")==0) {
	Stop_Max = strtod (File_Name,NULL);
	if  (errno == ERANGE)
	fprintf (stderr, "ERROR:  Bad acceptor threshold score in config_file: %s\n", File_Name);
	continue;
	}
      */
      if(strcmp (Param,"donor_threshold")==0) {
	if(Don_Thr==NO_SCORE) {
	  score = strtod (File_Name,NULL);
	  if  (errno == ERANGE)
	    fprintf (stderr, "ERROR:  Bad donor threshold score in config_file: %s\n", File_Name);
	  else Don_Thr=score;
	}
	continue;
      }
      if(strcmp (Param,"ATG_detection")==0) {
	isatg=atoi(File_Name);
	assert(isatg == 0 || isatg ==1 || isatg==2);
	continue;
      }
      if(strcmp (Param,"ATG_threshold")==0) {
	if(Atg_Thr==NO_SCORE) {
	  score = strtod (File_Name,NULL);
	  if  (errno == ERANGE)
	    fprintf (stderr, "ERROR:  Bad atg threshold score in config_file: %s\n", File_Name);
	  else Atg_Thr=score;
	}
	continue;
      }
      if(strcmp (Param,"Stop_detection")==0) {
	isstop=atoi(File_Name);
	assert(isstop == 0 || isstop ==1 || isstop ==2);
	continue;
      }
      if(strcmp (Param,"Stop_threshold")==0) {
	if(Stop_Thr==NO_SCORE) {
	  score = strtod (File_Name,NULL);
	  if  (errno == ERANGE)
	    fprintf (stderr, "ERROR:  Bad stop threshold score in config_file: %s\n", File_Name);
	  else Stop_Thr=score;
	}
	continue;
      }
      if(strcmp (Param,"Init5'UTR")==0) {
	i5utr = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init 5'UTR parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"split_penalty")==0) {
	splitpenalty[i] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad split_penalty parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"Init3'UTR")==0) {
	i3utr = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init 3'UTR parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"InitInterg")==0) {
	iinterg = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init intergenic parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"InitIntron")==0) {
	iintron = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init intron parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"InitI0")==0) {
	iin[0] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init intron 0 parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"InitI1")==0) {
	iin[1] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init intron 1 parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"InitI2")==0) {
	iin[2] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init intron 2 parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"MeanIntergen")==0) {
	minterg[i] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad mean intergenic parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp(Param,"MinGene")==0) {
	min_gene_len== strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad min gene length parameter in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"Mean5'UTR")==0) {
	m5utr[i] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad mean5'utr threshold score in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"Mean3'UTR")==0) {
	m3utr[i] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad mean3'utr threshold score in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"MeanIntron")==0) {
	mintron[i] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad meanintron threshold score in config_file: %s\n", File_Name);
	  exit(0);
	}
	continue;
      }
      if(strcmp (Param,"UseExonCount")==0) {
	ret=atoi(File_Name);
	assert(ret==0 || ret==1);
	Use_Exon_Count=ret;
	continue;
      }
      if(strcmp (Param,"PSngl")==0) {
	score = strtod (File_Name,NULL);
	score *= snglfactor[i];
	
	Trans[i][INTERG][ESGLPLUS] = 0.5 *score;
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad sgl transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][ESGLMINUS][INTERG]=Trans[i][INTERG][ESGLPLUS];
	Trans[i][INTERG][EINPLUS]=Trans[i][EINMINUS][INTERG]=0.5-Trans[i][INTERG][ESGLPLUS];
	continue;
      }
      if(strcmp (Param,"PTerm")==0) {
	Trans[i][I0PLUS][ETRPLUS] = strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad terminal transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][ETRMINUS][I0MINUS]=Trans[i][ETRMINUS][I1MINUS]=Trans[i][ETRMINUS][I2MINUS]=Trans[i][I1PLUS][ETRPLUS]=Trans[i][I2PLUS][ETRPLUS]=Trans[i][I0PLUS][ETRPLUS];
	
	if(Use_Exon_Count) 
	  Trans[i][E0MINUS][I0MINUS]=Trans[i][E1MINUS][I1MINUS]=Trans[i][E2MINUS][I2MINUS]=Trans[i][I0PLUS][E0PLUS]=Trans[i][I1PLUS][E1PLUS]=Trans[i][I2PLUS][E2PLUS]=1;
	else 
	  Trans[i][E0MINUS][I0MINUS]=Trans[i][E1MINUS][I1MINUS]=Trans[i][E2MINUS][I2MINUS]=Trans[i][I0PLUS][E0PLUS]=Trans[i][I1PLUS][E1PLUS]=Trans[i][I2PLUS][E2PLUS]=1-Trans[i][I0PLUS][ETRPLUS];
	
	continue;
      }
      if(strcmp (Param,"PInitIn0")==0) {
	Trans[i][EINPLUS][I0PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init-i0 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I0MINUS][EINMINUS]=Trans[i][EINPLUS][I0PLUS];
	continue;
      }
      if(strcmp (Param,"PInitIn1")==0) {
	Trans[i][EINPLUS][I1PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init-i1 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I1MINUS][EINMINUS]=Trans[i][EINPLUS][I1PLUS];
	continue;
      }
      if(strcmp (Param,"PInitIn2")==0) {
	Trans[i][EINPLUS][I2PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad init-i2 transition threshold score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I2MINUS][EINMINUS]=Trans[i][EINPLUS][I2PLUS];
	continue;
      }
      if(strcmp (Param,"PIn0In0")==0) {
	Trans[i][E0PLUS][I0PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i0-i0 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I0MINUS][E0MINUS]=Trans[i][E0PLUS][I0PLUS];
	continue;
      }
      if(strcmp (Param,"PIn0In1")==0) {
	Trans[i][E0PLUS][I1PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i0-i1 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I1MINUS][E0MINUS]=Trans[i][E0PLUS][I1PLUS];  
	continue;
      }
      if(strcmp (Param,"PIn0In2")==0) {
	Trans[i][E0PLUS][I2PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i0-i2 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I2MINUS][E0MINUS]=Trans[i][E0PLUS][I2PLUS];
	continue;
      }
      if(strcmp (Param,"PIn1In0")==0) {
	Trans[i][E1PLUS][I0PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i1-i0 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I0MINUS][E1MINUS]=Trans[i][E1PLUS][I0PLUS];
	continue;
      }
      if(strcmp (Param,"PIn1In1")==0) {
	Trans[i][E1PLUS][I1PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i1-i1 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I1MINUS][E1MINUS]=Trans[i][E1PLUS][I1PLUS];
	continue;
      }
      if(strcmp (Param,"PIn1In2")==0) {
	Trans[i][E1PLUS][I2PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i1-i2 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I2MINUS][E1MINUS]=Trans[i][E1PLUS][I2PLUS];
	continue;
      }
      if(strcmp (Param,"PIn2In0")==0) {
	Trans[i][E2PLUS][I0PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i2-i0 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I0MINUS][E2MINUS]=Trans[i][E2PLUS][I0PLUS];
	continue;
      }
      if(strcmp (Param,"PIn2In1")==0) {
	Trans[i][E2PLUS][I1PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i2-i1 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I1MINUS][E2MINUS]=Trans[i][E2PLUS][I1PLUS];
	continue;
      }
      if(strcmp (Param,"PIn2In2")==0) {
	Trans[i][E2PLUS][I2PLUS]=strtod (File_Name,NULL);
	if  (errno == ERANGE) {
	  fprintf (stderr, "ERROR:  Bad i2-i2 transition score in config_file: %s\n", File_Name);
	  exit(0);
	}
	Trans[i][I2MINUS][E2MINUS]=Trans[i][E2PLUS][I2PLUS];
	continue;
      }
      if(strcmp (Param,"LengthDistrFile")==0) {
	strcpy(EXON_DISTR[i],File_Name);
	continue;
      }
      if(strcmp (Param,"ExonNoDistrFile")==0) {
	strcpy(EXONNO_DISTR,File_Name);
	continue;
      }
      if(strcmp(Param,"SignalPFile")==0) {
	strcpy(SIGNALP_FILE,File_Name);
	ifSP=1;
	continue;
      }
      if(strcmp(Param,"BoostExon")==0) {
	boostexon[i]=strtod (File_Name,NULL);
	if  (errno == ERANGE)
	  fprintf (stderr, "ERROR:  Bad boostexon score in config_file: %s\n", File_Name);
	continue;
      }
      if(strcmp(Param,"BoostSplice")==0) {
	boostsplice[i]=strtod (File_Name,NULL);
	if  (errno == ERANGE)
	  fprintf (stderr, "ERROR:  Bad boostsplice score in config_file: %s\n", File_Name);
	continue;
      }
      if(strcmp(Param,"BoostSgl")==0) {
	score = strtod (File_Name,NULL);
	if  (errno == ERANGE)
	  fprintf (stderr, "ERROR:  Bad boostsgl score in config_file: %s\n", File_Name);
	else snglfactor[i]=score;
	continue;
      }
    }
    fclose(fp);

    // Update constant transitions:
    
    Trans[i][ETRPLUS][INTERG]=Trans[i][INTERG][ETRMINUS]=1;
    Trans[i][INTERG][ESGLMINUS]=Trans[i][ESGLPLUS][INTERG]=1;
    
    
  }
    
  // ---------------------------- INIT_DECISION_TREES ------------------

  if(!loadfalse) {

    // read atg/gt/ag/tag scores
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"atg.dt");
    
    atgroot=NULL;
    accroot=NULL;
    donroot=NULL;
    tagroot=NULL;
    
    if ((atgroot = read_tree(File_Name)) == NULL) {
      fprintf(stderr,"Mktree: Cannot read atg.dt\n");
      exit(-1);	
    }
    
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"acc.dt");
  
    if ((accroot = read_tree(File_Name)) == NULL) {
      fprintf(stderr,"Mktree: Cannot read acc.dt\n");
      exit(-1);	
    }

    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"don.dt");
  
    if ((donroot = read_tree(File_Name)) == NULL) {
      fprintf(stderr,"Mktree: Cannot read don.dt\n");
      exit(-1);	
    }

    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"stop.dt");
  
    if ((tagroot = read_tree(File_Name)) == NULL) {
      fprintf(stderr,"Mktree: Cannot read stop.dt\n");
      exit(-1);	
    }
  }
  else {
  
    // atg
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"atg.false");

    fp = fopen (File_Name, "r");
    if  (fp == NULL) {
      fprintf (stderr, "ERROR 3:  Unable to open file %s\n", File_Name);
      exit (0);
    }

    fgets (Line, MAX_LINE, fp);
    sscanf(Line,"%d",&AtgNo);

    AtgFalse=NULL;
    AtgFalse=(Specif *) malloc(AtgNo*sizeof(Specif));
    if(AtgFalse == NULL) {
      fprintf(stderr,"Memory allocation for atg false distribution failure.\n");
      abort();
    }
    
    for(i=0;i<AtgNo;i++){
      fscanf(fp,"%f",&readval);
      AtgFalse[i].score=readval;
      fscanf(fp,"%f",&readval);
      AtgFalse[i].sp=readval;
    }
    fclose(fp);
    
    // acc
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"acc.false");

    fp = fopen (File_Name, "r");
    if  (fp == NULL) {
      fprintf (stderr, "ERROR 4:  Unable to open file %s\n", File_Name);
      exit (0);
    }

    fgets (Line, MAX_LINE, fp);
    sscanf(Line,"%d",&AccNo);

    AccFalse=NULL;
    AccFalse=(Specif *) malloc(AccNo*sizeof(Specif));
    if(AccFalse == NULL) {
      fprintf(stderr,"Memory allocation for acc false distribution failure.\n");
      abort();
    }
    
    for(i=0;i<AccNo;i++){
      fscanf(fp,"%f",&readval);
      AccFalse[i].score=readval;
      fscanf(fp,"%f",&readval);
      AccFalse[i].sp=readval;
    }
    fclose(fp);

    // don
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"don.false");

    fp = fopen (File_Name, "r");
    if  (fp == NULL) {
      fprintf (stderr, "ERROR 5:  Unable to open file %s\n", File_Name);
      exit (0);
    }

    fgets (Line, MAX_LINE, fp);
    sscanf(Line,"%d",&DonNo);

    DonFalse=NULL;
    DonFalse=(Specif *) malloc(DonNo*sizeof(Specif));
    if(DonFalse == NULL) {
      fprintf(stderr,"Memory allocation for don false distribution failure.\n");
      abort();
    }
    
    for(i=0;i<DonNo;i++){
      fscanf(fp,"%f",&readval);
      DonFalse[i].score=readval;
      fscanf(fp,"%f",&readval);
      DonFalse[i].sp=readval;
    }
    fclose(fp);

    // stop
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name,"stop.false");

    fp = fopen (File_Name, "r");
    if  (fp == NULL) {
      fprintf (stderr, "ERROR 6:  Unable to open file %s\n", File_Name);
      exit (0);
    }

    fgets (Line, MAX_LINE, fp);
    sscanf(Line,"%d",&StopNo);

    StopFalse=NULL;
    StopFalse=(Specif *) malloc(StopNo*sizeof(Specif));
    if(StopFalse == NULL) {
      fprintf(stderr,"Memory allocation for stop false distribution failure.\n");
      abort();
    }
    
    for(i=0;i<StopNo;i++){
      fscanf(fp,"%f",&readval);
      StopFalse[i].score=readval;
      fscanf(fp,"%f",&readval);
      StopFalse[i].sp=readval;
    }
    fclose(fp);
  }


  // ---------------------------- SCORE --------------------------------

  for(m=0;m<MODEL_NO;m++) {

    // the model file names should be in the parameter file
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name, PROTEIN_FILE[m]);
    if(Use_Protein[m]) Read_AAC(File_Name,m);

    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name, CODING_FILE[m]);
 
    Read_Coding_Model (File_Name,m);
    
    // read the noncoding model
    strcpy (File_Name, TRAIN_DIR);
    strcat (File_Name, NONCODING_FILE[m]);
    NMODEL[m]=Read_NonCoding_Model(File_Name);
    
    // allocate space

    for(i=0;i<6;i++) {
      TCod[m][i]=NULL;
      TCod[m][i]=(double *) malloc((Data_Len+2)*sizeof(double));
      if (TCod[m][i] == NULL) {
	fprintf(stderr,"Memory allocation for coding %d failure.\n",i);
	abort();
      }
      TCod[m][i][0]=0;
      TCod[m][i][Data_Len+1]=0;
    }
    
    for(i=0;i<6;i++) {
      PDom[m][i]=NULL;
      PDom[m][i]=(double *) malloc((Data_Len+2)*sizeof(double));
      if (PDom[m][i] == NULL) {
	fprintf(stderr,"Memory allocation for coding %d failure.\n",i);
	abort();
      }
      PDom[m][i][0]=0;
      PDom[m][i][Data_Len+1]=0;
      memset(PDom[m][i],0,(Data_Len+2)*sizeof(double));
    }
    
  }

  for(i=0;i<2;i++) {
    SST[i]=NULL;
    SST[i]=(SpliceSite *) malloc((Data_Len+2)*sizeof(SpliceSite));
    if (SST[i] == NULL) {
      fprintf(stderr,"Memory allocation for splice sites failure.\n"); 
      abort();
    }

    // SST[i][0].type=-1;
    // SST[i][0].score=NO_SCORE;
    // SST[i][Data_Len+1].type=-1;
    // SST[i][Data_Len+1].score=NO_SCORE;
    for(m=0;m<MODEL_NO;m++) {
      if(Use_Protein[m]) {
	Protein[m][i]=NULL;
	Protein[m][i]=(double *) malloc((Data_Len+2)*sizeof(double));
	if (Protein[m][i] == NULL) {
	  fprintf(stderr,"Memory allocation for coding %d failure.\n",i);
	  abort();
	}
	Protein[m][i][0]=0;
	Protein[m][i][Data_Len+1]=0;
      }
    }

  }
  

  // do the actual scoring here

  // check to see if all parameters needed by ESE splice site determination are there
  if(esea) {
    if(!motiflena || !flanklena || !featnoa || !strcmp(WEIGHTA_FILE,"") || !strcmp(MATRIXA_FILE,"") ) {
      fprintf(stderr,"Not enough parameters for ESE acceptor splice site detection!\n");
      esea=0;
    }
    else {
      
      // load matrix file
      strcpy (File_Name, TRAIN_DIR);
      strcat (File_Name, MATRIXA_FILE);
      Ma = (double **) malloc ((featnoa-1) * sizeof(double *));
      if(Ma==NULL) {
	fprintf(stderr,"Memory allocation for motif features failure.\n"); 
	abort();
      }
      for(l=0;l<featnoa-1;l++) {
	Ma[l] = (double *) malloc(4*motiflena*sizeof(double));
	if(Ma[l]==NULL) {
	  fprintf(stderr,"Memory allocation for motif features failure.\n"); 
	  abort();
	}
      }
      loadmatrix(File_Name,Bga,Ma,featnoa,motiflena);
      
      // load weight file
      strcpy (File_Name, TRAIN_DIR);
      strcat (File_Name, WEIGHTA_FILE);
      Wa = (double *) malloc((featnoa+leafnoa+1)*sizeof(double));
      if(Wa == NULL) {
	fprintf(stderr,"Memory allocation for SVM weights failure.\n"); 
	abort();
      }
      loadweights(Wa,File_Name,featnoa+leafnoa);

      //load scale data file
      if(!strcmp(SCALEA_FILE,"")) {
	fprintf(stderr,"No ESE scale file found! Using non scaled data instead.\n");
	scaledataa=0;
      }
      else {
	strcpy (File_Name, TRAIN_DIR);
	strcat (File_Name, SCALEA_FILE);
	MinSa= (double *) malloc(featnoa*sizeof(double));
	if(MinSa == NULL) {
	  fprintf(stderr,"Memory allocation for ESE scale data failure.\n"); 
	  abort();
	}
	
	MaxSa= (double *) malloc(featnoa*sizeof(double));
	if(MaxSa == NULL) {
	  fprintf(stderr,"Memory allocation for ESE scale data failure.\n"); 
	  abort();
	}
	scaledataa=loadscaledata(MinSa,MaxSa,featnoa,File_Name,&begscalea,&endscalea);
      }
    }
  }

  if(esed) {
    if(!motiflend || !flanklend || !featnod || !strcmp(WEIGHTD_FILE,"") || !strcmp(MATRIXD_FILE,"") ) {
      fprintf(stderr,"Not enough parameters for ESE donor splice site detection!\n");
      esed=0;
    }
    else {

      // load matrix file
      strcpy (File_Name, TRAIN_DIR);
      strcat (File_Name, MATRIXD_FILE);
      Md = (double **) malloc ((featnod-1) * sizeof(double *));
      if(Md==NULL) {
	fprintf(stderr,"Memory allocation for motif features failure.\n"); 
	abort();
      }
      for(l=0;l<featnod-1;l++) {
	Md[l] = (double *) malloc(4*motiflend*sizeof(double));
	if(Md[l]==NULL) {
	  fprintf(stderr,"Memory allocation for motif features failure.\n"); 
	  abort();
	}
      }
      loadmatrix(File_Name,Bgd,Md,featnod,motiflend);
      
      // load weight file
      strcpy (File_Name, TRAIN_DIR);
      strcat (File_Name, WEIGHTD_FILE);
      Wd = (double *) malloc((featnod+leafnod+1)*sizeof(double));
      if(Wd == NULL) {
	fprintf(stderr,"Memory allocation for SVM weights failure.\n"); 
	abort();
      }
      loadweights(Wd,File_Name,featnod+leafnod);
      
      //load scale data file
      if(!strcmp(SCALED_FILE,"")) {
	fprintf(stderr,"No ESE scale file found! Using non scaled data instead.\n");
	scaledatad=0;
      }
      else {
	strcpy (File_Name, TRAIN_DIR);
	strcat (File_Name, SCALED_FILE);
	MinSd= (double *) malloc(featnod*sizeof(double));
	if(MinSd == NULL) {
	  fprintf(stderr,"Memory allocation for ESE scale data failure.\n"); 
	  abort();
	}
	
	MaxSd= (double *) malloc(featnod*sizeof(double));
	if(MaxSd == NULL) {
	  fprintf(stderr,"Memory allocation for ESE scale data failure.\n"); 
	  abort();
	}
	scaledatad=loadscaledata(MinSd,MaxSd,featnod,File_Name,&begscaled,&endscaled);
      }
    }
  }


  // introduce using domains if given

  if( ProtDomFile != NULL ) {

    fp = fopen (ProtDomFile, "r");

    if  (fp == NULL)
      {
	fprintf (stderr, "ERROR 7:  Unable to open file %s\n", ProtDomFile);
	exit (0);
      }

    while(fgets (Line, MAX_LINE, fp) != NULL) {
      sscanf(Line,"%s %d %d %*s %*d %*d %f %e %*s",File_Name,&begdom,&enddom,&bitscore,&eval);

      if(strcmp(Name,File_Name)==0 && bitscore>0) {
	if(begdom<enddom) { // domain on forward strand

	  if(begdom>=offset && begdom-offset<=Data_Len) {

	    //fprintf(stderr,"begdom=%d enddom=%d bitscore=%f eval=%e i=%ld\n",begdom,enddom,bitscore,eval,begdom-offset);

	    i=begdom-offset;  // CHECK HERE TO SEE THAT I GET THE COORDINATE RIGHT
	    j=(i-1)%3;
	    baseprob=(double)bitscore/(enddom-begdom+1);

	    //fprintf(stderr,"i=%d baseprob=%f j=%d enddom=%d Data_len=%d\n",i,baseprob,j,enddom,Data_Len);

	    while(i<=enddom-offset && i<=Data_Len) { 
	      for(m=0;m<MODEL_NO;m++) {
		PDom[m][j][i]+=baseprob;
	      }
	      i++;
	    }

	  }
	}
	else { //domain on reverse strand
	  if(enddom>=offset && enddom-offset<=Data_Len) {
	    i=enddom-offset;
	    j=(i-1)%3;
	    baseprob=(double)bitscore/(begdom-enddom+1);
	    while(i<=begdom-offset && i<=Data_Len) {
	      for(m=0;m<MODEL_NO;m++) {
		PDom[m][3+j][i]+=baseprob;
	      }
	      i++;
	    }
	  }
	}
      }
    }
  }
	

  if(esea || esed) {
    if(featnoa>=featnod) 
      maxmotif = (double *) malloc((featnoa-1)*sizeof(double));
    else 
      maxmotif = (double *) malloc((featnod-1)*sizeof(double));
    if(maxmotif== NULL) {
      fprintf(stderr,"Memory allocation for ESE scoring failure.\n"); 
      abort();
    }
  }




  /* forward strand */

  ssFno=0;

  for(i=1;i<Data_Len+1;i++) {

    for(m=0;m<MODEL_NO;m++) {
      if(Use_Protein[m]) {
	if(i>=9) {
	  val = valaac[basetoint(Data[i-2],1)*16+basetoint(Data[i-1],1)*4+basetoint(Data[i],1)]+
	    valaac[basetoint(Data[i-5],1)*16+basetoint(Data[i-4],1)*4+basetoint(Data[i-3],1)]*23+
	    valaac[basetoint(Data[i-8],1)*16+basetoint(Data[i-7],1)*4+basetoint(Data[i-6],1)]*529;
	  Protein[m][0][i]=Protein[m][0][i-3]+Facc[m][val];
	}
	else if(i>=6) {
	  val = valaac[basetoint(Data[i-2],1)*16+basetoint(Data[i-1],1)*4+basetoint(Data[i],1)]+
	    valaac[basetoint(Data[i-5],1)*16+basetoint(Data[i-4],1)*4+basetoint(Data[i-3],1)]*23;
	  Protein[m][0][i]=Protein[m][0][i-3]+Facc2[m][val];
	}
	else if(i>=3) {
	  val = valaac[basetoint(Data[i-2],1)*16+basetoint(Data[i-1],1)*4+basetoint(Data[i],1)];
	  Protein[m][0][i]=Facc1[m][val];
	}
	else Protein[m][0][i]=0;
	
	//      if(i%3==0) { printf("%d %f %f %f\n",i,Protein[m][0][i-2],Protein[m][0][i-1],Protein[m][0][i]);}

      }

      // coding 

      // if sequence is in the same frame as Data starting at 1 then use TCod[0]
      // if sequence is in the same frame as Data starting at 2 then use TCod[1]
      // if sequence is in the same frame as Data starting at 3 then use TCod[2]

      if(i<MODEL_LEN[m]) {
	noncoding = get_prob_of_window2(i-1,NMODEL[m],Data+1);
      }
      else {
	noncoding = get_prob_of_window1(i-MODEL_LEN[m]+1,NMODEL[m],Data);
      }
      if(noncoding==0) noncoding=0.00001; // take a very low probability

      for(j=0;j<3;j++) 
	if(i>j) {
	  
	  if(i>=j+MODEL_LEN[m]) {
	    model = (int)(i-MODEL_LEN[m]-j)%3;
	    coding = get_prob_of_window1(i-MODEL_LEN[m]+1,MODEL[m][model],Data);
	  }
	  else {
	    model = ((int)(i-j-1)%3 +1)%3;
	    coding = get_prob_of_window2(i-j-1,MODEL[m][model],Data+j+1);
	  }
	  if(coding==0) coding=0.00001; // take a very low probability
	  
	  TCod[m][j][i]=PDom[m][j][i]+TCod[m][j][i-1]+log2(coding)-log2(noncoding);
	  //Cod[j][i]=Cod[j][i-1]+0.5*log2(coding)-log2(noncoding); //if using DT

	}
	else { TCod[m][j][i]=0; }

      //printf("%d %c %f %f %f %f\n",i,Data[i],Cod[0][i],Cod[1][i],Cod[2][i],noncoding);
    }
      
    // splice sites
      
    /* atg's : 0 */

    if(i<Data_Len-1 && Data[i]=='a' && Data[i+1]=='t' && Data[i+2]=='g') { // Deal w/ start sites

      if(isatg==2) {
	if(i>80 && i<=Data_Len-81) {
	  k=0;
	  for(j=i-80;j<i+82;j++){
	    switch (Data[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Atg162(B,&score,Atg_Thr,TRAIN_DIR,md);
	  if(score>=Atg_Thr) {
	    SST[0][ssFno].type=1;
	    SST[0][ssFno].poz=i;
	    SST[0][ssFno].score=score;
	    ssFno++;
	  }

	}
	else {
	  SST[0][ssFno].score=NO_SCORE;
	  SST[0][ssFno].type=1;
	  SST[0][ssFno].poz=i;
	  ssFno++;
	}
	  
      }
      else {
	if(i>12 && i<=Data_Len-6) {
	  k=0;
	  for(j=i-12;j<i+7;j++) {
	    switch (Data[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  ret=Is_Atg(B,&score,Atg_Thr,TRAIN_DIR,md);
	  if(score>=Atg_Thr) {
	    SST[0][ssFno].type=1;
	    SST[0][ssFno].poz=i;
	    SST[0][ssFno].score=score;
	    ssFno++;
	  }
	}
	else {
	  SST[0][ssFno].score=NO_SCORE;
	  SST[0][ssFno].type=1;
	  SST[0][ssFno].poz=i;
	  ssFno++;
	}
      }

    }

    /* gt's : 1 */

    if(i<Data_Len && Data[i]=='g' && Data[i+1]=='t') { // Deal with donors

      if(i>80 && i<=Data_Len-81) {
	k=0;
	for(j=i-80;j<i+82;j++){
	  switch (Data[j]){
	  case 'a': B[k]=0;break;
	  case 'c': B[k]=1;break;
	  case 'g': B[k]=2;break;
	  case 't': B[k]=3;break;
	  }
	  k++;
	}

	ret=Is_Donor(B,&score,&leaf, Don_Thr, istdon,TRAIN_DIR,nocod,md);
	motifscore=0;
	if(score>=Don_Thr) motifscore=1;

	if(motifscore && esed) {
	  for(l=0;l<featnod-1;l++) maxmotif[l]=-10000;

	  motifscore=Wd[1]*scale(score,scaledatad,begscaled,endscaled,MaxSd[0],MinSd[0])-Wd[0];

	  for(k=82;k<82+flanklend-motiflend;k++)
	    for(l=0;l<featnod-1;l++) {
	      prob=0;
	      for(ti=0;ti<motiflend;ti++) {
		if(Md[l][B[k+ti]*motiflend+ti]!=0) {
		  prob+=Md[l][B[k+ti]*motiflend+ti]*log(Md[l][B[k+ti]*motiflend+ti]/Bgd[B[k+ti]]/log(2));
		}
	      }
	      if(prob>maxmotif[l]) maxmotif[l]=prob;
	    }
	  
	  for(l=0;l<featnod-1;l++) 
	    motifscore+=Wd[l+2]*scale(maxmotif[l],scaledatad,begscaled,endscaled,MaxSd[l+1],MinSd[l+1]);

	  if(leafnod) {
	    motifscore+=Wd[leaf+featnod+1];
	  }

	}

	if(motifscore>0) {
	  SST[0][ssFno].type=2;
	  SST[0][ssFno].poz=i;
	  SST[0][ssFno].score=score;
	  ssFno++;
	}
      }
      else {
	SST[0][ssFno].score=NO_SCORE;
	SST[0][ssFno].type=2;
	SST[0][ssFno].poz=i;
	ssFno++;
      }


    }

    /* ag's : 2 */

    if(i<Data_Len && Data[i]=='a' && Data[i+1]=='g') { // Deal with acceptors


      if(i>80 && i<=Data_Len-81) {
	k=0;
	for(j=i-80;j<i+82;j++){
	  switch (Data[j]){
	  case 'a': B[k]=0;break;
	  case 'c': B[k]=1;break;
	  case 'g': B[k]=2;break;
	  case 't': B[k]=3;break;
	  }
	  k++;
	}

	ret=Is_Acceptor(B,&score, &leaf,Acc_Thr, istacc,TRAIN_DIR,nocod,md);

	motifscore=0;
	if(score>=Acc_Thr) motifscore=1;

	if(motifscore && esea) {
	  for(l=0;l<featnoa-1;l++) maxmotif[l]=-10000;
	      
	  motifscore=Wa[1]*scale(score,scaledataa,begscalea,endscalea,MaxSa[0],MinSa[0])-Wa[0];

	  for(k=82;k<82+flanklena-motiflena;k++)
	    for(l=0;l<featnoa-1;l++) {
	      prob=0;
	      for(ti=0;ti<motiflena;ti++) {
		if(Ma[l][B[k+ti]*motiflena+ti]!=0) {
		  prob+=Ma[l][B[k+ti]*motiflena+ti]*log(Ma[l][B[k+ti]*motiflena+ti]/Bga[B[k+ti]]/log(2));
		}
	      }
	      if(prob>maxmotif[l]) maxmotif[l]=prob;
	    }
	  
	  for(l=0;l<featnoa-1;l++) 
	    motifscore+=Wa[l+2]*scale(maxmotif[l],scaledataa,begscalea,endscalea,MaxSa[l+1],MinSa[l+1]);

	  if(leafnoa) {
	    motifscore+=Wa[leaf+featnoa+1];
	  }

	  //	  printf("Acceptor %d %d : score=%f leaf=%d motifscore=%f Wa[0]=%f Wa[1]=%f maxmotif[0]=%f begscalea=%d endscalea=%d\n",i,i+1,score,leaf,motifscore,Wa[0],Wa[1],maxmotif[0],begscalea,endscalea);


	}


	if(motifscore>0) {
	  SST[0][ssFno].type=3;
	  SST[0][ssFno].poz=i;
	  SST[0][ssFno].score=score;
	  ssFno++;
	}
      }
      else {
	SST[0][ssFno].score=NO_SCORE;
	SST[0][ssFno].type=3;
	SST[0][ssFno].poz=i;
	ssFno++;
      }

    }

    /* stop codons : 3 */
    
    if( i < Data_Len-1 && ((onlytga && (Data[i]=='t' && Data[i+1]=='g' && Data[i+2]=='a')) 
			   ||(onlytaa && (Data[i]=='t' && Data[i+1]=='a' && Data[i+2]=='a'))
			   ||(onlytag && (Data[i]=='t' && Data[i+1]=='a' && Data[i+2]=='g'))
			   || (!onlytga && !onlytaa && !onlytag &&
			       ((Data[i]=='t' && Data[i+1]=='a' && Data[i+2]=='a') ||
				(Data[i]=='t' && Data[i+1]=='g' && Data[i+2]=='a') ||
				(Data[i]=='t' && Data[i+1]=='a' && Data[i+2]=='g'))))) {

      SST[0][ssFno].type=4;
      SST[0][ssFno].poz=i;
      SST[0][ssFno].score=NO_SCORE;

      
      if(isstop==2) {
	if(i>80 && i<=Data_Len-81) {
	  k=0;
	  for(j=i-80;j<i+82;j++){
	    switch (Data[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Stop162(B,&score,Stop_Thr,TRAIN_DIR,md);
	  if(score>=Stop_Thr) {
	    SST[0][ssFno].score=score;
	  }

	}
      }
      else {
	if(i>4 && i<=Data_Len-14) {
	  k=0;
	  for(j=i-4;j<i+15;j++) {
	    switch (Data[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Stop(B,&score,Stop_Thr,TRAIN_DIR,md);
	  SST[0][ssFno].score=score;
	}
      }
      //else SST[0][ssFno].score=NO_SCORE;

      ssFno++;
    }
  }


  //exit(0);

  /* reverse strand */

  ssRno=0;

  char *Copy=NULL;

  Copy=(char *) malloc((Data_Len+2)*sizeof(char));
  if (Copy == NULL) {
    fprintf(stderr,"Memory allocation for copy failure.\n");
    abort();
  }
  
  strcpy(Copy,Data);
  Reverse_Complement(Copy,Data_Len);

  for(i=1;i<Data_Len+1;i++) {

    for(m=0;m<MODEL_NO;m++) {
      if(Use_Protein[m]) {
	if(i>=9) {
	  val = valaac[basetoint(Copy[i-2],1)*16+basetoint(Copy[i-1],1)*4+basetoint(Copy[i],1)]+
	    valaac[basetoint(Copy[i-5],1)*16+basetoint(Copy[i-4],1)*4+basetoint(Copy[i-3],1)]*23+
	    valaac[basetoint(Copy[i-8],1)*16+basetoint(Copy[i-7],1)*4+basetoint(Copy[i-6],1)]*529;
	  Protein[m][1][i]=Protein[m][1][i-3]+Facc[m][val];
	}
	else if(i>=6) {
	  val = valaac[basetoint(Copy[i-2],1)*16+basetoint(Copy[i-1],1)*4+basetoint(Copy[i],1)]+
	    valaac[basetoint(Copy[i-5],1)*16+basetoint(Copy[i-4],1)*4+basetoint(Copy[i-3],1)]*23;
	  Protein[m][1][i]=Protein[m][1][i-3]+Facc2[m][val];
	}
	else if(i>=3) {
	  val = valaac[basetoint(Copy[i-2],1)*16+basetoint(Copy[i-1],1)*4+basetoint(Copy[i],1)];
	  Protein[m][1][i]=Facc1[m][val];
	}
      }
      
      // coding 

      if(i<MODEL_LEN[m]) {
	noncoding = get_prob_of_window2(i-1,NMODEL[m],Copy+1);
      }
      else {
	noncoding = get_prob_of_window1(i-MODEL_LEN[m]+1,NMODEL[m],Copy);
      }
      if(noncoding==0) noncoding=0.00001; // take a very low probability
      
      for(j=0;j<3;j++) 
	if(i>j) {
	  
	  if(i>=j+MODEL_LEN[m]) {
	    model = (int)(i-MODEL_LEN[m]-j)%3;
	    coding = get_prob_of_window1(i-MODEL_LEN[m]+1,MODEL[m][model],Copy);
	  }
	  else {
	    model = ((int)(i-j-1)%3 +1)%3;
	    coding = get_prob_of_window2(i-j-1,MODEL[m][model],Copy+j+1);
	  }
	  if(coding==0) coding=0.00001; // take a very low probability
	  
	  TCod[m][3+j][i]=PDom[m][3+j][i]+TCod[m][3+j][i-1]+log2(coding)-log2(noncoding);
	  //Cod[3+j][i]=Cod[3+j][i-1]+0.5*log2(coding)-log2(noncoding); //if using DT
	  
	}
	else { TCod[m][3+j][i]=0; }
    }
      
    // splice sites

    /* atg's : 0 */

    if(i<Data_Len-1 && Copy[i]=='a' && Copy[i+1]=='t' && Copy[i+2]=='g') { // Deal w/ start sites
      
      //SST[1][ssRno].type=-1;
      //SST[1][ssRno].poz=Data_Len-i+1;
      if(isatg==2) {
	if(i>80 && i<=Data_Len-81) {
	  k=0;
	  for(j=i-80;j<i+82;j++){
	    switch (Copy[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Atg162(B,&score,Atg_Thr,TRAIN_DIR,md);
	  if(score>=Atg_Thr) {
	    SST[1][ssRno].type=-1;
	    SST[1][ssRno].poz=Data_Len-i+1;
	    SST[1][ssRno].score=score;
	    ssRno++;
	  }
	}
	else {
	  SST[1][ssRno].score=NO_SCORE;
	  SST[1][ssRno].type=-1;
	  SST[1][ssRno].poz=Data_Len-i+1;
	  ssRno++;
	}
	
      }
      else {
	if(i>12 && i<=Data_Len-6) {
	  k=0;
	  for(j=i-12;j<i+7;j++) {
	    switch (Copy[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  ret=Is_Atg(B,&score,Atg_Thr,TRAIN_DIR,md);
	  if(score>=Atg_Thr) {
	    SST[1][ssRno].type=-1;
	    SST[1][ssRno].poz=Data_Len-i+1;
	    SST[1][ssRno].score=score;
	    ssRno++;
	  }
	}
	else {
	  SST[1][ssRno].score=NO_SCORE;
	  SST[1][ssRno].type=-1;
	  SST[1][ssRno].poz=Data_Len-i+1;
	  ssRno++;
	}
      }
    }

    /* gt's : 1 */

    if(i<Data_Len && Copy[i]=='g' && Copy[i+1]=='t') { // Deal with donors

      if(i>80 && i<=Data_Len-81) {
	k=0;
	for(j=i-80;j<i+82;j++){
	  switch (Copy[j]){
	  case 'a': B[k]=0;break;
	  case 'c': B[k]=1;break;
	  case 'g': B[k]=2;break;
	  case 't': B[k]=3;break;
	  }
	  k++;
	}

	ret=Is_Donor(B,&score,&leaf, Don_Thr, istdon,TRAIN_DIR,nocod,md);
	
	motifscore=0;
	if(score>=Don_Thr) motifscore=1;

	if(motifscore && esed) {
	  for(l=0;l<featnod-1;l++) maxmotif[l]=-10000;

	  motifscore=Wd[1]*scale(score,scaledatad,begscaled,endscaled,MaxSd[0],MinSd[0])-Wd[0];

	  for(k=82;k<82+flanklend-motiflend;k++)
	    for(l=0;l<featnod-1;l++) {
	      prob=0;
	      for(ti=0;ti<motiflend;ti++) {
		if(Md[l][B[k+ti]*motiflend+ti]!=0) {
		  prob+=Md[l][B[k+ti]*motiflend+ti]*log(Md[l][B[k+ti]*motiflend+ti]/Bgd[B[k+ti]]/log(2));
		}
	      }
	      if(prob>maxmotif[l]) maxmotif[l]=prob;
	    }
	  
	  for(l=0;l<featnod-1;l++) 
	    motifscore+=Wd[l+2]*scale(maxmotif[l],scaledatad,begscaled,endscaled,MaxSd[l+1],MinSd[l+1]);

	  if(leafnod) {
	    motifscore+=Wd[leaf+featnod+1];
	  }

	}

	if(motifscore>0) {
	  SST[1][ssRno].score=score;
	  SST[1][ssRno].type=-2;
	  SST[1][ssRno].poz=Data_Len-i+1;
	  ssRno++;
	}
      }
      else {
	SST[1][ssRno].score=NO_SCORE;
	SST[1][ssRno].type=-2;
	SST[1][ssRno].poz=Data_Len-i+1;
	ssRno++;
      }
    }

    /* ag's : 2 */

    if(i<Data_Len && Copy[i]=='a' && Copy[i+1]=='g') { // Deal with acceptors

      SST[1][ssRno].type=-3;
      SST[1][ssRno].poz=Data_Len-i+1;

      if(i>80 && i<=Data_Len-81) {
	k=0;
	for(j=i-80;j<i+82;j++){
	  switch (Copy[j]){
	  case 'a': B[k]=0;break;
	  case 'c': B[k]=1;break;
	  case 'g': B[k]=2;break;
	  case 't': B[k]=3;break;
	  }
	  k++;
	}

	ret=Is_Acceptor(B,&score, &leaf,Acc_Thr, istacc,TRAIN_DIR,nocod,md);
	
	motifscore=0;
	if(score>=Acc_Thr) motifscore=1;

	if(motifscore && esea) {
	  for(l=0;l<featnoa-1;l++) maxmotif[l]=-10000;
	      
	  motifscore=Wa[1]*scale(score,scaledataa,begscalea,endscalea,MaxSa[0],MinSa[0])-Wa[0];

	  for(k=82;k<82+flanklena-motiflena;k++)
	    for(l=0;l<featnoa-1;l++) {
	      prob=0;
	      for(ti=0;ti<motiflena;ti++) {
		if(Ma[l][B[k+ti]*motiflena+ti]!=0) {
		  prob+=Ma[l][B[k+ti]*motiflena+ti]*log(Ma[l][B[k+ti]*motiflena+ti]/Bga[B[k+ti]]/log(2));
		}
	      }
	      if(prob>maxmotif[l]) maxmotif[l]=prob;
	    }
	  
	  for(l=0;l<featnoa-1;l++) 
	    motifscore+=Wa[l+2]*scale(maxmotif[l],scaledataa,begscalea,endscalea,MaxSa[l+1],MinSa[l+1]);

	  if(leafnoa) {
	    motifscore+=Wa[leaf+featnoa+1];
	  }

	}

	if(motifscore>0) {
	  SST[1][ssRno].type=-3;
	  SST[1][ssRno].poz=Data_Len-i+1;
	  SST[1][ssRno].score=score;
	  ssRno++;
	}
      }
      else {
	SST[1][ssRno].score=NO_SCORE;
	SST[1][ssRno].type=-3;
	SST[1][ssRno].poz=Data_Len-i+1;
	ssRno++;
      }
    }

    /* stop codons : 3 */
    
    if( i < Data_Len-1 && ((onlytga && (Copy[i]=='t' && Copy[i+1]=='g' && Copy[i+2]=='a')) 
			   ||(onlytaa && (Copy[i]=='t' && Copy[i+1]=='a' && Copy[i+2]=='a')) 
			   ||(onlytag && (Copy[i]=='t' && Copy[i+1]=='a' && Copy[i+2]=='g')) 
			   || (!onlytga && !onlytaa && !onlytag &&
       ((Copy[i]=='t' && Copy[i+1]=='a' && Copy[i+2]=='a') ||
       (Copy[i]=='t' && Copy[i+1]=='g' && Copy[i+2]=='a') ||
       (Copy[i]=='t' && Copy[i+1]=='a' && Copy[i+2]=='g'))))) {
      
      SST[1][ssRno].type=-4;
      SST[1][ssRno].poz=Data_Len-i+1;
      SST[1][ssRno].score=NO_SCORE;

      if(isstop==2) {
	if(i>80 && i<=Data_Len-81) {
	  k=0;
	  for(j=i-80;j<i+82;j++){
	    switch (Copy[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Stop162(B,&score,Stop_Thr,TRAIN_DIR,md);
	  if(score>=Stop_Thr) {
	    SST[1][ssRno].score=score;
	  }
	  
	}
      }
      else {
	if(i>4 && i<=Data_Len-14) {
	  k=0;
	  for(j=i-4;j<i+15;j++) {
	    switch (Copy[j]){
	    case 'a': B[k]=0;break;
	    case 'c': B[k]=1;break;
	    case 'g': B[k]=2;break;
	    case 't': B[k]=3;break;
	    }
	    k++;
	  }
	  
	  ret=Is_Stop(B,&score,Stop_Thr,TRAIN_DIR,md);
	  SST[1][ssRno].score=score;
	}
      }
      //else SST[1][ssRno].score=NO_SCORE;

      ssRno++;

    }
  }
  free(Copy);

  // free ESE svm data
  if(esea || esed) {
    free(maxmotif);

    if(esea) {
      // free matrix file
      for(l=0;l<featnoa-1;l++) {
	if(Ma[l] != NULL) { free(Ma[l]);}
      }
      free(Ma);
      // free wieghts
      free(Wa);
      // free scale data
      if(scaledataa) {
	free(MinSa);
	free(MaxSa);
      }
    }

    if(esed) {
      // free matrix file
      for(l=0;l<featnod-1;l++) {
	if(Md[l] != NULL) { free(Md[l]);}
      }
      free(Md);
      // free wieghts
      free(Wd);
      // free scale data
      if(scaledatad) {
	free(MinSd);
	free(MaxSd);
      }
    }
  }

  if(Use_Filter) {
    // filter acceptor & donor sites
    
    for(i=0; i<ssFno;i++) 
      if(SST[0][i].type==2 || SST[0][i].type==3) {
	j=i-1;
	ret=1;
	while(j>=0 && SST[0][i].poz-SST[0][j].poz<Win_Len) {
	  if(SST[0][j].type==SST[0][j].type && SST[0][j].score>SST[0][i].score) {
	    SST[0][i].score=NO_SCORE;
	    ret=0;
	    break;
	  }
	  j--;
	}
	if(ret) {
	  j=i+1;
	  while(j<ssFno && SST[0][j].poz-SST[0][i].poz<Win_Len) {
	    if(SST[0][j].type==SST[0][j].type && SST[0][j].score>SST[0][i].score) {
	      SST[0][i].score=NO_SCORE;
	      ret=0;
	      break;
	    }
	    j++;
	  }
	}
      }

    for(i=0; i<ssRno;i++) 
      if(SST[1][i].type==-2 || SST[1][i].type==-3) {
	j=i-1;
	ret=1;
	while(j>=0 && SST[1][i].poz-SST[1][j].poz<Win_Len) {
	  if(SST[1][j].type==SST[1][j].type && SST[1][j].score>SST[1][i].score) {
	    SST[1][i].score=NO_SCORE;
	    ret=0;
	    break;
	  }
	  j--;
	}
	if(ret) {
	  j=i+1;
	  while(j<ssRno && SST[1][j].poz-SST[1][i].poz<Win_Len) {
	    if(SST[1][j].type==SST[1][j].type && SST[1][j].score>SST[1][i].score) {
	      SST[1][i].score=NO_SCORE;
	      ret=0;
	      break;
	    }
	    j++;
	  }
	}
      }
  }


  /*for(i=1;i<=Data_Len;i++) {
    printf("%d %f %f %f\n",i,TCod[0][0][i],TCod[0][1][i],TCod[0][2][i]);
  }
  exit(0);*/

  // now create the splice sites SS and coding arrays Cod

  ssno=ssFno+ssRno+2;

  SS=(SpliceSite *) malloc((ssno)*sizeof(SpliceSite));
  if (SS == NULL) {
    fprintf(stderr,"Memory allocation for splice sites failure.\n"); 
    abort();
  }


  SS[0].type=0;
  SS[0].poz=0;
  SS[0].score=NO_SCORE;

  k=1;
  i=0;
  j=ssRno-1;
  while(i<ssFno && j>=0) {
    if(SST[0][i].poz<=SST[1][j].poz) {
      SS[k].type=SST[0][i].type;
      SS[k].poz=SST[0][i].poz;
      SS[k++].score=SST[0][i].score;
      i++;
    }
    else {
      SS[k].type=SST[1][j].type;
      SS[k].poz=SST[1][j].poz;
      SS[k++].score=SST[1][j].score;
      j--;
    }
  }

  while(i<ssFno) {
    SS[k].type=SST[0][i].type;
    SS[k].poz=SST[0][i].poz;
    SS[k++].score=SST[0][i].score;
    i++;
  }

  while(j>=0) {
    SS[k].type=SST[1][j].type;
    SS[k].poz=SST[1][j].poz;
    SS[k++].score=SST[1][j].score;
    j--;
  }

  assert(k==ssno-1);

  SS[ssno-1].type=0;
  SS[ssno-1].poz=Data_Len+1;
  SS[ssno-1].score=NO_SCORE;


  // free temporary splice sites SST
  for(i=0;i<2;i++) if(SST[i] != NULL ) free(SST[i]);


  // here compute Cod[]
  for(m=0;m<MODEL_NO;m++)
    for(i=0;i<3;i++) {
      Cod[m][i]=NULL;
      Cod[m][i]=(double *) malloc(ssno*sizeof(double));
      if (Cod[m][i] == NULL) {
	fprintf(stderr,"Memory allocation for coding %d failure.\n",i);
	abort();
      }
    }

  for(i=0;i<ssno;i++)
    for(j=0;j<3;j++) {
      k=SS[i].type;
      switch (k) {
      case 0: // start and stop of sequence
	if(i==0) { // beginning of sequence: store the cumulative score from reverse strand
	  for(m=0;m<MODEL_NO;m++)  Cod[m][j][0]=TCod[m][3+j][Data_Len];
	}
	else { // end of sequence (i==ssno-1) : store the cumulative score from forward strand
	  for(m=0;m<MODEL_NO;m++) Cod[m][j][ssno-1]=TCod[m][j][Data_Len];
	}
	break;
      case 1: // ATG
      case 2: // GT
      case 4: // TAG|TAA|TGA
	for(m=0;m<MODEL_NO;m++) Cod[m][j][i]=TCod[m][j][SS[i].poz-1];
	break;
      case 3: // AG
	for(m=0;m<MODEL_NO;m++) Cod[m][j][i]=TCod[m][j][SS[i].poz+1];
	break;
      case -1: // ATG
      case -2: // GT
      case -4: // TAG|TAA|TGA
	for(m=0;m<MODEL_NO;m++) Cod[m][j][i]=TCod[m][3+j][Data_Len-SS[i].poz];
	break;
      case -3: // AG
	for(m=0;m<MODEL_NO;m++) Cod[m][j][i]=TCod[m][3+j][Data_Len-SS[i].poz+2];
	break;
      }

    }
  
  /*
  for(i=0;i<ssno;i++) {
    if(SS[i].type>0) {
      double left;
      double right;
      int kprev;
      int ind;
      //      printf("%d: %f %f %f\n",i,Cod[0][i],Cod[1][i],Cod[2][i]);
      for(j=0;j<3;j++) {
	switch (SS[i].type) {
	case 1:
	case 3:
	  left=0;
	  right=0;
	  k=i+1;
	  kprev=i;
	  ind=1;
	  while(k<ssno && ind) {
	    if(SS[k].type>0) {
	      if(Cod[j][k]>=Cod[j][kprev]) {
		right+=Cod[j][k]-Cod[j][kprev];
		kprev=k;
	      }
	      else { ind=0;}
	    }
	    k++;
	  }
	  k=i-1;
	  kprev=i;
	  ind=1;
	  while(k>0 && ind) {
	    if(SS[k].type>0) {
	      if(Cod[j][k]>=Cod[j][kprev]) {
		left+=Cod[j][k]-Cod[j][kprev];
		kprev=k;
	      }
	      else { ind=0;}
	    }
	    k--;
	  }
	  break;
	case 2:
	case 4:
	  left=0;
	  right=0;
	  k=i+1;
	  kprev=i;
	  ind=1;
	  while(k<ssno && ind) {
	    if(SS[k].type>0) {
	      if(Cod[j][k]<=Cod[j][kprev]) {
		right+=Cod[j][kprev]-Cod[j][k];
		kprev=k;
	      }
	      else { ind=0;}
	    }
	    k++;
	  }
	  k=i-1;
	  kprev=i;
	  ind=1;
	  while(k>0 && ind) {
	    if(SS[k].type>0) {
	      if(Cod[j][k]<=Cod[j][kprev]) {
		left+=Cod[j][kprev]-Cod[j][k];
		kprev=k;
	      }
	      else { ind=0;}
	    }
	    k--;
	  }
	  break;
	}
	printf("%d %d %f %f %f %f\n",SS[i].poz,SS[i].type,SS[i].score,left,right,Cod[j][i]);
      }
    }

  }
  exit(0);
  */

  // free temporary coding arrays TCod
  for(m=0;m<MODEL_NO;m++)
    for(i=0;i<6;i++) {
      if(TCod[m][i] != NULL ) free(TCod[m][i]);  
      if(PDom[m][i] != NULL ) free(PDom[m][i]);  
    }

  // load exon no distr file
  strcpy(File_Name,TRAIN_DIR);
  strcat(File_Name,EXONNO_DISTR);
  fp = fopen (File_Name, "r");
  if  (fp == NULL)
    {
      fprintf (stderr, "ERROR 8:  Unable to open file %s\n", File_Name);
      exit (0);
    }
  
  fgets (Line, MAX_LINE, fp);
  sscanf(Line,"%d",&ExNo);
  
  ExDistr=NULL;
  ExDistr=(double *) malloc((ExNo+1)*sizeof(double));
  if (ExDistr == NULL) {
    fprintf(stderr,"Memory allocation for no of exons distribution failure.\n");
    abort();
  }
  
  if(Use_Exon_Count) {
    ExTail=NULL;
    ExTail=(double *) malloc((ExNo+1)*sizeof(double));
    if (ExTail == NULL) {
      fprintf(stderr,"Memory allocation for tail no of exons distribution failure.\n");
      abort();
    }
  }
  
  for(i=0;i<=ExNo;i++) {
    fscanf(fp,"%f",&readval);
    if (readval < 0.000001) readval = 0.000001;
    ExDistr[i]=readval;
  }
  
  fclose(fp);
    
  if(Use_Exon_Count) {
    ExTail[0]=1;
    ExTail[ExNo]=ExDistr[ExNo];
    for(i=ExNo-1;i>0;i--) {
      ExTail[i]=ExDistr[i]+ExTail[i+1];
    }
  }
  

  for(m=0;m<MODEL_NO;m++) {

    // load exon distr file
    
    strcpy(File_Name,TRAIN_DIR);
    strcat(File_Name,EXON_DISTR[m]);
    fp = fopen (File_Name, "r");
    if  (fp == NULL)
      {
	fprintf (stderr, "ERROR 9:  Unable to open file %s\n", File_Name);
	exit (0);
      }
    
    while(fgets (Line, MAX_LINE, fp) != NULL) {
      if(strncmp(Line,"Initial",7)==0) {
	sscanf(Line,"%*s %d",&ret);
	LenDistr[m][0]=NULL;
	LenDistr[m][0]=(double *) malloc(ret*sizeof(double));
	if (LenDistr[m][0] == NULL) {
	  fprintf(stderr,"Memory allocation for initial exon length distribution failure.\n");
	  abort();
	}
	LenNo[m][0]=ret;
	for(i=0;i<LenNo[m][0];i++) {
	  fscanf(fp,"%f",&readval);
	  if (readval < 0.000001) readval = 0.000001;
	  LenDistr[m][0][i]=boostdistr[m]+readval;
	}
      }
      else if(strncmp(Line,"Internal",8)==0) {
	sscanf(Line,"%*s %d",&ret);
	LenDistr[m][1]=NULL;
	LenDistr[m][1]=(double *) malloc(ret*sizeof(double));
	if (LenDistr[m][1] == NULL) {
	  fprintf(stderr,"Memory allocation for internal exon length distribution failure.\n");
	  abort();
	}
	LenNo[m][1]=ret;
	for(i=0;i<LenNo[m][1];i++) {
	  fscanf(fp,"%f",&readval);
	  if (readval < 0.000001) readval = 0.000001;
	  LenDistr[m][1][i]=boostdistr[m]+readval;
	}
      }
      else if(strncmp(Line,"Terminal",8)==0) {
	sscanf(Line,"%*s %d",&ret);
	LenDistr[m][2]=NULL;
	LenDistr[m][2]=(double *) malloc(ret*sizeof(double));
	if (LenDistr[m][2] == NULL) {
	  fprintf(stderr,"Memory allocation for terminal exon length distribution failure.\n");
	  abort();
	}
	LenNo[m][2]=ret;
	for(i=0;i<LenNo[m][2];i++) {
	  fscanf(fp,"%f",&readval);
	  if (readval < 0.000001) readval = 0.000001;
	  LenDistr[m][2][i]=boostdistr[m]+readval;
	}
      }
      else if(strncmp(Line,"Single",6)==0) {
	sscanf(Line,"%*s %d",&ret);
	LenDistr[m][3]=NULL;
	LenDistr[m][3]=(double *) malloc(ret*sizeof(double));
	if (LenDistr[m][3] == NULL) {
	  fprintf(stderr,"Memory allocation for single exon length distribution failure.\n");
	  abort();
	}
	LenNo[m][3]=ret;
	for(i=0;i<LenNo[m][3];i++) {
	  fscanf(fp,"%f",&readval);
	  if (readval < 0.000001) readval = 0.000001;
	  LenDistr[m][3][i]=boostdistr[m]+readval;
	}
      }
    }
    
    fclose(fp);
  
  // now compute the geometric parameters
    for(i=4;i<6;i++) {
      LenDistr[m][i]=NULL;
      LenDistr[m][i]=(double *) malloc(4*sizeof(double)); // first two are normal probabilities (1=stop; 2 =continue); 
      // last two are log probabilities
      if (LenDistr[m][i] == NULL) {
	fprintf(stderr,"Memory allocation for noncoding length distribution failure.\n");
	abort();
      }
    }
  
    // intron
    LenDistr[m][4][0]=1/(mintron[m]+1);
    
    // intergenic
    LenDistr[m][5][0]=1/(minterg[m]+m5utr[m]+m3utr[m]+1);
    
    for(i=4;i<6;i++) {
      LenDistr[m][i][1]=1-LenDistr[m][i][0];
      //LenDistr[m][i][2]=boostexon[m]+log2(LenDistr[i][0]);
      LenDistr[m][i][2]=log2(LenDistr[m][i][0]);
      LenDistr[m][i][3]=log2(LenDistr[m][i][1]);
    }
  }  
  
  // load signalp matrix
    
  if(ifSP) { // there is a signalp file in the data
    strcpy(File_Name,TRAIN_DIR);
    strcat(File_Name,SIGNALP_FILE);
    fp = fopen (File_Name, "r");
    if  (fp == NULL)
      {
	fprintf (stderr, "ERROR 10:  Unable to open file %s\n", File_Name);
	exit (0);
      }
    for(i=0;i<32;i++) {
      for(j=0;j<4;j++) {
	fscanf(fp,"%d",&ret);
	SignalP[i][j]=ret;
      }
    }
  }


  // alloc sites memory and init sites

  iinterg+=i5utr+i3utr;

  for(val=INTERG; val<= I2MINUS; val++) {
    Sites[val]==NULL;
    Sites[val] = (Site *) malloc(ssno*sizeof(Site));
    if (Sites[val] == NULL) {
      fprintf(stderr,"Memory allocation for state sites failure.\n"); 
      abort();
    }
    for(j=0;j<ssno;j++) {
      Sites[val][j].score=(double *)malloc(PREDNO*sizeof(double));
      if (Sites[val][j].score == NULL) {
	fprintf(stderr,"Memory allocation for state sites scores failure.\n"); 
	abort();
      }
      Sites[val][j].prevpredno=(int *)malloc(PREDNO*sizeof(int));
      if (Sites[val][j].prevpredno == NULL) {
	fprintf(stderr,"Memory allocation for state sites scores failure.\n"); 
	abort();
      }
      Sites[val][j].prevstatetype=(int *)malloc(PREDNO*sizeof(int));
      if (Sites[val][j].prevstatetype == NULL) {
	fprintf(stderr,"Memory allocation for previous state type failure.\n"); 
	abort();
      }
      Sites[val][j].prevstateno=(long int *)malloc(PREDNO*sizeof(long int));
      if (Sites[val][j].prevstateno == NULL) {
	fprintf(stderr,"Memory allocation for previous state no failure.\n"); 
	abort();
      }
      Sites[val][j].prevex=(struct exon **)malloc(PREDNO*sizeof(struct exon *));
      if (Sites[val][j].prevex == NULL) {
	fprintf(stderr,"Memory allocation for state sites previous exons failure.\n"); 
	abort();
      }
    }

    switch(val) {
    case INTERG:
      for(i=0;i<PREDNO;i++) {
	Sites[val][0].score[i]=0.5*log2(iinterg/iinterg);
      } 
      break;
    case I0PLUS:
    case I1PLUS:
    case I2PLUS:
      for(i=0;i<PREDNO;i++) { 
	if(force) {
	  Sites[val][0].score[i]=-10000;
	}
	else Sites[val][0].score[i]=0.5*(log2(iintron/iinterg)+log2(iin[val-1]*3));
      } 
      break;
    case I0MINUS:
    case I1MINUS:
    case I2MINUS:
      for(i=0;i<PREDNO;i++) {
	if(force) {
	  Sites[val][0].score[i]=-10000;
	}
	else Sites[val][0].score[i]=0.5*(log2(iintron/iinterg)+log2(iin[val-4]*3));
      }
      break;
    }
    for(i=0;i<PREDNO;i++) {
      Sites[val][0].prevex[i]=NULL;
      Sites[val][0].prevstatetype[i]=0;
      Sites[val][0].prevstateno[i]=0;
    } 
  }

  
  // process strand
  
  DP(1,ssno-1,PREDNO,force);

  for(val=I0PLUS; val<=I2MINUS; val++) 
    for(i=0;i<PREDNO;i++) 
      Sites[val][ssno-1].score[i]-=end_partial_penalty;


  // ---------------------------- FREE DATA ------------------------------
  
  if(!loadfalse) {
    // free splice site trees
    freetreenode(atgroot);
    freetreenode(accroot);
    freetreenode(donroot);
    freetreenode(tagroot);
  }
  else {
    if(AtgFalse !=NULL ) free(AtgFalse);
    if(AccFalse !=NULL ) free(AccFalse);
    if(DonFalse !=NULL ) free(DonFalse);
    if(StopFalse !=NULL ) free(StopFalse);
  }
    
  // free splice sites
  if(SS != NULL) free(SS);

  // free Cod structure
  for(i=0;i<3;i++) for(m=0;m<MODEL_NO;m++) if(Cod[m][i] != NULL ) free(Cod[m][i]);

  // free exon distrib
  if(ExDistr != NULL ) free(ExDistr);
  if(Use_Exon_Count) if(ExTail != NULL ) free(ExTail);
  for(i=0;i<6;i++) for(m=0;m<MODEL_NO;m++) if(LenDistr[m][i] != NULL ) free(LenDistr[m][i]);
  
  // free splice site tables
  UnLoadTables();

  // free non-coding/coding models
  for(m=0;m<MODEL_NO;m++) {
    if(Use_Protein[m]) {
      for(i=0;i<2;i++) 
	if(Protein[m][i] != NULL) free(Protein[m][i]);
    }
    if(NMODEL[m] != NULL) free(NMODEL[m]);
    for (i = 0; i < 3; i++) if(MODEL[m][i] != NULL) free(MODEL[m][i]);
    free(MODEL[m]);
    MODEL_READ[m] = FALSE; 
  }
  
  *splicesiteno=ssno;
  return(Sites);

}

void freetreenode(struct tree_node *t)
{
  if(t==NULL) return;
  free_vector(t->coefficients,0,no_of_dimensions+1);
  free_ivector(t->left_count,0,no_of_categories);
  free_ivector(t->right_count,0,no_of_categories);
  freetreenode(t->left);
  freetreenode(t->right);
  free(t);
  t=NULL;
}


exon *scoreinitexon(long int start,long int stop, int *prevstate, int *nextstate,int dir,int m) 
{
  exon *newexon;
  POINT point;
  int len, codlen, frame;
  double splicescore1,splicescore2;
  double transcore,codscore=0;
  double dts1,dts2;
  double SPimmscore,nSPimmscore;
  double Pscore=0;
  long int i,j;
  double sigp;

  newexon=(exon *) malloc(sizeof(exon));
  if (newexon == NULL) {
    fprintf(stderr,"Memory allocation for exon failure.\n"); 
    abort();
  }


  if(ifSP) sigp=score_signalp(start,stop);
  newexon->prev=NULL;
  newexon->exon_no=2;
  
  if(dir>0) { // forward init exon (can only be ending partial exon)
    
    newexon->start=SS[start].poz;
    newexon->stop=SS[stop].poz-1;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=0;
    newexon->rphase=len%3;
    newexon->exon_no=1;
  
    switch(newexon->rphase) {
    case 0: *nextstate=I0PLUS; break;
    case 1: *nextstate=I1PLUS; break;
    case 2: *nextstate=I2PLUS; break;
    }
    
    *prevstate=INTERG;

    newexon->type=0;
    
    transcore=0.5*log2(Trans[m][EINPLUS][*nextstate]);
    // hmm: transcore=log2(Trans[EINPLUS][*nextstate])+ log2(Trans[INTERG][EINPLUS]);
    
    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame=(newexon->start-1)%3;
    if(Use_Protein[m] && newexon->stop-newexon->rphase > newexon->start+newexon->lphase-1 ) Pscore=Protein[m][0][newexon->stop-newexon->rphase]-Protein[m][0][newexon->start-newexon->lphase-1];

  }
  else { // reverse init exon (can only be starting partial exon)
    newexon->start=SS[stop].poz+1;
    newexon->stop=SS[start].poz;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=len%3;
    newexon->rphase=0;

    switch(newexon->lphase) {
    case 0: *prevstate=I0MINUS; break;
    case 1: *prevstate=I1MINUS; break;
    case 2: *prevstate=I2MINUS; break;
    }

    *nextstate=INTERG;

    transcore=0.5*log2(Trans[m][*prevstate][EINMINUS]);
    // hmm: transcore=log2(Trans[I0PLUS][ETRPLUS]);

    newexon->type=4;

    splicescore2=SS[stop].score;
    splicescore1=SS[start].score;

    frame=(Data_Len-newexon->stop)%3;
    if(Use_Protein[m] && newexon->start-newexon->lphase<newexon->stop+newexon->rphase-1) Pscore=Protein[m][1][Data_Len-newexon->start+newexon->lphase+1]-Protein[m][1][Data_Len-newexon->stop-newexon->rphase+2];
  }

  if(!loadfalse) {
    // score splice sites
    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore1;
    classifysgl(&point, atgroot);
    dts1 = point.prob[0];
    free(point.dimension);
    
    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore2;
    classifysgl(&point, donroot);
    dts2 = point.prob[0];
    free(point.dimension);
  }
  else {
    
    if(splicescore1>=AtgFalse[0].score) dts1=loadbin(AtgFalse,0,AtgNo-1,splicescore1);
    else dts1=0;
    if(splicescore2>=DonFalse[0].score) dts2=loadbin(DonFalse,0,DonNo-1,splicescore2);
    else dts2=0;
    /*
    if(splicescore1>=Atg_Thr) dts1=(splicescore1-Atg_Thr)/(Atg_Max-Atg_Thr);
    else dts1=0;
    if(splicescore2>=Don_Thr) dts2=(splicescore2-Don_Thr)/(Don_Max-Don_Thr);
    else dts2=0;
    */
  }



#ifdef MESSAGE
  printf("Exon %x %d %d type =%d : %f %f %f %f %f %f \n",newexon,newexon->start,newexon->stop,newexon->type,splicescore1,splicescore2,dts1,dts2,AtgFalse[0].score,DonFalse[0].score);
#endif

  nSPimmscore=Cod[m][frame][stop]-Cod[m][frame][start];


  if(ifSP) {
    SPimmscore=sigp;
    if(len>63) SPimmscore+=Cod[m][frame][stop]-Cod[m][frame][start+62];
  }

  codlen=len/3;
  if(codlen>=LenNo[m][0]) codlen=LenNo[m][0]-1;

  if(ifSP) codscore = log2(0.2*pow(SPimmscore,2.0)+0.8*pow(nSPimmscore,2.0));
  else
    codscore = nSPimmscore;
  //if(Use_Protein[m]) codscore+=Pscore;
  if(Use_Protein[m] && Pscore<=0) codscore=0;

  if(dts1==0) { dts1=0.000001;}
  if(dts1==1) { dts1=0.999999;}
  //if(sscore[m]) codscore += boostsplice[m]*log2(dts1/(1-dts1));
  if(sscore[m]) {
    codscore += log2(dts1/(1-dts1));
    codscore += boostsplice[m]*splicescore1;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts1/(1-dts1));} 
    else { codscore += boostsplice[m]*splicescore1;}
  }
  

  if(dts2==0) { dts2=0.000001;}
  if(dts2==1) { dts2=0.999999;}
  //if(sscore[m]) codscore += boostsplice[m]*log2(dts2/(1-dts2));
  if(sscore[m]) {
    codscore += log2(dts2/(1-dts2));
    codscore += boostsplice[m]*splicescore2;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts2/(1-dts2));} 
    else { codscore += boostsplice[m]*splicescore2;}
  }
  

  transcore += LenDistr[m][5][2] + log2(Trans[m][INTERG][EINPLUS]) - (double)len*LenDistr[m][5][3];
  //transcore += LenDistr[5][2] - (double)len*LenDistr[5][3];
  
  if(use_dts[m]) newexon->score=dts1*dts2*boostexon[m];
  else newexon->score=boostexon[m];

  newexon->score+=transcore-0.5*splitpenalty[m]+codscore+log2(LenDistr[m][0][codlen]);

  
#ifdef MESSAGE
  printf("initExon %ld %ld logpNP=%f interg->esgl+=%f logpNN=%f transcore=%f codscore=%f immscore=%f logdistr=%f totalscore=%f Pscore=%f\n",newexon->start,newexon->stop,LenDistr[m][5][2],log2(Trans[m][INTERG][EINPLUS]),LenDistr[m][5][3],transcore,codscore,nSPimmscore,log2(LenDistr[m][0][codlen]),newexon->score,Pscore);
#endif
  
  return(newexon);
}

exon *scoretermexon(long int start,long int stop, int *prevstate, int *nextstate,int dir,int m) 
{
  exon *newexon;
  POINT point;
  int len, codlen, frame, ind;
  double splicescore1,splicescore2;
  double immscore,codscore=0,transcore;
  double Pscore=0;
  long int i,j;
  int lterm;
  double dts1,dts2;

  newexon=(exon *) malloc(sizeof(exon));
  if (newexon == NULL) {
    fprintf(stderr,"Memory allocation for exon failure.\n"); 
    abort();
  }
  newexon->prev=NULL;  
  newexon->exon_no=2;
  
  if(dir>0) { // forward term exon (can only be starting partial exon)
    newexon->start=SS[start].poz+1;
    if(SS[start].type) newexon->start++;
    newexon->stop=SS[stop].poz+2;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=len%3;
    newexon->rphase=0;

    if(Use_Protein[m] && newexon->stop-3-newexon->rphase > newexon->start+newexon->lphase-1 ) {
      Pscore=Protein[m][0][newexon->stop-3-newexon->rphase]-Protein[m][0][newexon->start+newexon->lphase-1];
      //fprintf(stderr, "Protein[0][%d]=%f Protein[0][%d]=%f Pscore=%f\n",newexon->stop-3-newexon->rphase,Protein[0][newexon->stop-3-newexon->rphase],newexon->start+newexon->lphase-1,Protein[0][newexon->start+newexon->lphase-1],Pscore);

    }

    frame=newexon->lphase;

    switch(newexon->lphase) {
    case 0: *prevstate=I0PLUS; break;
    case 1: *prevstate=I2PLUS; break;
    case 2: *prevstate=I1PLUS; break;
    }
    
    *nextstate=INTERG;

    newexon->type=2;

    transcore=0.5*log2(Trans[m][EINPLUS][*prevstate]);
    //transcore=log2(Trans[I0PLUS][ETRPLUS]);
    
    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame+=newexon->start-1; // forward
  }
  else { // reverse term exon (can only be ending partial exon)
    newexon->start=SS[stop].poz-1;
    if(SS[start].type) newexon->start--;
    newexon->stop=SS[start].poz-2;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=0;
    newexon->rphase=len%3;
    newexon->exon_no=1;

    frame=newexon->rphase;

    switch(newexon->rphase) {
    case 0: *nextstate=I0MINUS; break;
    case 1: *nextstate=I2MINUS; break;
    case 2: *nextstate=I1MINUS; break;
    }

    *prevstate=INTERG;

    transcore=0.5*log2(Trans[m][*nextstate][EINMINUS]);
    //transcore=log2(Trans[*nextstate][EINMINUS])+ log2(Trans[INTERG][EINPLUS]);

    newexon->type=6;

    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame+=Data_Len-newexon->stop;

    if(Use_Protein[m] && newexon->start-newexon->lphase-3<newexon->stop+newexon->rphase-1) {
      //      fprintf(stderr, "Protein[1][%d] Protein[1][%d]\n",Data_Len-newexon->start+newexon->lphase+4,Data_Len-newexon->stop-newexon->rphase+2);
      Pscore=Protein[m][1][Data_Len-newexon->start+newexon->lphase+4]-Protein[m][1][Data_Len-newexon->stop-newexon->rphase+2];
      //      fprintf(stderr, "Protein[1][%d]=%f Protein[1][%d]=%f Pscore=%f\n",Data_Len-newexon->start+newexon->lphase+4,Protein[1][Data_Len-newexon->start+newexon->lphase+4],Data_Len-newexon->stop-newexon->rphase+2,Protein[1][Data_Len-newexon->stop-newexon->rphase+2]);

    }

  }

  if(!loadfalse) {
    // score splice sites

    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore1;
    classifysgl(&point, accroot);
    dts1 = point.prob[0];
    free(point.dimension);
  
    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore2;
    classifysgl(&point, tagroot);
    dts2 = point.prob[0];
    free(point.dimension);
  }
  else {
    
    if(splicescore1>=AccFalse[0].score) dts1=loadbin(AccFalse,0,AccNo-1,splicescore1);
    else dts1=0;
    if(splicescore2>=StopFalse[0].score) dts2=loadbin(StopFalse,0,StopNo-1,splicescore2);
    else dts2=0;
    /*
    if(splicescore1>=Acc_Thr) dts1=(splicescore1-Acc_Thr)/(Acc_Max-Acc_Thr);
    else dts1=0;
    if(splicescore2>=Stop_Thr) dts2=(splicescore2-Stop_Thr)/(Stop_Max-Stop_Thr);
    else dts2=0;
    */
  }



  frame=frame%3;

#ifdef MESSAGE
  printf("Exon %x %d %d type =%d : %f %f %f %f %f %f\n",newexon,newexon->start,newexon->stop,newexon->type,splicescore1,splicescore2,dts1,dts2,AccFalse[0].score,StopFalse[0].score);

#endif
  
  immscore=Cod[m][frame][stop];
  if(SS[start].type) immscore -= Cod[m][frame][start];
  
  codlen=len/3;
  if(codlen>=LenNo[m][2]) codlen=LenNo[m][2]-1;

  codscore = immscore;
  //if(Use_Protein[m]) codscore+=Pscore;
  if(Use_Protein[m] && Pscore<=0) codscore=0;

  if(dts1==0) { dts1=0.000001;}
  if(dts1==1) { dts1=0.999999;}
  //if(sscore[m]) codscore += boostsplice[m]*log2(dts1/(1-dts1));
  if(sscore[m]) {
    codscore += log2(dts1/(1-dts1));
    codscore += boostsplice[m]*splicescore1;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts1/(1-dts1));}
    else { codscore += boostsplice[m]*splicescore1; }
  }
  
  if(dts2==0) { dts2=0.000001;}
  if(dts2==1) { dts2=0.999999;}
  //  if(sscore[m]) codscore += boostsplice[m]*log2(dts2/(1-dts2));
  if(sscore[m]) {
    codscore += log2(dts2/(1-dts2));
    codscore += boostsplice[m]*splicescore2;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts2/(1-dts2));}
    else { codscore += boostsplice[m]*splicescore2;}
  }
  

  transcore += LenDistr[m][4][2]+log2(Trans[m][I0PLUS][ETRPLUS])- (double)len*LenDistr[m][5][3];
  //transcore += LenDistr[4][2]- (double)len*LenDistr[5][3];

  if(use_dts[m]) newexon->score=dts1*dts2*boostexon[m];
  else newexon->score=boostexon[m];

  newexon->score+=transcore-0.5*splitpenalty[m]+codscore+log2(LenDistr[m][2][codlen]);
  
  
#ifdef MESSAGE
  printf("termExon %ld %ld logpNP=%f intron->etr+=%f logpNN=%f transcore=%f codscore=%f immscore=%f logdistr=%f LenDisrt[2][%d]=%f totalscore=%f Pscore=%f\n",newexon->start,newexon->stop,LenDistr[m][4][2],log2(Trans[m][I0PLUS][ETRPLUS]),LenDistr[m][5][3],transcore,codscore,immscore,log2(LenDistr[m][2][codlen]),codlen,LenDistr[m][2][codlen],newexon->score,Pscore);
#endif


  return(newexon);

}

exon *scoreinternexon(long int start,long int stop, int rphase, int *prevstate, int *nextstate,int dir,int m) 
{
  exon *newexon;
  POINT point;
  int len, codlen, frame, exonstate;
  double splicescore1,splicescore2;
  double immscore,codscore=0,transcore;
  double Pscore=0;
  long int i,j;
  double dts1,dts2;

  newexon=(exon *) malloc(sizeof(exon));
  if (newexon == NULL) {
    fprintf(stderr,"Memory allocation for exon failure.\n"); 
    abort();
  }
  newexon->prev=NULL;
  newexon->rphase=rphase;
  newexon->exon_no=2;

  if(dir>0) { // forward internal exon
    newexon->start=SS[start].poz+1;
    if(SS[start].type) newexon->start++;
    newexon->stop=SS[stop].poz-1;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=(len-rphase)%3;

    if(Use_Protein[m] && newexon->stop-newexon->rphase > newexon->start+newexon->lphase-1 ) Pscore=Protein[m][0][newexon->stop-newexon->rphase]-Protein[m][0][newexon->start+newexon->lphase-1];

    frame=newexon->lphase;

    switch(newexon->lphase) {
    case 0: *prevstate=I0PLUS; exonstate=E0PLUS; break;
    case 1: *prevstate=I2PLUS; exonstate=E2PLUS; break;
    case 2: *prevstate=I1PLUS; exonstate=E1PLUS; break;
    }
    switch(rphase) {
    case 0: *nextstate=I0PLUS; break;
    case 1: *nextstate=I1PLUS; break;
    case 2: *nextstate=I2PLUS; break;
    }

    newexon->type=1;

    transcore = log2(Trans[m][exonstate][*nextstate]);

    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame+=newexon->start-1;
    
  }
  else {
    newexon->start=SS[stop].poz+1;
    newexon->stop=SS[start].poz-1;
    if(SS[start].type) newexon->stop--;
    len=newexon->stop-newexon->start+1;
    newexon->lphase=(len-rphase)%3;
    frame=rphase;
    
    // reverse
    switch(newexon->lphase) {
    case 0: *prevstate=I0MINUS; break;
    case 1: *prevstate=I1MINUS; break;
    case 2: *prevstate=I2MINUS; break;
    }
    switch(rphase) {
    case 0: *nextstate=I0MINUS; exonstate=E0MINUS; break;
    case 1: *nextstate=I2MINUS; exonstate=E0MINUS; break;
    case 2: *nextstate=I1MINUS; exonstate=E0MINUS; break;
    }

    transcore = log2(Trans[m][*prevstate][exonstate]);

    newexon->type=5;

    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame+=Data_Len-newexon->stop;

    //    fprintf(stderr, "Protein[1][%d]=%f Protein[1][%d]=%f Pscore=%f\n",Data_Len-newexon->start+newexon->lphase+1,Protein[1][Data_Len-newexon->start+newexon->lphase+1],Data_Len-newexon->stop-newexon->rphase+2,Protein[1][Data_Len-newexon->stop-newexon->rphase+2],Pscore);

    if(Use_Protein[m] && newexon->start-newexon->lphase<newexon->stop+newexon->rphase-1) Pscore=Protein[m][1][Data_Len-newexon->start+newexon->lphase+1]-Protein[m][1][Data_Len-newexon->stop-newexon->rphase+2];

  }


  if(!loadfalse) {
    // score splice sites  

    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore1;
    classifysgl(&point, accroot);
    dts1 = point.prob[0];
    free(point.dimension);
    
    point.dimension = (float *) malloc (sizeof(float));
    //  no_of_dimensions = 1;
    point.dimension[0] = splicescore2;
    classifysgl(&point, donroot);
    dts2 = point.prob[0];
    free(point.dimension);
  }
  else {
    
    if(splicescore1>=AccFalse[0].score) dts1=loadbin(AccFalse,0,AccNo-1,splicescore1);
    else dts1=0;
    if(splicescore2>=DonFalse[0].score) dts2=loadbin(DonFalse,0,DonNo-1,splicescore2);
    else dts2=0;
    /*
    if(splicescore1>=Acc_Thr) dts1=(splicescore1-Acc_Thr)/(Acc_Max-Acc_Thr);
    else dts1=0;
    if(splicescore2>=Don_Thr) dts2=(splicescore2-Don_Thr)/(Don_Max-Don_Thr);
    else dts2=0;
    */
  }




  frame=frame%3;

#ifdef MESSAGE
  printf("Exon %x %d %d type =%d : %f %f %f %f %f %f start=%d stop=%d %f %f\n",newexon,newexon->start,newexon->stop,newexon->type,splicescore1,splicescore2,dts1,dts2,AccFalse[0].score,DonFalse[0].score,start,stop,Cod[m][frame][stop],Cod[m][frame][start]);
#endif

  immscore=Cod[m][frame][stop];
  if(SS[start].type) immscore-=Cod[m][frame][start];


  codlen=len/3;
  if(codlen>=LenNo[m][1]) codlen=LenNo[m][1]-1;

  codscore = immscore;
  //if(Use_Protein[m]) codscore+=Pscore;
  if(Use_Protein[m] && Pscore<=0) codscore=0;

  if(dts1==0) { dts1=0.000001;}
  if(dts1==1) { dts1=0.999999;}
  // if(sscore[m]) codscore += boostsplice[m]*log2(dts1/(1-dts1));
  if(sscore[m]) {
    codscore += log2(dts1/(1-dts1));
    codscore += boostsplice[m]*splicescore1;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts1/(1-dts1));}
    else { codscore +=  boostsplice[m]*splicescore1;}
  }
  

  if(dts2==0) { dts2=0.000001;}
  if(dts2==1) { dts2=0.999999;}
  //  if(sscore[m]) codscore += boostsplice[m]*log2(dts2/(1-dts2));
  if(sscore[m]) {
    codscore += log2(dts2/(1-dts2));
    codscore += boostsplice[m]*splicescore2;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts2/(1-dts2));}
    else { codscore += boostsplice[m]*splicescore2;}
  }
  

  transcore += LenDistr[m][4][2]+log2(Trans[m][I0PLUS][E0PLUS])- (double)len*LenDistr[m][5][3];
  
  if(use_dts[m]) newexon->score=dts1*dts2*boostexon[m];
  else newexon->score=boostexon[m];

  newexon->score+=transcore+codscore+log2(LenDistr[m][1][codlen]);
  

#ifdef  MESSAGE
  printf("internExon %ld %ld logpNP=%f intron->ex+=%f logpNN=%f transcore=%f codscore=%f immscore=%f logdistr=%f totalscore=%f Pscore=%f\n",newexon->start,newexon->stop,LenDistr[m][4][2],log2(Trans[m][I0PLUS][E0PLUS]),LenDistr[m][5][3],transcore,codscore,immscore,log2(LenDistr[m][1][codlen]),newexon->score,Pscore);
#endif

  return(newexon);
}

int check_for_stops(long int prevstop,long int newstart,int prevrphase)
{
  char stop[4];

  if(prevstop<newstart) { // check for forward stop codons at junction
    switch(prevrphase) {
    case 0: return(1);
    case 1: stop[0]=Data[prevstop]; stop[1]=Data[newstart]; stop[2]=Data[newstart+1]; break;
    case 2: stop[0]=Data[prevstop-1]; stop[1]=Data[prevstop]; stop[2]=Data[newstart]; break;
    }
    if(!onlytga && !onlytaa && !onlytag && ((stop[0]=='t' && stop[1]=='a' && stop[2]=='a') ||
       (stop[0]=='t' && stop[1]=='g' && stop[2]=='a') ||
       (stop[0]=='t' && stop[1]=='a' && stop[2]=='g'))) return(0);
    if(onlytga && (stop[0]=='t' && stop[1]=='g' && stop[2]=='a')) return(0);
    if(onlytaa && (stop[0]=='t' && stop[1]=='a' && stop[2]=='a')) return(0);
    if(onlytag && (stop[0]=='t' && stop[1]=='a' && stop[2]=='g')) return(0);
  }
  else {
    switch(prevrphase) {
    case 0: return(1);
    case 1: stop[0]=Data[prevstop]; stop[1]=Data[newstart]; stop[2]=Data[newstart-1]; break;
    case 2: stop[0]=Data[prevstop+1]; stop[1]=Data[prevstop]; stop[2]=Data[newstart]; break;
    }
    if(!onlytga && !onlytaa && !onlytag && ((stop[0]=='a' && stop[1]=='t' && stop[2]=='t') ||
       (stop[0]=='a' && stop[1]=='c' && stop[2]=='t') ||
       (stop[0]=='a' && stop[1]=='t' && stop[2]=='c'))) return(0);
    if(onlytga && (stop[0]=='a' && stop[1]=='c' && stop[2]=='t')) return(0);
    if(onlytaa && (stop[0]=='a' && stop[1]=='t' && stop[2]=='t')) return(0);
    if(onlytag && (stop[0]=='a' && stop[1]=='t' && stop[2]=='c')) return(0);
  }

  return(1);
}

exon *scoresglexon(long int start,long int stop,int dir,int m) 
{
  exon *newexon;
  POINT point;
  int len, codlen, frame;
  double splicescore1,splicescore2;
  double codscore=0, transcore;
  double SPimmscore,nSPimmscore;
  long int i,j;
  double Pscore=0;
  double sigp;
  double dts1,dts2;

  newexon=(exon *) malloc(sizeof(exon));
  if (newexon == NULL) {
    fprintf(stderr,"Memory allocation for exon failure.\n"); 
    abort();
  }
  newexon->prev=NULL;
  newexon->lphase=0;
  newexon->rphase=0;
  newexon->exon_no=1;
  if(ifSP) sigp=score_signalp(start,stop);

  if(dir>0) { // forward sgl exon
    
    newexon->start=SS[start].poz;
    newexon->stop=SS[stop].poz+2;
    newexon->type=3;

    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame=(newexon->start-1)%3;
    
    if(Use_Protein[m] && newexon->stop-3-newexon->rphase > newexon->start+newexon->lphase-1 ) Pscore=Protein[m][0][newexon->stop-3-newexon->rphase]-Protein[m][0][newexon->start+newexon->lphase-1];

  }
  else { // reverse sgl exon
    newexon->start=SS[stop].poz-2;
    newexon->stop=SS[start].poz;
    newexon->type=7;
    
    splicescore1=SS[start].score;
    splicescore2=SS[stop].score;

    frame=(Data_Len-newexon->stop)%3;

    if(Use_Protein[m] && newexon->start-newexon->lphase-3<newexon->stop+newexon->rphase-1) Pscore=Protein[m][1][Data_Len-newexon->start+newexon->lphase+4]-Protein[m][1][Data_Len-newexon->stop-newexon->rphase+2];

  }

    len=(int)(newexon->stop-newexon->start+1);

    if(!loadfalse) {
      // score splice sites

      point.dimension = (float *) malloc (sizeof(float));
      //  no_of_dimensions = 1;
      point.dimension[0] = splicescore1;
      classifysgl(&point, atgroot);
      dts1 = point.prob[0];
      free(point.dimension);
      
      point.dimension = (float *) malloc (sizeof(float));
      //  no_of_dimensions = 1;
      point.dimension[0] = splicescore2;
      classifysgl(&point, tagroot);
      dts2 = point.prob[0];
      free(point.dimension);
    }
    else {
      
      if(splicescore1>=AtgFalse[0].score) dts1=loadbin(AtgFalse,0,AtgNo-1,splicescore1);
      else dts1=0;
      if(splicescore2>=StopFalse[0].score) dts2=loadbin(StopFalse,0,StopNo-1,splicescore2);
      else dts2=0;
      /*
      if(splicescore1>=Atg_Thr) dts1=(splicescore1-Atg_Thr)/(Atg_Max-Atg_Thr);
      else dts1=0;
      if(splicescore2>=Stop_Thr) dts2=(splicescore2-Stop_Thr)/(Stop_Max-Stop_Thr);
      else dts2=0;
      */
    }




#ifdef MESSAGE
    printf("Exon %x %d %d type =%d : %f %f %f %f %f %f\n",newexon,newexon->start,newexon->stop,newexon->type,splicescore1,splicescore2,dts1,dts2,AtgFalse[0].score,StopFalse[0].score);
#endif

  
  nSPimmscore=Cod[m][frame][stop]-Cod[m][frame][start];

  
  if(ifSP) {
    SPimmscore=sigp;
    if(len>66) SPimmscore+=Cod[m][frame][stop-3]-Cod[m][frame][start+62];
  }

  codlen=len/3;
  if(codlen>=LenNo[m][3]) codlen=LenNo[m][3]-1;

  if(ifSP) codscore = log2(0.2*pow(SPimmscore,2.0)+0.8*pow(nSPimmscore,2.0));
  else
    codscore = nSPimmscore;
  //if(Use_Protein[m]) codscore+=Pscore;
  if(Use_Protein[m] && Pscore<=0) codscore=0;

  if(dts1==0) { dts1=0.000001;}
  if(dts1==1) { dts1=0.999999;}
  //if(sscore[m]) codscore += boostsplice[m]*log2(dts1/(1-dts1));
  if(sscore[m]) {
    codscore += log2(dts1/(1-dts1));
    codscore += boostsplice[m]*splicescore1;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts1/(1-dts1));}
    else { codscore += boostsplice[m]*splicescore1;}
  }
  

  if(dts2==0) { dts2=0.000001;}
  if(dts2==1) { dts2=0.999999;}
  //if(sscore[m]) codscore += boostsplice[m]*log2(dts2/(1-dts2));
  if(sscore[m]) {
    codscore += log2(dts2/(1-dts2));
    codscore += boostsplice[m]*splicescore2;
  }
  else {
    if(!boostsplice[m]) { codscore += log2(dts2/(1-dts2));}
    else { codscore += boostsplice[m]*splicescore2;}
  }
  

  transcore= LenDistr[m][5][2] + log2(Trans[m][INTERG][ESGLPLUS]) - (double)len*LenDistr[m][5][3];

  if(use_dts[m]) newexon->score=dts1*dts2*boostexon[m];
  else newexon->score=boostexon[m];

  newexon->score+=transcore-splitpenalty[m]+codscore+log2(LenDistr[m][3][codlen]);
  

#ifdef MESSAGE
  printf("sglExon %ld %ld logpNP=%f interg->esgl+=%f logpNN=%f transcore=%f codscore=%f immscore=%f logdistr=%f LenDistr[3][%d]=%f totalscore=%f Pscore=%f\n",newexon->start,newexon->stop,LenDistr[m][5][2],log2(Trans[m][INTERG][ESGLPLUS]),LenDistr[m][5][3],transcore,codscore,nSPimmscore,log2(LenDistr[m][3][codlen]),codlen,LenDistr[m][3][codlen],newexon->score,Pscore);  
#endif

  return(newexon);

}


void DP(long int start, long int stop,int PREDNO,int force) 
{
  long int i,j, thisstop;
  int s,k,h,m,lenin;
  long int laststop[6]; // remembers stops in all six reading frames
  exon **newexon;
  int prevstate,nextstate, rphase;
  double tempscore;
  int *changed[NCSTATES];
  int pos;

  newexon = (exon **) malloc(PREDNO*sizeof(exon*));
  if(newexon==NULL) {
    fprintf(stderr,"Memory allocation for newexon failure!\n");
    abort();
  }
  
  for(s=0;s<NCSTATES;s++) {
    changed[s]=(int *) malloc(PREDNO*sizeof(int));
    if(changed[s]==NULL) {
      fprintf(stderr,"Memory allocation for changed status failure!\n");
      abort();
    }
  }


  for(i=0;i<6;i++) laststop[i]=0;


  for(i=start;i<=stop;i++) {

    /*    if(i==stop) {
      printf("\n");
      }*/

    for(s=INTERG; s<= I2MINUS; s++) 
      for(j=0;j<PREDNO;j++) {
	Sites[s][i].score[j]=Sites[s][i-1].score[j];
	if(s==INTERG) Sites[s][i].score[j]=Sites[s][i-1].score[0]; // this is more like an heuristic for longer sequences; doesn't give the perfect answer
	Sites[s][i].prevex[j]=Sites[s][i-1].prevex[j];
	Sites[s][i].prevstatetype[j]=Sites[s][i-1].prevstatetype[j];
	Sites[s][i].prevstateno[j]=Sites[s][i-1].prevstateno[j];
	Sites[s][i].prevpredno[j]=Sites[s][i-1].prevpredno[j];
	changed[s][j]=0;

#ifdef MESSAGE
	printf("Sites[%d][%d].score=%f poz=%d type=%d score=%f",s,i,Sites[s][i].score[j],SS[i].poz,SS[i].type,SS[i].score);
	if(Sites[s][i].prevex[j]!=NULL) {
	  printf(" prevexon=%d %d %d",Sites[s][i].prevex[j]->start,Sites[s][i].prevex[j]->stop,Sites[s][i].prevex[j]->type);
	}
	printf("\n");
#endif

      }

    // forward strand

    if(SS[i].type==4 && SS[i].score>=Stop_Thr) { // stop codon on forw strand at SS[i].poz,SS[i].poz+1,SS[i].poz+2
      // go back to score exons
      j=i-1;
      thisstop=laststop[(SS[i].poz-1)%3];
      while(j>=0 && j>=thisstop) {
	if(SS[j].type==1 && SS[j].poz%3 == SS[i].poz%3 && SS[i].poz-SS[j].poz+3 > min_gene_len) { // single exon

	  for(m=0;m<MODEL_NO;m++) {
	    newexon[0]=scoresglexon(j,i,1,m);

	    //	  if(newexon[0]->score>0) {

	    for(k=1;k<PREDNO;k++) 
	      newexon[k]=copyexon(newexon[0]);
	    
	    for(k=0;k<PREDNO;k++) {	    
	      newexon[k]->prev=Sites[INTERG][j].prevex[k];
	      
	      tempscore = newexon[k]->score + Sites[INTERG][j].score[k];
	      h=j;
	      if(intergval[m]) {
		while(Sites[INTERG][h].prevex[k]!=NULL && newexon[k]->start-Sites[INTERG][h].prevex[k]->stop-1<= intergval[m]) {
		  h--;
		}
		if(h<j && tempscore-intergpen[m]<newexon[k]->score + Sites[INTERG][h].score[k]) {
		  newexon[k]->prev=Sites[INTERG][h].prevex[k];
		  tempscore=newexon[k]->score + Sites[INTERG][h].score[k];
		}
	      }
	      
	      //if(intergval[m] && newexon[k]->prev!=NULL && newexon[k]->start-newexon[k]->prev->stop-1<= intergval[m]) tempscore-=intergpen[m]; // short interg. penalty

#ifdef MESSAGE
	      if(!intergval[m]) h=j;
	      printf("k=%d Prev state : Sites[%d][%d].score=%f",k,INTERG,h, Sites[INTERG][h].score[k]);
	      if(Sites[INTERG][h].prevex[k] !=NULL) {
		printf(" prevexon=%d %d %d",Sites[INTERG][h].prevex[k]->start,Sites[INTERG][h].prevex[k]->stop,Sites[INTERG][h].prevex[k]->type);
	      }
	      printf("\n");
#endif
	      
	      // introduce the heuristic here to allow for variation in prediction
	      pos=insertmaxscore(tempscore,newexon[k],INTERG,i,changed[INTERG],PREDNO,INTERG,h,k);
	      if(pos>k && Sites[INTERG][h].score[pos]==Sites[INTERG][h].score[k]) {
		newexon[k]->prev=Sites[INTERG][h].prevex[pos];
		Sites[INTERG][i].prevpredno[pos]=pos;
	      }
	    }
	    
	    /*}
	      else {
	      free(newexon[0]);
	      newexon[0]=NULL;
	      }*/
	    
	  }
	}

	if((!force && SS[j].type==0) || SS[j].type==3) { // (partial) terminal exon -- don't link to beginning of sequence if whole gene predictions are enforced (when force==1)
	  for(m=0;m<MODEL_NO;m++) {
	    newexon[0] = scoretermexon(j,i,&prevstate,&nextstate,1,m);  

	    //if(newexon[0]->score>0) {

	    for(k=1;k<PREDNO;k++) 
	      newexon[k]=copyexon(newexon[0]);

	    for(k=0;k<PREDNO;k++) {

	      newexon[k]->prev=Sites[prevstate][j].prevex[k];
	      if(newexon[k]->prev) {
		newexon[k]->exon_no=addexon(newexon[k]->prev->exon_no);
		lenin=1+newexon[k]->start-newexon[k]->prev->stop;
		if(stopcodon(0,newexon[k]->lphase,newexon[k]->prev->stop,newexon[k]->start)) {
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif
		  free(newexon[k]);
		  newexon[k]=NULL;
		}
	      
	      }
	      else {
		lenin=newexon[k]->start-1;
	      }
	      
	      if(newexon[k]) {

		if(ok_gene_length(newexon[k],prevstate,j,k,1)) {
		//if(1) {
		  if(Use_Intron_Distrib[m])	newexon[k]->score+=(double)lenin*(LenDistr[m][4][3]-LenDistr[m][5][3]);
		
		
#ifdef MESSAGE
		  printf("k=%d Prev state : Sites[%d][%d].score=%f exon_score=%f",k,prevstate,j, Sites[prevstate][j].score[k],newexon[k]->score);
		  if(Sites[prevstate][j].prevex[k] !=NULL) {
		  printf(" prevexon=%d %d %d",Sites[prevstate][j].prevex[k]->start,Sites[prevstate][j].prevex[k]->stop,Sites[prevstate][j].prevex[k]->type);
		}
		  printf("\n");
#endif

		
		
		  tempscore = newexon[k]->score + Sites[prevstate][j].score[k];
		
		  if(Use_Exon_Count) { 
		    //tempscore += log2(ExDistr[newexon->exon_no])-log2(ExTail[newexon->exon_no]);
		    tempscore -= log2(ExTail[newexon[k]->exon_no]); 
		  }

		  pos=insertmaxscore(tempscore,newexon[k],INTERG,i,changed[INTERG],PREDNO,prevstate,j,k);
		  if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
		    newexon[k]->prev=Sites[prevstate][j].prevex[pos];
		    Sites[INTERG][i].prevpredno[pos]=pos;
		  }
		}
		else {
		  free(newexon[k]); newexon[k]=NULL;
		}
	      }
	    }
	    /*}
	      else {
	      free(newexon[0]);
	      newexon[0]=NULL;
	      }*/
	  }
	}

	j--;
      }

    }


    if(SS[i].type==4) {
      laststop[(SS[i].poz-1)%3]=i;
    }
    
    if((!force && SS[i].type==0) || SS[i].type==2) { // GT: donor on forw strand at i,i+1

      // go back to score exons
      for(rphase=0;rphase<3;rphase++) {
	j=i-1;
	thisstop=laststop[(SS[i].poz-rphase-1)%3];

	while(j>=1 && j>=thisstop) { 

	  if(SS[j].type==1 && SS[j].poz<=SS[i].poz-3-rphase &&
	     (SS[j].poz+2)%3 == (SS[i].poz-rphase-1)%3) { // (partial) initial exon to Irphase+
	    
	    for(m=0;m<MODEL_NO;m++) {
	      newexon[0] = scoreinitexon(j,i,&prevstate,&nextstate,1,m);

	      //if(newexon[0]->score>0) {
	      for(k=1;k<PREDNO;k++) 
		newexon[k]=copyexon(newexon[0]);

	      for(k=0;k<PREDNO;k++) {

		newexon[k]->prev=Sites[INTERG][j].prevex[k];

		tempscore = newexon[k]->score + Sites[INTERG][j].score[k];
		h=j;
		if(intergval[m]) {
		  while(Sites[INTERG][h].prevex[k]!=NULL && newexon[k]->start-Sites[INTERG][h].prevex[k]->stop-1<= intergval[m]) {
		    h--;
		  }
		  if(h<j && tempscore-intergpen[m]<newexon[k]->score + Sites[INTERG][h].score[k]) {
		    newexon[k]->prev=Sites[INTERG][h].prevex[k];
		    tempscore=newexon[k]->score + Sites[INTERG][h].score[k];
		  }
		}
	      
		//if(intergval[m] && newexon[k]->prev!=NULL && newexon[k]->start-newexon[k]->prev->stop-1<= intergval[m]) tempscore-=intergpen[m]; // short interg. penalty

#ifdef MESSAGE
		if(!intergval[m]) h=j;
		printf("k=%d Prev state : Sites[%d][%d].score=%f",k,INTERG,h, Sites[INTERG][h].score[k]);
		if(Sites[INTERG][h].prevex[k] !=NULL) {
		  printf(" prevexon=%d %d %d",Sites[INTERG][h].prevex[k]->start,Sites[INTERG][h].prevex[k]->stop,Sites[INTERG][h].prevex[k]->type);
		}
		printf("\n");
		printf("last stop : %ld\n",SS[thisstop].poz);
#endif

		if(Use_Exon_Count) { tempscore += log2(ExTail[2]);}
		
		pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,INTERG,h,k);
		if(pos>k && Sites[INTERG][h].score[pos]==Sites[INTERG][h].score[k]) {
		  newexon[k]->prev=Sites[INTERG][h].prevex[pos];
		  Sites[nextstate][i].prevpredno[pos]=pos;
		}
		
	      }
	      /*}
		else {
		free(newexon[0]);
		newexon[0]=NULL;
		}*/
	    }
	  }

	  if((!force && SS[j].type==0) || (SS[j].type==3 && SS[j].poz+2<SS[i].poz-rphase-1)) { // (partial) internal exon

	    for(m=0;m<MODEL_NO;m++) {
	      newexon[0] = scoreinternexon(j,i,rphase,&prevstate,&nextstate,1,m);

	      //if(newexon[0]->score>0) {
	      for(k=1;k<PREDNO;k++) 
		newexon[k]=copyexon(newexon[0]);

	      for(k=0;k<PREDNO;k++) {

		newexon[k]->prev=Sites[prevstate][j].prevex[k];

		if(newexon[k]->prev) {
		  newexon[k]->exon_no=addexon(newexon[k]->prev->exon_no);
		  lenin=1+newexon[k]->start-newexon[k]->prev->stop;
		  if(stopcodon(0,newexon[k]->lphase,newexon[k]->prev->stop,newexon[k]->start)){
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif
		    free(newexon[k]);
		    newexon[k]=NULL;
		  }
		}
		else {
		  lenin=newexon[k]->start-1;
		}

		if(newexon[k]) {
		
		  if(Use_Intron_Distrib[m]) newexon[k]->score+=(double)lenin*(LenDistr[m][4][3]-LenDistr[m][5][3]);
		 

#ifdef MESSAGE
		  printf("k=%d Prev state : Sites[%d][%d].score=%f",k,prevstate,j, Sites[prevstate][j].score[k]);
		  if(Sites[prevstate][j].prevex[k] !=NULL) {
		    printf(" prevexon=%d %d %d",Sites[prevstate][j].prevex[k]->start,Sites[prevstate][j].prevex[k]->stop,Sites[prevstate][j].prevex[k]->type);
		  }
		  printf("\n");
#endif
 
		  tempscore=newexon[k]->score+Sites[prevstate][j].score[k];

		  if(Use_Exon_Count) { 
		    tempscore += log2(ExTail[newexon[k]->exon_no+1])-log2(ExTail[newexon[k]->exon_no]);
		  }
		  
		  if(partial[m]) {
		    pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,prevstate,j,k); // here I allow partial genes in the beginning
		    if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
		      newexon[k]->prev=Sites[prevstate][j].prevex[pos];
		      Sites[nextstate][i].prevpredno[pos]=pos;
		    }
		  }
		  else  
		    if(newexon[k]->prev!=NULL) {
		      pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,prevstate,j,k);
		      if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
			newexon[k]->prev=Sites[prevstate][j].prevex[pos];
			Sites[nextstate][i].prevpredno[pos]=pos;
		      }
		    }
		    else {
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif
		      free(newexon[k]);
		      newexon[k]=NULL;
		    }

		}
	      }
	      /*}
		else {
		free(newexon[0]);
		newexon[0]=NULL;
		} */
	    }
	  }

	  j--;
	}

      }
    }

    // reverse strand
      
    if(SS[i].type==-4) {
      laststop[3+SS[i].poz%3]=i;
    }

    if(SS[i].type==-1) { // start codon on rev strand 
      
      // go back to score introns
      
      j=laststop[3+SS[i].poz%3];
      if(j>=1 && SS[j].score>=Stop_Thr && SS[j].poz<SS[i].poz-2 && SS[i].poz-SS[j].poz+3 > min_gene_len) { // single exon

	for(m=0;m<MODEL_NO;m++) {
	  newexon[0]=scoresglexon(i,j,-1,m); 

	  //if(newexon[0]->score>0) {
	  for(k=1;k<PREDNO;k++) 
	    newexon[k]=copyexon(newexon[0]);
	  
	  for(k=0;k<PREDNO;k++) {

	    newexon[k]->prev=Sites[INTERG][j].prevex[k];

	    tempscore = newexon[k]->score + Sites[INTERG][j].score[k];
	    h=j;
	    if(intergval[m]) {
	      while(Sites[INTERG][h].prevex[k]!=NULL && newexon[k]->start-Sites[INTERG][h].prevex[k]->stop-1<= intergval[m]) {
		h--;
	      }


	      if(h<j && tempscore-intergpen[m]<newexon[k]->score + Sites[INTERG][h].score[k]) {
		newexon[k]->prev=Sites[INTERG][h].prevex[k];
		tempscore=newexon[k]->score + Sites[INTERG][h].score[k];
	      }
	    }
	    
	    //if(intergval[m] && newexon[k]->prev!=NULL && newexon[k]->start-newexon[k]->prev->stop-1<= intergval[m]) tempscore-=intergpen[m]; // short interg. penalty

#ifdef MESSAGE
	    if(!intergval[m]) h=j;
	    printf("k=%d Prev state : Sites[%d][%d].score=%f",k,INTERG,h, Sites[INTERG][h].score[k]);
	    if(Sites[INTERG][h].prevex[k] !=NULL) {
	      printf(" prevexon=%d %d %d",Sites[INTERG][h].prevex[k]->start,Sites[INTERG][h].prevex[k]->stop,Sites[INTERG][h].prevex[k]->type);
	    }
	    printf("\n");
#endif

	    pos=insertmaxscore(tempscore,newexon[k],INTERG,i,changed[INTERG],PREDNO,INTERG,h,k);
	    if(pos>k && Sites[INTERG][h].score[pos]==Sites[INTERG][h].score[k]) {
	      newexon[k]->prev=Sites[INTERG][h].prevex[pos];
	      Sites[INTERG][i].prevpredno[pos]=pos;
	    }
	  }
	  /*}
	    else {
	    free(newexon[0]);
	    newexon[0]=NULL;
	    }*/
	}
      }

      thisstop=j+1;
      j=i-1;
      while(j>=0 && j>=thisstop) {
	
	if((!force && SS[j].type==0) || (SS[j].type==-2 && SS[j].poz<SS[i].poz-2)) { // (partial) initial exon
	  for(m=0;m<MODEL_NO;m++) {
	    newexon[0] = scoreinitexon(i,j,&prevstate,&nextstate,-1,m);

	    //if(newexon[0]->score>0) {
	    for(k=1;k<PREDNO;k++) 
	      newexon[k]=copyexon(newexon[0]);

	    for(k=0;k<PREDNO;k++) {

	      newexon[k]->prev=Sites[prevstate][j].prevex[k];

	      if(newexon[k]->prev) {
		newexon[k]->exon_no=addexon(newexon[k]->prev->exon_no);
		lenin=1+newexon[k]->start-newexon[k]->prev->stop;
		if(stopcodon(1,newexon[k]->lphase,newexon[k]->prev->stop,newexon[k]->start)) {
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif
		  free(newexon[k]);
		  newexon[k]=NULL;
		}
	      }
	      else {
		lenin=newexon[k]->start-1;
	      }
	      
	      if(newexon[k]) {
		if(ok_gene_length(newexon[k],prevstate,j,k,-1)) {
		  //if(1) {
		  if(Use_Intron_Distrib[m]) newexon[k]->score+=(double)lenin*(LenDistr[m][4][3]-LenDistr[m][5][3]);
		
#ifdef MESSAGE
		  printf("k=%d Prev state : Sites[%d][%d].score=%f",k,prevstate,j, Sites[prevstate][j].score[k]);
		  if(Sites[prevstate][j].prevex[k] !=NULL) {
		    printf(" prevexon=%d %d %d",Sites[prevstate][j].prevex[k]->start,Sites[prevstate][j].prevex[k]->stop,Sites[prevstate][j].prevex[k]->type);
		  }
		  printf("\n");
		  printf("last stop : %ld\n",SS[thisstop].poz);
#endif

		  tempscore = newexon[k]->score + Sites[prevstate][j].score[k];

		  if(Use_Exon_Count) { 
		    //tempscore += log2(ExDistr[newexon->exon_no])-log2(ExTail[newexon->exon_no]);
		    tempscore -= log2(ExTail[newexon[k]->exon_no]);
		  }

		  pos=insertmaxscore(tempscore,newexon[k],INTERG,i,changed[INTERG],PREDNO,prevstate,j,k);
		  if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
		    newexon[k]->prev=Sites[prevstate][j].prevex[pos];
		    Sites[INTERG][i].prevpredno[pos]=pos;
		  }
		}
		else {
		  free(newexon[k]); newexon[k]=NULL;
		}
	      }
	    }
	    /*}
	      else {
	      free(newexon[0]);
	      newexon[0]=NULL;
	      }*/
	  }
	}

	j--;
      }
    }

    if((!force && SS[i].type==0) || SS[i].type==-3) { // AG: acceptor on rev strand 

      
      // go back to score exons
      for(rphase=0;rphase<3;rphase++) {
	j=i-1;
	thisstop=laststop[3+(SS[i].poz-2-rphase)%3];

#ifdef MESSAGE
	printf("rphase=%d stop=%d\n",rphase,thisstop);
#endif

	
	if(thisstop>=1 && SS[thisstop].score>=Stop_Thr) {
	  
	  for(m=0;m<MODEL_NO;m++) {
	    newexon[0]=scoretermexon(i,thisstop,&prevstate,&nextstate,-1,m);  // first score terminal exon

	    //if(newexon[0]->score>0) {

	    for(k=1;k<PREDNO;k++) 
	      newexon[k]=copyexon(newexon[0]);

	    for(k=0;k<PREDNO;k++) {

	      newexon[k]->prev=Sites[INTERG][thisstop].prevex[k];
	      
	      tempscore = newexon[k]->score + Sites[INTERG][thisstop].score[k];
	      h=thisstop;
	      if(intergval[m]) {
		while(Sites[INTERG][h].prevex[k]!=NULL && newexon[k]->start-Sites[INTERG][h].prevex[k]->stop-1<= intergval[m]) {
		  h--;
		}
		if(h<thisstop && tempscore-intergpen[m]<newexon[k]->score + Sites[INTERG][h].score[k]) {
		  newexon[k]->prev=Sites[INTERG][h].prevex[k];
		  tempscore=newexon[k]->score + Sites[INTERG][h].score[k];
		}
	      }
	      
	      //if(intergval[m] && newexon[k]->prev!=NULL && newexon[k]->start-newexon[k]->prev->stop-1<= intergval[m]) tempscore-=intergpen[m]; // short interg. penalty
	      
#ifdef MESSAGE
	      if(!intergval[m]) h=thisstop;
	      printf("k=%d Prev state : Sites[%d][%d].score=%f",k,INTERG,h, Sites[INTERG][h].score[k]);
	      if(Sites[INTERG][h].prevex[k] !=NULL) {
		printf(" prevexon=%d %d %d",Sites[INTERG][h].prevex[k]->start,Sites[INTERG][h].prevex[k]->stop,Sites[INTERG][h].prevex[k]->type);
	      }
	      printf("\n");
#endif
	      
	      if(Use_Exon_Count) { tempscore += log2(ExTail[2]);}
	      
	      pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,INTERG,h,k);
	      if(pos>k && Sites[INTERG][h].score[pos]==Sites[INTERG][h].score[k]) {
		newexon[k]->prev=Sites[INTERG][h].prevex[pos];
		Sites[nextstate][i].prevpredno[pos]=pos;
	      }

	    }
	    /*}
	      else {
	      free(newexon[0]);
	      newexon[0]=NULL;
	      }*/
	  }
	}

	while(j>=0 && j>=thisstop) {
	    
	  if((j==0 && SS[i].poz>2 && !force) || (SS[j].type==-2 && SS[j].poz<=SS[i].poz-3-rphase)) { // (partial) internal exon

	    for(m=0;m<MODEL_NO;m++) {
	      newexon[0] = scoreinternexon(i,j,rphase, &prevstate, &nextstate,-1,m);

	      //if(newexon[0]->score>0) {
	      for(k=1;k<PREDNO;k++) 
		newexon[k]=copyexon(newexon[0]);
	      
	      for(k=0;k<PREDNO;k++) {
		
		newexon[k]->prev=Sites[prevstate][j].prevex[k];
		if(newexon[k]->prev) {
		  newexon[k]->exon_no=addexon(newexon[k]->prev->exon_no);
		  lenin=1+newexon[k]->start-newexon[k]->prev->stop;
		  if(stopcodon(1,newexon[k]->lphase,newexon[k]->prev->stop,newexon[k]->start)) {
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif
		    free(newexon[k]);
		    newexon[k]=NULL;
		  }
		}
		else {
		  lenin=newexon[k]->start -1;
		}
		
		if(newexon[k]) {
		  if(Use_Intron_Distrib[m]) newexon[k]->score+=(double)lenin*(LenDistr[m][4][3]-LenDistr[m][5][3]);
		  
#ifdef MESSAGE
		  printf("k=%d Prev state : Sites[%d][%d].score=%f",k,prevstate,j, Sites[prevstate][j].score[k]);
		  if(Sites[prevstate][j].prevex[k] !=NULL) {
		    printf(" prevexon=%d %d %d",Sites[prevstate][j].prevex[k]->start,Sites[prevstate][j].prevex[k]->stop,Sites[prevstate][j].prevex[k]->type);
		  }
		  printf("\n");
#endif

		  tempscore=newexon[k]->score+Sites[prevstate][j].score[k];

		  if(Use_Exon_Count) { 
		    tempscore += log2(ExTail[newexon[k]->exon_no+1])-log2(ExTail[newexon[k]->exon_no]);
		  }

		  if(partial[m]) {
		    pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,prevstate,j,k); // here I allow partial genes in the beginning
		    if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
		      newexon[k]->prev=Sites[prevstate][j].prevex[pos];
		      Sites[nextstate][i].prevpredno[pos]=pos;
		    }
		  }
		  else 
		    if(newexon[k]->prev!=NULL) {
		      pos=insertmaxscore(tempscore,newexon[k],nextstate,i,changed[nextstate],PREDNO,prevstate,j,k);
		      if(pos>k && Sites[prevstate][j].score[pos]==Sites[prevstate][j].score[k]) {
			newexon[k]->prev=Sites[prevstate][j].prevex[pos];
			Sites[nextstate][i].prevpredno[pos]=pos;
		      }
		    }
		    else { 
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon[k]);
#endif   
		      free(newexon[k]);            
		      newexon[k]=NULL;  
		    }
		  
		}
	      }
	      /*}
		else {
		free(newexon[0]);
		newexon[0]=NULL;
		}*/
	    }
	  }
	  
	  j--;
	}
      }
    }

#ifdef MESSAGE
    for(s=INTERG; s<= I2MINUS; s++) 
      for(j=0;j<PREDNO;j++) {
	printf("Sites[%d][%d].score=%f poz=%d type=%d score=%f",s,i,Sites[s][i].score[j],SS[i].poz,SS[i].type,SS[i].score);
	if(Sites[s][i].prevex[j]!=NULL) {
	  printf(" prevexon=%d %d %d",Sites[s][i].prevex[j]->start,Sites[s][i].prevex[j]->stop,Sites[s][i].prevex[j]->type);
	}
	printf("\n");
      }
#endif


  }


  free(newexon);
  for(s=0;s<NCSTATES;s++) {
    free(changed[s]);
  }

}
  

void  Read_Coding_Model  (char * Param,int m)

//  Read in the probability model indicated by  Param .

{
  FILE  * fp;
  
  fp = File_Open (Param, "r");   // maybe rb ?
  
  Read_Scoring_Model (fp,m);
  
  fclose (fp);
  
  return;
}

void Read_AAC (char * Param, int m)
{
  FILE *fp;
  fp = File_Open (Param, "r");
  int i;
  float readval;

  for(i=0;i<23;i++) {
    fscanf(fp,"%f",&readval);
      Facc1[m][i]=(double) readval;
  }
  
  for(i=0;i<529;i++) {
    fscanf(fp,"%f",&readval);
    Facc2[m][i]=(double) readval;
  }

  for(i=0;i<12167;i++) {
    fscanf(fp,"%f",&readval);
    Facc[m][i]=(double) readval;
  }
}


tModel *  Read_NonCoding_Model  (char * Param)

//  Read in the probability models of the non-coding regions.

{
  tModel * Delta;
  FILE  * fp;
  
  fp = File_Open (Param, "r");   // maybe rb ?
  
  Delta=Read_NonScoring_Model (fp);
  
  fclose (fp);

  return(Delta);
}


void classify (POINT *point,struct tree_node **roots,int no_of_trees)
{
  int j, t;
  struct tree_node *cur_node;
  double sum;
  double probtmp;

  /* initialize Exon and Intron prob */
  point->prob[0]=0;
  point->prob[1]=0;
  /* get length and skip if <= 2 */
  for (t=1; t<=no_of_trees; t++) {
    //printf("Tree %d\n",t);
    cur_node = roots[t-1];
    while (cur_node != NULL) {
      sum = cur_node->coefficients[no_of_dimensions];
      for (j=1;j<=no_of_dimensions;j++)
	sum += cur_node->coefficients[j-1] * point->dimension[j-1];
      if (sum < 0) {
	if (cur_node->left != NULL) 
	  cur_node = cur_node->left;
	else {
	  /* New for prob. classification, added by Xin Chen */
	  probtmp =
	    (double)cur_node->left_count[(cur_node->left_cat)-1]/
	    cur_node->left_total;
	  
	  if(cur_node->left_cat == 1) {
	    point->prob[0] += probtmp;
	    point->prob[1] += 1-probtmp;
	  }
	  else {
	    point->prob[0] += 1-probtmp;
	    point->prob[1] += probtmp;
	  }
	  
	  /****/
	  /*printf("left cat= %d  left_count[%d]= %d no_points= %d\n",
	     cur_node->left_cat,
	     cur_node->left_cat,
	     cur_node->left_count[cur_node->left_cat],
	     cur_node->left_total);*/
	  
	  break;
	}
      }
      else {
	if (cur_node->right != NULL) 
	  cur_node = cur_node->right;
	else {
	  /* New for prob. classification, added by Xin Chen */
	  probtmp =
	    (double)cur_node->right_count[(cur_node->right_cat)-1]/
	    cur_node->right_total;
	  
	  if(cur_node->right_cat == 1) {
	    point->prob[0] += probtmp;
	    point->prob[1] += 1-probtmp;
	  }
	  else {
	    point->prob[0] += 1-probtmp;
	    point->prob[1] += probtmp;
	  }
	  
	  /****/
	  /*printf("right cat= %d  right_count[%d]= %d no_points= %d\n",
	       cur_node->right_cat,
	       cur_node->right_cat,
	       cur_node->right_count[cur_node->right_cat],
	       cur_node->right_total);*/
	    
	  break;
	}
      }
    }
  }

  point->prob[0] /= (double) no_of_trees;
  point->prob[1] /= (double) no_of_trees;
  if(point->prob[0] >= point->prob[1]){
    point->category = 1;
  }
  else{
    point->category = 2;
  }
}


void classifysgl (POINT *point,struct tree_node *root)
{
  int j, t;
  double sum;
  double probtmp;
  struct tree_node *cur_node;

  /* initialize Exon and Intron prob */
  point->prob[0]=0;
  point->prob[1]=0;
  /* get length and skip if <= 2 */
  cur_node=root;
  while (cur_node != NULL) {
    sum = cur_node->coefficients[no_of_dimensions];
    for (j=1;j<=no_of_dimensions;j++)
      sum += cur_node->coefficients[j-1] * point->dimension[j-1];
    if (sum < 0) {
      if (cur_node->left != NULL) 
	cur_node = cur_node->left;
      else {
	/* New for prob. classification, added by Xin Chen */
	probtmp =
	  (double)cur_node->left_count[(cur_node->left_cat)-1]/
	  cur_node->left_total;
	
	if(cur_node->left_cat == 1) {
	  point->prob[0] += probtmp;
	  point->prob[1] += 1-probtmp;
	}
	else {
	  point->prob[0] += 1-probtmp;
	  point->prob[1] += probtmp;
	}
	
	/****/
	/*printf("left cat= %d  left_count[%d]= %d no_points= %d\n",
	  cur_node->left_cat,
	  cur_node->left_cat,
	  cur_node->left_count[cur_node->left_cat],
	  cur_node->left_total);*/
	
	break;
      }
    }
    else {
      if (cur_node->right != NULL) 
	cur_node = cur_node->right;
      else {
	/* New for prob. classification, added by Xin Chen */
	probtmp =
	  (double)cur_node->right_count[(cur_node->right_cat)-1]/
	  cur_node->right_total;
	
	if(cur_node->right_cat == 1) {
	  point->prob[0] += probtmp;
	  point->prob[1] += 1-probtmp;
	}
	else {
	  point->prob[0] += 1-probtmp;
	  point->prob[1] += probtmp;
	}
	
	/****/
	/*printf("right cat= %d  right_count[%d]= %d no_points= %d\n",
	  cur_node->right_cat,
	  cur_node->right_cat,
	  cur_node->right_count[cur_node->right_cat],
	  cur_node->right_total);*/
	
	break;
      }
    }
  }
  

  if(point->prob[0] >= point->prob[1]){
    point->category = 1;
  }
  else{
    point->category = 2;
  }
}


int basetoint(char c, int dir)
{
  if(dir==1) {
    switch(c) {
    case 'a':
    case 'A': return(0);
    case 'c':
    case 'C': return(1);
    case 'g':
    case 'G': return(2);
    case 't':
    case 'T': return(3);
    }
  }
  else {
    switch(c) {
    case 'a':
    case 'A': return(3);
    case 'c':
    case 'C': return(2);
    case 'g':
    case 'G': return(1);
    case 't':
    case 'T': return(0);
    }
  }

  return(-1);
}

double score_signalp(long int start, long int stop)
{
  int i,j,k;
  double score=0;
  int c1,c2,c3;

  j=0;
  if(start<stop) {
    for(k=start+3;k<stop-1 && k<=start+60;k+=3) {
      if(k==start+18) j=1;
      c1=basetoint(Data[k],1);
      c2=basetoint(Data[k+1],1);
      c3=basetoint(Data[k+2],1);
      i=c1*4+c2+16*j;
      score+=SignalP[i][c3];
    }
  }
  else {
    for(k=start-3;k>stop+1 && k>=start-60; k-=3) {
      if(k==start-18) j=1;
      c1=basetoint(Data[k],-1);
      c2=basetoint(Data[k-1],-1);
      c3=basetoint(Data[k-2],-1);
      i=c1*4+c2+16*j;
      score+=SignalP[i][c3];
    }
  }

  score/=10;

  return(score);
}

int stopcodon(int frame,int phase,long int stop,long int start)
{

  if(frame) { // reverse strand
    
    switch(phase) {
    case 0: return(0);
    case 1: 
      if(!onlytga && !onlytaa && !onlytag &&  (
	 (Data[start]=='a' && Data[stop]=='t' && Data[stop-1]=='c')||
	 (Data[start]=='a' && Data[stop]=='t' && Data[stop-1]=='t')||
	 (Data[start]=='a' && Data[stop]=='c' && Data[stop-1]=='t'))) 
	return(1);
      if(onlytga && (Data[start]=='a' && Data[stop]=='c' && Data[stop-1]=='t')) return(1);
      if(onlytaa && (Data[start]=='a' && Data[stop]=='t' && Data[stop-1]=='t')) return(1);
      if(onlytag && (Data[start]=='a' && Data[stop]=='t' && Data[stop-1]=='c')) return(1);
      else return(0);
    case 2:
      if(!onlytga && !onlytaa && !onlytag && ((Data[start+1]=='a' && Data[start]=='t' && Data[stop]=='c')||
		      (Data[start+1]=='a' && Data[start]=='t' && Data[stop]=='t')||
		      (Data[start+1]=='a' && Data[start]=='c' && Data[stop]=='t')))
	return(1);
      if(onlytga && (Data[start+1]=='a' && Data[start]=='c' && Data[stop]=='t')) return(1);
      if(onlytaa && (Data[start+1]=='a' && Data[start]=='t' && Data[stop]=='t')) return(1);
      if(onlytag && (Data[start+1]=='a' && Data[start]=='t' && Data[stop]=='c')) return(1);
      else return(0);
    }
  }
  else { // forward strand

    switch(phase) {
    case 0: return(0);
    case 1: 
      if(!onlytga && !onlytaa && !onlytag && ((Data[stop-1]=='t' && Data[stop]=='a' && Data[start]=='g')||
		      (Data[stop-1]=='t' && Data[stop]=='a' && Data[start]=='a')||
		      (Data[stop-1]=='t' && Data[stop]=='g' && Data[start]=='a')))
	return(1);
      if(onlytga && (Data[stop-1]=='t' && Data[stop]=='g' && Data[start]=='a')) return(1);
      if(onlytaa && (Data[stop-1]=='t' && Data[stop]=='a' && Data[start]=='a')) return(1);
      if(onlytag && (Data[stop-1]=='t' && Data[stop]=='a' && Data[start]=='g')) return(1);
      else return(0);
    case 2: 
      if(!onlytga && !onlytaa && !onlytag && ((Data[stop]=='t' && Data[start]=='a' && Data[start+1]=='g')||
		      (Data[stop]=='t' && Data[start]=='a' && Data[start+1]=='a')||
		      (Data[stop]=='t' && Data[start]=='g' && Data[start+1]=='a')))
	return(1);
      if(onlytga && (Data[stop]=='t' && Data[start]=='g' && Data[start+1]=='a')) return(1);
      if(onlytaa && (Data[stop]=='t' && Data[start]=='a' && Data[start+1]=='a')) return(1);
      if(onlytag && (Data[stop]=='t' && Data[start]=='a' && Data[start+1]=='g')) return(1);
      else return(0);
    }
  }
}

int addexon(int number)
{
  int ret;

  ret = number+1 < ExNo ? number+1 : ExNo-1;

  return(ret);
}

exon *copyexon(exon *initexon)
{
  exon *newexon;
  
  newexon=(exon *) malloc(sizeof(exon));
  if (newexon == NULL) {
    fprintf(stderr,"Memory allocation for exon failure.\n"); 
    abort();
  }

#ifdef MESSAGE
  printf("Copy exon %x %d %d %d\n",newexon,initexon->start,initexon->stop,initexon->type);
#endif

  newexon->start=initexon->start;
  newexon->stop=initexon->stop;
  newexon->type=initexon->type;
  newexon->lphase=initexon->lphase;
  newexon->rphase=initexon->rphase;
  newexon->score=initexon->score;
  newexon->prev=initexon->prev;
  newexon->exon_no=initexon->exon_no;

  return(newexon);
}

int insertmaxscore(double score,exon *newexon,int state,long int pos,int *changed,int PREDNO,int prevstate,long int prevstateno,int prevpredno)
{
  int i,k;
  double keepscore;
  exon *keepexon;
  int keepchanged,thischange,keepstatetype,keeppredno;
  exon *tempexon;
  int ret;
  long int keepstateno;

#ifdef MESSAGE
  for(i=0;i<PREDNO;i++) 
    printf("Insert %f vs. %f for Sites[%d][%d]\n",score,Sites[state][pos].score[i],state,pos);

#endif

  i=0;
  while(i<PREDNO && score<Sites[state][pos].score[i]) i++;

  if(i==PREDNO || score==Sites[state][pos].score[i]) { 
#ifdef MESSAGE
		  printf("Free exon %x\n",newexon);
#endif
    free(newexon); newexon=NULL;return(-1);
  }

#ifdef MESSAGE
  printf("Score inserted in position %d\n",i);
#endif


  tempexon=newexon;
  thischange=1;

  ret=i;

  while(i<PREDNO) {

    keepscore=Sites[state][pos].score[i];
    keepexon=Sites[state][pos].prevex[i];
    keepchanged=changed[i];
    keepstatetype=Sites[state][pos].prevstatetype[i];
    keepstateno=Sites[state][pos].prevstateno[i];
    keeppredno=Sites[state][pos].prevpredno[i];

    Sites[state][pos].score[i]=score;
    Sites[state][pos].prevex[i]=tempexon;
    Sites[state][pos].prevstatetype[i]=prevstate;
    Sites[state][pos].prevstateno[i]=prevstateno;
    Sites[state][pos].prevpredno[i]=prevpredno;
    changed[i]=thischange;

    score=keepscore;
    tempexon=keepexon;
    thischange=keepchanged;
    prevstate=keepstatetype;
    prevstateno=keepstateno;
    prevpredno=keeppredno;

    i++;
  }

  // clean memory if necessary
  if(keepchanged) {
#ifdef MESSAGE
    printf("Free exon %x\n",keepexon);
#endif
    free(keepexon);keepexon=NULL;
  }
  
  return(ret);
}


double loadbin(struct Specif *F,int beg,int end,double score) {
  int i;

  if(beg==end) return(F[beg].sp);
  
  i=beg+(int)(end-beg)/2;

  if(score==F[i].score) return(F[i].sp);
  if(score<F[i].score) return(loadbin(F,beg,i-1,score));
  if(i==end || score<F[i+1].score ) return(F[i].sp);
  return(loadbin(F,i+1,end,score));
    
}


void initparam( double *mintron, double *minterg, double *m5utr, double *m3utr) {

  int i;

  // initparam
  // constant parameters over all windows: window_len, use_filter, istacc,istdon,isatg,isstop,nocod,md,load_falses,
  // Acc_Thr,Don_thr,Atg_Thr,Stop_Thr,i5utr,i3utr,iinterg,iintron,iin[i],Use_Exon_Count,Use_protein,
  // SIGNALP_FILE,EXONNO_DISTR,ExNo

  for(i=0;i<MODEL_NO;i++) {
    strcpy(CODING_FILE[i],"");
    strcpy(PROTEIN_FILE[i],"");
    strcpy(NONCODING_FILE[i],"");
    strcpy(EXON_DISTR[i],"");
    boostexon[i]=5;
    boostdistr[i]=0;
    snglfactor[i]=1;
    boostsplice[i]=0;
    MODEL_LEN[i]=12;
    partial[i]=0;
    sscore[i]=0;
    Use_Intron_Distrib[i]=1;
    intergval[i]=0;
    intergpen[i]=0;
    splitpenalty[i]=0;
    minterg[i]=0;
    mintron[i]=0;
    m5utr[i]=0;
    m3utr[i]=0;
    MODEL_READ[i]=FALSE;
    Use_Protein[i]=0;
    use_dts[i]=0;
  }

}

void loadmatrix(char *filename,double *Bg,double **M,int featno,int motiflen)
{
  int i,k;
  FILE *fp;
  float readval;
  
  fp = fopen(filename,"r");
  if  (fp == NULL) {
    fprintf (stderr, "ERROR:  Could not open file  %s \n", filename);
    exit (0);
  }

  for(i=0;i<4;i++) {
    fscanf(fp,"%f",&readval);
    Bg[i]=(double)readval;
  }
  for(i=0;i<featno-1;i++) {
    for(k=0;k<4*motiflen;k++) {
      fscanf(fp,"%f",&readval);
      M[i][k]=(double)readval;
    }
  }

  fclose(fp);
}

void loadweights(double *W,char * filename,int featno)
{
  int i,n;
  float readval;
  FILE *fp;

  fp = fopen(filename,"r");
  if  (fp == NULL) {
    fprintf (stderr, "ERROR:  Could not open file  %s \n", filename);
    exit (0);
  }

  n=0;
  while(!feof(fp) ) {
    fscanf(fp,"%d %f",&i,&readval);
    if(i>featno) {
      fprintf(stderr,"Too many features!\n");
      abort();
    }
    while(n<i) {
      W[n++]=0;
    }
    W[i]=(double)readval;n++;
  }
  fclose(fp);
}

int loadscaledata(double *MinS,double *MaxS,int featno,char *filename,int *begscale,int *endscale)
{
  FILE *fp;
  int i;
  float readval;

  fp = fopen(filename,"r");
  if  (fp == NULL) {
    fprintf (stderr, "ERROR:  Could not open file  %s \n", filename);
    exit (0);
  }

  fscanf(fp,"%d %d",begscale,endscale);
  
  for(i=0;i<featno;i++) {
    fscanf(fp,"%f",&readval);
    MinS[i]=(double)readval;
    fscanf(fp,"%f",&readval);
    MaxS[i]=(double)readval;
    if(MinS[i]==MaxS[i]) {
      fprintf(stderr,"min and max values are equal for feature %d; using non scaled data!\n",i+1);
      return(0);
    }
  }
  fclose(fp);
  return(1);
}

double scale(double val,int scaledata,int begscale, int endscale,double maxs,double mins)
{
  double retval;

  if(scaledata) 
    retval=((endscale-begscale)*val+maxs*begscale-mins*endscale)/(maxs-mins);
  else retval=val;

  return(retval);
}

int ok_gene_length(exon *newexon, int curstatetype,long int curstateno,int curpredno,int forw) 
{
  exon *curexon;
  long int stateno;
  int statetype;
  int predno;

  long int len=newexon->stop-newexon->start+1;
  if(len>min_gene_len) return(1);
  
  //fprintf(stderr,"init len=%ld for exon %ld %ld type=%d\n",len,newexon->start,newexon->stop,newexon->type);

  curexon=Sites[curstatetype][curstateno].prevex[curpredno];
  
  while(curexon != NULL && (curexon->type != 0 || forw<0) && (curexon->type != 6 || forw>0))  {
    if(SS[curstateno].type==0) return(1);
    len+=curexon->stop-curexon->start+1;
    if(len>min_gene_len) return(1);
    //fprintf(stderr,"len=%ld for exon %ld %ld type=%d\n",len,curexon->start,curexon->stop,curexon->type);
    curexon=curexon->prev;
    statetype=Sites[curstatetype][curstateno].prevstatetype[curpredno];
    stateno=Sites[curstatetype][curstateno].prevstateno[curpredno];
    predno=Sites[curstatetype][curstateno].prevpredno[curpredno];
    curstatetype=statetype;
    curstateno=stateno;
    curpredno=predno;
  }

  if(curexon==NULL) return(1);
  
  len+=curexon->stop-curexon->start+1;
  //fprintf(stderr,"len=%ld for exon %ld %ld type=%d\n",len,curexon->start,curexon->stop,curexon->type);

  if(len>min_gene_len) return(1);
  else return(0);
}
  
