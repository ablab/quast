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
#include "blt_util/input_stream_handler.hh"

#include <cstdlib>

#include <iostream>
#include <sstream>



static
const char*
input_type_label(const INPUT_TYPE::index_t i)
{
    using namespace INPUT_TYPE;

    switch (i)
    {
    case NONE   :
        return "NONE";
    case READ   :
        return "READ";
    default :
        log_os << "ERROR: unrecognized event type.\n";
        exit(EXIT_FAILURE);
    }
}



void
input_stream_data::
register_error(const char* label,
               const int sample_no) const
{
    log_os << "ERROR: attempting to register " << label
           << " with sample number: " << sample_no
           << " more than once\n";
    exit(EXIT_FAILURE);
}




input_stream_handler::
input_stream_handler(
    const input_stream_data& data)
    : _data(data)
    , _is_end(false)
    , _is_head_pos(false)
    , _head_pos(0)
{
    // initial loading for _stream_queue:
    const unsigned rs(_data._reads.size());
    for (unsigned i(0); i<rs; ++i)
    {
        push_next(INPUT_TYPE::READ,_data._reads.get_key(i),i);
    }
}



bool
input_stream_handler::
next()
{
    if (_is_end) return false;

    while (true)
    {
        if (_current.itype != INPUT_TYPE::NONE)
        {
            // reload stream_queue with current type and sample_no;
            push_next(_current.itype,_current.sample_no,_current._order);
            _last=_current;
        }

        if (_stream_queue.empty())
        {
            _current=input_record_info();
            _is_end=true;
            return false;
        }
        bool is_usable(true);
        _current=_stream_queue.top();
        _stream_queue.pop();

        if (_is_head_pos &&
            (_current.pos < _head_pos))
        {
            if (_current.itype == INPUT_TYPE::READ)
            {
                std::ostringstream oss;
                oss << "ERROR: unexpected read order:\n"
                    << "\tInput-record with pos/type/sample_no: "
                    << (_current.pos+1) << "/" << input_type_label(_current.itype) << "/" << _current.sample_no
                    << " follows pos/type/sample_no: "
                    << (_last.pos+1) << "/" << input_type_label(_last.itype) << "/" << _current.sample_no << "\n";
                throw blt_exception(oss.str().c_str());
            }
            else
            {
                std::ostringstream oss;
                oss << "ERROR: unexpected input type: " << _current.itype << "\n";
                throw blt_exception(oss.str().c_str());
            }
        }

        if (_is_head_pos)
        {
            _head_pos=std::max(_head_pos,_current.pos);
        }
        else
        {
            _is_head_pos=true;
            _head_pos=_current.pos;
        }

        if (is_usable) break;
    }
    return true;
}



static
void
get_next_read_pos(bool& is_next_read,
                  pos_t& next_read_pos,
                  bam_streamer& read_stream)
{

    is_next_read=read_stream.next();
    if (is_next_read)
    {
        const bam_record& read_rec(*(read_stream.get_record_ptr()));
        next_read_pos=(read_rec.pos()-1);
    }
    else
    {
        next_read_pos=0;
    }
}



void
input_stream_handler::
push_next(const INPUT_TYPE::index_t itype,
          const int sample_no,
          const unsigned order)
{

    bool is_next(false);
    pos_t next_pos;
    if       (itype == INPUT_TYPE::READ)
    {
        bam_streamer& read_stream(*(_data._reads.get_value(order)));
        get_next_read_pos(is_next,next_pos,read_stream);
    }
    else
    {
        std::ostringstream oss;
        oss << "ERROR: unexpected input type: " << itype << "\n";
        throw blt_exception(oss.str().c_str());
    }
    if (! is_next) return;
    _stream_queue.push(input_record_info(next_pos,itype,sample_no,order));
}

