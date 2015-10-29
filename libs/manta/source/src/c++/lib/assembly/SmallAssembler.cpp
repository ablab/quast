// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Manta - Structural Variant and Indel Caller
// Copyright (c) 2013-2015 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

///
/// \author Ole Schulz-Trieglaff and Xiaoyu Chen
///


#include "assembly/SmallAssembler.hh"
#include "blt_util/set_util.hh"

#include <cassert>

#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// compile with this macro to get verbose output:
//#define DEBUG_ASBL


// stream used by DEBUG_ASBL:
#ifdef DEBUG_ASBL
#include "blt_util/log.hh"
#include <iostream>

static
void
print_readSet(
    const std::set<unsigned>& readSet)
{
    bool isFirst(true);

    log_os << "[";
    for (const unsigned rd : readSet)
    {
        if (! isFirst) log_os << ",";
        log_os << rd ;
        isFirst=false;
    }
    log_os << "]\n";
}
#endif


// maps kmers to positions in read
typedef std::unordered_map<std::string,unsigned> str_uint_map_t;
// maps kmers to support reads
typedef std::unordered_map<std::string,std::set<unsigned> > str_set_uint_map_t;

typedef std::unordered_set<std::string> str_set_t;


/**
 * Adds base @p base to the end (isEnd is true) or start (otherwise) of the contig.
 *
 *	@return The extended contig.
 */
static
std::string
addBase(
    const std::string& contig,
    const char base,
    const bool isEnd)
{
    if (isEnd) return contig + base;
    else       return base + contig;
}



/**
 * Returns a suffix (isEnd is true) or prefix (otherwise) of @p contig with length @p length.
 *
 *	@return The suffix or prefix.
 */
static
std::string
getEnd(
    const std::string& contig,
    const unsigned length,
    const bool isEnd)
{

    const unsigned csize(contig.size());
    assert(length <= csize);

    if (isEnd) return contig.substr((csize-length),length);
    else       return contig.substr(0,length);
}


#if 0
/// adapt Ole's function to represent word hash as a graph:
///
void
wordHashToDot(
    const str_uint_map_t& wordCount,
    std::ostream& os)
{
    static const unsigned MIN_KMER_FREQ(1);

    // kmer nodes with coverage higher than this get a different color
    //static const int lowCovGraphVisThreshold(3);
    static const unsigned lowCovGraphVisThreshold(MIN_KMER_FREQ);
    static const std::string lowCovNodeColor("red");
    static const std::string highCovNodeColor("green");

    os << "graph {\n";
    os << "node [ style = filled ];\n";
    str_uint_map_t aliasH;
    unsigned n(0);
    for (const str_uint_map_t::value_type& val : wordCount)
    {
        const std::string& word(val.first);
        const unsigned cov(val.second);
        aliasH[word] = n++;
        const std::string& color(cov>lowCovGraphVisThreshold ? highCovNodeColor : lowCovNodeColor);
        os << word << "[label=\"cov" << cov << "\" color=" << color << "]\n";
    }

    // need to add edges here
    static const bool isEnd(true);
    for (const str_uint_map_t::value_type& val : wordCount)
    {
        const std::string& word(val.first);
        const std::string tmp(getEnd(word,word.size()-1,isEnd));
        for (const char symbol : alphabet)
        {
            const std::string newKey(addBase(tmp,symbol,isEnd));
            if (wordCount.find(newKey) != wordCount.end())
            {
                os << aliasH[word] << " -- " << aliasH[newKey] << ";\n";
            }
        }
    }
    os << "}\n";
}
#endif



/**
 * Extends the seed contig (aka most frequent k-mer)
 *
 */
