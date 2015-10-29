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
#include "blt_util/stage_manager.hh"

#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <sstream>



void
stage_data::
add_stage(const int id,
          const int parent_id,
          const unsigned parent_distance,
          const bool is_parent)
{
    unsigned pos(0);
    if (is_parent)
    {
        idmap_t::iterator pit(_ids.find(parent_id));

        if (pit==_ids.end())
        {
            std::ostringstream oss;
            oss << "ERROR: stage_data.add_stage() parent_id " << parent_id << " does not exist\n";
            throw blt_exception(oss.str().c_str());
        }

        pos=(pit->second+parent_distance);
    }
    const std::pair<idmap_t::iterator,bool> ret(_ids.insert(std::make_pair(id,pos)));
    if (! ret.second)
    {
        std::ostringstream oss;
        oss << "ERROR: stage_data.add_stage() id " << id << " already exists\n";
        throw blt_exception(oss.str().c_str());
    }
    _stage_pos.push_back(std::make_pair(pos,id));
    // not efficient to do this every time, but we always expect the
    // number of added stages to be very small:
    std::sort(_stage_pos.begin(),_stage_pos.end());
}



void
stage_data::
unknown_id_error(const int id) const
{
    std::ostringstream oss;
    oss << "ERROR: unknown stage_id requested: " << id << "\n";
    throw blt_exception(oss.str().c_str());
}


void
stage_data::
dump(std::ostream& os) const
{
    os << "stage_pos:\n";
    for (const auto& val : _stage_pos)
    {
        os << "pos: " << val.first << " id: " << val.second << "\n";
    }
}



stage_manager::
stage_manager(const stage_data& sdata,
              const pos_range& report_range,
              pos_processor_base& ppb)
    : _sdata(sdata)
    , _is_head_pos(false)
    , _is_first_pos_set(false)
    , _report_range(report_range)
    , _ppb(ppb)
    , _stage_pos_ptr(&(_sdata.stage_pos()))
    , _stage_size(_stage_pos_ptr->size())
    , _is_minpos(_stage_size,0)
    , _minpos(_stage_size,0)
    , _is_any_minpos(0)
{
    assert(_stage_size>0);
}

void
stage_manager::
revise_stage_data(const stage_data& sdata)
{
    // check to see if new sdata qualifies against the restrictions
    // of data revision:
    const stage_data::stage_pos_t& sp(sdata.stage_pos());
    const unsigned sps(sp.size());

    assert(sps==_stage_size);

    for (unsigned i(0); i<sps; ++i)
    {
        const pos_t new_pos(sp[i].first);
        const int new_id(sp[i].second);
        const pos_t old_pos(_sdata.get_stage_id_shift(new_id));
        assert(old_pos<=new_pos);
    }
    // passed!!

    // map up any old minimum values:
    std::map<int,std::pair<bool,pos_t> > old_minpos;
    for (unsigned i(0); i<sps; ++i)
    {
        const int old_id(_stage_pos_ptr->operator[](i).second);
        old_minpos[old_id] = std::make_pair((_is_minpos[i] != 0),_minpos[i]);
    }

    // create new minimum values:
    for (unsigned i(0); i<sps; ++i)
    {
        const pos_t new_pos(sp[i].first);
        const int new_id(sp[i].second);
        const pos_t old_pos(_sdata.get_stage_id_shift(new_id));
        _is_minpos[i] = old_minpos[new_id].first;
        _minpos[i] = old_minpos[new_id].second;
        if (old_pos==new_pos || (!_is_head_pos)) continue;
        _is_minpos[i] = 1;
        _is_any_minpos = 1;
        _minpos[i] = std::max(_minpos[i],_head_pos-old_pos);
    }

    // transfer new value in:
    _sdata=sdata;
    _stage_pos_ptr=&(_sdata.stage_pos());

}


void
stage_manager::
reset()
{
    if (_is_first_pos_set)
    {
        if (_report_range.is_end_pos)
        {
            pos_t final_pos(_report_range.end_pos);
            for (pos_t i(_max_pos+1); i<final_pos; ++i)
            {
                process_pos(i);
            }
        }
    }
    else if (_report_range.is_begin_pos && _report_range.is_end_pos)
    {
        // never read any data in this case, so we just write out
        // the approriate range of zeros for consistency:
        //
        _min_pos=_report_range.begin_pos;
        const pos_t end(_report_range.end_pos);
        for (pos_t i(_report_range.begin_pos); i<end; ++i)
        {
            process_pos(i);
        }
    }
    finish_process_pos();

    // reset to ground state:
    _is_first_pos_set=false;
    _is_head_pos=false;
}



