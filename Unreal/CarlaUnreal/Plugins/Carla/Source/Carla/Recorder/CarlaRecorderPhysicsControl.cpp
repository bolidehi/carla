// Copyright (c) 2024 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "CarlaRecorderPhysicsControl.h"
#include "CarlaRecorder.h"
#include "CarlaRecorderHelpers.h"

#include <compiler/disable-ue4-macros.h>
#include "carla/rpc/VehiclePhysicsControl.h"
#include <compiler/enable-ue4-macros.h>


void CarlaRecorderPhysicsControl::Write(std::ostream &OutFile)
{
  carla::rpc::VehiclePhysicsControl RPCPhysicsControl(VehiclePhysicsControl);
  WriteValue<uint32_t>(OutFile, this->DatabaseId);
  WriteValue(OutFile, RPCPhysicsControl.max_torque);
  WriteValue(OutFile, RPCPhysicsControl.max_rpm);
  WriteValue(OutFile, RPCPhysicsControl.moi);
  WriteValue(OutFile, RPCPhysicsControl.rev_down_rate);
  WriteValue(OutFile, RPCPhysicsControl.differential_type);
  WriteValue(OutFile, RPCPhysicsControl.front_rear_split);
  WriteValue(OutFile, RPCPhysicsControl.use_gear_autobox);
  WriteValue(OutFile, RPCPhysicsControl.final_ratio);
  WriteValue(OutFile, RPCPhysicsControl.change_up_rpm);
  WriteValue(OutFile, RPCPhysicsControl.change_down_rpm);
  WriteValue(OutFile, RPCPhysicsControl.transmission_efficiency);
  WriteValue(OutFile, RPCPhysicsControl.mass);
  WriteValue(OutFile, RPCPhysicsControl.drag_coefficient);
  WriteValue(OutFile, RPCPhysicsControl.center_of_mass);

  // torque curve
  WriteStdVector(OutFile, RPCPhysicsControl.torque_curve);

  // forward gears
  WriteStdVector(OutFile, RPCPhysicsControl.forward_gears);

  // reverse gears
  WriteStdVector(OutFile, RPCPhysicsControl.reverse_gears);

  // steering curve
  WriteStdVector(OutFile, RPCPhysicsControl.steering_curve);

  // wheels
  WriteStdVector(OutFile, RPCPhysicsControl.wheels);
}

void CarlaRecorderPhysicsControl::Read(std::istream &InFile)
{
  carla::rpc::VehiclePhysicsControl RPCPhysicsControl;
  ReadValue<uint32_t>(InFile, this->DatabaseId);
  ReadValue(InFile, RPCPhysicsControl.max_torque);
  ReadValue(InFile, RPCPhysicsControl.max_rpm);
  ReadValue(InFile, RPCPhysicsControl.moi);
  ReadValue(InFile, RPCPhysicsControl.rev_down_rate);
  ReadValue(InFile, RPCPhysicsControl.differential_type);
  ReadValue(InFile, RPCPhysicsControl.front_rear_split);
  ReadValue(InFile, RPCPhysicsControl.use_gear_autobox);
  ReadValue(InFile, RPCPhysicsControl.final_ratio);
  ReadValue(InFile, RPCPhysicsControl.change_up_rpm);
  ReadValue(InFile, RPCPhysicsControl.transmission_efficiency);
  ReadValue(InFile, RPCPhysicsControl.final_ratio);
  ReadValue(InFile, RPCPhysicsControl.mass);
  ReadValue(InFile, RPCPhysicsControl.drag_coefficient);
  ReadValue(InFile, RPCPhysicsControl.center_of_mass);

  // torque curve
  ReadStdVector(InFile, RPCPhysicsControl.torque_curve);

  // forward gears
  ReadStdVector(InFile, RPCPhysicsControl.forward_gears);

  // reverse gears
  ReadStdVector(InFile, RPCPhysicsControl.reverse_gears);

  // steering curve
  ReadStdVector(InFile, RPCPhysicsControl.steering_curve);

  // wheels
  ReadStdVector(InFile, RPCPhysicsControl.wheels);

  VehiclePhysicsControl = FVehiclePhysicsControl(RPCPhysicsControl);
}

// ---------------------------------------------

void CarlaRecorderPhysicsControls::Clear(void)
{
  PhysicsControls.clear();
}

void CarlaRecorderPhysicsControls::Add(const CarlaRecorderPhysicsControl &InObj)
{
  PhysicsControls.push_back(InObj);
}

void CarlaRecorderPhysicsControls::Write(std::ostream &OutFile)
{
  if (PhysicsControls.size() == 0)
  {
    return;
  }
  // write the packet id
  WriteValue<char>(OutFile, static_cast<char>(CarlaRecorderPacketId::PhysicsControl));

  std::streampos PosStart = OutFile.tellp();
  // write dummy packet size
  uint32_t Total = 0;
  WriteValue<uint32_t>(OutFile, Total);

  // write total records
  Total = PhysicsControls.size();
  WriteValue<uint16_t>(OutFile, Total);

  // write records
  for (auto& PhysicsControl : PhysicsControls)
  {
    PhysicsControl.Write(OutFile);
  }

  // write the real packet size
  std::streampos PosEnd = OutFile.tellp();
  Total = PosEnd - PosStart - sizeof(uint32_t);
  OutFile.seekp(PosStart, std::ios::beg);
  WriteValue<uint32_t>(OutFile, Total);
  OutFile.seekp(PosEnd, std::ios::beg);
}
