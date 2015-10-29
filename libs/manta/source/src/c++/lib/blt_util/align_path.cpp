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


#include "blt_util/align_path.hh"
#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "blt_util/parse_util.hh"
#include "blt_util/seq_util.hh"

#include "boost/foreach.hpp"
#include "boost/lexical_cast.hpp"

#include <cassert>

#include <algorithm>
#include <iostream>
#include <sstream>



static
void
unknown_cigar_error(const char* const cigar,
                    const char* const cptr)
{
    std::ostringstream oss;
    oss << "ERROR: can't parse cigar string: " << cigar << "\n"
        << "\tunexpected character: '" << *cptr << "' at position: " << (cptr-cigar+1) << "\n";
    throw blt_exception(oss.str().c_str());
}



namespace ALIGNPATH
{


void
apath_to_cigar(const path_t& apath,
               std::string& cigar)
{
    cigar.clear();
    for (const path_segment& ps : apath)
    {
        cigar += boost::lexical_cast<std::string>(ps.length);
        cigar.push_back(segment_type_to_cigar_code(ps.type));
    }
}



std::ostream&
operator<<(std::ostream& os, const path_t& apath)
{
    for (const path_segment& ps : apath)
    {
        os << ps.length << segment_type_to_cigar_code(ps.type);
    }
    return os;
}



void
cigar_to_apath(const char* cigar,
               path_t& apath)
{
    using illumina::blt_util::parse_unsigned;

    assert(NULL != cigar);

    apath.clear();

    path_segment lps;
    const char* cptr(cigar);
    while (*cptr)
    {
        path_segment ps;
        // expect sequences of digits and cigar codes:
        if (! isdigit(*cptr)) unknown_cigar_error(cigar,cptr);
        ps.length = parse_unsigned(cptr);
        ps.type = cigar_code_to_segment_type(*cptr);
        if (ps.type == NONE) unknown_cigar_error(cigar,cptr);
        cptr++;
        if ((ps.type == PAD) || (ps.length == 0)) continue;

        if (ps.type != lps.type)
        {
            if (lps.type != NONE) apath.push_back(lps);
            lps = ps;
        }
        else
        {
            lps.length += ps.length;
        }
    }

    if (lps.type != NONE) apath.push_back(lps);
}



unsigned
apath_read_length(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if (! is_segment_type_read_length(ps.type)) continue;
        val += ps.length;
    }
    return val;
}

unsigned
apath_matched_length(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if (! is_segment_align_match(ps.type)) continue;
        val += ps.length;
    }
    return val;
}

unsigned
apath_ref_length(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if (! is_segment_type_ref_length(ps.type)) continue;
        val += ps.length;
    }
    return val;
}



static
inline
bool
is_segment_type_unaligned_read_edge(const align_t id)
{
    switch (id)
    {
    case INSERT    :
    case HARD_CLIP :
    case SOFT_CLIP :
        return true;
    default        :
        return false;
    }
}



unsigned
apath_read_lead_size(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if (! is_segment_type_unaligned_read_edge(ps.type)) return val;
        if (is_segment_type_read_length(ps.type)) val += ps.length;
    }
    return val;
}



unsigned
apath_read_trail_size(const path_t& apath)
{
    unsigned val(0);
    BOOST_REVERSE_FOREACH(const path_segment& ps, apath)
    {
        if (! is_segment_type_unaligned_read_edge(ps.type)) return val;
        if (is_segment_type_read_length(ps.type)) val += ps.length;
    }
    return val;
}



unsigned
apath_soft_clip_lead_size(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if       (HARD_CLIP == ps.type)
        {
            // do nothing:
        }
        else if (SOFT_CLIP == ps.type)
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}



