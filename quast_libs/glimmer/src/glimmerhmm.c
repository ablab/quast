#include "delcher.h"
#include "graph.h"
#include "gene.h"

//#define MESS

const int OVLP_SIZE=100000;

struct List
{
  exon *predexon;
  List *link;
};


void freeList(List *L);
void  Process_Options  (int, char * []);
long int printgenes(Site **Sites, long int ssno,int start, int stop, int ignore,int offset,int *offgene, char *Name,long int len);
long int printexon(List *Predexon,int *geneno,int *exonno, int *totlen,long int offset, long int startprint,long int stopprint, int *oktoprint,long int lastseen,FILE *outf,char *Name, int path,int *parentprint);
void markexon(struct exon *e,int val);
void freeexon(struct exon *e,int val);
long int datalen(FILE *fp, char Name[]);
long int LoadData(FILE *fp,char *Data, long int chunk, long int goback);
long int getrange(List *Predexon,int strand);

char *TRAIN_DIR;
char *proteindom_file=NULL;
char *outfile=NULL;

int ese=1;
int gff=0;
int PREDNO=1;
int force=0;  // force no partial genes

int main  (int argc, char * argv [])
{
  FILE  * fp;
  char *Data1;
  long int Input_Size;
  long int Data1_Len;
  char  Name1[MAX_LINE];
  long int ssno;
  Site **Sites;
  long int len, proclen,offset,printlen,goback;
  int start,stop;
  int *offgene;
  int i;
  int predno;

  // ---------------------------- OPTIONS ------------------------------

  if  (argc < 2) {
    fprintf (stderr,
	     "USAGE:  %s <genome1-file> <training-dir-for-genome1> [options] \n",
	     argv [0]);
    fprintf(stderr,"Options:\n");
    fprintf(stderr,"-p file_name     If protein domain searches are available, read them from file file_name\n");
    fprintf(stderr,"-d dir_name      Training directory is specified by dir_name (introduced for compatibility with earlier versions)\n");
    fprintf(stderr,"-o file_name     Print output in file_name; if n>1 for top best predictions, output is in file_name.1, file_name.2, ... , file_name.n f\n");
    fprintf(stderr,"-n n             Print top n best predictions\n");
    fprintf(stderr,"-g               Print output in gff format\n");
    fprintf(stderr,"-v               Don't use svm splice site predictions\n");
    fprintf(stderr,"-f               Don't make partial gene predictions\n");
    fprintf(stderr,"-h               Display the options of the program\n");
    exit (-1);
  }

  Process_Options (argc, argv);   // Set global variables to reflect status of
                                  // command-line options.


  //  -> options should include the possibility to predict in only one genome (maximize score only for one, and not for the other <- kept constant)

  // --------------------------- READ DATA ------------------------------

  fp = File_Open (argv [1], "r");
  
  len=datalen(fp,Name1);


  if(len<INIT_SIZE) Input_Size=len+2;
  else Input_Size=INIT_SIZE+2;
  
  Data1 = (char *) malloc(Input_Size*sizeof(char));
  if(Data1 == NULL ) {
      fprintf (stderr, "ERROR:  Unable to alloc memory for input sequence\n");
      exit (0);
  }

  Data1[0]='n';


  start=1;
  goback=0;
  proclen=0;

  offgene = (int *) malloc(PREDNO*sizeof(int));
  if(offgene == NULL) { 
    fprintf(stderr,"ERROR: Unable to alloc memory for number of genes\n");
    exit(0);
  }
  for(i=0;i<PREDNO;i++) offgene[i]=1;


  while(proclen<len) {

    Data1_Len = LoadData(fp,Data1,INIT_SIZE,goback)-1;

    if(!start) offset=proclen-OVLP_SIZE/2;
    else offset=0;

    // run graph file
    /*if(PREDNO==1) { predno=2;} // this here might be useful when using intron distribution; but you need to remember it when freeing the memory for Sites
      else*/ 
    predno=PREDNO;
    Sites=graph(Data1,Data1_Len,TRAIN_DIR,&ssno,proteindom_file,offset,ese,Name1,predno,force);

    // print data
    stop = proclen+Data1_Len<len ? 0 : 1;

    printlen=printgenes(Sites,ssno,start,stop,OVLP_SIZE/2,offset,offgene,Name1,len);
    if(!stop) {
      proclen=printlen;
      goback=INIT_SIZE-proclen+offset+OVLP_SIZE/2;
    }
    else proclen=len;
    start=0;
    fprintf(stderr,"Done %d bp\n",proclen);
  }

  free(offgene);

}