static
void
walk(const SmallAssemblerOptions& opt,
     const std::string& seed,
     const unsigned wordLength,
     const str_uint_map_t& wordCount,
     const str_set_uint_map_t& wordReads,
     std::set<std::string>& seenEdgeBefore,
     AssembledContig& contig)
{
    const str_uint_map_t::const_iterator wordCountEnd(wordCount.end());
    const str_set_uint_map_t::const_iterator wordReadsEnd(wordReads.end());

    // we start with the seed
    str_set_uint_map_t::const_iterator wordReadsIter(wordReads.find(seed));
    assert(wordReadsIter != wordReadsEnd);
    contig.supportReads = wordReadsIter->second;
    contig.seq = seed;

    seenEdgeBefore.clear();
    seenEdgeBefore.insert(seed);

    str_set_t seenVertexBefore;

    // 0 => walk to the right, 1 => walk to the left
    for (unsigned mode(0); mode<2; ++mode)
    {
        unsigned conservativeEndOffset(0);

        const bool isEnd(mode==0);

        while (true)
        {
            const std::string tmp(getEnd(contig.seq, wordLength-1, isEnd));

#ifdef DEBUG_ASBL
            log_os << "# current contig : " << contig.seq << " size : " << contig.seq.size() << "\n"
                   << " getEnd : " << tmp << "\n";
            log_os << "contig rejecting reads : ";
            print_readSet(contig.rejectReads);
            log_os << "contig supporting reads : ";
            print_readSet(contig.supportReads);
#endif

            if (seenVertexBefore.count(tmp))
            {
#ifdef DEBUG_ASBL
                log_os << "Seen word " << tmp << " before on this walk, terminating" << "\n";
#endif
                break;
            }

            seenVertexBefore.insert(tmp);

            unsigned maxBaseCount(0);
            unsigned maxSharedReadCount(0);
            char maxBase(opt.alphabet[0]);
            std::set<unsigned> maxSharedReads;
            std::set<unsigned> supportReads2Remove;
            std::set<unsigned> supportReads2Add;
            std::set<unsigned> rejectReads2Add;

            for (const char symbol : opt.alphabet)
            {
                const std::string newKey(addBase(tmp, symbol, isEnd));
#ifdef DEBUG_ASBL
                log_os << "Extending end : base " << symbol << " " << newKey << "\n";
#endif
                const str_uint_map_t::const_iterator wordCountIter(wordCount.find(newKey));
                if (wordCountIter == wordCountEnd) continue;
                const unsigned currWordCount(wordCountIter->second);

                wordReadsIter= wordReads.find(newKey);
                if (wordReadsIter == wordReadsEnd) continue;
                const std::set<unsigned>& currWordReads(wordReadsIter->second);

                // get the shared supporting reads between the contig and the current word
                std::set<unsigned> sharedReads;
                std::set_intersection(contig.supportReads.begin(), contig.supportReads.end(),
                                      currWordReads.begin(), currWordReads.end(),
                                      std::inserter(sharedReads, sharedReads.begin()));
#ifdef DEBUG_ASBL
                log_os << "Word supporting reads : ";
                print_readSet(currWordReads);
                log_os << "Contig-word shared reads : ";
                print_readSet(sharedReads);
#endif

                if (sharedReads.empty()) continue;

                const unsigned sharedReadCount(sharedReads.size());
                if (sharedReadCount > maxSharedReadCount)
                {
                    // the old shared reads support an unselected allele
                    // remove them from the contig's supporting reads
                    if (!maxSharedReads.empty())
                        supportReads2Remove.insert(maxSharedReads.begin(), maxSharedReads.end());
                    // the old supporting reads is for an unselected allele
                    // they become rejecting reads for the currently selected allele
                    if (!supportReads2Add.empty())
                        rejectReads2Add.insert(supportReads2Add.begin(), supportReads2Add.end());
                    // new supporting reads for the currently selected allele
                    supportReads2Add = currWordReads;

                    maxSharedReadCount = sharedReadCount;
                    maxSharedReads = sharedReads;
                    maxBaseCount = currWordCount;
                    maxBase = symbol;
                }
                else
                {
                    supportReads2Remove.insert(sharedReads.begin(), sharedReads.end());
                    rejectReads2Add.insert(currWordReads.begin(), currWordReads.end());
                }
            }

#ifdef DEBUG_ASBL
            log_os << "Winner is : " << maxBase << " with " << maxBaseCount << " occurrences." << "\n";
#endif

            if (maxBaseCount < opt.minCoverage)
            {
#ifdef DEBUG_ASBL
                log_os << "Coverage or error rate below threshold.\n"
                       << "maxBaseCount : " << maxBaseCount << " minCverage: " << opt.minCoverage << "\n";
#endif
                break;
            }

            /// double check that word exists in reads at least once:
            if (maxBaseCount == 0) break;

            {
                const std::string newEdge(addBase(tmp, maxBase, isEnd));
                seenEdgeBefore.insert(newEdge);
            }

#ifdef DEBUG_ASBL
            log_os << "Adding base " << contig.seq << " " << maxBase << " " << mode << "\n";
#endif
            contig.seq = addBase(contig.seq, maxBase, isEnd);

            if ((conservativeEndOffset != 0) || (maxBaseCount < opt.minConservativeCoverage))
            {
                conservativeEndOffset += 1;
            }

#ifdef DEBUG_ASBL
            log_os << "New contig : " << contig.seq << "\n";
#endif

            // TODO: can add threshold for the count or percentage of shared reads
            {
#ifdef DEBUG_ASBL
                log_os << "Adding rejecting reads " << "\n"
                       << " Old : ";
                print_readSet(contig.rejectReads);
                log_os << " To be added : ";
                print_readSet(rejectReads2Add);
#endif
                // update rejecting reads
                // add reads that support the unselected allele
                for (const unsigned rd : rejectReads2Add)
                {
                    contig.rejectReads.insert(rd);
                }
#ifdef DEBUG_ASBL
                log_os << " New : ";
                print_readSet(contig.rejectReads);
#endif

#ifdef DEBUG_ASBL
                log_os << "Updating supporting reads " << "\n"
                       << " Old : ";
                print_readSet(contig.supportReads);
                log_os << " To be added : ";
                print_readSet(supportReads2Add);
#endif
                // update supporting reads
                // add reads that support the selected allel
                for (const unsigned rd : supportReads2Add)
                {
                    if (contig.rejectReads.find(rd) == contig.rejectReads.end())
                        contig.supportReads.insert(rd);
#ifdef DEBUG_ASBL
                    if (contig.rejectReads.find(rd) != contig.rejectReads.end())
                        log_os << "  Excluding rejected " << rd << "\n";
#endif
                }

#ifdef DEBUG_ASBL
                log_os << " To be removed : ";
                print_readSet(supportReads2Remove);
#endif
                // remove reads that do NOT support the selected allel anymore
                for (const unsigned rd : supportReads2Remove)
                {
                    contig.supportReads.erase(rd);
                }
#ifdef DEBUG_ASBL
                log_os << " New : ";
                print_readSet(contig.supportReads);
#endif
            }
        }

        if (mode == 0)
        {
            contig.conservativeRange.set_end_pos(conservativeEndOffset);
        }
        else
        {
            contig.conservativeRange.set_begin_pos(conservativeEndOffset);
        }

#ifdef DEBUG_ASBL
        log_os << "mode change. Current mode " << mode << "\n";
#endif
    }

    contig.conservativeRange.set_end_pos(contig.seq.size()-contig.conservativeRange.end_pos());
}



