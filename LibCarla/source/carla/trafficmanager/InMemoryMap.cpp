#include "InMemoryMap.h"

namespace traffic_manager {

namespace MapConstants {
  // Very important that this is less than 10^-4.
  static const float ZERO_LENGTH = 0.0001f;
  static const float INFINITE_DISTANCE = std::numeric_limits<float>::max();
  static const uint LANE_CHANGE_LOOK_AHEAD = 5u;
  // Cosine of the angle.
  static const float LANE_CHANGE_ANGULAR_THRESHOLD = 0.5f;
  static const float GRID_SIZE = 4.0f;
}
  using namespace MapConstants;

  InMemoryMap::InMemoryMap(TopologyList topology) {
    _topology = topology;
  }
  InMemoryMap::~InMemoryMap() {}

  void InMemoryMap::SetUp(float sampling_resolution) {

    NodeList entry_node_list;
    NodeList exit_node_list;

    auto distance_squared = [](cg::Location l1, cg::Location l2) {
          return cg::Math::DistanceSquared(l1, l2);
        };
    auto square = [](float input) {return std::pow(input, 2);};

    // Creating dense topology.
    for (auto &pair : _topology) {

      // Looping through every topology segment.
      WaypointPtr begin_waypoint = pair.first;
      WaypointPtr end_waypoint = pair.second;
      cg::Location begin_location = begin_waypoint->GetTransform().location;
      cg::Location end_location = end_waypoint->GetTransform().location;

      if (distance_squared(begin_location, end_location) > square(ZERO_LENGTH)) {

        // Adding entry waypoint.
        WaypointPtr current_waypoint = begin_waypoint;
        dense_topology.push_back(std::make_shared<SimpleWaypoint>(current_waypoint));

        entry_node_list.push_back(dense_topology.back());

        // Populating waypoints from begin_waypoint to end_waypoint.
        while (distance_squared(current_waypoint->GetTransform().location,
            end_location) > square(sampling_resolution)) {

          current_waypoint = current_waypoint->GetNext(sampling_resolution)[0];
          SimpleWaypointPtr previous_wp = dense_topology.back();
          dense_topology.push_back(std::make_shared<SimpleWaypoint>(current_waypoint));

          previous_wp->SetNextWaypoint({dense_topology.back()});
        }

        // Adding exit waypoint.
        SimpleWaypointPtr previous_wp = dense_topology.back();
        dense_topology.push_back(std::make_shared<SimpleWaypoint>(end_waypoint));

        previous_wp->SetNextWaypoint({dense_topology.back()});
        exit_node_list.push_back(dense_topology.back());
      }
    }

    // Linking segments.
    uint i = 0, j = 0;
    for (SimpleWaypointPtr end_point : exit_node_list) {
      for (SimpleWaypointPtr begin_point : entry_node_list) {
        if (end_point->DistanceSquared(begin_point) < square(ZERO_LENGTH) and i != j) {
          end_point->SetNextWaypoint({begin_point});
        }
        ++j;
      }
      ++i;
    }

    // Tying up loose ends.
    // Loop through all exit nodes of topology segments,
    // connect any dangling endpoints to the nearest entry point
    // of another topology segment.
    i = 0;
    for (auto &end_point : exit_node_list) {
      if (end_point->GetNextWaypoint().size() == 0) {
        j = 0;
        float min_distance = INFINITE_DISTANCE;
        SimpleWaypointPtr closest_connection;
        for (auto &begin_point : entry_node_list) {
          float new_distance = end_point->DistanceSquared(begin_point);
          if (new_distance < min_distance and i != j) {
            min_distance = new_distance;
            closest_connection = begin_point;
          }
          ++j;
        }
        cg::Vector3D end_point_vector = end_point->GetForwardVector();
        cg::Vector3D relative_vector = closest_connection->GetLocation() - end_point->GetLocation();
        relative_vector = relative_vector.MakeUnitVector();
        float relative_dot = cg::Math::Dot(end_point_vector, relative_vector);
        if (relative_dot < LANE_CHANGE_ANGULAR_THRESHOLD) {
          uint count = LANE_CHANGE_LOOK_AHEAD;
          while (count > 0) {
            closest_connection = closest_connection->GetNextWaypoint()[0];
            --count;
          }
        }
        end_point->SetNextWaypoint({closest_connection});
      }
      ++i;
    }

    // Localizing waypoints into grids.
    for (auto &simple_waypoint: dense_topology) {
      cg::Location loc = simple_waypoint->GetLocation();
      std::string grid_key = MakeGridKey(MakeGridId(loc.x, loc.y));
      if (waypoint_grid.find(grid_key) == waypoint_grid.end()) {
        waypoint_grid.insert({grid_key, {simple_waypoint}});
      } else {
        waypoint_grid.at(grid_key).insert(simple_waypoint);
      }
    }

    // Linking lane change connections.
    for (auto &simple_waypoint:dense_topology) {
      if (!simple_waypoint->CheckJunction()) {
        FindAndLinkLaneChange(simple_waypoint);
      }
    }
  }

