#include "CollisionStage.h"

<<<<<<< HEAD
namespace bg = boost::geometry;
namespace cg = carla::geom;

namespace CollisionStageConstants
{
  static const float SEARCH_RADIUS = 20.0;
  static const float VERTICAL_OVERLAP_THRESHOLD = 2.0;
  static const float ZERO_AREA = 0.0001;
  static const float BOUNDARY_EXTENSION_MINIMUM = 2.0;
  static const float EXTENSION_SQUARE_POINT = 7.0;
  static const float TIME_HORIZON = 0.5;
  static const float HIGHWAY_SPEED = 50 / 3.6;
  static const float HIGHWAY_TIME_HORIZON = 5.0;
}

using namespace  CollisionStageConstants;
using Actor = carla::SharedPtr<carla::client::Actor>;

namespace traffic_manager {
=======
namespace traffic_manager {

  namespace CollisionStageConstants
  {
    static const float SEARCH_RADIUS = 20.0;
    static const float VERTICAL_OVERLAP_THRESHOLD = 2.0;
    static const float ZERO_AREA = 0.0001;
    static const float BOUNDARY_EXTENSION_MINIMUM = 2.0;
    static const float EXTENSION_SQUARE_POINT = 7.0;
    static const float TIME_HORIZON = 0.5;
    static const float HIGHWAY_SPEED = 50 / 3.6;
    static const float HIGHWAY_TIME_HORIZON = 5.0;
  }

  namespace bg = boost::geometry;
  namespace cg = carla::geom;

  using namespace  CollisionStageConstants;
  using Actor = carla::SharedPtr<carla::client::Actor>;
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488

  CollisionStage::CollisionStage(

    std::shared_ptr<LocalizationToCollisionMessenger> localization_messenger,
    std::shared_ptr<CollisionToPlannerMessenger> planner_messenger,
    int number_of_vehicle,
    int pool_size,
    carla::client::World &world,
    carla::client::DebugHelper &debug_helper):
    localization_messenger(localization_messenger),
    planner_messenger(planner_messenger),
    world(world),
    debug_helper(debug_helper),
    PipelineStage(pool_size, number_of_vehicle) {

      // Initializing clock for checking unregistered actors periodically
      last_world_actors_pass_instance = std::chrono::system_clock::now();
      // Initializing output array selector
      frame_selector = true;
      // Allocating output arrays to be shared with motion planner stage
      planner_frame_a = std::make_shared<CollisionToPlannerFrame>(number_of_vehicle);
      planner_frame_b = std::make_shared<CollisionToPlannerFrame>(number_of_vehicle);
      // Initializing messenger states
      localization_messenger_state = localization_messenger->GetState();
      // Initialize this messenger to preemptively write since it precedes
      // motion planner stage
      planner_messenger_state = planner_messenger->GetState() - 1;

    }

  CollisionStage::~CollisionStage() {}

  void CollisionStage::Action(const int start_index, const int end_index) {

    auto current_planner_frame = frame_selector? planner_frame_a: planner_frame_b;

    // Handle vehicles not spawned by TrafficManager
    // Choosing an arbitrary thread
    if (start_index == 0) {
      auto current_time = std::chrono::system_clock::now();
      std::chrono::duration<double> diff = current_time - last_world_actors_pass_instance;

      // Periodically check for actors not spawned by TrafficManager
      if (diff.count() > 0.5f) {
        auto world_actors = world.GetActors()->Filter("vehicle.*");
        for (auto actor: *world_actors.get()) {
          auto unregistered_id = actor->GetId();
          if (
            id_to_index.find(unregistered_id) == id_to_index.end() &&
            unregistered_actors.find(unregistered_id) == unregistered_actors.end()) {
            unregistered_actors.insert({unregistered_id, actor});
          }
        }
        last_world_actors_pass_instance = current_time;
      }

      // Regularly update unregistered actors
      std::vector<uint> actor_ids_to_erase;
      for (auto actor_info: unregistered_actors) {
        if (actor_info.second->IsAlive()) {
          vicinity_grid.UpdateGrid(actor_info.second);
        } else {
          vicinity_grid.EraseActor(actor_info.first);
          actor_ids_to_erase.push_back(actor_info.first);
        }
      }
      for (auto actor_id: actor_ids_to_erase) {
        unregistered_actors.erase(actor_id);
      }
    }

    // Looping over arrays' partitions for current thread
    for (int i = start_index; i <= end_index; ++i) {

      auto &data = localization_frame->at(i);
      auto ego_actor = data.actor;
      auto ego_actor_id = ego_actor->GetId();

      // Retreive actors around ego actor
      auto actor_id_list = vicinity_grid.GetActors(ego_actor);
      bool collision_hazard = false;

      // Check every actor in vicinity if it poses a collision hazard
      for (auto i = actor_id_list.begin(); (i != actor_id_list.end()) && !collision_hazard; ++i) {
        auto actor_id = *i;
        try {
          
          if (actor_id != ego_actor_id) {

            Actor actor = nullptr;
            if (id_to_index.find(actor_id) != id_to_index.end()) {
              actor = localization_frame->at(id_to_index.at(actor_id)).actor;
            } else if (unregistered_actors.find(actor_id) != unregistered_actors.end()) {
              actor = unregistered_actors.at(actor_id);
            }

            auto ego_actor_location = ego_actor->GetLocation();
            float actor_distance = actor->GetLocation().Distance(ego_actor_location);
            if (actor_distance <= SEARCH_RADIUS) {
              if (NegotiateCollision(ego_actor, actor)) {
                collision_hazard = true;
              }
            }
          }
        } catch (const std::exception &e) {
          carla::log_warning("Encountered problem while determining collision \n");
          carla::log_info("Actor might not be alive \n");
        }

      }

      auto &message = current_planner_frame->at(i);
      message.hazard = collision_hazard;

    }
  }

