/* ================================================================= *
 *  file.h : Header file with supporting class definitions           *
 *                                                                   *
 *  E-MEM: An efficient (MUMmer-like) tool to retrieve Maximum Exact *
 *         Matches using hashing based algorithm                     *
 *                                                                   *
 *  Copyright (c) 2014, Nilesh Khiste                                *
 *  All rights reserved                                              *
 *                                                                   * 
 *  This program is free software: you can redistribute it and/or    *
 *  modify it under the terms of the GNU General Public License as   *
 *  published by the Free Software Foundation, either version 3 of   *
 *  the License, or (at your option) any later version.              *
 *                                                                   *
 *  This program is distributed in the hope that it will be useful,  *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 *  GNU General Public License for more details.                     *
 *                                                                   *
 *  You should have received a copy of the GNU General Public        *
 *  License along with this program.                                 *
 *                                                                   *
 *  This file is subject to the terms and conditions defined in the  *
 *  file 'LICENSE', which is part of this source code package.       *
 * ================================================================= */
using namespace std;

#define CHARS2BITS(x) 		(2*(x)) //convert char position to bit position
#define DATATYPE_WIDTH          64 	// number of bits
#define RANDOM_SEQ_SIZE         10
#define NUM_TMP_FILES           24

class commonData {
  public:
    static int32_t minMemLen;
    static int32_t d;
    static int32_t numThreads;
    static int32_t kmerSize;
    static int32_t ignoreN;
    static int32_t fourColOutput;
    static int32_t lenInHeader;
    static int32_t relQueryPos;
    static char nucmer_path[256];
    static std::string pfx_path;
};
    
int32_t commonData::minMemLen=100; // 2 bit representation=50
int32_t commonData::d=1;
int32_t commonData::numThreads=1;
int32_t commonData::kmerSize=56; //2 bit representation = 28 
int32_t commonData::ignoreN=0;
int32_t commonData::fourColOutput=0;
int32_t commonData::lenInHeader=0;
int32_t commonData::relQueryPos=0;
char commonData::nucmer_path[256]={'\0'};
std::string commonData::pfx_path;


class seqData {
  public:
    uint64_t start;
    uint64_t end;
    std::string seq;
    seqData() 
    {
        start=0;
        end=0;
    };

    bool operator ()(const seqData &obj1, const seqData &obj2)
    {
      return (obj2.start>obj1.end?true:false);
    }
};

class mapObject {
    public:
      uint64_t left;
      uint64_t right;
      mapObject() {
          left=0;
          right=0;
      }

      mapObject(uint64_t x, uint64_t y) {
          left=x;
          right=y;
      }

      bool operator()(const uint64_t &x, const mapObject &y) 
      {
          return x < y.left;
      }
};

class seqFileReadInfo {
      fstream file;
      uint64_t size;
      string strTmp, strName;
      uint64_t binReadSize;
      uint64_t binReadsLocation;
      uint64_t currPos;
      uint64_t numSequences;

      string& randomStr()
      {
         static string str("NNNNNNNNNN");
         return str;
      } 
   
      void processTmpString(uint64_t &sz, uint64_t &blockNCount)
      {
          string line = strTmp;
          strTmp.clear();
          totalBases=0;
          binReadsLocation=0;
          processInput(line, sz, blockNCount);
      }