  std::pair<int, int> InMemoryMap::MakeGridId (float x, float y) {
    return {static_cast<int>(std::floor(x/GRID_SIZE)), static_cast<int>(std::floor(y/GRID_SIZE))};
  }

  std::string InMemoryMap::MakeGridKey (std::pair<int , int> grid_key) {
    return std::to_string(grid_key.first) + std::to_string(grid_key.second);
  }

  SimpleWaypointPtr InMemoryMap::GetWaypointInVicinity(cg::Location location) {

    std::pair<int, int> grid_ids = MakeGridId(location.x, location.y);
    SimpleWaypointPtr closest_waypoint = nullptr;
    float closest_distance = INFINITE_DISTANCE;

    // Search all surrounding grids for closest waypoint.
    for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {

        std::string grid_key = MakeGridKey({grid_ids.first + i, grid_ids.second + j});
        if (waypoint_grid.find(grid_key) != waypoint_grid.end()) {

          auto& waypoint_set = waypoint_grid.at(grid_key);
          if (closest_waypoint == nullptr) {
            closest_waypoint = *waypoint_set.begin();
          }

          for (auto &simple_waypoint: waypoint_set) {

            if (simple_waypoint->DistanceSquared(location) < std::pow(closest_distance, 2)) {
              closest_waypoint = simple_waypoint;
              closest_distance = simple_waypoint->DistanceSquared(location);
            }
          }
        }
      }
    }

    // Return the closest waypoint in the surrounding grids
    // only if it is in the same horizontal plane as the requested location.
    if (std::abs(closest_waypoint->GetLocation().z - location.z) > 1.0) { // Account for constant.
      closest_waypoint = nullptr;
    }

    return closest_waypoint;
  }

  SimpleWaypointPtr InMemoryMap::GetWaypoint(const cg::Location &location) const {

    SimpleWaypointPtr closest_waypoint;
    float min_distance = INFINITE_DISTANCE;
    for (auto &simple_waypoint : dense_topology) {
      float current_distance = simple_waypoint->DistanceSquared(location);
      if (current_distance < min_distance) {
        min_distance = current_distance;
        closest_waypoint = simple_waypoint;
      }
    }
    return closest_waypoint;
  }

  std::vector<SimpleWaypointPtr> InMemoryMap::GetDenseTopology() const {
    return dense_topology;
  }

  void InMemoryMap::FindAndLinkLaneChange(SimpleWaypointPtr reference_waypoint) {

    WaypointPtr raw_waypoint = reference_waypoint->GetWaypoint();
    uint8_t lane_change = static_cast<uint8_t>(raw_waypoint->GetLaneChange());
    uint8_t change_right = static_cast<uint8_t>(carla::road::element::LaneMarking::LaneChange::Right);
    uint8_t change_left = static_cast<uint8_t>(carla::road::element::LaneMarking::LaneChange::Left);

    try {
      if ((lane_change & change_right) > 0) {
        WaypointPtr right_waypoint =  raw_waypoint->GetRight();
        SimpleWaypointPtr closest_simple_waypoint = GetWaypointInVicinity(right_waypoint->GetTransform().location);
        if (closest_simple_waypoint == nullptr) {
          closest_simple_waypoint = GetWaypoint(right_waypoint->GetTransform().location);
        }
        reference_waypoint->SetRightWaypoint(closest_simple_waypoint);
      }
      if ((lane_change & change_left) > 0) {
        WaypointPtr left_waypoint =  raw_waypoint->GetLeft();
        SimpleWaypointPtr closest_simple_waypoint = GetWaypointInVicinity(left_waypoint->GetTransform().location);
        if (closest_simple_waypoint == nullptr) {
          closest_simple_waypoint = GetWaypoint(left_waypoint->GetTransform().location);
        }
        reference_waypoint->SetLeftWaypoint(closest_simple_waypoint);
      }
    } catch (const std::invalid_argument &e) {
      cg::Location loc = reference_waypoint->GetLocation();
      carla::log_info(
        "Unable to link lane change connection at: "
        + std::to_string(loc.x) + " "
        + std::to_string(loc.y) + " "
        + std::to_string(loc.z));
    }
  }
}
