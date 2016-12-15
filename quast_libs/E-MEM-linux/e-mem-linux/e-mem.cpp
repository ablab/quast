/* ================================================================= *
 *  e-mem.cpp : Main program                                         *
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

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <unordered_map>
#include <map>
#include <vector>
#include <iterator>
#include <omp.h>
#include <boost/algorithm/string.hpp>
#include <boost/tokenizer.hpp>
#include <sys/stat.h>

#include "e-mem.h"
#include "file.h"
#include "qlist.h"


using namespace std;
using namespace boost;

/* 
 * Function builds a kmer hash for a reference sequence.
 * Input: empty refHash
 * Output: populated refHash
 */
void buildRefHash(Knode* &refHash, uint64_t totalBits, seqFileReadInfo &RefFile)
{
    uint64_t j=0;
    uint64_t currKmerPos=0, currKmer=0;
    int32_t offset=0; 
    int nextKmerPosition = commonData::minMemLen - commonData::kmerSize + 2;
    vector<mapObject>::iterator it;
    it = upper_bound(RefFile.blockOfNs.begin(), RefFile.blockOfNs.end(), currKmerPos, mapObject()); 
    while (currKmerPos<=totalBits)
    {
        if (currKmerPos + commonData::kmerSize - 2 > totalBits)
            break;

        if(RefFile.checkKmerForNs(currKmerPos, it)){
            currKmerPos+=nextKmerPosition; // Move L-K+2 bits = 50-28+1=23 char = 46 bits
            continue;
        }

        offset = currKmerPos%DATATYPE_WIDTH;
        j=currKmerPos/DATATYPE_WIDTH; // next loc in binReads 
        
        currKmer = RefFile.binReads[j];
        currKmer <<= offset;

        if (offset > DATATYPE_WIDTH-commonData::kmerSize) // Kmer split in two integers
            currKmer |= ((RefFile.binReads[j+1] & global_mask_left[(commonData::kmerSize-(DATATYPE_WIDTH-offset))/2 -1])>>(DATATYPE_WIDTH-offset));
        else
            currKmer &= global_mask_left[commonData::kmerSize/2 - 1];
        /* Add kmer to the hash table */
        refHash->addKmerNode(currKmer, currKmerPos);
        currKmerPos+=nextKmerPosition; // Move L-K+2 bits = 50-28+1=23 char = 46 bits
    }
}

/* 
 * Function extends the kmer match in left/right direction for 
 * possible MEMs.
 * Input: currRPos : current position of matching reference Kmer 
 * Input: currRPos : current position of matching query Kmer 
 * Input: totalRBits : total number of bits in reference  
 * Input: totalQBits : total number of bits in query  
 * Input: name : reference sequence string for output  
 *
 */
