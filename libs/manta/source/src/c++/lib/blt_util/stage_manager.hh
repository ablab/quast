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

#include "blt_util/pos_processor_base.hh"

#include "blt_util/pos_range.hh"

#include <cassert>

#include <iosfwd>
#include <map>
#include <vector>


/// \brief describes stages to the stage_manager
///
/// Each stage has an integer id.
///
/// All stages are related to each other by a tree. Edge distance on
/// this tree represent bases of the reference. A root stage must be
/// defined first, all following stages must have a parent stage and
/// a distance to the parent.
///
/// \example As used by the stage_manager, a simple two-stage tree
/// where the stages are separated by 100 cycles would mean that the
/// root stage is executed at the reference position pointer (as
/// always), but the second stage is executed at positions 100 bases
/// behind the reference position pointer.
///
struct stage_data
{
    // position,stage_id pair, where position is total distance of this stage from the root stage:
    typedef std::pair<unsigned,int> pos_stage_id;
    // pos_stage_ids, sorted by increasing position and stage id:
    typedef std::vector<pos_stage_id> stage_pos_t;

    /// \brief Add a "root" stage:
    void
    add_stage(const int id)
    {
        return add_stage(id,0,0,false);
    }

    /// \brief Add a child stage which follows at a certain distance from its
    /// parent
    ///
    /// parent id must already have been entered
    ///
    void
    add_stage(const int id,
              const int parent_id,
              const unsigned parent_distance,
              const bool is_parent=true);

    /// The stages are summarized to the stage manager through the
    /// following interface:
    ///
    const stage_pos_t&
    stage_pos() const
    {
        return _stage_pos;
    }

    /// lookup total distance from root stage for any stage id:
    ///
    unsigned
    get_stage_id_shift(const int id) const
    {
        idmap_t::const_iterator i(_ids.find(id));
        if (i==_ids.end()) unknown_id_error(id);
        return i->second;
    }

    /// debug output:
    void
    dump(std::ostream& os) const;

private:
    void
    unknown_id_error(const int id) const;

    // map of stage-id -> total distance from the root stage
    typedef std::map<int,unsigned> idmap_t;

    idmap_t _ids;
    stage_pos_t _stage_pos;
};



/// \brief help to manage information which is being gathered in an
/// approximately sequential fashion and processed in sequence in
/// multiple stages.
///
/// assumes that information related to each position will be
/// available in an approximately sequential fashion, where all
/// position values submitted after position X will be greater than
/// (X - first_stage_buffer_size + 1). A violation of this assumption will
/// trigger a runtime error.
///
/// range policy:
///
/// if begin_pos is not specified, then event processing and
/// reporting start at the first pos >= 0 with position information
/// submitted, else at begin_pos
///
/// if end_pos is not specified, then processing ends after last_pos
/// with information submitted, else at end_pos.
///
struct stage_manager
{
    // stage_data structure is described above
    //
    // report_range is what is sounds like
    //
    // pos_processor_base is where stage manager signals stage
    // execution to
    //
    stage_manager(const stage_data& sdata,
                  const pos_range& report_range,
                  pos_processor_base& ppb);

    // Process any remaining positions
    void
    reset();

    // Handle new pos value is used to indicate a possible advance of
    // the head position -- this represents the position of the input
    // information. Note that you must explicitly check whether pos is
    // too low for any particular stage using the methods further
    // below, handle_new_pos_value does not do this for you.
    //
    // When the head position is advanced, it triggers a series of
    // stage processing steps along the way. The head position is the
    // position from which the distances used in stage_data are
    // measured.
    //
    // Example: extending the stage_data example above -- if
    // stage_data has a root_stage (distance 0) and a child_stage
    // (distance 100 from root), and the current head_position is
    // 1000, then setting handle_new_pos_value(1002) would cause the
    // following sequence of calls to pos_process_base:
    //
    // process_pos(1001,root_stage_id);
    // process_pos(901,child_stage_id);
    // process_pos(1002,root_stage_id);
    // process_pos(902,child_stage_id);
    //
    void
    handle_new_pos_value(const pos_t pos);

    // Return true if stage 'stage_id' has not been run on position
    // yet (ie. pos value is not too low).
    //
    bool
    is_new_pos_value_valid(const pos_t pos,
                           const int stage_id);

    // Test as above, except that an exception is thrown if the
    // position is too low.
    //
    void
    validate_new_pos_value(const pos_t pos,
                           const int stage_id);

    pos_t
    max_pos() const
    {
        return _max_pos;
    }

    pos_t
    min_pos() const
    {
        return _min_pos;
    }

    bool
    is_first_pos_set() const
    {
        return _is_first_pos_set;
    }

    // Revising stage data is very restricted -- the new sdata
    // must have the the same number of stages, with the same set
    // of ids. Stages may only increase in length relative to their
    // prior values.
    //
    void
    revise_stage_data(const stage_data& sdata);

    const stage_data&
    get_stage_data() const
    {
        return _sdata;
    }

private:
    // advances head position from its current value to pos,
    // signaling all stage processing steps to pos_process_base along
    // the way:
    //
    void
    process_pos(const pos_t pos);

    // advances head position until all remaining stage processing is
    // complete based on the current head position value.
    //
    void
    finish_process_pos();

    stage_data _sdata;

    bool _is_head_pos;
    pos_t _head_pos;
    pos_t _max_pos;
    pos_t _min_pos;
    bool _is_first_pos_set;

    const pos_range _report_range;
    pos_processor_base& _ppb;

    const stage_data::stage_pos_t* _stage_pos_ptr;
    unsigned _stage_size;
    std::vector<uint8_t> _is_minpos; //faster lu than vector<bool>
    std::vector<pos_t> _minpos;
    bool _is_any_minpos;
};