long int LoadData(FILE *fp,char *Data, long int chunk, long int goback) 
{
  int ch;
  long int i;
  
  // find the start
  while(goback) {
    fseek(fp,-1,SEEK_CUR);
    ch=fgetc(fp);
    fseek(fp,-1,SEEK_CUR);
    if(isspace(ch)) continue;
    goback--;
  }

  // copy the data
  Data[0]='n';
  i=1;

  while(i<=chunk && (ch=fgetc(fp))!= EOF && ch !='>') {
    if(isspace(ch)) continue;
    ch = tolower (ch);
    switch  (ch) {
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
      fprintf (stderr, "Unexpected character `%c\' in input sequence\n",ch);
      ch = 'n';
    }
    Data[i++]=ch;
  }
  Data[i]='\0';

  return(i);
}

long int printgenes(Site **Sites, long int ssno,int start, int stop, int ignore,int offset,int *offgene, char *Name1,long int len)
{
  long int j;
  int i,k;
  int ret,totlen,parentprint;
  double *maxscore;
  int val, *maxval,*maxi;
  exon *curexon;
  List *PredExon, **CurExon;
  int geneno,exonno,oktoprint;
  long int startprint,stopprint,lastseen=0;
  FILE *outf;
  char *outfilename;
  long int curstateno,stateno;
  int curstatetype,statetype;
  int curpredno,predno;
  
  maxscore=(double *) malloc(PREDNO*sizeof(double));
  if(maxscore==NULL) {
    fprintf(stderr,"Memory allocation for maxscore failure!\n");
    abort();
  }

  maxval=(int *) malloc(PREDNO*sizeof(int));
  if(maxval==NULL) {
    fprintf(stderr,"Memory allocation for maxval failure!\n");
    abort();
  }

  maxi=(int *) malloc(PREDNO*sizeof(int));
  if(maxi==NULL) {
    fprintf(stderr,"Memory allocation for maxi failure!\n");
    abort();
  }

  CurExon=(List **)malloc(PREDNO*sizeof(List *));
  if(CurExon==NULL) {
    fprintf(stderr,"Memory allocation for CurExon failure!\n");
    abort();
  }

  for(i=0;i<PREDNO;i++) {
    maxscore[i]=Sites[INTERG][ssno-1].score[i];
    maxval[i]=INTERG;
    maxi[i]=i;
  }

  if(!force) {
    for(val=I0PLUS; val<=I2MINUS; val++) {
      i=0;
      ret=1;
      j=0;
      while(i<PREDNO && ret) {
	while(j<PREDNO && Sites[val][ssno-1].score[i]<=maxscore[j]) { j++;}
	if(j==PREDNO) ret=0;
	else {
	  for(k=PREDNO-1;k>=j+1;k--) {
	    maxscore[k]=maxscore[k-1];
	    maxval[k]=maxval[k-1];
	    maxi[k]=maxi[k-1];
	  }
	  maxscore[j]=Sites[val][ssno-1].score[i];
	  maxval[j]=val;
	  maxi[j]=i;
	  i++;
	  j++;
	}
      }
    }
  }

  /*  for(i=0;i<PREDNO;i++) {
      printf("state=%d maxscore=%f maxi=%d\n",maxval[i],maxscore[i],maxi[i]);}*/

  for(i=0;i<PREDNO;i++) {


    if(outfile == NULL) {
      outf=stdout;
    }
    else {
      if(PREDNO==1) {
	if(offset==0) { outf=fopen(outfile, "w");}
	else { outf=fopen(outfile, "a");}
	if (outf==NULL)
	  fprintf(stderr,"Cannot open output file %s for writing!\n",outfile);
      }
      else {
	outfilename=(char *)malloc((10+strlen(outfile))*sizeof(char));
	if (outfilename == NULL) {
	  fprintf(stderr,"Memory allocation for output file name %s.%d failure.\n",outfile,i);
	  abort();
	}
	sprintf(outfilename,"%s.%d",outfile,i+1);
	if(offset==0) { outf=fopen(outfilename, "w");}
	else { outf=fopen(outfilename, "a");}
	if (outf==NULL)
	  fprintf(stderr,"Cannot open output file %s for writing!\n",outfilename);
      }
    }

    if(offset==0) { // print header
      if(gff) {
	fprintf(outf,"##gff-version 3\n");
	fprintf(outf,"##sequence-region %s 1 %ld\n",Name1,len);
      }
      else {
	fprintf(outf,"GlimmerHMM\n");    
	fprintf(outf,"Sequence name: %s\n",Name1); 
	
	fprintf(outf,"Sequence length: %ld bp\n",len);
	
	fprintf(outf,"\nPredicted genes/exons\n\n");
	fprintf(outf,"Gene Exon Strand  Exon            Exon Range      Exon\n");
	fprintf(outf,"   #    #         Type                           Length\n\n");
	
      }

    }

    if(outf==stdout && PREDNO>1) { fprintf(outf,"\nPrediction %d:\n",i);}

    
    curexon=Sites[maxval[i]][ssno-1].prevex[maxi[i]];
    curstatetype=maxval[i];
    curstateno=ssno-1;
    curpredno=maxi[i];

    if(!stop) 
      while(curexon!=NULL && curexon->type!=2 && curexon->type!=3 && curexon->type!=4 && curexon->type!=7) {
	curexon=curexon->prev;
	statetype=Sites[curstatetype][curstateno].prevstatetype[curpredno];
	stateno=Sites[curstatetype][curstateno].prevstateno[curpredno];
	predno=Sites[curstatetype][curstateno].prevpredno[curpredno];
	curstatetype=statetype;
	curstateno=stateno;
	curpredno=predno;
      }

    PredExon=NULL;

    while(curexon!=NULL) {
      CurExon[i] = (List *) malloc(sizeof(List));
      if (CurExon[i] == NULL) {
	fprintf(stderr,"Memory allocation for current exon failure.\n"); 
	abort();
      }
      CurExon[i]->predexon=curexon;
      CurExon[i]->link=PredExon;
      PredExon=CurExon[i];

      statetype=Sites[curstatetype][curstateno].prevstatetype[curpredno];
      stateno=Sites[curstatetype][curstateno].prevstateno[curpredno];
      if(statetype==0) predno=i;
      else predno=Sites[curstatetype][curstateno].prevpredno[curpredno];

      curstatetype=statetype;
      curstateno=stateno;
      curpredno=predno;

      //curexon=curexon->prev;
      curexon=Sites[curstatetype][curstateno].prevex[curpredno];
    }

    geneno = offgene[i];
    exonno = 1;

    CurExon[i]=PredExon; // I need this later for freeList
    

    if(start) { startprint=0; oktoprint=1;}
    else {startprint=ignore; oktoprint=0;}
    if(stop) { stopprint=INIT_SIZE;}
    else { stopprint=INIT_SIZE-OVLP_SIZE/2;}

    totlen=0;
    parentprint=1;
    while(PredExon!=NULL) {
      lastseen=printexon(PredExon,&geneno,&exonno,&totlen,offset,startprint,stopprint,&oktoprint,lastseen,outf,Name1,i+1,&parentprint);
      PredExon=PredExon->link;
    }

    offgene[i]=geneno;
   
    if(outfile != NULL ) 
      fclose(outf);
 
  }

  if(!stop) { 
    if(lastseen<INIT_SIZE-OVLP_SIZE/2) lastseen=INIT_SIZE-OVLP_SIZE/2+offset;
    else lastseen+=offset;
  }
  else { lastseen=INIT_SIZE+offset;}

  // ---------------------------- FREE DATA ------------------------------
  
  // free sites 
  for(val=INTERG; val<= I2MINUS; val++) {
      for(j=0;j<ssno;j++) {
	for(i=0;i<PREDNO;i++) {
	  if(Sites[val][j].prevex[i]!=NULL) {
	    if(Sites[val][j].prevex[i]->type !=100) {
	      Sites[val][j].score[i]=-10000;
	      Sites[val][j].prevex[i]->type = 100;
	    }
	  }
	}
      }
      for(j=0;j<ssno;j++) {
	for(i=0;i<PREDNO;i++) {
	  if(Sites[val][j].prevex[i]!=NULL && Sites[val][j].score[i]==-10000) {
#ifdef MESS
	    printf("Free exon %x score=%f\n",Sites[val][j].prevex[i],Sites[val][j].score[i]);
#endif

	    free(Sites[val][j].prevex[i]);
	  }
	}
	if(Sites[val][j].score != NULL)  free(Sites[val][j].score);
	if(Sites[val][j].prevstatetype != NULL)  free(Sites[val][j].prevstatetype);
	if(Sites[val][j].prevpredno != NULL)  free(Sites[val][j].prevpredno);
	if(Sites[val][j].prevstateno != NULL)  free(Sites[val][j].prevstateno);
	if(Sites[val][j].prevex != NULL)  free(Sites[val][j].prevex);
      }
      free(Sites[val]);
  }


  /*  for(val=INTERG; val<= I2MINUS; val++) {
    if(Sites[val] != NULL ) {
      for(i=0;i<PREDNO;i++) 
	markexon(Sites[val][ssno-1].prevex[i],val*PREDNO+i);
    }
  }
  for(val=INTERG; val<= I2MINUS; val++) {
    if(Sites[val] !=NULL) {
      for(i=0;i<PREDNO;i++) 
	freeexon(Sites[val][ssno-1].prevex[i],val*PREDNO+i);
    }
    for(j=0;j<ssno;j++) {
      if(Sites[val][j].score != NULL)  free(Sites[val][j].score);
      if(Sites[val][j].prevstatetype != NULL)  free(Sites[val][j].prevstatetype);
      if(Sites[val][j].prevpredno != NULL)  free(Sites[val][j].prevpredno);
      if(Sites[val][j].prevstateno != NULL)  free(Sites[val][j].prevstateno);
      if(Sites[val][j].prevex != NULL)  free(Sites[val][j].prevex);
    }
    free(Sites[val]);
    }*/

  // free List
  for(i=0;i<PREDNO;i++) {
    freeList(CurExon[i]);
  }
  free(CurExon);
  free(maxi);
  free(maxval);
  free(maxscore);
  
  
  return(lastseen);

}

