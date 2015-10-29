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
/// object which accepts as input bam (and formerly vcf/other chromosome ordered) files from multiple
/// samples and merges them in order
///
/// \author Chris Saunders
///


#pragma once


#include "blt_util/id_map.hh"
#include "htsapi/bam_streamer.hh"

#include <map>
#include <queue>
#include <utility>


namespace INPUT_TYPE
{
enum index_t { NONE, READ };
}

struct input_stream_hander;

/// all inputs to be merged are registered to
/// this object first
struct input_stream_data
{
    void
    register_reads(bam_streamer& bs,
                   const int sample_no = 0)
    {
        if (_reads.test_key(sample_no)) register_error("reads",sample_no);
        _reads.insert(sample_no,&bs);
    }

private:

    void
    register_error(const char* label,
                   const int sample_no) const;


/////////// data:
    friend struct input_stream_handler;
    typedef id_map<int,bam_streamer*> reads_t;

    reads_t _reads;
};



struct input_record_info
{
    input_record_info(const pos_t p = 0,
                      const INPUT_TYPE::index_t t = INPUT_TYPE::NONE,
                      const int i = 0,
                      const unsigned s = 0)
        :  pos(p), itype(t), sample_no(i), _order(s) {}

    // reverse logic implied by operator< such that the 'lower' values
    // we'd like to see first will come up on top of the
    // priority_queue
    //
    bool
    operator<(const input_record_info& rhs) const
    {
        if (pos > rhs.pos) return true;
        if (pos == rhs.pos)
        {
            if (itype < rhs.itype) return true;
            if (itype==rhs.itype)
            {
                if (sample_no > rhs.sample_no) return true;
                if (sample_no == rhs.sample_no)
                {
                    return (_order > rhs._order);
                }
            }
        }
        return false;
    }

    unsigned get_order() const
    {
        return _order;
    }

    pos_t pos;
    INPUT_TYPE::index_t itype;
    int sample_no;

private:
    friend struct input_stream_handler;

    // record the submission order:
    unsigned _order;
};



// streams multiple bam (and vcf) files to present the data
// in positional order (but with offsets for vcfs to
// run ahead of the bam reads)
//
struct input_stream_handler
{
    input_stream_handler(
        const input_stream_data& data);

    bool next();

    input_record_info
    get_current() const
    {
        return _current;
    }

    pos_t
    get_head_pos() const
    {
        return _head_pos;
    }

private:

    void
    push_next(const INPUT_TYPE::index_t itype,
              const int sample_no,
              const unsigned order);


///////////////////////////////// data:
    const input_stream_data _data;

    input_record_info _current;
    input_record_info _last;

    bool _is_end;

    bool _is_head_pos;
    pos_t _head_pos;

    std::priority_queue<input_record_info> _stream_queue;
};