      /*
       * Function converts a character sequence into an array of integers.
       * Input: character string
       * Output: array of integers, total number of bases
       */
      void processInput(string &str, uint64_t &sz, uint64_t &blockNCount)
      {
          int chooseLetter=0;
          uint64_t k=0;

          if (!totalBases) {
              for (k=0; k<binReadSize; ++k)
                 binReads[k]=0;
          }

          /* Processing the sequences by encoding the base pairs into 2 bits. */
          for ( std::string::iterator it=str.begin(); it!=str.end(); ++it)
          {
              if (totalBases == sz){ //sz=size+minSize
                  strTmp += *it;
                  continue;
              }else if (totalBases >= size) {
                  strTmp += *it;
              }
              switch(*it)
              {
                  case 'A':
                  case 'a':
                      binReads[binReadsLocation] <<= 2;
                      if (commonData::ignoreN && blockNCount){
                         blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                         blockNCount=0;
                      }
                      break;
                  case 'C':
                  case 'c':
                      binReads[binReadsLocation] <<= 2;
                      binReads[binReadsLocation] |= 1;
                      if (commonData::ignoreN && blockNCount){
                         blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                         blockNCount=0;
                      }
                      break;
                  case 'G':
                  case 'g':
                      binReads[binReadsLocation] <<= 2;
                      binReads[binReadsLocation] |= 2;
                      if (commonData::ignoreN && blockNCount){
                         blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                         blockNCount=0;
                      }
                      break;
                  case 'T':
                  case 't':
                      binReads[binReadsLocation] <<= 2;
                      binReads[binReadsLocation] |= 3;
                      if (commonData::ignoreN && blockNCount){
                         blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                         blockNCount=0;
                      }
                      break;
                  default:
                      if(!blockNCount)
                          blockNCount=totalBases+1;
                      chooseLetter = rand() % 4;
                      if (chooseLetter == 0)
                          binReads[binReadsLocation] <<= 2;
                      else if (chooseLetter == 1)
                      {
                          binReads[binReadsLocation] <<= 2;
                          binReads[binReadsLocation] |= 1;
                      }
                      else if (chooseLetter == 2)
                      {
                          binReads[binReadsLocation] <<= 2;
                          binReads[binReadsLocation] |= 2;
                      }
                      else
                      {
                         binReads[binReadsLocation] <<= 2;
                         binReads[binReadsLocation] |= 3;
                      }
              }
              totalBases++;
              if ((totalBases%32)==0){
                  binReadsLocation++;
              }
          }
      }

    public:
      uint64_t *binReads;
      uint64_t totalBases;
      std::vector <mapObject> blockOfNs;
      
      seqFileReadInfo() {
          size=0;
          currPos=0;
          binReadSize=0;
          binReadsLocation=0;
          numSequences=0;
          totalBases=0;
      }
      
      seqFileReadInfo(string str)
      {
          size=0;
          currPos=0;
          binReadSize=0;
          binReadsLocation=0;
          numSequences=0;
          totalBases=0;
          file.open(str, ios::in);
          if(!file.is_open()) {
              cout << "ERROR: unable to open "<< str << " file" << endl;
              exit( EXIT_FAILURE );
          }
      }

      uint64_t &getNumSequences() {
          return numSequences;
      }

      void openFile(string s){
          file.open(s, ios::in);
          if(!file.is_open()) {
              cout << "ERROR: unable to open "<< s << " file" << endl;
              exit( EXIT_FAILURE );
          }
      }

      void setReverseFile() {
          char buffer[256];
          memset(buffer,0,256);
          sprintf(buffer, "%s/revComp", commonData::nucmer_path);
          file.close();
          openFile(buffer);
      }
      void closeFile() {
          file.close();
      }
    
      void destroy() {
          currPos=0;
          binReadSize=0;
          binReadsLocation=0;
          totalBases=0;
          strName.clear();
          strTmp.clear();
          clearMapForNs();
          delete [] binReads;
      }

      void clearFileFlag()
      {
          file.clear();
          file.seekg(0, ios::beg);
      } 
    
      uint64_t allocBinArray()
      {
          size = size/commonData::d;
          binReadSize = floor((size+numSequences*RANDOM_SEQ_SIZE+commonData::d)/32+4);
          binReads = new uint64_t[binReadSize];
          return size;
      }
  
      void clearMapForNs()
      {
          blockOfNs.clear();
      } 

      void clearTmpString()
      {
          strTmp.clear();
          strName.clear();
          clearMapForNs();
      }
      
      void getKmerLeftnRightBoundForNs(uint64_t &currKmerPos, mapObject &bounds)
      {
          uint64_t right=0;
          /*
           * Since we do all computation with bits, all our
           * positions are even. Here I return 1 (odd position),
           * an indication of no Ns towards left   
           */

          if (!blockOfNs.size()){
              bounds.left=0x1;
              bounds.right=CHARS2BITS(totalBases-1);
              return;
          }

          vector<mapObject>::iterator it;
          it=upper_bound(blockOfNs.begin(), blockOfNs.end(), currKmerPos, mapObject()); 
          /* No N block beyond this point */
          if (it == blockOfNs.end())
              right = CHARS2BITS(totalBases-1);
          else
              right = (*it).left-2;

          /* This function never gets a position which is N */
          if (!currKmerPos || it==blockOfNs.begin()){
              bounds.left=0x1;
              bounds.right=right;
              return;
          }

          --it;

          bounds.left=(*it).right+2;
          bounds.right=right;
          return;
      }

