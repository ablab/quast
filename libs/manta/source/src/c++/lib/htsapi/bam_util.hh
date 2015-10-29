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

/// \brief bam record manipulation functions
///

#pragma once

#include "blt_util/thirdparty_push.h"

extern "C" {
#include <unistd.h> // this simplifies zlib on windows
#define __STDC_LIMIT_MACROS
#include "htslib/bgzf.h"
#include "htslib/sam.h"
}

#include "blt_util/thirdparty_pop.h"

/// pull two remaining functions in from samtools API:

/*!
  @abstract    Calculate the minimum bin that contains a region [beg,end).
  @param  beg  start of the region, 0-based
  @param  end  end of the region, 0-based
  @return      bin
 */
static inline int bam_reg2bin(uint32_t beg, uint32_t end)
{
    return hts_reg2bin(beg, end, 14, 5);
}

/*!
  @abstract Calculate the rightmost coordinate of an alignment on the
  reference genome.

  @param  c      pointer to the bam1_core_t structure
  @param  cigar  the corresponding CIGAR array (from bam1_t::cigar)
  @return        the rightmost coordinate, 0-based
*/
static inline uint32_t bam_calend(const bam1_core_t* c, const uint32_t* cigar)
{
    return c->pos + (c->n_cigar? bam_cigar2rlen(c->n_cigar, cigar) : 1);
}


namespace BAM_FLAG
{
enum index_t
{
    PAIRED        = 0x001,
    PROPER_PAIR   = 0x002,
    UNMAPPED      = 0x004,
    MATE_UNMAPPED = 0x008,
    STRAND        = 0x010,
    MATE_STRAND   = 0x020,
    FIRST_READ    = 0x040,
    SECOND_READ   = 0x080,
    SECONDARY     = 0x100,
    FILTER        = 0x200,
    DUPLICATE     = 0x400,
    SUPPLEMENT    = 0x800
};
}


/// insert new qname
///
void
edit_bam_qname(const char* name,
               bam1_t& br);

/// Read length is taken from the read string. Assumes offset has
/// already been removed from qual
void
edit_bam_read_and_quality(const char* read,
                          const uint8_t* qual,
                          bam1_t& br);

/// remove all copies of optional field "tag"
///
void
nuke_bam_aux_field(bam1_t& br,
                   const char* tag);


/// store an unsigned int to optional field "tag", optimize storage
/// based on size of x
void
bam_aux_append_unsigned(bam1_t& br,
                        const char* tag,
                        uint32_t x);

/// change the size of a subsegment of the bam data, 'end' identifies
/// the byte offset of the end of the segment and 'delta' is the change
/// to the segment size
///
void
change_bam_data_segment_len(const int end,
                            const int delta,
                            bam1_t& br);


/// Update bam record bin value -- call after updating pos and/or
/// cigar fields.
///
inline
void
bam_update_bin(bam1_t& br)
{
    // set bin value:
    //
    // Test for position rather than looking at the unmapped flag
    // because we want to index shadow reads.
    //
    bam1_core_t& brc(br.core);
    if (brc.pos>=0)
    {
        if (brc.n_cigar!=0)
        {
            // normal case:
            brc.bin = bam_reg2bin(brc.pos, bam_calend(&brc, bam_get_cigar(&br)));
        }
        else
        {
            // shadow case: (match logic from samtools)
            brc.bin = bam_reg2bin(brc.pos, brc.pos+1);
        }
    }
    else
    {
        // unmapped, non-shadow reads:
        brc.bin = 0;
    }
}