void helperReportMem(uint64_t &currRPos, uint64_t &currQPos, uint64_t totalRBits, uint64_t totalQBits, queryList* &currQueryMEMs, std::unordered_multimap <uint64_t, uint64_t> &currMEMs, seqFileReadInfo &RefFile, seqFileReadInfo &QueryFile, tmpFilesInfo &arrayTmpFile, mapObject &RefNpos, mapObject &QueryNpos, uint32_t &revComplement)
{
    /*
     * lRef and lQue are local variables for left extension of
     * reference and query sequence respectively. rRef and rQue
     * are their right counterparts.
     */
    uint64_t lRef=currRPos, lQue=currQPos; // Keeping lRef on currRPos-this makes offset computation simpler
    uint64_t offsetR=0,offsetQ=0;
    uint64_t rRef=currRPos+commonData::kmerSize, rQue=currQPos+commonData::kmerSize; // one character ahead of current match
    uint64_t currR=0, currQ=0;
    int i=0,j=0,mismatch=0;
    uint64_t matchSize=0;

    if (!(((QueryNpos.left==0x1)?true:QueryNpos.left<=lQue) && rQue<=QueryNpos.right)) 
        QueryFile.getKmerLeftnRightBoundForNs(lQue, QueryNpos); 

    if (!(((RefNpos.left==0x1)?true:RefNpos.left<=lRef) && rRef<=RefNpos.right)) 
        RefFile.getKmerLeftnRightBoundForNs(lRef, RefNpos); 

    if (RefNpos.right-((RefNpos.left==0x1)?0:RefNpos.left)+2 < static_cast<uint64_t>(commonData::minMemLen))
        return;

    if (QueryNpos.right-((QueryNpos.left==0x1)?0:QueryNpos.left)+2 < static_cast<uint64_t>(commonData::minMemLen))
        return;

    //match towards left
    while (lRef && lQue && ((QueryNpos.left==0x1)?true:QueryNpos.left<=lQue) && ((RefNpos.left==0x1)?true:RefNpos.left<=lRef))
    {
        if (!mismatch)
        {
            offsetR=(lRef)%DATATYPE_WIDTH;
            i=(lRef)/DATATYPE_WIDTH;
            offsetQ=(lQue)%DATATYPE_WIDTH;
            j=(lQue)/DATATYPE_WIDTH;
       
            if (offsetR > offsetQ)
                matchSize = offsetQ;
            else 
                matchSize = offsetR;

            if (!matchSize)
                matchSize=2;

            if ((QueryNpos.left!=0x1) && (matchSize > lQue-QueryNpos.left))
                matchSize = lQue-QueryNpos.left;
            
            if ((RefNpos.left!=0x1) && (matchSize > lRef-RefNpos.left))
                matchSize = lRef-RefNpos.left;
            
            if (!matchSize)
                break;

            /* 
             * There will never be case with offset=0 and i=0 because
             * i=0 happens only when lRef=0 and in that case we do not 
             * enter this loop.
             */
            currR = RefFile.binReads[offsetR?i:i-1];
            currR >>= DATATYPE_WIDTH-offsetR;
            currQ = QueryFile.binReads[offsetQ?j:j-1];
            currQ >>= DATATYPE_WIDTH-offsetQ;
        } 

        if((currR & global_mask_right[matchSize/2 - 1]) != (currQ &  global_mask_right[matchSize/2 - 1])) {
            if (matchSize==2)
                break;

            mismatch=1;
            matchSize/=2;
            if (matchSize%2)
                matchSize+=1;
        }else {
            lRef-=matchSize;
            lQue-=matchSize;
            if (mismatch) {
                if (matchSize==2) 
                    break;
                currR >>= matchSize;
                currQ >>= matchSize;
            }
        }
    }
    
    if (totalRBits-lRef+2 < static_cast<uint64_t>(commonData::minMemLen))
        return;
    
    if (totalQBits-lQue+2 < static_cast<uint64_t>(commonData::minMemLen))
        return;

    //match towards right
    mismatch=0;
    while ((rRef <= totalRBits) && (rQue <= totalQBits) && (rRef <= RefNpos.right) && (rQue <= QueryNpos.right)) 
    {
        if (!mismatch)
        {
            offsetR=rRef%DATATYPE_WIDTH;
            i=rRef/DATATYPE_WIDTH;
            offsetQ=rQue%DATATYPE_WIDTH;
            j=rQue/DATATYPE_WIDTH;

            if (offsetR > offsetQ)
                matchSize = DATATYPE_WIDTH-offsetR;
            else
                matchSize = DATATYPE_WIDTH-offsetQ;
    
            if (rRef+matchSize > totalRBits)
                matchSize = totalRBits-rRef;
        
            if (rQue+matchSize > totalQBits)
                matchSize = totalQBits-rQue;

            if (rQue+matchSize > QueryNpos.right)
                matchSize = QueryNpos.right-rQue;
            
            if (rRef+matchSize > RefNpos.right)
                matchSize = RefNpos.right-rRef;

            if(!matchSize)
                matchSize=2;

            
        
            currR = RefFile.binReads[i];
            currR <<= offsetR;
            currQ = QueryFile.binReads[j];
            currQ <<= offsetQ;
        }
         
        if((currR & global_mask_left[matchSize/2 - 1]) != (currQ &  global_mask_left[matchSize/2 - 1])) {
            if (matchSize==2){
                rRef-=2;
                rQue-=2;
                break;
            }
            mismatch=1;
            matchSize/=2;
            if (matchSize%2)
                matchSize+=1;
        }else {
            if (mismatch) {
                if (matchSize==2)
                    break;
            }
            if ((rRef == totalRBits) || (rQue == totalQBits))
                break;
            
            currR <<= matchSize;
            currQ <<= matchSize;
            rRef+=matchSize;
            rQue+=matchSize;
        }
    }

    /* Adjust rRef and rQue locations */

    if (rRef > RefNpos.right){
        rQue-=(rRef-RefNpos.right);
        rRef=RefNpos.right;
    }
    if (rQue > QueryNpos.right){
        rRef-=(rQue-QueryNpos.right);
        rQue=QueryNpos.right;
    }

    if (rRef > totalRBits){
        rQue-=(rRef-totalRBits);
        rRef=totalRBits;
    }
    if (rQue > totalQBits){
        rRef-=(rQue-totalQBits);
        rQue=totalQBits;
    }

    if (arrayTmpFile.writeMemInTmpFiles(lRef, rRef, lQue, rQue, QueryFile, RefFile, revComplement)) {
        uint64_t key = ((lRef << 32) | rRef);
        uint64_t value = ((lQue << 32) | rQue);
        currMEMs.insert(std::make_pair(key, value));
        currQueryMEMs->ListAdd(&currQueryMEMs, lQue, rQue, key);
    }
}

