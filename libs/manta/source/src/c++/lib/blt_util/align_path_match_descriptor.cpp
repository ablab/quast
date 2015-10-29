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
/// \author Chris Saunders
///


#include "blt_util/align_path_match_descriptor.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "blt_util/parse_util.hh"
#include "blt_util/seq_util.hh"

#include "boost/lexical_cast.hpp"

#include <cassert>

#include <sstream>


enum
{
    INDEL_BEGIN='^',
    INDEL_END='$'
};



static
void
unknown_md_error(const char* const md,
                 const char* const mdptr)
{

    std::ostringstream oss;
    oss << "ERROR: can't parse match descriptor string: " << md << "\n"
        << "\tunexpected character: '" << *mdptr << "' at position: " << (mdptr-md+1) << "\n";
    throw blt_exception(oss.str().c_str());
}



namespace ALIGNPATH
{


static
void
apath_push(path_t& apath,
           path_segment& ps,
           const align_t t)
{

    if ( (0==ps.length) || (ps.type==t) ) return;
    apath.push_back(ps);
    ps.clear();
}



static
void
export_md_to_apath_impl(const char* md,
                        path_t& apath)
{

    using illumina::blt_util::parse_unsigned;

    const char* mdptr(md);
    path_segment ps;

    while (*mdptr)
    {
        if       (isdigit(*mdptr))
        {
            apath_push(apath,ps,MATCH);
            const unsigned mlen(parse_unsigned(mdptr));
            ps.length += mlen;
            ps.type = MATCH;

        }
        else if (is_valid_base(*mdptr))
        {
            apath_push(apath,ps,MATCH);
            mdptr++;
            ps.length++;
            ps.type = MATCH;

        }
        else if (*mdptr == INDEL_BEGIN)
        {
            mdptr++; // eat INDEL_BEGIN

            while (*mdptr != INDEL_END)
            {
                if       (isdigit(*mdptr))
                {
                    apath_push(apath,ps,INSERT);
                    const unsigned mlen(parse_unsigned(mdptr));
                    ps.length=mlen;
                    ps.type=INSERT;

                }
                else if (is_valid_base(*mdptr))
                {
                    apath_push(apath,ps,DELETE);
                    mdptr++;
                    ps.length++;
                    ps.type=DELETE;

                }
                else
                {
                    unknown_md_error(md,mdptr);
                }
            }

            mdptr++; // eat INDEL_END

        }
        else
        {
            unknown_md_error(md,mdptr);
        }
    }

    apath_push(apath,ps,NONE);
}



void
export_md_to_apath(const char* md,
                   const bool is_fwd_strand,
                   path_t& apath,
                   const bool is_edge_deletion_error)
{

    // to make best use of previous code, we parse the MD in the
    // alignment direction and then orient apath to the forward strand
    // as a second step if required
    //
    assert(NULL != md);

    apath.clear();
    export_md_to_apath_impl(md,apath);

    unsigned as(apath.size());

    if ( ((as>0) and (apath.front().type == DELETE)) or
         ((as>1) and (apath.back().type == DELETE)) )
    {
        std::ostringstream oss;
        if (is_edge_deletion_error)
        {
            oss << "ERROR: ";
        }
        else
        {
            oss << "WARNING: ";
        }
        oss << "alignment path: " << apath_to_cigar(apath) << " contains meaningless edge deletion.\n";
        if (is_edge_deletion_error)
        {
            throw blt_exception(oss.str().c_str());
        }
        else
        {
            log_os << oss.str();
            path_t apath2;
            for (unsigned i(0); i<as; ++i)
            {
                if (((i==0) or ((i+1)==as)) and
                    apath[i].type == DELETE) continue;
                apath2.push_back(apath[i]);
            }
            apath=apath2;
            as=apath.size();
        }
    }

    if ( (not is_fwd_strand) and (as>1) )
    {
        std::reverse(apath.begin(),apath.end());
    }
}



static
void
fwd_apath_to_export_md(path_t& apath,
                       const char* ref_begin,
                       const char* ref_bases,
                       const char* ref_end,
                       const char* read_bases,
                       std::string& md)
{

    // process the align path
    bool foundUnsupportedCigar = false;
    path_t::const_iterator pCIter;
    for (pCIter = apath.begin(); pCIter != apath.end(); ++pCIter)
    {

        if (pCIter->type == DELETE)
        {

            // handle deletion
            md.push_back('^');
            for (uint32_t i = 0; i < pCIter->length; ++i, ++ref_bases)
            {
                md.push_back(*ref_bases);
            }
            md.push_back('$');

        }
        else if (pCIter->type == INSERT)
        {

            // handle insertion
            md.push_back('^');
            md += boost::lexical_cast<std::string>(pCIter->length);
            read_bases += pCIter->length;
            md.push_back('$');

        }
        else if (is_segment_align_match(pCIter->type))
        {

            // handle match/mismatch
            uint32_t numMatchingBases = 0;
            for (uint32_t i = 0; i < pCIter->length; ++i, ++ref_bases, ++read_bases)
            {

                // handle circular genome
                if ((ref_bases < ref_begin) || (ref_bases > ref_end))
                {
                    md.push_back('N');
                    continue;
                }

                if (*ref_bases != *read_bases)
                {

                    // write the number of preceding matching bases
                    if (numMatchingBases != 0)
                    {
                        md += boost::lexical_cast<std::string>(numMatchingBases);
                        numMatchingBases = 0;
                    }

                    // output the mismatched base
                    md.push_back(*ref_bases);

                }
                else ++numMatchingBases;
            }

            // write the number of trailing matching bases
            if (numMatchingBases != 0)
            {
                md += boost::lexical_cast<std::string>(numMatchingBases);
            }

        }
        else
        {

            // handle unsupported CIGAR operation
            foundUnsupportedCigar = true;
            break;
        }
    }

    if (foundUnsupportedCigar) md = "UNSUPPORTED";
}



static
void
rev_apath_to_export_md(path_t& apath,
                       const char* ref_begin,
                       const char* ref_bases,
                       const char* ref_end,
                       const char* read_bases,
                       std::string& md)
{

    // process the align path
    bool foundUnsupportedCigar = false;
    path_t::const_reverse_iterator pCRIter;
    for (pCRIter = apath.rbegin(); pCRIter != apath.rend(); ++pCRIter)
    {
        if (pCRIter->type == DELETE)
        {
            // handle deletion
            md.push_back('^');
            for (uint32_t i = 0; i < pCRIter->length; ++i, --ref_bases)
            {
                md.push_back(comp_base(*ref_bases));
            }
            md.push_back('$');

        }
        else if (pCRIter->type == INSERT)
        {
            // handle insertion
            md.push_back('^');
            md += boost::lexical_cast<std::string>(pCRIter->length);
            read_bases += pCRIter->length;
            md.push_back('$');

        }
        else if (is_segment_align_match(pCRIter->type))
        {
            // recreate the the match descriptor for this non-INDEL region
            uint32_t numMatchingBases = 0;
            for (uint32_t i = 0; i < pCRIter->length; ++i, --ref_bases, ++read_bases)
            {
                // handle circular genome
                if ((ref_bases < ref_begin) || (ref_bases > ref_end))
                {
                    md.push_back('N');
                    continue;
                }

                const char rcRefBase = comp_base(*ref_bases);

                if (rcRefBase != *read_bases)
                {

                    // write the number of preceding matching bases
                    if (numMatchingBases != 0)
                    {
                        md += boost::lexical_cast<std::string>(numMatchingBases);
                        numMatchingBases = 0;
                    }

                    // output the mismatched base
                    md.push_back(rcRefBase);

                }
                else ++numMatchingBases;
            }

            // write the number of trailing matching bases
            if (numMatchingBases != 0)
            {
                md += boost::lexical_cast<std::string>(numMatchingBases);
            }
        }
        else
        {
            // handle unsupported CIGAR operation
            foundUnsupportedCigar = true;
            break;
        }
    }

    if (foundUnsupportedCigar) md = "UNSUPPORTED";
}



void
apath_to_export_md(path_t& apath,
                   const char* ref_seq,
                   const char* ref_end,
                   const int32_t ref_pos,
                   const std::string& read_bases,
                   const bool is_fwd_strand,
                   std::string& md)
{
    md.clear();

    if (is_fwd_strand)
    {
        const char* pRead      = read_bases.c_str();
        const char* pReference = ref_seq + ref_pos - 1;
        fwd_apath_to_export_md(apath, ref_seq, pReference, ref_end, pRead, md);

    }
    else
    {
        uint32_t numRefBases = 0;
        for (const auto& ps : apath)
        {
            if (is_segment_align_match(ps.type) || (ps.type == DELETE))
            {
                numRefBases += ps.length;
            }
        }

        const char* pRead      = read_bases.c_str();
        const char* pReference = ref_seq + ref_pos + numRefBases - 2;
        rev_apath_to_export_md(apath, ref_seq, pReference, ref_end, pRead, md);
    }
}

}