void  Process_Options  (int argc, char * argv [])
  
//  Process command-line options and set corresponding global switches
//  and parameters.
//
{
   char  * P;
   long int  W;
   double D;
   int  i,start;
   FILE  * fp;
   
   if(strcmp(argv[1],"-h")==0) {
     fprintf (stderr,"USAGE:  %s <genome1-file> <training-dir-for-genome1> [options] \n",argv [0]);
     fprintf(stderr,"Options:\n");
     fprintf(stderr,"-p file_name     If protein domain searches are available, read them from file file_name\n");
     fprintf(stderr,"-d dir_name      Training directory is specified by dir_name (introduced for compatibility with earlier versions)\n");
     fprintf(stderr,"-o file_name     Print output in file_name; if n>1 for top best predictions, output is in file_name.1, file_name.2, ... , file_name.n f\n");
     fprintf(stderr,"-n n             Print top n best predictions\n");
     fprintf(stderr,"-g               Print output in gff format\n");
     fprintf(stderr,"-v               Don't use svm splice site predictions\n");
     fprintf(stderr,"-f               Don't make partial gene predictions\n");
     fprintf(stderr,"-h               Display the options of the program\n");
     exit(0);
   }

   if(strcmp(argv[2],"-d")==0) { start=2;}
   else {
     if(argc>2) {
       TRAIN_DIR=(char *)malloc((2+strlen(argv[2]))*sizeof(char));
       if (TRAIN_DIR == NULL) {
	 fprintf(stderr,"Memory allocation for training directory name failure.\n");
	 abort();
       }
       start=3;
       strcpy(TRAIN_DIR,argv[2]);
       strcat(TRAIN_DIR,"/");
     }
     else {
       TRAIN_DIR=(char *)malloc(sizeof(char));
       if (TRAIN_DIR == NULL) {
	 fprintf(stderr,"Memory allocation for training directory name failure.\n");
	 abort();
       }
       strcpy(TRAIN_DIR,"");
     }
   }

   for  (i = start;  i < argc;  i ++)
     {
      switch  (argv [i] [0])
        {
         case  '-' :
           switch  (argv [i] [1])
             {
	     case 'd' :   //  the training files' directory
	       TRAIN_DIR=(char *)malloc((2+strlen(argv[i+1]))*sizeof(char));
	       if (TRAIN_DIR == NULL) {
		 fprintf(stderr,"Memory allocation for training directory name failure.\n");
		 abort();
	       }
	       strcpy(TRAIN_DIR,argv[++i]);strcat(TRAIN_DIR,"/");
	       //printf("train dir = %s\n",TRAIN_DIR);fflush(stdout);
	       break;
	       /*	     case  'm' : // give other PAM/BLOSUM matrix if desired (a 21x21 matrix)
	       fp = fopen(argv[++i],"r");
	       if(fp == NULL) {
	       fprintf(stderr,"ERROR:  Unable to open PAMfile\n");
	       exit(0);
	       }
	       pam=loadpam(fp);
	       usepam=1;
	       fclose(fp);
	       break;
	       case 'p' : // use PAM120 instead of BLOSUM 62
	       pam=pam120;
	       break;
	       case 'b': // use BLOSUM 62 instead of PAM120
	       pam = blosum62;
	       break;*/
	     case 'p' :
	       proteindom_file=(char *)malloc((1+strlen(argv[i+1]))*sizeof(char));
	       if (proteindom_file == NULL) {
		 fprintf(stderr,"Memory allocation for protein domains file name failure.\n");
		 abort();
	       }
	       strcpy(proteindom_file,argv[++i]);
	       //printf("protein dom file = %s\n",proteindom_file);fflush(stdout);exit(0);
	       break;
	     case 'f' :
	       force=1;
	       break;
	     case 'o':
	       outfile=(char *)malloc((1+strlen(argv[i+1]))*sizeof(char));
	       if (outfile == NULL) {
		 fprintf(stderr,"Memory allocation for output file name failure.\n");
		 abort();
	       }
	       strcpy(outfile,argv[++i]);
	       break;
	     case 'g': // print output in gff3 format
	       gff=1;
	       break;
	     case 'v': 
	       ese=0;
	       break;
	     case 'n': // number of predictions
	       PREDNO=atoi(argv[++i]);
	       assert(PREDNO>0);
	       break;
	     case 'h' :
	       printf("Options:\n");
	       printf("-p file_name     If protein domain searches are available, read them from file file_name\n");
	       printf("-d dir_name      Training directory is specified by dir_name (introduced for compatibility with earlier versions)\n");
	       printf("-o file_name     Print output in file_name; if n>1 for top best predictions, output is in file_name.1, file_name.2, ... , file_name.n f\n");
	       printf("-n n             Print top n best predictions\n");
	       printf("-g               Print output in gff format\n");
	       printf("-v               Don't use svm splice site predictions\n");
	       printf("-h               Display the options of the program\n");
	       exit(0);
	       
	     default :
	       fprintf (stderr, "Unrecognized option %s\n", argv [i]);
             }
           break;
	   /*	case  '+' :
		switch  (argv [i] [1])
		{
		case  'x' :
		// do something else
		break;
		default :
                fprintf (stderr, "Unrecognized option %s\n", argv [i]);
		}
		break;*/
	default :
	  fprintf (stderr, "Unrecognized option %s\n", argv [i]);
        }
     }

   return;
}