void reportMEM(Knode * &refHash, uint64_t totalBases, uint64_t totalQBases, seqFileReadInfo &RefFile, seqFileReadInfo &QueryFile, tmpFilesInfo &arrayTmpFile, uint32_t &revComplement)
{
  uint64_t totalQBits = CHARS2BITS(totalQBases);
  uint32_t copyBits=0;
  #pragma omp parallel num_threads(commonData::numThreads) 
  {
      queryList *currQueryMEMs = NULL;
      unordered_multimap <uint64_t, uint64_t> currMEMs;
      uint64_t currKmer=0, j=0;
      int32_t offset=0;
      uint32_t first=1;
      int kmerWithNs=0;
      mapObject QueryNpos, RefNpos;
      vector<mapObject>::iterator it;
      it = upper_bound(QueryFile.blockOfNs.begin(), QueryFile.blockOfNs.end(), 0, mapObject()); 

      #pragma omp single
      {
      /*
       * Number of copy bits during query kmer processing depends on kmer size.
       */
      if (DATATYPE_WIDTH-commonData::kmerSize > 32 )
          copyBits=32; //16 characters
      else if (DATATYPE_WIDTH-commonData::kmerSize > 16)
          copyBits=16; //8 characters
      else
          copyBits=8; //4 characters

      /* If copyBits more than 8, the for loop parallelisation will give 
       * incorrect results - miss some Mems
       */
      if(commonData::numThreads > 1)   
          copyBits=8; //4 characters
      }   
      

      #pragma omp for 
      for (uint64_t currKmerPos=0; currKmerPos<=totalQBits; currKmerPos+=2)
      {
          if ((currKmerPos + commonData::kmerSize - 2) > totalQBits)
              continue;
        
          if(QueryFile.checkKmerForNs(currKmerPos, it)){
              kmerWithNs=1;
          }

          j=currKmerPos/DATATYPE_WIDTH;// current location in binReads 
          offset = currKmerPos%DATATYPE_WIDTH;
          if(first || !offset){
              currKmer = QueryFile.binReads[j];
              currKmer <<= offset;
              if(offset > DATATYPE_WIDTH-commonData::kmerSize)
                  currKmer |= ((QueryFile.binReads[j+1] & global_mask_left[offset/2-1])>>(DATATYPE_WIDTH-offset));
              first=0;
          }else
              currKmer <<= 2;

          if(offset  && !(offset % copyBits))
              currKmer |= ((QueryFile.binReads[j+1] & global_mask_left[offset/2-1])>>(DATATYPE_WIDTH-offset));

          if (kmerWithNs){
             /* Do not process this Kmer, Ns in it */
             kmerWithNs=0;
             continue;
          }
          /* Find the K-mer in the refHash */
          uint64_t *dataPtr=NULL;
          if (refHash->findKmer(currKmer & global_mask_left[commonData::kmerSize/2 - 1], dataPtr)) 
          {
              // We have a match
              for (uint64_t n=1; n<=dataPtr[0]; n++) {   
                  // Check if MEM has already been discovered, if not proces it
                  if (!(currQueryMEMs->checkRedundantMEM(&currQueryMEMs, dataPtr[n], currKmerPos, CHARS2BITS(totalBases), currMEMs)))
                      helperReportMem(dataPtr[n], currKmerPos, CHARS2BITS(totalBases), CHARS2BITS(totalQBases), currQueryMEMs, currMEMs, RefFile, QueryFile, arrayTmpFile, RefNpos, QueryNpos, revComplement);
              }
          }
      }
      currMEMs.clear();
      currQueryMEMs->ListFree(&currQueryMEMs);
  }  
}

