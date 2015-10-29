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

#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "htsapi/bam_header_util.hh"
#include "htsapi/bam_streamer.hh"

#include <cassert>
#include <cstdlib>

#include <fstream>
#include <iostream>
#include <sstream>



bam_streamer::
bam_streamer(
    const char* filename,
    const char* region)
    : _is_record_set(false),
      _hfp(nullptr),
      _hdr(nullptr),
      _hidx(nullptr),
      _hitr(nullptr),
      _record_no(0),
      _stream_name(filename),
      _is_region(false)
{
    assert(nullptr != filename);
    if ('\0' == *filename)
    {
        throw blt_exception("Can't initialize bam_streamer with empty filename\n");
    }

    _hfp = hts_open(filename, "rb");

    if (nullptr == _hfp)
    {
        std::ostringstream oss;
        oss << "Failed to open SAM/BAM/CRAM file for reading: '" << name() << "'";
        throw blt_exception(oss.str().c_str());
    }

    _hdr = sam_hdr_read(_hfp);

    if (nullptr == _hdr)
    {
        std::ostringstream oss;
        oss << "Failed to parse header from SAM/BAM/CRAM file: " << name();
        throw blt_exception(oss.str().c_str());
    }

    if (nullptr == region)
    {
        // read the whole BAM file:

        if (_hdr->n_targets)
        {
            // parse any contig name so that header->hash is created
            // ignore returned tid value, so doesn't matter if fake name
            // exists
            target_name_to_id("fake_name");
        }
        return;
    }

    // read a specific region of the bam file:
    set_new_region(region);
}



bam_streamer::
~bam_streamer()
{
    if (nullptr != _hitr) hts_itr_destroy(_hitr);
    if (nullptr != _hidx) hts_idx_destroy(_hidx);
    if (nullptr != _hdr) bam_hdr_destroy(_hdr);
    if (nullptr != _hfp)
    {
        const int retval = hts_close(_hfp);
        if (retval != 0)
        {
            std::ostringstream oss;
            oss << "Failed to close SAM/BAM/CRAM file: " << name();
            throw blt_exception(oss.str().c_str());
        }
    }
}



static
bool
fexists(const char* filename)
{
    std::ifstream ifile(filename);
    return (! ifile.fail());
}



static
bool
hasEnding(
    const std::string& fullString,
    const std::string& ending)
{
    if (fullString.length() < ending.length()) return false;
    return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
}



// load index if it hasn't been set already:
void
bam_streamer::
_load_index()
{
    /// TODO: Find out whether _hidx can be destroyed after the HTS
    /// iterator is created, in which case this could be a local
    /// variable. Until we know, _hidx should persist for the lifetime
    /// of _hiter
    if (nullptr != _hidx) return;

    std::string index_base(name());

    // hack to allow GATK/Picard bai name convention:
    if ((! fexists((index_base+".bai").c_str())) &&
        (! fexists((index_base+".csa").c_str())) &&
        (! fexists((index_base+".crai").c_str())))
    {
        static const std::string bamext(".bam");
        if (hasEnding(index_base,bamext))
        {
            index_base=index_base.substr(0,index_base.length()-bamext.length());
        }
    }

    _hidx = sam_index_load(_hfp, index_base.c_str());
    if (nullptr == _hidx)
    {
        std::ostringstream oss;
        oss << "BAM/CRAM index is not available for file: " << name();
        throw blt_exception(oss.str().c_str());
    }
}



void
bam_streamer::
set_new_region(const char* region)
{
    int32_t ref,beg,end;
    parse_bam_region_from_hdr(_hdr, region, ref, beg, end);

    try
    {
        set_new_region(ref,beg,end);
        _region=region;
    }
    catch (const std::exception& /*e*/)
    {
        log_os << "ERROR: exception while fetching BAM/CRAM region: '" << region
               << "' from file '" << name() << "'\n";
        throw;
    }
}



void
bam_streamer::
set_new_region(const int ref, const int beg, const int end)
{
    if (nullptr != _hitr) hts_itr_destroy(_hitr);

    _load_index();

    if (ref < 0)
    {
        std::ostringstream oss;
        oss << "Invalid region (contig index: " << ref << ") specified for BAM/CRAM file: " << name();
        throw blt_exception(oss.str().c_str());
    }

    _hitr = sam_itr_queryi(_hidx,ref,beg,end);
    if (_hitr == nullptr)
    {
        std::ostringstream oss;
        oss << "Failed to fetch region: #" << ref << ":" << beg << "-" << end << " specified for BAM/CRAM file: " << name();
        throw blt_exception(oss.str().c_str());
    }
    _is_region = true;
    _region.clear();

    _is_record_set = false;
    _record_no = 0;
}


bool
bam_streamer::
next()
{
    if (nullptr == _hfp) return false;

    int ret;
    if (nullptr == _hitr)
    {
        ret = sam_read1(_hfp,_hdr, _brec._bp);
    }
    else
    {
        ret = sam_itr_next(_hfp, _hitr, _brec._bp);
    }

    _is_record_set=(ret >= 0);
    if (_is_record_set) _record_no++;

    return _is_record_set;
}



const char*
bam_streamer::
target_id_to_name(const int32_t tid) const
{
    // assert(tid < _bfp->header->n_targets);
    if (tid<0)
    {
        static const char unmapped[] = "*";
        return unmapped;
    }
    return _hdr->target_name[tid];
}



int32_t
bam_streamer::
target_name_to_id(const char* seq_name) const
{
    return bam_name2id(_hdr,seq_name);
}



void
bam_streamer::
report_state(std::ostream& os) const
{
    const bam_record* bamp(get_record_ptr());

    os << "\tbam_stream_label: " << name() << "\n";
    if (_is_region && (! _region.empty()))
    {
        os << "\tbam_stream_selected_region: " << _region << "\n";
    }
    if (nullptr != bamp)
    {
        os << "\tbam_stream_record_no: " << record_no() << "\n";
        os << "\tbam_record QNAME/read_number: " << bamp->qname() << "/" << bamp->read_no() << "\n";
        const char* chrom_name(target_id_to_name(bamp->target_id()));
        os << "\tbam record RNAME: " << chrom_name << "\n";
        os << "\tbam record POS: " << bamp->pos() << "\n";

    }
    else
    {
        os << "\tno bam record currently set\n";
    }
}