  void CollisionStage::DataReceiver() {
    auto packet = localization_messenger->ReceiveData(localization_messenger_state);
    localization_frame = packet.data;
    localization_messenger_state = packet.id;

    // Connect actor ids to their position index on data arrays (intput and output)
    // This map also provides us the additional benifit of being able to
    // Quickly identify if a vehicle id is registered with traffic manager or not.
    int index = 0;
    for (auto &element: *localization_frame.get()) {
      id_to_index.insert(std::pair<uint, int>(element.actor->GetId(), index));
      index++;
    }
  }

  void CollisionStage::DataSender() {
    DataPacket<std::shared_ptr<CollisionToPlannerFrame>> packet{
      planner_messenger_state,
      frame_selector? planner_frame_a: planner_frame_b
    };
    frame_selector = !frame_selector;
    planner_messenger_state = planner_messenger->SendData(packet);
  }

  bool CollisionStage::NegotiateCollision(
      Actor ego_vehicle,
      Actor other_vehicle) const {

    // For each vehicle, calculating the dot product between heading vector
    // and relative position vector to the other vehicle.

    auto reference_heading_vector = ego_vehicle->GetTransform().GetForwardVector();
    auto relative_other_vector = other_vehicle->GetLocation() - ego_vehicle->GetLocation();
    relative_other_vector = relative_other_vector.MakeUnitVector();
    auto reference_relative_dot = cg::Math::Dot(reference_heading_vector, relative_other_vector);

    auto other_heading_vector = other_vehicle->GetTransform().GetForwardVector();
    auto relative_reference_vector = ego_vehicle->GetLocation() - other_vehicle->GetLocation();
    relative_reference_vector = relative_reference_vector.MakeUnitVector();
    auto other_relative_dot = cg::Math::Dot(other_heading_vector, relative_reference_vector);

    // Give preference to vehicle who's path has higher angular separation
    // with relative position vector to the other vehicle.
    return (reference_relative_dot > other_relative_dot &&
           CheckGeodesicCollision(ego_vehicle, other_vehicle));
  }

