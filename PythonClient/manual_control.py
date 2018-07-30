#!/usr/bin/env python3

# Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de
# Barcelona (UAB).
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.

# Keyboard controlling for CARLA. Please refer to client_example.py for a simpler
# and more documented example.

"""
Welcome to CARLA control example for humans.

Use ARROWS or WASD keys for control.

    W            : throttle
    S            : brake
    AD           : steer
    Q            : toggle reverse
    Space        : hand-brake
    P            : toggle autopilot

    R            : restart level

If running with map enabled (-m option) try to reach the green dot.

STARTING in a moment...
"""

from __future__ import print_function

import argparse
import logging
import random
import time

from carla import sensor
from carla.client import make_carla_client

from carla.settings import CarlaSettings
from carla.tcp import TCPConnectionError
from game.carla_game import CarlaGame, WindowProperties
from carla.planner import Planner, Waypointer
from carla.agent import HumanAgent, ForwardAgent

WINDOW_WIDTH = 800
WINDOW_HEIGHT = 600
MINI_WINDOW_WIDTH = 320
MINI_WINDOW_HEIGHT = 180


def make_carla_settings(args):
    """
    Make a CarlaSettings object with the settings we need..
    For now that is hardcoded for this version.
    """
    settings = CarlaSettings()
    settings.set(
        SynchronousMode=False,
        SendNonPlayerAgentsInfo=True,
        NumberOfVehicles=15,
        NumberOfPedestrians=30,
        WeatherId=random.choice([1, 3, 7, 8, 14]),
        QualityLevel=args.quality_level)
    settings.randomize_seeds()
    camera0 = sensor.Camera('CameraRGB')
    camera0.set_image_size(WINDOW_WIDTH, WINDOW_HEIGHT)
    camera0.set_position(2.0, 0.0, 1.4)
    camera0.set_rotation(0.0, 0.0, 0.0)
    settings.add_sensor(camera0)
    camera1 = sensor.Camera('CameraDepth', PostProcessing='Depth')
    camera1.set_image_size(MINI_WINDOW_WIDTH, MINI_WINDOW_HEIGHT)
    camera1.set_position(2.0, 0.0, 1.4)
    camera1.set_rotation(0.0, 0.0, 0.0)
    settings.add_sensor(camera1)
    camera2 = sensor.Camera('CameraSemSeg', PostProcessing='SemanticSegmentation')
    camera2.set_image_size(MINI_WINDOW_WIDTH, MINI_WINDOW_HEIGHT)
    camera2.set_position(2.0, 0.0, 1.4)
    camera2.set_rotation(0.0, 0.0, 0.0)
    settings.add_sensor(camera2)
    if args.lidar:
        lidar = sensor.Lidar('Lidar32')
        lidar.set_position(0, 0, 2.5)
        lidar.set_rotation(0, 0, 0)
        lidar.set(
            Channels=32,
            Range=50,
            PointsPerSecond=100000,
            RotationFrequency=10,
            UpperFovLimit=10,
            LowerFovLimit=-30)
        settings.add_sensor(lidar)
    return settings


def make_controlling_agent(args):
    """
    Make the controlling agent object depending on what was selected.
    Right now we have the following options:
    Forward Agent: A trivial agent that just accelerate forward.
    Human Agent: An agent controlled by a human driver, currently only by keyboard.
    """

    if args.controlling_agent == "ForwardAgent":
        return ForwardAgent()
    elif args.controlling_agent == "HumanAgent":
        # TDNextPR: Add parameters such as joysticks to the human agent.
        return HumanAgent()

    else:
        raise ValueError("Selected Agent Does not exist")


def get_directions(measurements, target_transform, planner, waypointer):
    """
    Function to get the high level commands and the waypoints.
    The waypoints correspond to the local planning, the near path the car has to follow.
    """

    #  TD 0.9.X  or 0.10.X: add a new more general and robust planner with wider variety of repr

    # Get the current position from the measurements
    current_point = measurements.player_measurements.transform

    directions = planner.get_next_command(
        (current_point.location.x,
         current_point.location.y, 0.22),
        (current_point.orientation.x,
         current_point.orientation.y,
         current_point.orientation.z),
        (target_transform.location.x, target_transform.location.y, 0.22),
        (target_transform.orientation.x, target_transform.orientation.y,
         target_transform.orientation.z)
    )

    _, waypoints = waypointer.get_next_waypoints(
        (current_point.location.x,
         current_point.location.y, 0.22),
        (current_point.orientation.x, current_point.orientation.y,
         current_point.orientation.z),
        (target_transform.location.x, target_transform.location.y, target_transform.location.z),
        (target_transform.orientation.x, target_transform.orientation.y,
         target_transform.orientation.z)
    )

    return directions, waypoints