unsigned
apath_soft_clip_trail_size(const path_t& apath)
{
    unsigned val(0);
    BOOST_REVERSE_FOREACH(const path_segment& ps, apath)
    {
        if       (HARD_CLIP == ps.type)
        {
            // do nothing:
        }
        else if (SOFT_CLIP == ps.type)
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}

unsigned
apath_clip_lead_size(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if ((HARD_CLIP == ps.type) || (SOFT_CLIP == ps.type))
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}

unsigned
apath_clip_trail_size(const path_t& apath)
{
    unsigned val(0);
    BOOST_REVERSE_FOREACH(const path_segment& ps, apath)
    {
        if  ((HARD_CLIP == ps.type) || (SOFT_CLIP == ps.type))
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}

unsigned
apath_insert_lead_size(const path_t& apath)
{
    unsigned val(0);
    for (const path_segment& ps : apath)
    {
        if ((HARD_CLIP == ps.type) || (SOFT_CLIP == ps.type))
        {
            // do nothing:
        }
        else if (INSERT == ps.type)
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}



unsigned
apath_insert_trail_size(const path_t& apath)
{
    unsigned val(0);
    BOOST_REVERSE_FOREACH(const path_segment& ps, apath)
    {
        if ((HARD_CLIP == ps.type) || (SOFT_CLIP == ps.type))
        {
            // do nothing:
        }
        else if (INSERT == ps.type)
        {
            val += ps.length;
        }
        else
        {
            break;
        }
    }
    return val;
}



void
apath_limit_ref_length(
    const unsigned target_ref_length,
    path_t& apath)
{
    unsigned ref_length(0);

    const unsigned as(apath.size());
    for (unsigned i(0); i<as; ++i)
    {
        path_segment& ps(apath[i]);
        if (! is_segment_type_ref_length(ps.type)) continue;
        ref_length += ps.length;

        if (ref_length < target_ref_length) continue;

        if (ref_length > target_ref_length)
        {
            const unsigned extra(ref_length - target_ref_length);
            assert(ps.length > extra);
            ps.length -= extra;
        }
        apath.resize(i+1);
        break;
    }
}


void
apath_limit_read_length(
    const unsigned target_read_start,
    const unsigned target_read_end,
    path_t& apath)
{
    bool isStartSet(false);

    unsigned read_length(0);
    const unsigned as(apath.size());
    unsigned startSegment(0);
    unsigned endSegment(as);
    for (unsigned i(0); i<as; ++i)
    {
        path_segment& ps(apath[i]);
        if (! is_segment_type_read_length(ps.type)) continue;
        read_length += ps.length;

        if ((! isStartSet) && (read_length > target_read_start))
        {
            {
                const unsigned extra(ps.length - (read_length - target_read_start));
                assert(ps.length > extra);
                ps.length -= extra;
            }
            startSegment=i;
            isStartSet=true;
        }

        if (read_length >= target_read_end)
        {
            if (read_length > target_read_end)
            {
                const unsigned extra(read_length - target_read_end);
                assert(ps.length > extra);
                ps.length -= extra;
            }
            endSegment=i+1;
            break;
        }
    }
    apath = path_t(apath.begin()+startSegment,apath.begin()+endSegment);
}


void
apath_append(
    path_t& apath,
    const align_t seg_type,
    const unsigned length)
{
    if (apath.size() && apath.back().type == seg_type)
    {
        apath.back().length += length;
    }
    else
    {
        apath.emplace_back(seg_type,length);
    }
}



void
apath_clip_clipper(path_t& apath,
                   unsigned& hc_lead,
                   unsigned& hc_trail,
                   unsigned& sc_lead,
                   unsigned& sc_trail)
{
    hc_lead=0;
    hc_trail=0;
    sc_lead=0;
    sc_trail=0;

    bool is_lead(true);
    path_t apath2;
    for (const path_segment& ps : apath)
    {
        if       (HARD_CLIP == ps.type)
        {
            if (is_lead)
            {
                hc_lead += ps.length;
            }
            else
            {
                hc_trail += ps.length;
            }
        }
        else if (SOFT_CLIP == ps.type)
        {
            if (is_lead)
            {
                sc_lead += ps.length;
            }
            else
            {
                sc_trail += ps.length;
            }
        }
        else
        {
            is_lead=false;
            assert(0==hc_trail);
            assert(0==sc_trail);
            apath2.push_back(ps);
        }
    }
    apath=apath2;
}



void
apath_clip_adder(path_t& apath,
                 const unsigned hc_lead,
                 const unsigned hc_trail,
                 const unsigned sc_lead,
                 const unsigned sc_trail)
{
    path_t apath2;
    path_segment ps;
    if (hc_lead>0)
    {
        ps.type = HARD_CLIP;
        ps.length = hc_lead;
        apath2.push_back(ps);
    }
    if (sc_lead>0)
    {
        ps.type = SOFT_CLIP;
        ps.length = sc_lead;
        apath2.push_back(ps);
    }
    apath2.insert(apath2.end(),apath.begin(),apath.end());
    if (sc_trail>0)
    {
        ps.type = SOFT_CLIP;
        ps.length = sc_trail;
        apath2.push_back(ps);
    }
    if (hc_trail>0)
    {
        ps.type = HARD_CLIP;
        ps.length = hc_trail;
        apath2.push_back(ps);
    }
    apath=apath2;
}



// 1. remove zero-length segments
// 2. remove pads
// 3. condense repeated segment types
// 4. reduce adjacent insertion/deletion tags to a single pair
// 5. replace NDN pattern with single SKIP segment
//
// return true if path has been altered
//
bool
apath_cleaner(path_t& apath)
{
    bool is_cleaned(false);
    const unsigned as(apath.size());
    unsigned insertIndex(as);
    unsigned deleteIndex(as);
    unsigned otherIndex(as);
    for (unsigned i(0); i<as; ++i)
    {
        path_segment& ps(apath[i]);
        if       (ps.length == 0)
        {
            is_cleaned = true;
        }
        else if (ps.type == PAD)
        {
            ps.length = 0;
            is_cleaned = true;
        }
        else if (ps.type == INSERT)
        {
            if (insertIndex < as)
            {
                apath[insertIndex].length += ps.length;
                ps.length = 0;
                is_cleaned = true;
            }
            else
            {
                insertIndex = i;
            }
        }
        else if (ps.type == DELETE)
        {
            if (deleteIndex < as)
            {
                apath[deleteIndex].length += ps.length;
                ps.length = 0;
                is_cleaned = true;
            }
            else
            {
                deleteIndex = i;
            }
        }
        else
        {
            if ((insertIndex<as) || (deleteIndex<as))
            {
                insertIndex = as;
                deleteIndex = as;
                otherIndex = as;
            }
            if ((otherIndex < as) && (apath[otherIndex].type == ps.type))
            {
                apath[otherIndex].length += ps.length;
                ps.length = 0;
                is_cleaned = true;
            }
            else
            {
                otherIndex = i;
            }
        }
    }

    // convert NDN to single N:
    for (unsigned i(0); i<as; ++i)
    {
        path_segment& ps(apath[i]);
        if (ps.type == SKIP)
        {
            if ( (i+2)<as)
            {
                if ((apath[i+1].type == DELETE) && (apath[i+2].type == SKIP))
                {
                    for (unsigned j(1); j<3; ++j)
                    {
                        ps.length += apath[i+j].length;
                        apath[i+j].length = 0;
                    }
                    is_cleaned = true;
                }
            }
        }
    }

    if (is_cleaned)
    {
        path_t apath2;
        for (const path_segment& ps : apath)
        {
            if (ps.length == 0) continue;
            apath2.push_back(ps);
        }
        apath = apath2;
    }
    return is_cleaned;
}



void
apath_clean_seqmatch(path_t& apath)
{
    path_t apath2;
    bool is_match(false);
    for (const path_segment& ps : apath)
    {
        if (is_segment_align_match(ps.type))
        {
            if (is_match)
            {
                apath2.back().length += ps.length;
            }
            else
            {
                apath2.emplace_back(MATCH,ps.length);
            }
            is_match=true;
        }
        else
        {
            apath2.push_back(ps);
            is_match=false;
        }
    }

    apath = apath2;
}

#if 0
std::pair<unsigned,unsigned>
get_nonclip_end_segments(const path_t& apath)
{
    const unsigned as(apath.size());
    std::pair<unsigned,unsigned> res(as,as);
    bool is_first_nonclip(false);
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);
        if (! (ps.type == SOFT_CLIP ||
               ps.type == HARD_CLIP))
        {
            if (! is_first_nonclip)
            {
                res.first=i;
                is_first_nonclip=true;
            }
            res.second=i;
        }
    }
    return res;
}
#endif


pos_range
get_nonclip_range(const path_t& apath)
{
    pos_range pr;
    unsigned read_offset(0);
    for (const path_segment& ps : apath)
    {
        const bool is_rt(is_segment_type_read_length(ps.type));
        if (! (ps.type == SOFT_CLIP ||
               ps.type == HARD_CLIP))
        {
            if (! pr.is_begin_pos)
            {
                pr.set_begin_pos(read_offset);
            }
            pr.set_end_pos(read_offset + (is_rt ? ps.length : 0));
        }
        if (is_rt) read_offset+=ps.length;
    }
    return pr;
}



std::pair<unsigned,unsigned>
get_match_edge_segments(const path_t& apath)
{
    const unsigned as(apath.size());
    std::pair<unsigned,unsigned> res(as,as);
    bool is_first_match(false);
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);
        if (is_segment_align_match(ps.type))
        {
            if (! is_first_match) res.first=i;
            is_first_match=true;
            res.second=i;
        }
    }
    return res;
}