void
stage_manager::
handle_new_pos_value(const pos_t pos)
{
    if (! _is_first_pos_set)
    {
        _max_pos = pos;
        _min_pos = pos;
        if (_report_range.is_begin_pos)
        {
            _min_pos = _report_range.begin_pos;
        }
        for (pos_t i(_min_pos); i<=pos; ++i) process_pos(i);
        _is_first_pos_set = true;
    }

    if (pos < _min_pos)
    {
        _min_pos = pos;
    }

    if (pos > _max_pos)
    {
        // process older positions:
        if (! _is_head_pos)
        {
            _head_pos = _max_pos+1;
            _is_head_pos = true;
        }
        process_pos(pos);
        //for(pos_t i(_max_pos+1);i<=pos;++i) process_pos(i);
        _max_pos = pos;
    }
}



// new positional information has to fit into the buffer for its
// stage:
//
bool
stage_manager::
is_new_pos_value_valid(const pos_t pos,
                       const int stage_id)
{
    // get fshift first to validate stage_id:
    const pos_t fshift(_sdata.get_stage_id_shift(stage_id));
    if (! _is_first_pos_set) return true;
    return (pos > (_max_pos-fshift));
}



void
stage_manager::
validate_new_pos_value(const pos_t pos,
                       const int stage_id)
{
    if (! is_new_pos_value_valid(pos,stage_id))
    {
        std::ostringstream oss;
        oss << "ERROR:: reference sequence position difference too high for multi_stage_circular_buffer\n"
            << "current position:\t" << (pos+1) << "\n"
            << "top position for stage:\t" << (_max_pos+1) << "\n"
            << "stage id:\t" << stage_id << "\n";
        throw blt_exception(oss.str().c_str());
    }
}



static
bool
get_is_any_minpos(const std::vector<uint8_t>& minpos,
                  const unsigned stage_size)
{
    for (unsigned i(0); i<stage_size; ++i) if (minpos[i]) return true;
    return false;
}



void
stage_manager::
process_pos(const pos_t pos)
{
    if (! _is_head_pos)
    {
        _head_pos = pos;
        _is_head_pos = true;
    }

    if (_is_any_minpos)
    {
        for (pos_t p(_head_pos); p<=pos; ++p)
        {
            for (unsigned s(0); s<_stage_size; ++s)
            {
                const pos_t stage_pos(p-static_cast<pos_t>(_stage_pos_ptr->operator[](s).first));
                if (stage_pos<_min_pos) break;
                if (_is_minpos[s] != 0)
                {
                    if (stage_pos<_minpos[s]) continue;
                    _is_minpos[s]=0;
                }
                _ppb.check_process_pos(_stage_pos_ptr->operator[](s).second,stage_pos);
            }
        }
        _is_any_minpos=get_is_any_minpos(_is_minpos,_stage_size);
    }
    else
    {
        for (pos_t p(_head_pos); p<=pos; ++p)
        {
            for (unsigned s(0); s<_stage_size; ++s)
            {
                const pos_t stage_pos(p-static_cast<pos_t>(_stage_pos_ptr->operator[](s).first));
                if (stage_pos<_min_pos) break;
                _ppb.check_process_pos(_stage_pos_ptr->operator[](s).second,stage_pos);
            }
        }
    }

    _head_pos=pos+1;
}



void
stage_manager::
finish_process_pos()
{
    if (! _is_head_pos) return;

    if (_is_any_minpos)
    {
        for (pos_t p(_head_pos); true; ++p)
        {
            pos_t stage_pos(p);
            for (unsigned s(0); s<_stage_size; ++s)
            {
                stage_pos=(p-static_cast<pos_t>(_stage_pos_ptr->operator[](s).first));
                if (stage_pos>=_head_pos) continue;
                if (stage_pos<_min_pos) break;
                if (_is_minpos[s] != 0)
                {
                    if (stage_pos<_minpos[s]) continue;
                    _is_minpos[s]=0;
                }
                _ppb.check_process_pos(_stage_pos_ptr->operator[](s).second,stage_pos);
            }

            if (stage_pos>=_head_pos) break;
        }
        _is_any_minpos=get_is_any_minpos(_is_minpos,_stage_size);
    }
    else
    {
        for (pos_t p(_head_pos); true; ++p)
        {
            pos_t stage_pos(p);
            for (unsigned s(0); s<_stage_size; ++s)
            {
                stage_pos=(p-static_cast<pos_t>(_stage_pos_ptr->operator[](s).first));
                if (stage_pos>=_head_pos) continue;
                if (stage_pos<_min_pos) break;
                _ppb.check_process_pos(_stage_pos_ptr->operator[](s).second,stage_pos);
            }

            if (stage_pos>=_head_pos) break;
        }
    }
}
