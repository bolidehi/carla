#pragma once

#include "carla/client/Actor.h"
#include "carla/client/Vehicle.h"
#include "carla/Memory.h"
#include "carla/rpc/ActorId.h"

#include "carla/trafficmanager/AtomicActorSet.h"
#include "carla/trafficmanager/AtomicMap.h"

namespace traffic_manager {

    namespace cc = carla::client;
    using ActorPtr = carla::SharedPtr<cc::Actor>;
    using ActorId = carla::ActorId;

    struct ChangeLaneInfo {
        bool change_lane = false;
        bool direction = false;
    };

    class Parameters {

    private:

        /// Target velocity map for individual vehicles.
        AtomicMap<ActorId, float> percentage_decrease_from_speed_limit;
        /// Global target velocity.
        float global_percentage_decrease_from_limit = 0;
        /// Map containing a set of actors to be ignored during collision detection.
        AtomicMap<ActorId, std::shared_ptr<AtomicActorSet>> ignore_collision;
        /// Map containing distance to leading vehicle command.
        AtomicMap<ActorId, float> distance_to_leading_vehicle;
        /// Map containing force lane change commands.
        AtomicMap<ActorId, ChangeLaneInfo> force_lane_change;
        /// Map containing auto lane change commands.
        AtomicMap<ActorId, bool> auto_lane_change;

    public:
        Parameters();
        ~Parameters();

        /// Set target velocity specific to a vehicle.
        void SetPercentageSpeedBelowLimit(const ActorPtr &actor, const float percentage);

        /// Set global target velocity.
        void SetGlobalPercentageBelowLimit(float percentage_below_limit);

        /// Set collision detection rules between vehicles.
        void SetCollisionDetection(const ActorPtr &reference_actor,
                                   const ActorPtr &other_actor,
                                   const bool detect_collision);

        /// Method to force lane change on a vehicle.
        /// Direction flag can be set to true for left and false for right.
        void SetForceLaneChange(const ActorPtr &actor, const bool direction);

        /// Enable / disable automatic lane change on a vehicle.
        void SetAutoLaneChange(const ActorPtr &actor, const bool enable);

        /// Method to specify how much distance a vehicle should maintain to
        /// the leading vehicle.
        void SetDistanceToLeadingVehicle(const ActorPtr &actor, const float distance);

        /// Method to query target velocity for a vehicle.
        float GetVehicleTargetVelocity(const ActorPtr &actor);

        /// Method to query collision avoidance rule between a pair of vehicles.
        bool GetCollisionDetection(const ActorPtr &reference_actor, const ActorPtr &other_actor);

        /// Method to query lane change command for a vehicle.
        ChangeLaneInfo GetForceLaneChange(const ActorPtr &actor);

        /// Method to query auto lane change rule for a vehicle.
        bool GetAutoLaneChange(const ActorPtr &actor);

        /// Method to query distance to leading vehicle for a given vehicle.
        float GetDistanceToLeadingVehicle(const ActorPtr &actor);

        };
}