void processQuery(Knode * &refHash, seqFileReadInfo &RefFile, seqFileReadInfo &QueryFile, tmpFilesInfo &arrayTmpFile, uint32_t &revComplement)
{
    QueryFile.clearFileFlag();
    QueryFile.resetCurrPos();
    for (int32_t i=0; i<commonData::d; i++) {
        if(QueryFile.readChunks()){
            reportMEM(refHash, RefFile.totalBases-1, QueryFile.totalBases-1, RefFile, QueryFile, arrayTmpFile, revComplement);
            QueryFile.setCurrPos();
            QueryFile.clearMapForNs();
        }
        else
            break;
    }
    QueryFile.clearTmpString();
}

void processReference(seqFileReadInfo &RefFile, seqFileReadInfo &QueryFile, tmpFilesInfo &arrayTmpFile, uint32_t &revComplement)
{
    uint64_t numberOfKmers=0,n=0;
    int hashTableSizeIndex=0;
    Knode *refHash;
    
    numberOfKmers = ceil((RefFile.totalBases-commonData::kmerSize/2+1)/((commonData::minMemLen/2-commonData::kmerSize/2 + 1)) + 1);

    /* Set the size of the hash table to the numberofKmers. */
    for (n=0; n<450; ++n)
    {
        if (hashTableSize[n] > 1.75*numberOfKmers)
        {
           hashTableSizeIndex = n;
           break;
        }
    }

    Knode::currHashTabSize = hashTableSize[hashTableSizeIndex];  //Store the size of the hash table.
    if (hashTableSizeIndex)
        Knode::prevHashTabSize = hashTableSize[hashTableSizeIndex-1];
    else
        Knode::prevHashTabSize = 3;

    /* Create the refHash for K-mers. */
    refHash = new Knode[Knode::currHashTabSize];

    buildRefHash(refHash, CHARS2BITS(RefFile.totalBases-1), RefFile);

    processQuery(refHash, RefFile, QueryFile, arrayTmpFile, revComplement);

    delete [] refHash; 
}

bool is_numeric(const string &str)
{
    return all_of(str.begin(), str.end(), ::isdigit); 
}

void checkCommandLineOptions(uint32_t &options)
{
    if (!IS_REF_FILE_DEF(options)){
        cout << "ERROR: reference file must be passed!" << endl;
        exit(EXIT_FAILURE);
    }
    if (!IS_QUERY_FILE_DEF(options)){
        cout << "ERROR: query file must be passed!" << endl;
        exit(EXIT_FAILURE);
    }
    
    if (IS_SPLIT_SIZE_DEF(options)){
        if (commonData::d <= 0){
            cout << "ERROR: -d cannot be less than or equal to zero!" << endl;
            exit(EXIT_FAILURE);
        }
    }
    
    if (IS_NUM_THREADS_DEF(options)){
        if (commonData::numThreads <= 0){
            cout << "ERROR: -t cannot be less than or equal to zero!" << endl;
            exit(EXIT_FAILURE);
        }
    }
    
    if (IS_LENGTH_DEF(options)){
        if (commonData::minMemLen <= 2){
            cout << "ERROR: -l cannot be less than or equal to one!" << endl;
            exit(EXIT_FAILURE);
        }
        if ((commonData::minMemLen - commonData::kmerSize) < 0)
        {
            if (IS_KMER_SIZE_DEF(options)){
                cout << "ERROR: kmer-size cannot be larger than min mem length!" << endl;
                exit( EXIT_FAILURE );
            }else {
                commonData::kmerSize = commonData::minMemLen; 
            }
        }
    }
    
    if (IS_KMER_SIZE_DEF(options)){
        if (commonData::kmerSize <= 0){
            cout << "ERROR: -k cannot be less than or equal to zero!" << endl;
            exit(EXIT_FAILURE);
        }

        if (commonData::kmerSize > CHARS2BITS(28))
        {
            cout << "ERROR: kmer-size cannot be greater than 28" << endl;
            exit( EXIT_FAILURE );
        }
    }

    if (IS_MATCH_REV_DEF(options) && IS_MATCH_BOTH_DEF(options)) {
        cout << "ERROR: option -b and option -r exclude each other!" << endl;
        exit(EXIT_FAILURE);
    }
  
    if(IS_RELREV_QUEPOS_DEF(options)) {
        if (!IS_MATCH_REV_DEF(options) && !IS_MATCH_BOTH_DEF(options)) {
            cout << "ERROR: option -c requires either option -r or - b" << endl;
            exit( EXIT_FAILURE );
        }
    }
}

