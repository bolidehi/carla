// Copyright (c) 2019 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>

#include "carla/client/Waypoint.h"
#include "carla/geom/Location.h"
#include "carla/geom/Math.h"
#include "carla/Memory.h"
#include "carla/road/Lane.h"
#include "carla/road/RoadTypes.h"

#include "carla/trafficmanager/SimpleWaypoint.h"

namespace carla {
namespace traffic_manager {

  namespace cg = carla::geom;
  namespace cc = carla::client;
  namespace crd = carla::road;

  using WaypointPtr = carla::SharedPtr<cc::Waypoint>;
  using TopologyList = std::vector<std::pair<WaypointPtr, WaypointPtr>>;
  using SimpleWaypointPtr = std::shared_ptr<SimpleWaypoint>;
  using NodeList = std::vector<SimpleWaypointPtr>;
  using RawNodeList = std::vector<WaypointPtr>;
  using GeoGridId = crd::JuncId;

  /// This class builds a discretized local map-cache.
  /// Instantiate the class with map topology from the simulator
  /// and run SetUp() to construct the local map.
  class InMemoryMap {

  private:

    /// Object to hold sparse topology received by the constructor.
    RawNodeList raw_dense_topology;
    /// Structure to hold all custom waypoint objects after
    /// interpolation of sparse topology.
    NodeList dense_topology;
    /// Grid localization map for all waypoints in the system.
    using WaypointGrid = std::unordered_map<std::string, std::unordered_set<SimpleWaypointPtr>>;
    WaypointGrid waypoint_grid;
    /// Larger localization map for all waypoints to be used for localizing pedestrians.
    WaypointGrid ped_waypoint_grid;
    /// Geodesic grid topology.
    std::unordered_map<GeoGridId, cg::Location> geodesic_grid_center;

    /// Method to generate the grid ids for given co-ordinates.
    std::pair<int, int> MakeGridId(float x, float y, bool vehicle_or_pedestrian);

    /// Method to generate map key for waypoint_grid.
    std::string MakeGridKey(std::pair<int, int> gird_id);

    /// This method is used to find and place lane change links.
    void FindAndLinkLaneChange(SimpleWaypointPtr reference_waypoint);

  public:

    InMemoryMap(RawNodeList _raw_dense_topology);
    ~InMemoryMap();

    /// This method constructs the local map with a resolution of
    /// sampling_resolution.
    void SetUp();

    /// This method returns the closest waypoint to a given location on the map.
    SimpleWaypointPtr GetWaypoint(const cg::Location &location) const;

    /// This method returns closest waypoint in the vicinity of the given co-ordinates.
    SimpleWaypointPtr GetWaypointInVicinity(cg::Location location);

    SimpleWaypointPtr GetPedWaypoint(cg::Location location);

    /// This method returns the full list of discrete samples of the map in the
    /// local cache.
    NodeList GetDenseTopology() const;

    void MakeGeodesiGridCenters();
    cg::Location GetGeodesicGridCenter(GeoGridId ggid);

  };

} // namespace traffic_manager
} // namespace carla
