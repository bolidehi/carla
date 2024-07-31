# Copyright (c) # Copyright (c) 2018-2020 CVC.
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.


""" This module implements an agent that roams around a track following random
waypoints and avoiding other vehicles. The agent also responds to traffic lights,
traffic signs, and has different possible configurations. """

import random
from typing import Union
import numpy as np
import carla
from agents.navigation.basic_agent import BasicAgent
from agents.navigation.local_planner import RoadOption
from agents.conf.behavior_types import Cautious, Aggressive, Normal
from agents.conf.agent_settings_backend import BehaviorAgentSettings, AgentConfig, SimpleConfig

from agents.tools.misc import get_speed, positive, is_within_distance, compute_distance, lanes_have_same_direction

class BehaviorAgent(BasicAgent):
    """
    BehaviorAgent implements an agent that navigates scenes to reach a given
    target destination, by computing the shortest possible path to it.
    This agent can correctly follow traffic signs, speed limitations,
    traffic lights, while also taking into account nearby vehicles. Lane changing
    decisions can be taken by analyzing the surrounding environment such as tailgating avoidance.
    Adding to these are possible behaviors, the agent can also keep safety distance
    from a car in front of it by tracking the instantaneous time to collision
    and keeping it in a certain range. Finally, different sets of behaviors
    are encoded in the agent, from cautious to a more aggressive ones.
    """
    config : BehaviorAgentSettings

    def __init__(self, vehicle, behavior: Union[str, AgentConfig]='normal', opt_dict={}, map_inst=None, grp_inst=None):
        """
        Constructor method.

            :param vehicle: actor to apply to local planner logic onto
            :param behavior: type of agent to apply
        """

        self._look_ahead_steps = 0

        # Parameters for agent behavior
        if isinstance(behavior, str):
            if behavior == 'cautious':
                self.config = Cautious(overwrites=opt_dict)

            elif behavior == 'normal':
                self.config = Normal(overwrites=opt_dict)

            elif behavior == 'aggressive':
                self.config = Aggressive(overwrites=opt_dict)
        elif isinstance(behavior, AgentConfig):
            self.config = behavior
            if opt_dict:
                print("Warning: opt_dict is ignored when behavior is an instance of AgentConfig. Initialize the settings with the updates parameters beforehand.") # TODO: maybe can call update.
        elif isinstance(behavior, type) and issubclass(behavior, (AgentConfig, SimpleConfig)):
            self.config = behavior(opt_dict)
        else:
            raise TypeError("Behavior must be a string or a subclass of AgentConfig or a SimpleConfig") # TODO: maybe raise a warning instead and assume user passed a fitting structure.
        
        if self.config.avoid_tailgators:
            self._tailgate_counter = 0
        else:
            self._tailgate_counter = -1 # Does not trigger the == 0 condition.
        
        super().__init__(vehicle, opt_dict=self.config, map_inst=map_inst, grp_inst=grp_inst)


    def _update_information(self):
        """
        This method updates the information regarding the ego
        vehicle based on the surrounding world.
        """
        # Speed
        self.config.live_info.current_speed = get_speed(self._vehicle)
        self.config.live_info.current_speed_limit = self._vehicle.get_speed_limit()
        self._local_planner.set_speed(self.config.live_info.current_speed_limit) # note: currently redundant, but will print warning.
        
        # Location
        self.config.live_info.current_location = self._vehicle.get_location()
        self.config.live_info.current_waypoint = self._map.get_waypoint(self.config.live_info.current_location)
        
        # Direction
        # NOTE: This is set at local_planner.run_step and the executed direction from the *last* step
        self._direction = self._local_planner.target_road_option
        if self._direction is None:
            self._direction = RoadOption.LANEFOLLOW

        # Upcoming direction
        self._look_ahead_steps = int((self.config.live_info.current_speed_limit) / 10)

        # Will take the direction that is a few meters in front of the vehicle depending on the speed
        self._incoming_waypoint, self._incoming_direction = self._local_planner.get_incoming_waypoint_and_direction(
            steps=self._look_ahead_steps)
        if self._incoming_direction is None:
            self._incoming_direction = RoadOption.LANEFOLLOW

    def traffic_light_manager(self):
        """
        This method is in charge of behaviors for red lights.
        """
        actor_list = self._world.get_actors()
        lights_list = actor_list.filter("*traffic_light*")
        if self.config.obstacles.use_dynamic_speed_threshold:
            # Basic agent setting:
            max_tlight_distance = self.config.obstacles.base_tlight_threshold + self.config.obstacles.detection_speed_ratio * self.config.live_info.current_speed
        else:
            # Behavior setting:
            max_tlight_distance = self.config.obstacles.base_tlight_threshold
        affected, _ = self._affected_by_traffic_light(lights_list, max_distance=max_tlight_distance)

        return affected

    def plan_lane_change(self, vehicle_list, order=["left", "right"], up_angle_th=180, low_angle_th=0, speed_limit_look_ahead_factor=0.5):
        if vehicle_list is None:
            vehicle_list = self._world.get_actors().filter("*vehicle*")
        waypoint = self.config.live_info.current_waypoint
        
        for direction in order:
            if direction == "right":
                right_turn = waypoint.right_lane_marking.lane_change
                can_change = (right_turn == carla.LaneChange.Right or right_turn == carla.LaneChange.Both)
                other_wpt = waypoint.get_right_lane()
                lane_offset = 1
            else:
                left_turn = waypoint.left_lane_marking.lane_change
                can_change = (left_turn == carla.LaneChange.Left or left_turn == carla.LaneChange.Both)
                other_wpt = waypoint.get_left_lane()
                lane_offset = -1
            if can_change and lanes_have_same_direction(waypoint, other_wpt) and other_wpt.lane_type == carla.LaneType.Driving:
                # Detect if right lane is free
                affected_by_vehicle, _, _ = self._vehicle_obstacle_detected(vehicle_list, 
                                                max(self.config.distance.min_proximity_threshold,
                                                    self.config.live_info.current_speed_limit * speed_limit_look_ahead_factor), 
                                                    up_angle_th=up_angle_th,
                                                    low_angle_th=low_angle_th,
                                                    lane_offset=lane_offset)
                if not affected_by_vehicle:
                    print("Changing lane, moving to the %s!", direction)
                    end_waypoint = self._local_planner.target_waypoint
                    self.set_destination(end_waypoint.transform.location,
                                            other_wpt.transform.location)
                    return True
        


    def _tailgating(self, waypoint, vehicle_list):
        """
        This method is in charge of tailgating behaviors.
        If a faster vehicle is behind the agent it will try to change the lane.

            :param waypoint: current waypoint of the agent
            :param vehicle_list: list of all the nearby vehicles
        """

        behind_vehicle_state, behind_vehicle, _ = self._vehicle_obstacle_detected(vehicle_list, max(
            self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 2), up_angle_th=180, low_angle_th=160)
        if behind_vehicle_state and self.config.live_info.current_speed < get_speed(behind_vehicle):
            changes_lane = self.plan_lane_change(vehicle_list, order=("right", "left"))
            if changes_lane:
                self._tailgate_counter = 200
        
    def collision_and_car_avoid_manager(self, waypoint):
        """
        This module is in charge of warning in case of a collision
        and managing possible tailgating chances.

            :param location: current location of the agent
            :param waypoint: current waypoint of the agent
            :return vehicle_state: True if there is a vehicle nearby, False if not
            :return vehicle: nearby vehicle
            :return distance: distance to nearby vehicle
        """

        vehicle_list = self._world.get_actors().filter("*vehicle*")
        def dist(v): return v.get_location().distance(waypoint.transform.location)
        vehicle_list = [v for v in vehicle_list if dist(v) < 45 and v.id != self._vehicle.id]

        if self._direction == RoadOption.CHANGELANELEFT:
            vehicle_state, vehicle, distance = self._vehicle_obstacle_detected(
                vehicle_list, max(
                    self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 2), up_angle_th=180, lane_offset=-1)
        elif self._direction == RoadOption.CHANGELANERIGHT:
            vehicle_state, vehicle, distance = self._vehicle_obstacle_detected(
                vehicle_list, max(
                    self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 2), up_angle_th=180, lane_offset=1)
        else:
            vehicle_state, vehicle, distance = self._vehicle_obstacle_detected(
                vehicle_list, max(
                    self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 3), up_angle_th=30)

            # Check for tailgating
            if not vehicle_state and self._direction == RoadOption.LANEFOLLOW \
                    and not waypoint.is_junction and self.config.live_info.current_speed > 10 \
                    and self._tailgate_counter == 0:
                self._tailgating(waypoint, vehicle_list)

        return vehicle_state, vehicle, distance

    def pedestrian_avoid_manager(self, waypoint):
        """
        This module is in charge of warning in case of a collision
        with any pedestrian.

            :param location: current location of the agent
            :param waypoint: current waypoint of the agent
            :return vehicle_state: True if there is a walker nearby, False if not
            :return vehicle: nearby walker
            :return distance: distance to nearby walker
        """

        walker_list = self._world.get_actors().filter("*walker.pedestrian*")
        def dist(w): return w.get_location().distance(waypoint.transform.location)
        walker_list = [w for w in walker_list if dist(w) < 10]

        if self._direction == RoadOption.CHANGELANELEFT:
            walker_state, walker, distance = self._vehicle_obstacle_detected(walker_list, max(
                self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 2), up_angle_th=90, lane_offset=-1)
        elif self._direction == RoadOption.CHANGELANERIGHT:
            walker_state, walker, distance = self._vehicle_obstacle_detected(walker_list, max(
                self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 2), up_angle_th=90, lane_offset=1)
        else:
            walker_state, walker, distance = self._vehicle_obstacle_detected(walker_list, max(
                self.config.distance.min_proximity_threshold, self.config.live_info.current_speed_limit / 3), up_angle_th=60)

        return walker_state, walker, distance

    _epsilon = np.nextafter(0., 1.)

    def car_following_manager(self, vehicle, distance, debug=False):
        """
        Module in charge of car-following behaviors when there's
        someone in front of us.

            :param vehicle: car to follow
            :param distance: distance from vehicle
            :param debug: boolean for debugging
            :return control: carla.VehicleControl
        """

        vehicle_speed = get_speed(vehicle)
        delta_v = max(1, (self.config.live_info.current_speed - vehicle_speed) / 3.6)
        time_to_collision = distance / delta_v if delta_v != 0 else distance / self._epsilon

        # Under safety time distance, slow down.
        if self.config.speed.safety_time > time_to_collision > 0.0:
            target_speed = min([
                positive(vehicle_speed - self.config.speed.speed_decrease),
                self.config.speed.max_speed,
                self.config.live_info.current_speed_limit - self.config.speed.speed_lim_dist])
            self._local_planner.set_speed(target_speed)
            control = self._local_planner.run_step(debug=debug)

        # Actual safety distance area, try to follow the speed of the vehicle in front.
        elif 2 * self.config.speed.safety_time > time_to_collision >= self.config.speed.safety_time:
            target_speed = min([
                max(self.config.speed.min_speed, vehicle_speed),
                self.config.speed.max_speed,
                self.config.live_info.current_speed_limit - self.config.speed.speed_lim_dist])
            self._local_planner.set_speed(target_speed)
            control = self._local_planner.run_step(debug=debug)

        # Normal behavior.
        else:
            target_speed = min([
                self.config.speed.max_speed,
                self.config.live_info.current_speed_limit - self.config.speed.speed_lim_dist])
            self._local_planner.set_speed(target_speed)
            control = self._local_planner.run_step(debug=debug)

        return control

    def run_step(self, debug=False):
        """
        Execute one step of navigation.

            :param debug: boolean for debugging
            :return control: carla.VehicleControl
        """
        self._update_information()

        control = None
        if self._tailgate_counter > 0:
            self._tailgate_counter -= 1

        ego_vehicle_loc = self.config.live_info.current_location
        ego_vehicle_wp = self.config.live_info.current_waypoint

        # 1: Red lights and stops behavior
        if self.traffic_light_manager():
            return self.emergency_stop()

        # 2.1: Pedestrian avoidance behaviors
        walker_state, walker, w_distance = self.pedestrian_avoid_manager(ego_vehicle_wp)

        if walker_state:
            # Distance is computed from the center of the two cars,
            # we use bounding boxes to calculate the actual distance
            distance = w_distance - max(
                walker.bounding_box.extent.y, walker.bounding_box.extent.x) - max(
                    self._vehicle.bounding_box.extent.y, self._vehicle.bounding_box.extent.x)

            # Emergency brake if the car is very close.
            if distance < self.config.distance.emergency_braking_distance:
                return self.emergency_stop()

        # 2.2: Car following behaviors
        vehicle_state, vehicle, distance = self.collision_and_car_avoid_manager(ego_vehicle_wp)

        if vehicle_state:
            # Distance is computed from the center of the two cars,
            # we use bounding boxes to calculate the actual distance
            distance = distance - max(
                vehicle.bounding_box.extent.y, vehicle.bounding_box.extent.x) - max(
                    self._vehicle.bounding_box.extent.y, self._vehicle.bounding_box.extent.x)

            # Emergency brake if the car is very close.
            if distance < self.config.distance.emergency_braking_distance:
                return self.emergency_stop()
            else:
                control = self.car_following_manager(vehicle, distance)

        # 3: Intersection behavior
        elif self._incoming_waypoint.is_junction and (self._incoming_direction in [RoadOption.LEFT, RoadOption.RIGHT]):
            target_speed = min([
                self.config.speed.max_speed,
                self.config.live_info.current_speed_limit - self.config.speed.intersection_speed_decrease])
            self._local_planner.set_speed(target_speed)
            control = self._local_planner.run_step(debug=debug)

        # 4: Normal behavior
        else:
            target_speed = min([
                self.config.speed.max_speed,
                self.config.live_info.current_speed_limit - self.config.speed.speed_lim_dist])
            self._local_planner.set_speed(target_speed)
            control = self._local_planner.run_step(debug=debug)

        return control

    def emergency_stop(self):
        """
        Overwrites the throttle a brake values of a control to perform an emergency stop.
        The steering is kept the same to avoid going out of the lane when stopping during turns

            :param speed (carl.VehicleControl): control to be modified
        """
        control = carla.VehicleControl()
        control.throttle = 0.0
        control.brake = self.config.controls.max_brake
        control.hand_brake = False
        return control