      bool checkKmerForNs(uint64_t &currKmerPos, vector<mapObject>::iterator &it)
      {
          if (!blockOfNs.size())
              return false;

          while(it != blockOfNs.end())
          {
              if ((*it).left>currKmerPos)
                  break;
              else
                  ++it;
          }    
    

          /* No N block beyond this point */
          if (it == blockOfNs.end()){
              --it;
              /* Current position within N block */
              if (((*it).left <=currKmerPos) && (currKmerPos <= (*it).right)){
                  ++it;
                  return true;
              }else{
                  ++it;
                  return false;
              }
          }
 
          if ((*it).left > (currKmerPos+commonData::kmerSize-2)){
              if (it != blockOfNs.begin()){
                  --it;
                  if ((*it).right < currKmerPos){
                      ++it;
                      return false;
                  }else {
                      ++it;
                      return true;
                  }
              }else
                  return false;
          }else {
              return true;
          }
      }


      void setCurrPos() {
          currPos+=size;;
      } 

      uint64_t getCurrPos() {
          return currPos;
      } 

      void resetCurrPos() {
          currPos=0;
      } 

      bool readChunks()
      {
          string line;
          uint64_t blockNCount=0;
          int minSize = commonData::minMemLen/2-1;
          uint64_t sz=size+minSize;
          /* Process anything remaining from the last iteration */
          processTmpString(sz, blockNCount);
          
          while(getline( file, line ).good() ){
              if(line[0] == '>' || (totalBases == sz)){
                  if( !strName.empty()){ // Process what we read from the last entry
                     if(line[0] != '>') {
                          processInput(line, sz, blockNCount);
                      }else {
                          processInput(randomStr(), sz, blockNCount);
                      }
                      if (totalBases == sz) {
                          if ((totalBases%32)!=0)
                          {
                              uint64_t offset = CHARS2BITS(totalBases)%DATATYPE_WIDTH;
                              binReads[binReadsLocation] <<= (DATATYPE_WIDTH-offset);
                              binReadsLocation++;
                              binReads[binReadsLocation]=0;
                          }
                          if (commonData::ignoreN && blockNCount){
                              blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                              blockNCount=0;
                          }
                          return true;
                      }
                  }
                  if( !line.empty() ){
                      strName = line.substr(1);
                  }
              } else if( !strName.empty() ){
                  processInput(line, sz, blockNCount);
              }
          }

          if( !strName.empty() ){ // Process what we read from the last entry
              if ((totalBases%32)!=0)
              {
                  uint64_t offset = CHARS2BITS(totalBases)%DATATYPE_WIDTH;
                  binReads[binReadsLocation] <<= (DATATYPE_WIDTH-offset);
                  binReadsLocation++;
                  binReads[binReadsLocation]=0;
              }
              if (commonData::ignoreN && blockNCount){
                  blockOfNs.push_back(mapObject(CHARS2BITS(blockNCount-1), CHARS2BITS(totalBases-1)));
                  blockNCount=0;
              }
              if (!strTmp.size())
                  strName.clear();
              return true;
          }
          return false;
      }

      void flipCharacter(char &in, char &out)
      {
          switch(in)
          {
              case 'A':
              case 'a':
                  out='T';
                  break;
              case 'C':
              case 'c':
                  out='G';
                  break;
              case 'G':
              case 'g':
                  out='C';
                  break;
              case 'T':
              case 't':
                  out='A';
                  break;
              default:
                  out=in;
          }
      }

      void flipNswap(string &content)
      {
          string::iterator itBeg = content.begin();
          string::iterator itEnd = --content.end();
          char beg=0, end=0;
          uint64_t d=0;
          while ((d=distance(itBeg,itEnd))) 
          {
              flipCharacter(*itBeg, end);
              flipCharacter(*itEnd, beg);
              (*itEnd)=end;
              (*itBeg)=beg;
              ++itBeg;
              --itEnd;
              if(d==1)
                  break;
          }
          if (!d)
              flipCharacter(*itEnd, *itEnd);
      }

