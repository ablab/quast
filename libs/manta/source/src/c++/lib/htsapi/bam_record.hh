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

#pragma once

#include "bam_util.hh"
#include "bam_seq.hh"

#include <iosfwd>


struct bam_record
{
    bam_record()
        : _bp(bam_init1()) {}

    ~bam_record()
    {
        freeBam();
    }

    bam_record(const bam_record& br)
        : _bp(br.empty() ? bam_init1() : bam_dup1(br._bp)) {}

    const bam_record&
    operator=(const bam_record& br)
    {
        if (this == &br) return (*this);

        if (empty())
        {
            if (! br.empty())
            {
                freeBam();
                _bp=bam_dup1(br._bp);
            }
            // else empty->empty : do nothing...
        }
        else
        {
            if (! br.empty())
            {
                bam_copy1(_bp,br._bp);
            }
            else
            {
                freeBam();
                _bp=bam_init1();
            }
        }
        return (*this);
    }

private:
    const bam_record&
    operator==(const bam_record& rhs);
public:

    const char*
    qname() const
    {
        return reinterpret_cast<const char*>(_bp->data);
    }

    void
    set_qname(const char* name)
    {
        edit_bam_qname(name,*_bp);
    }

    bool is_paired() const
    {
        return ((_bp->core.flag & BAM_FLAG::PAIRED) != 0);
    }
    bool is_proper_pair() const
    {
        return ((_bp->core.flag & BAM_FLAG::PROPER_PAIR) != 0);
    }
    bool is_unmapped() const
    {
        return ((_bp->core.flag & BAM_FLAG::UNMAPPED) != 0);
    }
    bool is_mate_unmapped() const
    {
        return ((_bp->core.flag & BAM_FLAG::MATE_UNMAPPED) != 0);
    }
    bool is_fwd_strand() const
    {
        return (! ((_bp->core.flag & BAM_FLAG::STRAND) != 0));
    }
    bool is_mate_fwd_strand() const
    {
        return (! ((_bp->core.flag & BAM_FLAG::MATE_STRAND) != 0));
    }
    bool is_dup() const
    {
        return ((_bp->core.flag & BAM_FLAG::DUPLICATE) != 0);
    }
    bool is_filter() const
    {
        return ((_bp->core.flag & BAM_FLAG::FILTER) != 0);
    }
    bool is_first() const
    {
        return ((_bp->core.flag & BAM_FLAG::FIRST_READ) != 0);
    }
    bool is_second() const
    {
        return ((_bp->core.flag & BAM_FLAG::SECOND_READ) != 0);
    }
    bool is_secondary() const
    {
        return ((_bp->core.flag & BAM_FLAG::SECONDARY) != 0);
    }
    bool is_supplement() const
    {
        return ((_bp->core.flag & BAM_FLAG::SUPPLEMENT) != 0);
    }

    void toggle_is_paired()
    {
        _bp->core.flag ^= BAM_FLAG::PAIRED;
    }
    void toggle_is_unmapped()
    {
        _bp->core.flag ^= BAM_FLAG::UNMAPPED;
    }
    void toggle_is_mate_unmapped()
    {
        _bp->core.flag ^= BAM_FLAG::MATE_UNMAPPED;
    }
    void toggle_is_fwd_strand()
    {
        _bp->core.flag ^= BAM_FLAG::STRAND;
    }
    void toggle_is_mate_fwd_strand()
    {
        _bp->core.flag ^= BAM_FLAG::MATE_STRAND;
    }
    void toggle_is_first()
    {
        _bp->core.flag ^= BAM_FLAG::FIRST_READ;
    }
    void toggle_is_second()
    {
        _bp->core.flag ^= BAM_FLAG::SECOND_READ;
    }
    void toggle_is_secondary()
    {
        _bp->core.flag ^= BAM_FLAG::SECONDARY;
    }

    int read_no() const
    {
        return  ( (is_second() && (! is_first())) ? 2 : 1 );
    }

    int target_id() const
    {
        return _bp->core.tid;
    }

    int mate_target_id() const
    {
        return _bp->core.mtid;
    }

    bool is_chimeric() const
    {
        return ((target_id()!=mate_target_id()) && (target_id()>=0) && (mate_target_id()>=0));
    }