void print_help_msg()
{
    cout << "e-mem finds and outputs the position and length of all maximal" << endl;
    cout << "exact matches (MEMs) between <query-file> and <reference-file>" << endl;
    cout << endl;
    cout << "Usage: ../e-mem [options]  <reference-file>  <query-file>" << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "-n\t" << "match only the characters a, c, g, or t" << endl;
    cout << "  \tthey can be in upper or in lower case" << endl;
    cout << "-l\t" << "set the minimum length of a match. The default length" << endl;
    cout << "  \tis 50" << endl;
    cout << "-b\t" << "compute forward and reverse complement matches" << endl;
    cout << "-r\t" << "only compute reverse complement matches" << endl;
    cout << "-c\t" << "report the query-position of a reverse complement match" << endl;
    cout << "  \trelative to the original query sequence" << endl;
    cout << "-F\t" << "force 4 column output format regardless of the number of" << endl;
    cout << "  \treference sequence input" << endl;
    cout << "-L\t" << "show the length of the query sequences on the header line" << endl;
    cout << "-d\t" << "set the split size. The default value is 1" << endl;
    cout << "-t\t" << "number of threads. The default is 1 thread" << endl;
    cout << "-h\t" << "show possible options" << endl;
}

int main (int argc, char *argv[])
{
    int32_t i=0, n=1;
    uint32_t options=0, revComplement=0;
    seqFileReadInfo RefFile, QueryFile;
  
    // Check Arguments
    if (argc==1 || argc==2){
       print_help_msg();
       exit(EXIT_SUCCESS);
    }
    
    while(argv[n]) {
        if(boost::equals(argv[n],"-l")){
            if (IS_LENGTH_DEF(options)) {
                cout << "ERROR: Length argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_LENGTH(options);
            if (!argv[n+1] || !is_numeric(argv[n+1])){
                cout << "ERROR: Invalid value for -l option!" << endl;
                exit(EXIT_FAILURE);
            }
            commonData::minMemLen = 2*std::stoi(argv[n+1]);
            n+=2;
        }else if (boost::equals(argv[n],"-d")){
            if (IS_SPLIT_SIZE_DEF(options)) {
                cout << "ERROR: Split size argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            if (!argv[n+1] || !is_numeric(argv[n+1])){
                cout << "ERROR: Invalid value for -d option!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_SPLIT_SIZE(options);
            commonData::d = std::stoi(argv[n+1]);
            n+=2;
        }else if (boost::equals(argv[n],"-t")){
            if (IS_NUM_THREADS_DEF(options)) {
                cout << "ERROR: Number of threads argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            if (!argv[n+1] || !is_numeric(argv[n+1])){
                cout << "ERROR: Invalid value for -t option!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_NUM_THREADS(options);
            commonData::numThreads = std::stoi(argv[n+1]);
            n+=2;
        }else if (boost::equals(argv[n],"-k")){
            if (IS_KMER_SIZE_DEF(options)) {
                cout << "ERROR: Kmer size argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            if (!argv[n+1] || !is_numeric(argv[n+1])){
                cout << "ERROR: Invalid value for -k option!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_KMER_SIZE(options);
            commonData::kmerSize = 2*std::stoi(argv[n+1]);
            n+=2;
        }else if (argv[n][0] != '-'){
            /* These are files */
            if (!IS_REF_FILE_DEF(options)) {
                /* Open referencead file provided by the user. */
                SET_REF_FILE(options);
                RefFile.openFile(argv[n]);
                n+=1;
                continue;
            }
            if (!IS_QUERY_FILE_DEF(options)) {
                /* Open query file provided by the user. */
                SET_QUERY_FILE(options);
                QueryFile.openFile(argv[n]);
                n+=1;
                continue;
            }
            cout << "ERROR: More input files than expected!" << endl;
            exit(EXIT_FAILURE);
        }else if (boost::equals(argv[n],"-r")){
            if (IS_MATCH_REV_DEF(options)) {
                cout << "ERROR: Reverse match argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_MATCH_REV(options);
            n+=1;
        }else if (boost::equals(argv[n],"-b")){
            if (IS_MATCH_BOTH_DEF(options)) {
                cout << "ERROR: option -b passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_MATCH_BOTH(options);
            n+=1;
        }else if (boost::equals(argv[n],"-n")){
            if (IS_IGNORE_N_DEF(options)) {
                cout << "ERROR: Ignore N's argument passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_IGNORE_N(options);
            commonData::ignoreN = 1;
            n+=1;
        }else if (boost::equals(argv[n],"-c")){
            if (IS_RELREV_QUEPOS_DEF(options)) {
                cout << "ERROR: option -c passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_RELREV_QUEPOS(options);
            n+=1;
            commonData::relQueryPos = 1;
        }else if (boost::equals(argv[n],"-F")){
            if (IS_FCOL_OUTPUT_DEF(options)) {
                cout << "ERROR: option -F passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_FCOL_OUTPUT(options);
            n+=1;
            commonData::fourColOutput = 1;
        }else if (boost::equals(argv[n],"-L")){
            if (IS_LEN_IN_HEADER_DEF(options)) {
                cout << "ERROR: option -L passed multiple times!" << endl;
                exit(EXIT_FAILURE);
            }
            SET_LEN_IN_HEADER(options);
            n+=1;
            commonData::lenInHeader = 1;
        }else if (boost::equals(argv[n],"-mum")){
            /* Ignore this option and continue */
            n+=1;
        }else if (boost::equals(argv[n],"-mumcand")){
            /* Ignore this option and continue */
            n+=1;
        }else if (boost::equals(argv[n],"-mumreference")){
            /* Ignore this option and continue */
            n+=1;
        }else if (boost::equals(argv[n],"-maxmatch")){
            /* Ignore this option and continue */
            n+=1;
        }else if (boost::equals(argv[n],"-s")){
            /* Ignore this option and continue */
            n+=1;
        }else if (boost::equals(argv[n],"-h")){
            print_help_msg();
            exit(EXIT_SUCCESS);
        }else if (boost::equals(argv[n],"-p")){
            commonData::pfx_path = argv[n+1];
            n+=2;
        }else {
            cout << "ERROR: Invalid option." << endl << flush;
            print_help_msg();
            exit( EXIT_FAILURE );
        }
    }

    checkCommandLineOptions(options);
    
    /*
     * Check if e-mem if being run from QUAST 
     */
    sprintf(commonData::nucmer_path, "%s/%d_tmp", commonData::pfx_path.empty() ? "." : commonData::pfx_path.c_str(), getpid());

    tmpFilesInfo arrayTmpFile(IS_MATCH_BOTH_DEF(options)?(2*NUM_TMP_FILES+2):NUM_TMP_FILES+2);
    arrayTmpFile.openFiles(ios::out|ios::binary, IS_MATCH_BOTH_DEF(options)?(2*NUM_TMP_FILES+2):NUM_TMP_FILES+2);

    RefFile.generateRevComplement(0); // This routine also computers size and num sequences
    QueryFile.generateRevComplement((IS_MATCH_REV_DEF(options) || IS_MATCH_BOTH_DEF(options))); // Reverse complement only for query

    /* Only reverse complement matches */
    if (IS_MATCH_REV_DEF(options)){
        QueryFile.setReverseFile();
        SET_MATCH_REV(revComplement);
    }
    arrayTmpFile.setNumMemsInFile(QueryFile.allocBinArray(), QueryFile.getNumSequences());
    RefFile.allocBinArray();
    RefFile.clearFileFlag();

    while (true)
    {
        for (i=0; i<commonData::d; i++) {
            if(RefFile.readChunks()){
                processReference(RefFile, QueryFile, arrayTmpFile, revComplement);
                RefFile.setCurrPos();
                RefFile.clearMapForNs();
            }
            else
                break;
        }

        /*
         * Process MemExt list 
         */ 

        arrayTmpFile.mergeMemExtVector(revComplement);

        if (revComplement)
            break;
        if (IS_MATCH_BOTH_DEF(options)){
            SET_MATCH_BOTH(revComplement);
            //revComplement=1;
            RefFile.clearFileFlag();
            RefFile.resetCurrPos();
            RefFile.totalBases=0;
            QueryFile.setReverseFile();
            QueryFile.totalBases=0;
        }
        else
            break;
    }

    /*
     * Free up the allocated arrays
     */
    arrayTmpFile.closeFiles(IS_MATCH_BOTH_DEF(options)?(2*NUM_TMP_FILES):NUM_TMP_FILES);
    RefFile.destroy();
    QueryFile.destroy();

    /* 
     * Populate sequence information in vectors. Use this to get MEM
     * positions relative to the original sequences.
     */
    vector<seqData> refSeqInfo;
    vector<seqData> querySeqInfo;
    refSeqInfo.reserve(RefFile.getNumSequences());
    querySeqInfo.reserve(QueryFile.getNumSequences());
    RefFile.generateSeqPos(refSeqInfo);
    QueryFile.generateSeqPos(querySeqInfo);
    RefFile.closeFile();
    QueryFile.closeFile();

    arrayTmpFile.removeDuplicates(refSeqInfo, querySeqInfo, revComplement);
    return 0;
}