unsigned
apath_exon_count(const path_t& apath)
{
    unsigned val(1);
    for (const auto& ps : apath)
    {
        if (ps.type==SKIP) val++;
    }
    return val;
}



bool
is_clipped(const path_t& apath)
{
    const unsigned as(apath.size());
    if (as==0) return false;
    if ((apath[0].type == SOFT_CLIP) || (apath[0].type == HARD_CLIP)) return true;
    if (as>1)
    {
        if ((apath[as-1].type == SOFT_CLIP) || (apath[as-1].type == HARD_CLIP)) return true;
    }
    return false;
}

bool
is_clipped_front(const path_t& apath)
{
    const unsigned as(apath.size());
    if (as==0) return false;
    if ((apath[0].type == SOFT_CLIP) || (apath[0].type == HARD_CLIP)) return true;
    return false;
}



unsigned
get_clip_len(const path_t& apath)
{
    const unsigned as(apath.size());
    if (as==0) return 0;
    if ((apath[0].type == SOFT_CLIP) || (apath[0].type == HARD_CLIP))
    {
        return apath[0].length;
    }
    if (as>1)
    {
        if ((apath[as-1].type == SOFT_CLIP) || (apath[as-1].type == HARD_CLIP))
        {
            return apath[as-1].length;
        }
    }
    return 0;
}