  bool CollisionStage::CheckGeodesicCollision(
      Actor reference_vehicle,
      Actor other_vehicle) const {
<<<<<<< HEAD
=======

>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
    bool overlap = false;
    auto reference_height = reference_vehicle->GetLocation().z;
    auto other_height = other_vehicle->GetLocation().z;
    if (abs(reference_height - other_height) < VERTICAL_OVERLAP_THRESHOLD) {

      auto reference_geodesic_boundary = GetGeodesicBoundary(reference_vehicle);
      auto other_geodesic_boundary = GetGeodesicBoundary(other_vehicle);
      
      if (reference_geodesic_boundary.size() > 0 && other_geodesic_boundary.size() > 0) {
      
        auto reference_polygon = GetPolygon(reference_geodesic_boundary);
        auto other_polygon = GetPolygon(other_geodesic_boundary);

        std::deque<polygon> output;
        bg::intersection(reference_polygon, other_polygon, output);

<<<<<<< HEAD
<<<<<<< HEAD
        for(polygon const& p: output) {
=======
        // for(polygon const& p: output) {
=======
>>>>>>> 8928735734ab55233731f4ea1ae9b2f039f40cdd
        for(int i = 0u; i < output.size() && !overlap; ++i) {
          auto& p = output.at(i);
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
          if (bg::area(p) > ZERO_AREA) {
            overlap = true;
          }
        }
      }
    }

    return overlap;
  }

<<<<<<< HEAD
  traffic_manager::polygon
  CollisionStage::GetPolygon(const std::vector<cg::Location> &boundary) const {

    std::string _string;
    for (auto location: boundary) {
      _string += std::to_string(location.x) + " " + std::to_string(location.y) + ",";
    }
    _string += std::to_string(boundary[0].x) + " " + std::to_string(boundary[0].y);

    traffic_manager::polygon boundary_polygon;
    bg::read_wkt("POLYGON((" + _string + "))", boundary_polygon);
=======
  traffic_manager::polygon CollisionStage::GetPolygon(const std::vector<cg::Location> &boundary) const {

    std::string boundary_polygon_string;
    for (auto location: boundary) {
      boundary_polygon_string += std::to_string(location.x) + " " + std::to_string(location.y) + ",";
    }
    boundary_polygon_string += std::to_string(boundary[0].x) + " " + std::to_string(boundary[0].y);

    traffic_manager::polygon boundary_polygon;
    bg::read_wkt("POLYGON((" + boundary_polygon_string + "))", boundary_polygon);
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488

    return boundary_polygon;
  }

  std::vector<cg::Location> CollisionStage::GetGeodesicBoundary(Actor actor) const {
  
    auto bbox = GetBoundary(actor);

    if (id_to_index.find(actor->GetId()) != id_to_index.end()) {

      auto velocity = actor->GetVelocity().Length();
      int bbox_extension = static_cast<int>(
        std::max(std::sqrt(EXTENSION_SQUARE_POINT * velocity), BOUNDARY_EXTENSION_MINIMUM) +
        std::max(velocity * TIME_HORIZON, BOUNDARY_EXTENSION_MINIMUM) +
        BOUNDARY_EXTENSION_MINIMUM
        );

      bbox_extension = (velocity > HIGHWAY_SPEED) ? (HIGHWAY_TIME_HORIZON * velocity) : bbox_extension;
      auto &waypoint_buffer =  localization_frame->at(id_to_index.at(actor->GetId())).buffer;

      std::vector<cg::Location> left_boundary;
      std::vector<cg::Location> right_boundary;
      auto vehicle = boost::static_pointer_cast<carla::client::Vehicle>(actor);
      float width = vehicle->GetBoundingBox().extent.y;

      for (int i = 0; (i < bbox_extension) && (i < waypoint_buffer->size()); ++i) {

        auto swp = waypoint_buffer->at(i);
        auto vector = swp->GetVector();
        auto location = swp->GetLocation();
        auto perpendicular_vector = cg::Vector3D(-vector.y, vector.x, 0);
        perpendicular_vector = perpendicular_vector.MakeUnitVector();
        // Direction determined for left handed system
        left_boundary.push_back(location + cg::Location(perpendicular_vector * width));
        right_boundary.push_back(location - cg::Location(perpendicular_vector * width));
      }

      // Connecting geodesic path boundary with vehicle bounding box
      std::vector<cg::Location> geodesic_boundary;   
      // Reversing right boundary to construct clocwise (left hand system) boundary
      // This is so because both left and right boundary vectors have the closest
      // point to the vehicle at their starting index
      // For right boundary we want to begin at the farthest point to have a clocwise trace
      std::reverse(right_boundary.begin(), right_boundary.end());
      geodesic_boundary.insert(geodesic_boundary.end(), right_boundary.begin(), right_boundary.end());
      geodesic_boundary.insert(geodesic_boundary.end(), bbox.begin(), bbox.end());
      geodesic_boundary.insert(geodesic_boundary.end(), left_boundary.begin(), left_boundary.end());

      return geodesic_boundary;
    } else {

      return bbox;  
    }
  }

<<<<<<< HEAD
  std::vector<cg::Location>
  CollisionStage::GetBoundary(Actor actor) const {
=======
  std::vector<cg::Location> CollisionStage::GetBoundary(Actor actor) const {
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488

    auto vehicle = boost::static_pointer_cast<carla::client::Vehicle>(actor);
    auto bbox = vehicle->GetBoundingBox();
    auto extent = bbox.extent;
    auto location = vehicle->GetLocation();
    auto heading_vector = vehicle->GetTransform().GetForwardVector();
    heading_vector.z = 0;
    auto perpendicular_vector = cg::Vector3D(-heading_vector.y, heading_vector.x, 0);

    // Four corners of the vehicle in top view clockwise order (left handed system)
    auto x_boundary_vector = heading_vector * extent.x;
    auto y_boundary_vector = perpendicular_vector * extent.y;
    return {
      location + cg::Location(x_boundary_vector - y_boundary_vector),
      location + cg::Location(-1* x_boundary_vector - y_boundary_vector),
      location + cg::Location(-1* x_boundary_vector + y_boundary_vector),
      location + cg::Location(x_boundary_vector + y_boundary_vector),
    };
  }

  void CollisionStage::DrawBoundary(const std::vector<cg::Location> &boundary) const {
    for (int i = 0; i < boundary.size(); ++i) {
      debug_helper.DrawLine(
          boundary[i] + cg::Location(0, 0, 1),
          boundary[(i + 1) % boundary.size()] + cg::Location(0, 0, 1),
<<<<<<< HEAD
          0.1f, {255U, 0U, 0U}, 0.1f);
=======
          0.1f, {255u, 0u, 0u}, 0.1f);
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
    }
  }
}