/// \params isFindRepeatReads if true record all reads with repeated words
///
static
bool
getKmerCounts(
    const AssemblyReadInput& reads,
    const AssemblyReadOutput& readInfo,
    const unsigned wordLength,
    const bool isFindRepeatReads,
    std::vector<int>& repeatReads,
    str_uint_map_t& wordCount,
    str_set_uint_map_t& wordSupportReads,
    std::vector<str_uint_map_t>& readWordOffsets)
{
    const unsigned readCount(reads.size());
    repeatReads.clear();

    for (unsigned readIndex(0); readIndex<readCount; ++readIndex)
    {
        const AssemblyReadInfo& rinfo(readInfo[readIndex]);

        // skip reads used in a previous iteration
        if (rinfo.isUsed) continue;

        // stores the index of a kmer in a read sequence
        const std::string& seq(reads[readIndex]);
        const unsigned readLen(seq.size());

        // this read is unusable for assembly:
        if (readLen < wordLength) continue;

        str_uint_map_t& readWordOffset(readWordOffsets[readIndex]);

        for (unsigned j(0); j<=(readLen-wordLength); ++j)
        {
            const std::string word(seq.substr(j,wordLength));

            // filter words with "N" (either directly from input alignment
            // or marked due to low basecall quality:
            if (word.find('N') != std::string::npos) continue;

            if (readWordOffset.find(word) != readWordOffset.end())
            {
#ifdef DEBUG_ASBL
                log_os << __FUNCTION__ << ": word " << word << " repeated in read " << readIndex << "\n";
#endif
                if (isFindRepeatReads)
                {
                    repeatReads.push_back(readIndex);
                    break;
                }
                else
                {
                    // try again with different k-mer size
                    return false;
                }
            }

            // record (0-indexed) start point for word in read
            //cout << "Recording " << word << " at " << j << "\n";
            readWordOffset[word]=j;
        }

        // total occurrences from this read:
        for (const str_uint_map_t::value_type& offset : readWordOffset)
        {
            wordCount[offset.first]++;
            // record the supporting read
            wordSupportReads[offset.first].insert(readIndex);
        }
    }

    return (repeatReads.empty());
}



