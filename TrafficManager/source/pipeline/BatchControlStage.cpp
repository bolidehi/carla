#include "BatchControlStage.h"

namespace traffic_manager {

  BatchControlStage::BatchControlStage(
      std::shared_ptr<PlannerToControlMessenger> messenger,
      cc::Client &carla_client,
      uint number_of_vehicles,
      uint pool_size)
    : messenger(messenger),
      carla_client(carla_client),
      PipelineStage(pool_size, number_of_vehicles) {

    // Initializing messenger state.
    messenger_state = messenger->GetState();
    // Allocating array for command batching.
    commands = std::make_shared<std::vector<carla::rpc::Command>>(number_of_vehicles);
  }

  BatchControlStage::~BatchControlStage() {}

  void BatchControlStage::Action(const uint start_index, const uint end_index) {

    // Looping over arrays' partitions for the current thread.
    for (uint i = start_index; i <= end_index; ++i) {

      carla::rpc::VehicleControl vehicle_control;

      PlannerToControlData &element = data_frame->at(i);
      carla::ActorId actor_id = element.actor_id;
      vehicle_control.throttle = element.throttle;
      vehicle_control.brake = element.brake;
      vehicle_control.steer = element.steer;

      commands->at(i) = carla::rpc::Command::ApplyVehicleControl(actor_id, vehicle_control);
    }
  }

  void BatchControlStage::DataReceiver() {

    auto packet = messenger->ReceiveData(messenger_state);
    data_frame = packet.data;
    messenger_state = packet.id;
  }

  void BatchControlStage::DataSender() {

    carla_client.ApplyBatch(*commands.get());

    // Limiting updates to 100 frames per second.
    std::this_thread::sleep_for(10ms);
  }
}