def new_episode(client, carla_settings):
    """
    Start a CARLA new episode and generate a target to be pursued on this episode
    Args:
        client: the client connected to CARLA now
        carla_settings: a carla settings object to be used

    Returns:
        Returns the target position for this episode and the name of the current carla map.

    """
    carla_settings.randomize_seeds()
    carla_settings.randomize_weather()
    scene = client.load_settings(carla_settings)

    # We have a predefined set of start end positions, since the planner is not general for
    # all end positions
    if scene.map_name == 'Town01':
        poses = [[105, 29], [27, 130], [102, 87], [132, 27], [24, 44],
                 [96, 26], [34, 67], [28, 1], [140, 134], [105, 9],
                 [148, 129], [65, 18], [21, 16], [147, 97], [42, 51],
                 [30, 41], [18, 107], [69, 45], [102, 95], [18, 145],
                 [111, 64], [79, 45], [84, 69], [73, 31], [37, 81]]

    elif scene.map_name == 'Town02':
        poses = [[19, 66], [79, 14], [19, 57], [23, 1],
                 [53, 76], [42, 13], [31, 71], [33, 5],
                 [54, 30], [10, 61], [66, 3], [27, 12],
                 [79, 19], [2, 29], [16, 14], [5, 57],
                 [70, 73], [46, 67], [57, 50], [61, 49], [21, 12],
                 [51, 81], [77, 68], [56, 65], [43, 54]]

    else:
        raise ValueError("Town not supported by manual control")

    start_end_tuple = random.choice(poses)

    player_target_transform = scene.player_start_spots[start_end_tuple[1]]
    player_start = start_end_tuple[0]

    print('Starting new episode...')
    client.start_episode(player_start)

    return player_target_transform, scene.map_name


def execute(client, args):
    """
    The main loop for the controller example.
    Args:
        client: carla client object
        args: arguments for the

    Returns:
        None
    """

    # Here we instantiate a sample carla settings, for now it is hardcoded on having a rgb a
    # a depth and a semantic segmentation image. If parameter is send there can also be a lidar.
    carla_settings = make_carla_settings(args)
    controlling_agent = make_controlling_agent(args)

    # Start a new episode and create the planner object. Also create a waypointer to generate
    # trajectories.
    player_target_transform, map_name = new_episode(client, carla_settings)

    planner = Planner(map_name)
    waypointer = Waypointer(map_name)
    window_props = WindowProperties(window_width=WINDOW_WIDTH, window_height=WINDOW_HEIGHT,
                                    mini_window_width=MINI_WINDOW_WIDTH,
                                    mini_window_height=MINI_WINDOW_HEIGHT)
    # Py game goes inside the carla game class
    carla_game = CarlaGame(args.autopilot, args.map, window_props)
    carla_game.initialize_game(map_name)
    carla_game.start_timer()
    carla_game.set_objective(player_target_transform.location)

    while carla_game.is_running():

        # we add the vehicle and the connection outside of the game.
        measurements, sensor_data = client.read_data()

        # TD: this will become a neutral route object that can be transformed after to the
        # TD: format needed by the agent.

        _, waypoints = get_directions(measurements, player_target_transform,
                                      planner, waypointer)

        # TD 0.9: This is going to be a vector of controls for each agent.
        # TD 0.9: We should select something like the viewport agent.

        # run a step for the agent. regardless of the type
        control = controlling_agent.run_step(measurements, sensor_data,
                                             waypoints,
                                             player_target_transform)

        # Set the player position
        if args.map:
            position = measurements.player_measurements.transform.location

            agents_positions = measurements.non_player_agents
            # Render with the provided map
            carla_game.draw(sensor_data, player_position=position,
                            waypoints=waypoints, agents_positions=agents_positions)
        else:
            #  For this case we don't need to plot the position

            carla_game.draw(sensor_data)

        if carla_game.is_reset(measurements.player_measurements.transform.location):
            # If you press the reset button, R, starts a new episode
            player_target_transform, map_name = new_episode(client, carla_settings)
            carla_game.start_timer()
            carla_game.set_objective(player_target_transform.location)

        # If you press the autopilot button, P, it changes to autopilot mode.
        if carla_game.is_autopilot_enabled():
            client.send_control(measurements.player_measurements.autopilot_control)
        else:
            client.send_control(control)

        carla_game.print_measurements(measurements)


def main():
    argparser = argparse.ArgumentParser(
        description='CARLA Manual Control Client')
    argparser.add_argument(
        '-v', '--verbose',
        action='store_true',
        dest='debug',
        help='print debug information')
    argparser.add_argument(
        '--host',
        metavar='H',
        default='localhost',
        help='IP of the host server (default: localhost)')
    argparser.add_argument(
        '-p', '--port',
        metavar='P',
        default=2000,
        type=int,
        help='TCP port to listen to (default: 2000)')
    argparser.add_argument(
        '-a', '--autopilot',
        action='store_true',
        help='enable autopilot')
    argparser.add_argument(
        '-l', '--lidar',
        action='store_true',
        help='enable Lidar')
    argparser.add_argument(
        '-q', '--quality-level',
        choices=['Low', 'Epic'],
        type=lambda s: s.title(),
        default='Epic',
        help='graphics quality level, a lower level makes the simulation run considerably faster')
    argparser.add_argument(
        '-m', '--map',
        action='store_true',
        help='plot the map of the current city')
    argparser.add_argument(
        '-c', '--controlling_agent',
        default='HumanAgent',
        help='the controller that is going to be used by the main vehicle.'
             ' Options: '
             ' HumanAgent - Control your agent with a keyboard.'
             ' ForwardAgent - A trivial agent that goes forward')
    args = argparser.parse_args()

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

    logging.info('listening to server %s:%s', args.host, args.port)

    if args.controlling_agent == "HumanAgent":
        print(__doc__)

    while True:
        try:

            with make_carla_client(args.host, args.port) as client:
                # game = CarlaGame(client, args)
                execute(client, args)
                break

        except TCPConnectionError as error:
            logging.error(error)
            time.sleep(1)


if __name__ == '__main__':

    try:
        main()
    except KeyboardInterrupt:
        print('\nCancelled by user. Bye!')
