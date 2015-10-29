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

#include "vcf_streamer.hh"

#include "blt_util/blt_exception.hh"
#include "blt_util/log.hh"
#include "blt_util/seq_util.hh"

#include <cassert>
#include <cstdlib>
#include <sys/stat.h>

#include <iostream>
#include <set>
#include <string>



// return true only if all chromosomes in the bcf/vcf exist in the
// bam header
static
void
check_bam_bcf_header_compatability(
    const char* bcf_filename,
    const bcf_hdr_t* bcfh,
    const bam_hdr_t* bamh)
{
    assert(nullptr != bamh);
    assert(nullptr != bcfh);

    // build set of chrom labels from BAM:
    std::set<std::string> bamlabels;
    for (int32_t i(0); i<bamh->n_targets; ++i)
    {
        bamlabels.insert(std::string(bamh->target_name[i]));
    }
    int n_labels(0);

    const char** bcf_labels = bcf_hdr_seqnames(bcfh, &n_labels);

    for (int i(0); i<n_labels; ++i)
    {
        if (bamlabels.find(std::string(bcf_labels[i])) != bamlabels.end()) continue;
        log_os << "ERROR: Chromosome label '" << bcf_labels[i] << "' in BCF/VCF file '" << bcf_filename << "' does not exist in the BAM header\n";
        exit(EXIT_FAILURE);
    }

    free(bcf_labels);
}



vcf_streamer::
vcf_streamer(
    const char* filename,
    const char* region,
    const bam_hdr_t* bh) :
    hts_streamer(filename,region),
    _hdr(nullptr)
{
    //
    // note with the switch to samtools 1.X vcf/bcf still involve predominantly separate
    // apis -- no bcf support added here, but a shared function has been chosen where possible
    // ... for_instance hts_open/bcf_hdr_read should work with both vcf and bcf
    //

    _hdr = bcf_hdr_read(_hfp);
    if (nullptr == _hdr)
    {
        log_os << "ERROR: Failed to load header for VCF file: '" << filename << "'\n";
        exit(EXIT_FAILURE);
    }

    if (nullptr != bh)
    {
        check_bam_bcf_header_compatability(filename, _hdr, bh);
    }
}



vcf_streamer::
~vcf_streamer()
{
    if (nullptr != _hdr) bcf_hdr_destroy(_hdr);
}



bool
vcf_streamer::
next(
    const bool is_indel_only)
{
    if (_is_stream_end || (nullptr==_hfp) || (nullptr==_titr)) return false;

    while (true)
    {
        if (tbx_itr_next(_hfp, _tidx, _titr, &_kstr) < 0)
        {
            _is_stream_end=true;
        }
        else
        {
            _is_stream_end=(nullptr == _kstr.s);
        }
        _is_record_set=(! _is_stream_end);
        if (! _is_record_set) break;

        // filter out header for whole file access case:
        if (_kstr.s[0] == '#') continue;

        _record_no++;

        if (! _vcfrec.set(_kstr.s))
        {
            log_os << "ERROR: Can't parse vcf record: '" << _kstr.s << "'\n";
            exit(EXIT_FAILURE);
        }
        if (! _vcfrec.is_valid()) continue;
        if (is_indel_only && (! _vcfrec.is_indel())) continue;

        break; // found expected vcf record type
    }

    return _is_record_set;
}



void
vcf_streamer::
report_state(std::ostream& os) const
{
    const vcf_record* vcfp(get_record_ptr());

    os << "\tvcf_stream_label: " << name() << "\n";
    if (nullptr != vcfp)
    {
        os << "\tvcf_stream_record_no: " << record_no() << "\n"
           << "\tvcf_record: " << *(vcfp) << "\n";
    }
    else
    {
        os << "\tno vcf record currently set\n";
    }
}