      void writeReverseComplementString(string &name, string &content, fstream &file)
      {
          file << ">" << name << endl;
          flipNswap(content);
          file << content ;
      }

      void generateSeqPos(vector<seqData> &vecSeqInfo) {
          seqData s;
          uint64_t i=0,j=0;
          string line;
          clearFileFlag();
          while(getline(file, line).good() ){
              if(line[0] == '>'){
                  if(!strName.empty()) {
                      s.start=CHARS2BITS(j);
                      s.end=CHARS2BITS(i-1);
                      s.seq.assign(strtok(const_cast<char *>(strName.c_str())," \t\n"));
                      vecSeqInfo.push_back(s);
                      s.seq.clear();
                      i+=RANDOM_SEQ_SIZE;
                      j=i;
                      strName.clear();
                  }
                  if(!line.empty())
                      strName=line.substr(1);
              } else if( !strName.empty() ) {
                  i+=line.length();
              }
          }
          if( !strName.empty() ) {
              i+=line.length();
              s.start=CHARS2BITS(j);
              s.end=CHARS2BITS(i-1);
              s.seq.assign(strtok(const_cast<char *>(strName.c_str())," \t\n"));
              vecSeqInfo.push_back(s);
              s.seq.clear();
              strName.clear();
          }
      }



      void generateRevComplement(uint32_t revComplement) {
          string line,content;
          fstream revFile;

          if (revComplement) {
              char buffer[256];
              memset(buffer,0,256);
              sprintf(buffer, "%s/revComp", commonData::nucmer_path);
              revFile.open(buffer, ios::out);
              if (!revFile.is_open())
              {
                  cout << "ERROR: unable to open temporary reverse complement file" << endl;
                  exit( EXIT_FAILURE );
              }
          }

          clearFileFlag();
          while(getline(file, line).good() ){
              size += (line.length()+1); 
              if(line[0] == '>'){
                  if(!strName.empty()) {
                      numSequences++;
                      size += RANDOM_SEQ_SIZE; 
                      if (revComplement){
                          writeReverseComplementString(strName, content, revFile);
                          content.clear();
                      }
                      strName.clear();
                  }
                  if(!line.empty())
                      strName=line.substr(1);
              } else if( !strName.empty() ) {
                  if (revComplement) {
                      content += "\n";
                      content += line;
                  }
              }
          }
          if( !strName.empty() ) {
              size += (line.length()+1); 
              if (revComplement) {
                  content += "\n";
                  content += line;
              }
              numSequences++;
              if (revComplement){
                  writeReverseComplementString(strName, content, revFile);
                  content.clear();
              }
              strName.clear();
          }
          if (revComplement)
              revFile.close();
      }
};

class MemExt {
  public:
    uint64_t lR;
    uint64_t rR;
    uint64_t lQ;
    uint64_t rQ;
    MemExt() {
    }

    MemExt(uint64_t lr, uint64_t rr, uint64_t lq, uint64_t rq)
    {
         lR=lr;
         rR=rr;
         lQ=lq;
         rQ=rq;
    }

    bool operator () (const MemExt &obj1, const MemExt &obj2)
    {
      if (obj1.lQ<obj2.lQ)
         return true;
      else if (obj1.lQ>obj2.lQ)
         return false;
      else{
         if (obj1.lR<obj2.lR)
             return true;
         else
             return false;
      } 
    }
};


class tmpFilesInfo {
    fstream *TmpFiles;   
    vector <MemExt> MemExtVec;
    uint64_t numMemsInFile; 

    bool checkMEMExt(uint64_t &lr, uint64_t &rr, uint64_t &lq, uint64_t &rq, seqFileReadInfo &QueryFile, seqFileReadInfo &RefFile) {
      if ((!lq && QueryFile.getCurrPos()) || rq == CHARS2BITS(QueryFile.totalBases-1)) {
         return true;
      }else if((!lr && RefFile.getCurrPos()) || rr == CHARS2BITS(RefFile.totalBases-1)) {
         return true;
      } 
      return false;
    }

    void writeToFile(uint64_t lQ, uint64_t rQ, uint64_t lR, uint64_t rR, uint32_t &revComplement) {
        MemExt m;
        m.lQ=lQ;
        m.lR=lR;
        m.rQ=rQ;
        m.rR=rR;
        if (IS_MATCH_BOTH_DEF(revComplement))
            TmpFiles[m.lQ/numMemsInFile+NUM_TMP_FILES].write((char *)&m, sizeof(MemExt));
        else
            TmpFiles[m.lQ/numMemsInFile].write((char *)&m, sizeof(MemExt));
    }
    