static
bool
buildContigs(
    const SmallAssemblerOptions& opt,
    const bool isLastWord,
    const AssemblyReadInput& reads,
    AssemblyReadOutput& readInfo,
    const unsigned wordLength,
    Assembly& contigs,
    unsigned& unusedReads)
{
    const unsigned readCount(reads.size());

#ifdef DEBUG_ASBL
    static const std::string logtag("buildContigs: ");
    log_os << logtag << "In SVLocusAssembler::buildContig. word length=" << wordLength << " readCount: " << readCount << "\n";
    for (unsigned readIndex(0); readIndex<readCount; ++readIndex)
    {
        log_os << reads[readIndex] << " used=" << readInfo[readIndex].isUsed << "\n";
    }
#endif

    // a set of read hashes; each read hash stores the starting positions of all kmers in the read
    std::vector<str_uint_map_t> readWordOffsets(readCount);
    // counts the number of occurrences for each kmer in all reads
    str_uint_map_t wordCount;
    // records the supporting reads for each kmer
    str_set_uint_map_t wordSupportReads;

    std::vector<int> repeatReads;
    const bool isGoodKmerCount(getKmerCounts(reads, readInfo, wordLength, isLastWord, repeatReads, wordCount, wordSupportReads, readWordOffsets));
    if (! isGoodKmerCount)
    {
        if (isLastWord)
        {
            for (const int readIndex : repeatReads)
            {
                readInfo[readIndex].isUsed = true;
                readInfo[readIndex].isFiltered = true;
                unusedReads--;
            }
        }
        return false;
    }

    // get the kmers corresponding the highest count
    std::set<std::string> maxWords;
    {
        unsigned maxWordCount(0);
        for (const auto& val : wordCount)
        {
            if (val.second < maxWordCount) continue;
            if (val.second > maxWordCount)
            {
                maxWords.clear();
                maxWordCount = val.second;
            }

            maxWords.insert(val.first);
        }

        if (maxWordCount < opt.minCoverage)
        {
#ifdef DEBUG_ASBL
            log_os << logtag << "Coverage too low : " << maxWordCount << " " << opt.minCoverage << "\n";
#endif
            return false;
        }
    }

    // solve for a best contig in the graph by a heuristic greedy maxflow-ish criteria
    AssembledContig contig;
    std::string maxWord;
    {
        // consider multiple possible most frequent seeding k-mers to find the one associated with the longest contig:
        //
        std::set<std::string> seenEdgeBefore;   // records k-mers already encountered during extension

        while (! maxWords.empty())
        {
#ifdef DEBUG_ASBL
            log_os << logtag << "Seeding kmer : " << maxWord << "\n";
#endif

            maxWord=(*maxWords.begin());
            maxWords.erase(maxWords.begin());

            AssembledContig newContig;
            walk(opt, maxWord, wordLength, wordCount, wordSupportReads, seenEdgeBefore, newContig);

            if (newContig.seq.size() > contig.seq.size())
            {
                contig = newContig;
            }

            // subtract seenBefore from maxWords
            inplaceSetSubtract(seenEdgeBefore,maxWords);
        }

        // done with this now
        wordCount.clear();
    }

#ifdef DEBUG_ASBL
    log_os << logtag << "First pass assembly resulted in "
           << contig.seq << "\n"
           << " with length " << contig.seq.size() << ". Input consisted of " << readCount << " reads.\n"
           << "Final supporting reads: ";
    print_readSet(contig.supportReads);
    log_os << "Final rejecting reads: ";
    print_readSet(contig.rejectReads);
#endif

    // WHY CHECK THIS AFTER WALK???
    // increment number of reads containing the seeding kmer
    //
    // TODO isn't this equal to maxWordCount? Do we need to sum it here?
    for (unsigned readIndex(0); readIndex<readCount; ++readIndex)
    {
        const str_uint_map_t& readWordOffset(readWordOffsets[readIndex]);
        if (readWordOffset.count(maxWord)) ++contig.seedReadCount;
    }

#ifdef DEBUG_ASBL
    log_os << logtag << "final seeding reading count: " << contig.seedReadCount << "\n";
#endif
    if (contig.seedReadCount < opt.minSeedReads)
    {
#ifdef DEBUG_ASBL
        log_os << "\t...which is below minSeedReadCount of " << opt.minSeedReads << " discarding.\n";
#endif
        return false;
    }

    // finally -- set isUsed and decrement unusedReads
    for (unsigned readIndex(0); readIndex<readCount; ++readIndex)
    {
        AssemblyReadInfo& rinfo(readInfo[readIndex]);
        if (rinfo.isUsed) continue;

        if (contig.supportReads.find(readIndex) != contig.supportReads.end())
        {
            rinfo.isUsed = true;
            rinfo.contigId = contigs.size();

            assert(unusedReads != 0);
            --unusedReads;
        }
    }

    // don't need this anymore:
    readWordOffsets.clear();

    contigs.push_back(contig);
    return true;
}



