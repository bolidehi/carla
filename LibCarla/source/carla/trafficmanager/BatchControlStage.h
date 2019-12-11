#pragma once

#include <memory>

#include "carla/client/Client.h"
#include "carla/Logging.h"
#include "carla/rpc/ActorId.h"
#include "carla/rpc/Command.h"
#include "carla/rpc/VehicleControl.h"

#include "MessengerAndDataTypes.h"
#include "PipelineStage.h"

namespace traffic_manager {

namespace cc = carla::client;
namespace cr = carla::rpc;

  /// This class receives actuation signals (throttle, brake, steer)
  /// from MotionPlannerStage class and communicates these signals to
  /// the simulator in batches to control vehicles' movement.
  class BatchControlStage : public PipelineStage {

  private:

    /// Variable to remember messenger state.
    int messenger_state;
    /// Pointer to frame received from MotionPlanner.
    std::shared_ptr<PlannerToControlFrame> data_frame;
    /// Pointer to a messenger from MotionPlanner.
    std::shared_ptr<PlannerToControlMessenger> messenger;
    /// Reference to carla client connection object.
    cc::Client &carla_client;
    /// Array to hold command batch.
    std::shared_ptr<std::vector<cr::Command>> commands;
    /// Number of vehicles registered with the traffic manager.
    uint number_of_vehicles;

  public:

    BatchControlStage(
        std::shared_ptr<PlannerToControlMessenger> messenger,
        cc::Client &carla_client);
    ~BatchControlStage();

    void DataReceiver() override;

    void Action() override;

    void DataSender() override;

  };

}