    void writeToVector(uint64_t lQ, uint64_t rQ, uint64_t lR, uint64_t rR) {
        MemExt m;
        m.lQ=lQ;
        m.lR=lR;
        m.rQ=rQ;
        m.rR=rR;
        MemExtVec.push_back(m);
    }


  public:
 
    tmpFilesInfo(int numFiles) {
        TmpFiles = new fstream[numFiles];
    }

    ~tmpFilesInfo() {
        delete [] TmpFiles;
    }
   
    void setNumMemsInFile(uint64_t size, uint64_t &numSequences) {
        numMemsInFile = ((2*(size*commonData::d+numSequences*RANDOM_SEQ_SIZE+commonData::d))/NUM_TMP_FILES);
    }
   
    static bool compare_reference (const MemExt &obj1, const MemExt &obj2)
    {
      return (obj1.lR>=obj2.lR?false:true);
    }

    static bool myUnique(const MemExt &obj1, const MemExt &obj2)
    {
      if((obj1.lQ==obj2.lQ) && (obj1.rQ==obj2.rQ) && (obj1.rR==obj2.rR) && (obj1.lR==obj2.lR))
          return true;
      else
          return false;
    }

    void openFiles(ios_base::openmode mode, int numFiles) {
        char buffer[256];
        memset(buffer,0,256);
        static int flag=0;
        sprintf(buffer, "%s", commonData::nucmer_path);
        if (!flag) {
            if(mkdir(buffer, S_IRWXU|S_IRGRP|S_IXGRP))
            {
                cout << "ERROR: unable to open temporary directory" << endl;
                exit( EXIT_FAILURE );
            }
            flag=1;
        }
        /* Last two files hold the sequence/pos mapping
         * for reference and query file respectively
         */   
        for (int32_t i=0;i<numFiles;i++) {
            /* Temporary file to hold the mems */
            sprintf(buffer, "%s/%d", commonData::nucmer_path, i);
            TmpFiles[i].open(buffer, mode);
            if (!TmpFiles[i].is_open())
            {
                cout << "ERROR: unable to open temporary file" << endl;
                exit( EXIT_FAILURE );
            }
        }
    }
    
    void closeFiles(int numFiles) {
        for (int32_t i=0;i<numFiles;i++){
           TmpFiles[i].close();
        }
    }
    
    fstream& getMapFile(int fIndex) {
        return TmpFiles[fIndex];
    }
    
    bool writeMemInTmpFiles(uint64_t &lRef, uint64_t &rRef, uint64_t &lQue, uint64_t &rQue, seqFileReadInfo &QueryFile, seqFileReadInfo &RefFile, uint32_t &revComplement) {
       MemExt m;
       uint64_t currPosQ = CHARS2BITS(QueryFile.getCurrPos());
       uint64_t currPosR = CHARS2BITS(RefFile.getCurrPos());
       if (rRef-lRef+2 >= static_cast<uint64_t>(commonData::minMemLen)) {
           if (!(commonData::d==1 && commonData::numThreads==1) && checkMEMExt(lRef, rRef, lQue, rQue, QueryFile, RefFile)) {
               #pragma omp critical(writeVector)
               writeToVector(currPosQ+lQue, currPosQ+rQue, currPosR+lRef,  currPosR+rRef);
           }else {
               #pragma omp critical(writeFile)
               writeToFile(currPosQ+lQue, currPosQ+rQue, currPosR+lRef,  currPosR+rRef, revComplement);
           }
           return true;
       }else
           return false;
    }

    void printQueryHeader(vector<seqData>::iterator &itQ, uint32_t &revComplement)
    {
        if (revComplement & 0x1){
            if (commonData::lenInHeader) {
                cout << "> " << (*itQ).seq << " Reverse" << " Len = " << ((*itQ).end-(*itQ).start+2)/2 << endl;
            }else{
                cout << "> " << (*itQ).seq << " Reverse" << endl;
            }
        }else{
            if (commonData::lenInHeader){
                cout << "> " << (*itQ).seq << " Len = " << ((*itQ).end-(*itQ).start+2)/2 << endl;
            }else{
                cout << "> " << (*itQ).seq << endl;
            }
        }
    }

