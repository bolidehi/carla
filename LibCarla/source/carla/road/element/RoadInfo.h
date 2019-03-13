// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "carla/road/element/RoadInfoVisitor.h"

#include <map>
#include <string>
#include <vector>

namespace carla {
namespace road {
  class MapBuilder;
namespace element {

  class RoadInfo {
  public:

    virtual ~RoadInfo() = default;

    virtual void AcceptVisitor(RoadInfoVisitor &) = 0;

    // distance from Road's start location
    double d; // [meters] @todo: change this to "s"

  protected:

    RoadInfo(double distance = 0) : d(distance) {}
  };

  class RoadGeneralInfo : public RoadInfo {
  private:

    int _junction_id = -1;

    /// A vector of pairs where the first double represents the
    /// offset from the begining of the road and the second represents the
    /// lateral offest of the lane
    std::vector<std::pair<double, double>> _lanes_offset;

  public:

    void AcceptVisitor(RoadInfoVisitor &v) final {
      v.Visit(*this);
    }

    void SetJunctionId(int junctionId) {
      _junction_id = junctionId;
    }

    int GetJunctionId() const {
      return _junction_id;
    }

    bool IsJunction() const {
      return _junction_id >= 0;
    }

    void SetLanesOffset(double start_pos, double lateral_offset) {
      _lanes_offset.emplace_back(std::pair<double, double>(start_pos, lateral_offset));
    }

    /// @returns A vector of pairs where the first double represents the
    /// offset from the begining of the road and the second represents the
    /// lateral offest of the lane
    std::vector<std::pair<double, double>> GetLanesOffset() const {
      return _lanes_offset;
    }
  };



  class LaneInfo {
  public:

    int _id;
    double _width;
    double _lane_center_offset;

    std::string _type;
    std::vector<int> _successor;
    std::vector<int> _predecessor;

    LaneInfo()
      : _id(0),
        _width(0.0),
        _lane_center_offset(0.0) {}

    LaneInfo(int id, double width, const std::string &type)
      : _id(id),
        _width(width),
        _lane_center_offset(0.0),
        _type(type) {}
  };

  class RoadInfoLane : public RoadInfo {
  private:

    friend MapBuilder;

    // int is the lane id (-inf, inf)
    using lane_t = std::map<int, LaneInfo>;
    lane_t _lanes;

  public:

    void AcceptVisitor(RoadInfoVisitor &v) final {
      v.Visit(*this);
    }

    enum class which_lane_e : int {
      Left,
      Right,
      Both
    };

    void addLaneInfo(int id, double width, const std::string &type) {
      _lanes[id] = LaneInfo(id, width, type);
    }

    int size() const {
      return (int) _lanes.size();
    }

    std::vector<int> getLanesIDs(which_lane_e whichLanes = which_lane_e::Both) const {
      std::vector<int> lanes_id;

      for (lane_t::const_iterator it = _lanes.begin(); it != _lanes.end(); ++it) {
        switch (whichLanes) {
          case which_lane_e::Both: {
              lanes_id.emplace_back(it->first);
          } break;

          case which_lane_e::Left: {
            if (it->first > 0) {
              lanes_id.emplace_back(it->first);
            }
          } break;

          case which_lane_e::Right: {
            if (it->first < 0) {
              lanes_id.emplace_back(it->first);
            }
          } break;
        }
      }

      // Sort the lanes IDs ascendent,
      // going from 1 to n
      std::sort(lanes_id.begin(), lanes_id.end());

      // For right lane the IDs are negative,
      // so reverse so sort order to haven them going
      // from -1 to -n
      if (whichLanes == which_lane_e::Right) {
        std::reverse(lanes_id.begin(), lanes_id.end());
      }

      return lanes_id;
    }

    const LaneInfo *getLane(int id) const {
      lane_t::const_iterator it = _lanes.find(id);
      return it == _lanes.end() ? nullptr : &it->second;
    }
  };

} // namespace element
} // namespace road
} // namespace carla