void freeList(List *L)
{
  if(L==NULL) return;
  freeList(L->link);
  free(L);
  L=NULL;
}


long int getrange(List *Predexon,int strand)
{
  List *tmpex;
  long int range;

  tmpex=Predexon;
  if(strand>0) {
    while(tmpex!=NULL && tmpex->predexon->type != 2 && tmpex->predexon->type !=3 ) { 
      range=tmpex->predexon->stop; 
      tmpex=tmpex->link;
    }
    if(tmpex != NULL) range=tmpex->predexon->stop;
  }
  else {
    while(tmpex!=NULL && tmpex->predexon->type != 4 && tmpex->predexon->type !=7 ) { 
      range=tmpex->predexon->stop; 
      tmpex=tmpex->link;
    }
    if(tmpex != NULL) range=tmpex->predexon->stop;
  }

  return(range);
}

long int printexon(List *Predexon,int *geneno,int *exonno, int *totlen,long int offset, long int startprint,long int stopprint,int *oktoprint,long int lastseen, FILE *outf, char *Name,int path,int *parentprint) 
{
  int frame;
  long int stoprange;
  exon *curexon=Predexon->predexon;

  switch(curexon->type) {
  case 0: 
    if(curexon->start<startprint || curexon->start>=stopprint ) { *oktoprint=0;}
    else { *oktoprint=1;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t+\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t+\t0\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=initial-exon\n",Name,curexon->start+offset,curexon->stop+offset,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  +  Initial  %10ld %10ld  %7d\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
    }
    *totlen+=curexon->stop-curexon->start+1;
    (*exonno)++;
    break;
  case 1: 
    if(curexon->start<startprint) { *oktoprint=0;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t+\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	frame=(3-(*totlen)%3)%3;
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t+\t%d\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=internal-exon\n",Name,curexon->start+offset,curexon->stop+offset,frame,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  +  Internal %10ld %10ld  %7d\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
    }
    *totlen+=curexon->stop-curexon->start+1;
    (*exonno)++;
    break;
  case 2: 
    if(curexon->start<startprint) { *oktoprint=0;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t+\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	frame=(3-(*totlen)%3)%3;
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t+\t%d\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=final-exon\n",Name,curexon->start+offset,curexon->stop+offset,frame,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  +  Terminal %10ld %10ld  %7d\n\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
      lastseen=curexon->stop;
      (*geneno)++;
    }
    *exonno=1;*totlen=0;*parentprint=1;
    break;
  case 3: 
    if(curexon->start<startprint || curexon->start>=stopprint ) { *oktoprint=0;}
    else { *oktoprint=1;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t+\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t+\t0\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=single-exon\n",Name,curexon->start+offset,curexon->stop+offset,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  +  Single   %10ld %10ld  %7d\n\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
      lastseen=curexon->stop;
      (*geneno)++;
    }
    *exonno=1;*totlen=0;*parentprint=1;
    break;
  case 4: 
    if(curexon->start<startprint) { *oktoprint=0;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,-1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t-\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t-\t0\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=initial-exon\n",Name,curexon->start+offset,curexon->stop+offset,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  -  Initial  %10ld %10ld  %7d\n\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
      lastseen=curexon->stop;
      (*geneno)++;
    }
    *exonno=1;*totlen=0;*parentprint=1;
    break;
  case 5: 
    *totlen+=curexon->stop-curexon->start+1;
    if(curexon->start<startprint) { *oktoprint=0;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,-1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t-\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	frame=(*totlen)%3;
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t-\t%d\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=internal-exon\n",Name,curexon->start+offset,curexon->stop+offset,frame,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  -  Internal %10ld %10ld  %7d\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
    }
    (*exonno)++;
    break;
  case 6: 
    *totlen+=curexon->stop-curexon->start+1;
    if(curexon->start<startprint || curexon->start>=stopprint ) { *oktoprint=0;}
    else { *oktoprint=1;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,-1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t-\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	frame=(*totlen)%3;
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t-\t%d\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=final-exon\n",Name,curexon->start+offset,curexon->stop+offset,frame,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  -  Terminal %10ld %10ld  %7d\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
    }
    (*exonno)++;
    break;
  case 7: 
    if(curexon->start<startprint || curexon->start>=stopprint ) { *oktoprint=0;}
    else { *oktoprint=1;}
    if(*oktoprint) {
      if(gff) {
	if(*parentprint) {
	  stoprange=getrange(Predexon,-1);
	  fprintf(outf,"%s\tGlimmerHMM\tmRNA\t%ld\t%ld\t.\t-\t.\tID=%s.path%d.gene%d;Name=%s.path%d.gene%d\n",Name,curexon->start+offset,stoprange+offset,Name,path,*geneno,Name,path,*geneno);
	  *parentprint=0;
	}
	fprintf(outf,"%s\tGlimmerHMM\tCDS\t%ld\t%ld\t.\t-\t0\tID=%s.cds%d.%d;Parent=%s.path%d.gene%d;Name=%s.path%d.gene%d;Note=single-exon\n",Name,curexon->start+offset,curexon->stop+offset,Name,*geneno,*exonno,Name,path,*geneno,Name,path,*geneno);
      }
      else {
	fprintf(outf,"%4d %4d  -  Single   %10ld %10ld  %7d\n\n",*geneno,*exonno,curexon->start+offset,curexon->stop+offset,curexon->stop-curexon->start+1);
      }
      lastseen=curexon->stop;
      (*geneno)++;
    }
    *exonno=1;*totlen=0;*parentprint=1;
    break;
  }

  return(lastseen);
}
   

void markexon(struct exon *e,int val)
{
  if(e==NULL) return;
  e->type=val;
  markexon(e->prev,val);
}

void freeexon(struct exon *e,int val)
{
  if(e == NULL) return;
  if(e->type>val) return;
  freeexon(e->prev,val);
  if(e->type==val) {
#ifdef MESS
    printf("Free exon %x\n",e);
#endif
    free(e);
    e=NULL;
  }
}


long int datalen(FILE *fp, char Name[])
{
  char line[MAX_LINE];
  char *P;
  long int len;
  int ch;
  fpos_t pos;

  while((ch=fgetc(fp)) != EOF && ch != '>') ;
  if(ch==EOF) return(0);

  fgets(line, MAX_LINE,fp);
  len=strlen(line);
  assert(len>0 && line[len-1]=='\n');

  fgetpos(fp,&pos);

  P= strtok (line, " \t\n");
  if(P != NULL) strcpy (Name, P);
  else Name [0] = '\0';

  len=0;
  while((ch=fgetc(fp)) != EOF && ch !='>') {
    if(isspace(ch)) continue;
    len++;
  }

  fsetpos(fp,&pos);

  return(len);

}
    