    void printMemOnTerminal(vector<seqData> &refSeqInfo, vector<seqData> &querySeqInfo, MemExt &m, uint32_t &revComplement) {
        uint64_t &lRef = m.lR;
        uint64_t &rRef = m.rR;
        uint64_t &lQue = m.lQ;
        uint64_t &rQue = m.rQ;
        static int flag=0;
        vector<seqData>::iterator itR;
        static vector<seqData>::iterator itQ=querySeqInfo.begin();
        seqData s;
        
        /* print remianing query sequences - if any */
        if (!lRef && !rRef && !lQue && !rQue) {
            /* No matches found - simply return */
            if (!flag){
                printQueryHeader(itQ, revComplement);
            }
            while(itQ != --querySeqInfo.end()){
                ++itQ;
                printQueryHeader(itQ, revComplement);
            }
            itQ=querySeqInfo.begin();
            flag=0;
            return;
        }


        s.start=lRef;
        s.end=rRef;
   
        if (rRef-lRef+2 < static_cast<uint64_t>(commonData::minMemLen))
            return;
           

        /* Process relative position for Reference sequence */
        itR = lower_bound(refSeqInfo.begin(), refSeqInfo.end(), s, seqData());

        if ((*itR).start <= lRef && (*itR).end >= rRef){
            // MEM within acutal sequence
            // s------e--s------e--s------e
            // s--|--|e
            lRef?lRef-=((*itR).start):lRef;
            rRef-=((*itR).start);
        }else if ((*itR).start > lRef && (*itR).end >= rRef) {
            if ((*itR).start > rRef) //mem within random character
                 return;
            // s------e--s------e--s------e
            // s------e-|s--|---e
            lQue+=((*itR).start-lRef);
            lRef=0;
            rRef-=((*itR).start);
        }else if ((*itR).start > lRef && (*itR).end < rRef) {
            // s------e--s------e--s------e
            // s------e-|s------e-|s------e
            lQue+=((*itR).start-lRef);
            lRef=0;
            rQue-=(rRef-(*itR).end);
            rRef=((*itR).end-(*itR).start);
        }else if ((*itR).start <= lRef && (*itR).end < rRef) {
            // s------e--s------e--s------e
            // s------e--s-----|e-|s------e
            rQue-=(rRef-(*itR).end);
            lRef?lRef-=((*itR).start):lRef;
            rRef=((*itR).end-(*itR).start);
        }else //mem within random character
            return;

        if (rRef-lRef+2 < static_cast<uint64_t>(commonData::minMemLen))
            return;
       
        /* Print first Query sequence */ 
        if (!flag){
            printQueryHeader(itQ, revComplement);
            flag=1;
        }
        /* Process relative position for Query sequence */
        while(lQue >= (*itQ).end){
            ++itQ;
            printQueryHeader(itQ, revComplement);
        }
        if ((*itQ).start <= lQue && (*itQ).end >= rQue){
            // MEM within acutal sequence
            // s------e--s------e--s------e
            // s--|--|e
            lQue?lQue-=((*itQ).start):lQue;
            rQue-=((*itQ).start);
        }else if ((*itQ).start > lQue && (*itQ).end >= rQue) {
            if ((*itQ).start > rQue) //mem within random character
                 return;
            // s------e--s------e--s------e
            // s------e-|s--|---e
            lRef+=((*itQ).start-lQue);
            lQue=0;
            rQue-=((*itQ).start);
        }else if ((*itQ).start > lQue && (*itQ).end < rQue) {
            // s------e--s------e--s------e
            // s------e-|s------e-|s------e
            lRef+=((*itQ).start-lQue);
            lQue=0;
            rRef-=(rQue-(*itQ).end);
            rQue=((*itQ).end-(*itQ).start);
        }else if ((*itQ).start <= lQue && (*itQ).end < rQue) {
            // s------e--s------e--s------e
            // s------e--s-----|e-|s------e
            rRef-=(rQue-(*itQ).end);
            lQue?lQue-=((*itQ).start):lQue;
            rQue=((*itQ).end-(*itQ).start);
        }else //mem within random character
            return;
 
        
        if (rRef-lRef+2 >= static_cast<uint64_t>(commonData::minMemLen)){
           if (refSeqInfo.size() == 1 && !commonData::fourColOutput) {
               if ((revComplement & 0x1) && commonData::relQueryPos)
                   cout << " " << setw(15) << ((lRef+2)/2) <<  setw(15) << ((*itQ).end-(*itQ).start-lQue+2)/2 << setw(15) << ((rRef-lRef+2)/2) << endl;
               else
                   cout << " " << setw(15) << ((lRef+2)/2) <<  setw(15) << ((lQue+2)/2) << setw(15) << ((rRef-lRef+2)/2) << endl;
           }else{
               if ((revComplement & 0x1) && commonData::relQueryPos) {
                   cout << " " << setw(30) << std::left <<(*itR).seq << setw(15) << ((lRef+2)/2) <<  setw(15) << ((*itQ).end-(*itQ).start-lQue+2)/2 << setw(15) << ((rRef-lRef+2)/2) << endl;
               }else{
                   cout << " " << setw(30) << std::left <<(*itR).seq << setw(15) << ((lRef+2)/2) <<  setw(15) << ((lQue+2)/2) << setw(15) << ((rRef-lRef+2)/2) << endl;
               }
           }
        }
    }
   