bool
is_soft_clipped(const path_t& apath)
{
    for (const path_segment& ps : apath)
    {
        if (SOFT_CLIP == ps.type) return true;
    }
    return false;
}



bool
is_edge_readref_len_segment(const path_t& apath)
{
    const unsigned as(apath.size());
    if (as==0) return false;

    const std::pair<unsigned,unsigned> ends(get_match_edge_segments(apath));

    // at this point we assume the alignment has been sanity checked for legal clipping,
    // where hard-clip is only on the outside, next soft-clipping, then anything else...
    //
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);

        const bool is_edge_segment((i<ends.first) || (i>ends.second));
        const bool is_clip_type(ps.type==INSERT || ps.type==DELETE || ps.type==SKIP || ps.type==SOFT_CLIP);
        if (is_edge_segment && is_clip_type) return true;
    }
    return false;
}



bool
is_seq_swap(const path_t& apath)
{
    const unsigned as(apath.size());
    for (unsigned i(0); (i+1)<as; ++i)
    {
        if (is_segment_type_indel(apath[i].type) &&
            is_segment_type_indel(apath[i+1].type))
        {
            return true;
        }
    }
    return false;
}



bool
is_segment_swap_start(const path_t& apath,
                      unsigned i)
{
    using namespace ALIGNPATH;

    bool is_insert(false);
    bool is_delete(false);

    const unsigned as(apath.size());
    for (; i<as; ++i)
    {
        if     (apath[i].type == INSERT)
        {
            is_insert=true;
        }
        else if (apath[i].type == DELETE)
        {
            is_delete=true;
        }
        else
        {
            break;
        }
    }

    return (is_insert && is_delete);
}