    int pos() const
    {
        return (_bp->core.pos+1);
    }

    int mate_pos() const
    {
        return (_bp->core.mpos+1);
    }

    uint8_t map_qual() const
    {
        return _bp->core.qual;
    }

    /// does read contain the new SA split-read tag?
    bool
    isSASplit() const
    {
        static const char satag[] = {'S','A'};
        return (nullptr != get_string_tag(satag));
    }

    /// test if the read is supplemental by more liberal criteria
    ///
    /// this generalizes the bam flag for supplemental to work
    /// correctly with the workaround typified by the bwamem '-M'
    /// option, which uses the 'secondary' flag
    bool
    isNonStrictSupplement() const
    {
        if (is_supplement()) return true;
        if (! is_secondary()) return false;
        return isSASplit();
    }


    /// return single read mapping score if it exists,
    /// else return MAPQ:
    unsigned se_map_qual() const
    {
        static const char smtag[] = {'S','M'};
        return alt_map_qual(smtag);
    }

#if 0
    // attempt to recover the ELAND paired-end mapping score if it
    // exists, else return MAPQ:
    unsigned pe_map_qual() const
    {
        static const char astag[] = {'A','S'};
        return alt_map_qual(astag);
    }
#endif

    int32_t template_size() const
    {
        return _bp->core.isize;
    }


    /// Test if SM and AM fields both exist and are equal to zero. Any
    /// other result returns false:
    ///
    bool
    is_unanchored() const
    {
        if (! is_paired()) return false;
        static const char amtag[] = {'A','M'};
        uint8_t* am_ptr(bam_aux_get(_bp,amtag));
        if (NULL == am_ptr)  return false;
        static const char smtag[] = {'S','M'};
        uint8_t* sm_ptr(bam_aux_get(_bp,smtag));
        if (NULL == sm_ptr)  return false;
        return (is_int_code(am_ptr[0]) &&
                is_int_code(sm_ptr[0]) &&
                (0 == bam_aux2i(am_ptr)) &&
                (0 == bam_aux2i(sm_ptr)));
    }

    const uint32_t* raw_cigar() const
    {
        return bam_get_cigar(_bp);
    }
    unsigned n_cigar() const
    {
        return _bp->core.n_cigar;
    }

    unsigned read_size() const
    {
        return _bp->core.l_qseq;
    }

    bam_seq get_bam_read() const
    {
        return bam_seq(bam_get_seq(_bp),read_size());
    }

    /// get string AUX field, return NULL if field is not found, or field is not a string
    ///
    /// \param[in] tag AUX field tag. This is a char array of length two, null term is not required
    ///
    /// example tag: static const char smtag[] = {'S','M'};
    ///
    const char* get_string_tag(const char* tag) const;

    bool get_num_tag(const char* tag, int32_t& num) const;

    const uint8_t* qual() const
    {
        return bam_get_qual(_bp);
    }

    void
    set_target_id(int32_t tid)
    {
        if (tid<-1) tid=-1;
        _bp->core.tid=tid;
    }

    // read should be null terminated, qual should already have offset removed:
    //
    void
    set_readqual(const char* read,
                 const uint8_t* init_qual)
    {
        edit_bam_read_and_quality(read,init_qual,*_bp);
    }

    bam1_t*
    get_data()
    {
        return _bp;
    }

    const bam1_t*
    get_data() const
    {
        return _bp;
    }

    bool
    empty() const
    {
        assert(NULL != _bp);
        return (_bp->l_data == 0);
    }

private:
    friend struct bam_streamer;

    unsigned alt_map_qual(const char* tag) const;

    static
    bool
    is_int_code(char c)
    {
        switch (c)
        {
        case 'c' :
        case 's' :
        case 'i' :
        case 'C' :
        case 'S' :
        case 'I' :
            return true;
        default  :
            return false;
        }
    }

    void
    freeBam()
    {
        if (NULL != _bp)
        {
            if (NULL != _bp->data) free(_bp->data);
            free(_bp);
        }
    }

    bam1_t* _bp;
};


std::ostream&
operator<<(std::ostream& os, const bam_record& br);