    void mergeMemExtVector (uint32_t &revComplement) {
        int flag=0;
        MemExt m;
        if (commonData::d==1 && commonData::numThreads==1)
            return;

        if (MemExtVec.size() > 1) {
            do {
                flag=0;
                sort(MemExtVec.begin(), MemExtVec.end(), MemExt());
                for (vector<MemExt>::iterator it=MemExtVec.begin(); it != --MemExtVec.end(); ++it) {
                    vector<MemExt>::iterator dup = it;
                    ++dup;
                    for (; dup != MemExtVec.end(); ++dup) {
                        if((*dup).lQ + static_cast<uint64_t>(commonData::minMemLen-2)-2 > (*it).rQ) 
                            break;
                        if((*dup).lQ + static_cast<uint64_t>(commonData::minMemLen-2)-2 == (*it).rQ) {
                            if((*dup).lR + static_cast<uint64_t>(commonData::minMemLen-2)-2 == (*it).rR) {
                                flag=1;
                                (*it).rQ=(*dup).rQ;
                                (*it).rR=(*dup).rR;
                                MemExtVec.erase(dup);
                                break;
                            }
                        }
                    }
                    if (flag)
                        break;
                }

                sort(MemExtVec.begin(), MemExtVec.end(), compare_reference);
                for (vector<MemExt>::iterator it=MemExtVec.begin(); it != --MemExtVec.end(); ++it) {
                    vector<MemExt>::iterator dup = it;
                    ++dup;
                    for (; dup != MemExtVec.end(); ++dup) {
                        if((*dup).lR + static_cast<uint64_t>(commonData::minMemLen-2)-2 > (*it).rR) 
                            break;
                        if((*dup).lR + static_cast<uint64_t>(commonData::minMemLen-2)-2 == (*it).rR) {
                            if((*dup).lQ + static_cast<uint64_t>(commonData::minMemLen-2)-2 == (*it).rQ) {
                                flag=1;
                                (*it).rQ=(*dup).rQ;
                                (*it).rR=(*dup).rR;
                                MemExtVec.erase(dup);
                                break;
                            }
                        }
                    }
                    if (flag)
                        break;
                }
            } while (flag);
        }

        for (vector<MemExt>::iterator it=MemExtVec.begin(); it != MemExtVec.end(); ++it) {
            writeToFile((*it).lQ, (*it).rQ, (*it).lR, (*it).rR, revComplement);
        }
        MemExtVec.clear();
    }