bool
is_apath_floating(const path_t& apath)
{
    for (const path_segment& ps : apath)
    {
        if (is_segment_align_match(ps.type)) return false;
    }
    return true;
}


std::string
get_apath_invalid_reason(const path_t& apath,
                         const unsigned seq_length)
{
    const ALIGN_ISSUE::issue_t ai(get_apath_invalid_type(apath,seq_length));

    if (ALIGN_ISSUE::LENGTH == ai)
    {
        std::ostringstream oss;
        oss << "alignment length (" << apath_read_length(apath) << ") does not match read length (" << seq_length << ")";
        return oss.str();
    }

    return std::string(ALIGN_ISSUE::description(ai));
}



ALIGN_ISSUE::issue_t
get_apath_invalid_type(const path_t& apath,
                       const unsigned seq_length)
{
    bool is_match(false);
    align_t last_type(NONE);
    const unsigned as(apath.size());
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[i]);

        if (ps.type==NONE) return ALIGN_ISSUE::UNKNOWN_SEGMENT;
        if ((i!=0) && ps.type==last_type) return ALIGN_ISSUE::REPEATED_SEGMENT;

        if (! is_match)
        {
            if (ps.type==SKIP) return ALIGN_ISSUE::EDGE_SKIP;
        }

        if (ps.type==HARD_CLIP)
        {
            if (! ((i==0) || ((i+1)==as))) return ALIGN_ISSUE::CLIPPING;
        }

        if (ps.type==SOFT_CLIP)
        {
            if (! ((i==0) || ((i+1)==as)))
            {
                if (i==1)
                {
                    if (as==3)
                    {
                        if ((apath[0].type != HARD_CLIP) && (apath[i+1].type != HARD_CLIP)) return ALIGN_ISSUE::CLIPPING;
                    }
                    else
                    {
                        if (apath[0].type != HARD_CLIP) return ALIGN_ISSUE::CLIPPING;
                    }
                }
                else if ((i+2)==as)
                {
                    if (apath[i+1].type != HARD_CLIP) return ALIGN_ISSUE::CLIPPING;
                }
                else
                {
                    return ALIGN_ISSUE::CLIPPING;
                }
            }
        }

        if ((! is_match) && (is_segment_align_match(ps.type))) is_match=true;

        last_type=ps.type;
    }

    if (! is_match) return ALIGN_ISSUE::FLOATING;

    // run in reverse to finish checking condition (2a):
    for (unsigned i(0); i<as; ++i)
    {
        const path_segment& ps(apath[as-(i+1)]);
        if (is_segment_align_match(ps.type)) break;
        //if(ps.type==DELETE) return ALIGN_ISSUE::EDGE_DELETE;
        if (ps.type==SKIP) return ALIGN_ISSUE::EDGE_SKIP;
    }

    if (seq_length != apath_read_length(apath)) return ALIGN_ISSUE::LENGTH;

    return ALIGN_ISSUE::NONE;
}



// Unlike the above function which tests for invalid alignment paths,
// this function test for valid alignment methods which starling
// simply cannot handle
//
bool
is_apath_starling_invalid(const path_t& apath)
{
    for (const path_segment& ps : apath)
    {
        if (ps.type==PAD) return true;
    }
    return false;
}


}  // namespace ALIGNPATH