void
runSmallAssembler(
    const SmallAssemblerOptions& opt,
    const AssemblyReadInput& reads,
    AssemblyReadOutput& assembledReadInfo,
    Assembly& contigs)
{
#ifdef DEBUG_ASBL
    static const std::string logtag("runSmallAssembler: ");
    log_os << logtag << "Starting assembly with " << reads.size() << " reads.\n";
#endif
    assert(opt.alphabet.size()>1);

    assembledReadInfo.clear();
    contigs.clear();

    assembledReadInfo.resize(reads.size());

    unsigned unusedReads(reads.size());

    for (unsigned iteration(0); iteration < opt.maxAssemblyIterations; ++iteration)
    {
        if (unusedReads < opt.minSeedReads) return;

        const unsigned lastUnusedReads(unusedReads);
        for (unsigned wordLength(opt.minWordLength); wordLength<=opt.maxWordLength; wordLength+=opt.wordStepSize)
        {
            const bool isLastWord(wordLength+opt.wordStepSize > opt.maxWordLength);
            const bool isAssemblySuccess = buildContigs(opt, isLastWord, reads, assembledReadInfo, wordLength, contigs, unusedReads);
            if (isAssemblySuccess) break;
        }

#ifdef DEBUG_ASBL
        log_os << logtag << "iter: " << iteration << " unused readMap now: " << unusedReads << "\n";
#endif

        // stop if no change in number of unused reads
        if (unusedReads == lastUnusedReads)
        {
#ifdef DEBUG_ASBL
            log_os << logtag << "Number of unused reads (" << unusedReads << ") did not change in this iteration. Stopping.\n";
#endif
            return;
        }
    }
#ifdef DEBUG_ASBL
    log_os << logtag << "Reached max number of assembly iterations: " << opt.maxAssemblyIterations << "\n";
#endif
}

