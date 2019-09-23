#pragma once

#include <algorithm>
#include <memory>
#include <vector>
#include <random>

#include "carla/client/Actor.h"
#include "carla/client/BlueprintLibrary.h"
#include "carla/client/Map.h"
#include "carla/client/World.h"
#include "carla/geom/Transform.h"
#include "carla/Memory.h"
#include "carla/Logging.h"
#include "carla/rpc/Command.h"

#include "BatchControlStage.h"
#include "CollisionStage.h"
#include "InMemoryMap.h"
#include "LocalizationStage.h"
#include "MotionPlannerStage.h"
#include "TrafficLightStage.h"

#define EXPECT_TRUE(pred) if (!(pred)) { throw std::runtime_error(# pred); }

namespace traffic_manager {

  namespace cc = carla::client;
  namespace cr = carla::rpc;
  using ActorPtr = carla::SharedPtr<cc::Actor>;

  /// Function to read hardware concurrency
  int read_core_count();

  /// Function to spawn specified number of vehicles
  std::vector<ActorPtr> spawn_traffic(
      cc::Client &client,
      cc::World &world,
      int core_count,
      int target_amount);

<<<<<<< HEAD
<<<<<<< HEAD
  /// Work pipeline of traffic manager
=======
=======
  /// Detroy actors
  void destroy_traffic(
    std::vector<ActorPtr>& actor_list,
    cc::Client& client
  );

>>>>>>> 8928735734ab55233731f4ea1ae9b2f039f40cdd
  /// The function of this class is to integrate all the various stages of
  /// the traffic manager appropriately using messengers
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
  class Pipeline {

  private:

    /// PID controller parameters
    std::vector<float> longitudinal_PID_parameters;
    std::vector<float> longitudinal_highway_PID_parameters;
    std::vector<float> lateral_PID_parameters;
<<<<<<< HEAD
    int pipeline_width;
    float highway_target_velocity;
    float urban_target_velocity;
    

=======
    /// Number of worker threads per stage
    int pipeline_width;
    /// Target velocities
    float highway_target_velocity;
    float urban_target_velocity;
    /// Reference to list of all actors registered with traffic manager
<<<<<<< HEAD
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
    std::vector<carla::SharedPtr<carla::client::Actor>> &actor_list;
=======
    std::vector<ActorPtr> &actor_list;
>>>>>>> 8928735734ab55233731f4ea1ae9b2f039f40cdd
    /// Reference to local map cache
    InMemoryMap &local_map;
    /// Reference to carla's debug helper object
    cc::DebugHelper &debug_helper;
    /// Reference to carla's client connection object
    cc::Client &client_connection;
    /// Reference to carla's world object
<<<<<<< HEAD
    carla::client::World &world;
<<<<<<< HEAD

=======
=======
    cc::World &world;
>>>>>>> 8928735734ab55233731f4ea1ae9b2f039f40cdd
    /// Pointers to messenger objects connecting stage pairs
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
    std::shared_ptr<CollisionToPlannerMessenger> collision_planner_messenger;
    std::shared_ptr<LocalizationToCollisionMessenger> localization_collision_messenger;
    std::shared_ptr<LocalizationToTrafficLightMessenger> localization_traffic_light_messenger;
    std::shared_ptr<LocalizationToPlannerMessenger> localization_planner_messenger;
    std::shared_ptr<PlannerToControlMessenger> planner_control_messenger;
    std::shared_ptr<TrafficLightToPlannerMessenger> traffic_light_planner_messenger;
<<<<<<< HEAD

=======
    /// Pointers to stage objects of traffic manager
>>>>>>> e2c8e19611819ecbb7026355674ba94b985ad488
    std::unique_ptr<CollisionStage> collision_stage;
    std::unique_ptr<BatchControlStage> control_stage;
    std::unique_ptr<LocalizationStage> localization_stage;
    std::unique_ptr<MotionPlannerStage> planner_stage;
    std::unique_ptr<TrafficLightStage> traffic_light_stage;

    

  public:

    Pipeline(
        std::vector<float> longitudinal_PID_parameters,
        std::vector<float> longitudinal_highway_PID_parameters,
        std::vector<float> lateral_PID_parameters,
        float urban_target_velocity,
        float highway_target_velocity,
        std::vector<ActorPtr> &actor_list,
        InMemoryMap &local_map,
        cc::Client &client_connection,
        cc::World &world,
        cc::DebugHelper &debug_helper,
        int pipeline_width);

    void Start();

    void Stop();

  };

}