    void outputInMummerFormat() {
        string line, last_line;
        static int first=1;
        char buffer[256];
        int n1=2*NUM_TMP_FILES,n2=2*NUM_TMP_FILES+1;
        fstream *filePtr, *forFile=&TmpFiles[n1], *revFile=&TmpFiles[n2];
        memset(buffer,0,256);
        sprintf(buffer, "%s/%d", commonData::nucmer_path, n1);
        (*forFile).open(buffer, ios::in);
        sprintf(buffer, "%s/%d", commonData::nucmer_path, n2);
        (*revFile).open(buffer, ios::in);

        filePtr = forFile;
        if(getline((*filePtr), line).good()) 
            cout << line << endl;

        while(getline((*filePtr), line).good()) {
            if(line[0] == '>'){
                if (last_line.size())
                    cout << last_line << endl;
                last_line = line;
                if (filePtr == forFile) {
                    filePtr = revFile;
                    if (first) {
                        if(getline((*filePtr), line).good()) 
                            cout << line << endl;
                        first=0;
                    }
                }else
                    filePtr = forFile;
                continue;
            }
            cout << line << endl;
        }

        cout << last_line << endl;
        filePtr = revFile;
        while(getline((*filePtr), line).good()) 
            cout << line << endl;
   
        (*revFile).close();
        (*forFile).close();
        remove(buffer);
        sprintf(buffer, "%s/%d", commonData::nucmer_path, n1);
        remove(buffer);
    }

    void removeDuplicates(vector<seqData> &refSeqInfo, vector<seqData> &querySeqInfo, uint32_t revComplement) {
        streambuf *coutbuf=std::cout.rdbuf();
        int numFiles=0;
        MemExt m;
        seqData s;
        char buffer[256];
        memset(buffer,0,256);

        if(IS_MATCH_BOTH_DEF(revComplement))
            numFiles=2*NUM_TMP_FILES;
        else
            numFiles=NUM_TMP_FILES;


        openFiles(ios::in|ios::binary, numFiles);

        sprintf(buffer, "%s/%d", commonData::nucmer_path, numFiles);
        if (IS_MATCH_BOTH_DEF(revComplement)) 
            TmpFiles[numFiles].open(buffer, ios::out|ios::trunc);
        else
            remove(buffer);


        sprintf(buffer, "%s/%d", commonData::nucmer_path, numFiles+1);
        if (IS_MATCH_BOTH_DEF(revComplement))
            TmpFiles[numFiles+1].open(buffer, ios::out|ios::trunc);
        else
            remove(buffer);

        /* Indication that reverse complement is being processed */ 
        if (IS_MATCH_REV_DEF(revComplement))
            revComplement|=0x1; 
 
        /* Redirect std::cout to a file */ 
        if (IS_MATCH_BOTH_DEF(revComplement)){
            std::cout.rdbuf(TmpFiles[numFiles].rdbuf());
        }

        for (int32_t i=0;i<numFiles;i++){
            vector<MemExt>::iterator last;
            sprintf(buffer, "%s/%d", commonData::nucmer_path, i);
            if (i==NUM_TMP_FILES) {
                /* Output any unsued query sequence */
                m.lR=m.lQ=m.rR=m.rQ=0;
                printMemOnTerminal(refSeqInfo, querySeqInfo, m, revComplement);
                /* Processing reverse complement files now*/ 
                revComplement|=0x1; 
                /* Redirect output to reverse complement file */ 
                std::cout.rdbuf(TmpFiles[numFiles+1].rdbuf());
            }

            while(!TmpFiles[i].read((char *)&m, sizeof (MemExt)).eof()) {
                MemExtVec.push_back(m);
            }
            sort(MemExtVec.begin(), MemExtVec.end(), MemExt());
            if (commonData::d==1 &&  commonData::numThreads==1)   // Everything is unique
                last=MemExtVec.end();
            else
                last=unique(MemExtVec.begin(), MemExtVec.end(), myUnique);
            TmpFiles[i].close();
            remove(buffer);
            for (vector<MemExt>::iterator it=MemExtVec.begin(); it!=last; ++it) {
                printMemOnTerminal(refSeqInfo, querySeqInfo, *it, revComplement);
            }
            MemExtVec.clear();
        }

        /* Output any unsued query sequence */
        m.lR=m.lQ=m.rR=m.rQ=0;
        printMemOnTerminal(refSeqInfo, querySeqInfo, m, revComplement);

        /* Restore std::cout */ 
        if (IS_MATCH_BOTH_DEF(revComplement)){
            TmpFiles[numFiles].close();
            TmpFiles[numFiles+1].close();
            std::cout.rdbuf(coutbuf);
            outputInMummerFormat();
        }

        if(revComplement) {
            sprintf(buffer, "%s/revComp", commonData::nucmer_path);
            remove(buffer);
        }
        sprintf(buffer, "%s", commonData::nucmer_path);
        remove(buffer);

    }
};
